// include/llc_logger.h
#ifndef LLC_LOGGER_H
#define LLC_LOGGER_H

#include <string>
#include <fstream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
#include <map>
#include <memory>
#include <cstring>
#include <utility>

// 日志文件大小配置宏
#define MAX_FILE_SIZE (500 * 1024)    // 单个日志文件最大500KB---500 * 1024
#define MAX_TOTAL_SIZE (200 * 1024 * 1024)  // 总日志文件最大200MB---200 * 1024 * 1024
#define LOG_FLUSH_INTERVAL 500 // 每处理多少条日志后执行检查和清理
#define LLC_LOG_DIR "/home/ftp/llc_logs" //日志文件路径

// RT 专用 SPSC 字节环总容量。不是单条日志上限；单条日志是变长记录。
// 如果一条 RT 日志大于整个 ring 的可用容量，只能丢弃，不能阻塞或动态扩容。
#ifndef LLC_LOG_RT_RING_BYTES
#define LLC_LOG_RT_RING_BYTES (8 * 1024 * 1024)
#endif

// NRT MPSC 队列最大节点数。用于防止非实时日志无限占用内存。
#ifndef LLC_LOG_NRT_QUEUE_CAP
#define LLC_LOG_NRT_QUEUE_CAP 50000
#endif

// 文件日志按时间归并的重排窗口，单位 us。
// 窗口应大于 log() 取时间戳到记录成功入队的最大延迟。
#ifndef LLC_LOG_ORDER_WINDOW_US
#define LLC_LOG_ORDER_WINDOW_US 20000
#endif

// 文件批量 flush 间隔。终端实时显示由 log() 内非阻塞 writev 保证。
#ifndef LLC_LOG_FILE_FLUSH_INTERVAL_MS
#define LLC_LOG_FILE_FLUSH_INTERVAL_MS 200
#endif

// 日志线程空闲时短睡眠，避免空转占 CPU。
#ifndef LLC_LOG_THREAD_IDLE_SLEEP_US
#define LLC_LOG_THREAD_IDLE_SLEEP_US 1000
#endif

// 日志线程每轮从各队列最多取出的记录数。必须有界，避免持续生产时一直
// 卡在 drain 阶段，无法执行文件写入、flush 和丢弃统计。
#ifndef LLC_LOG_DRAIN_BATCH_ITEMS
#define LLC_LOG_DRAIN_BATCH_ITEMS 4096
#endif

enum class LogLevel {
    INFO,
    ERROR
};

struct AxisStatus {
    double position;     // 空间位置
    double jointPos;     // 关节位置
    double velocity;     // 速度
    double acceleration; // 加速度
    double current;      // 电流
    uint16_t controlWord; // 控制字
    uint16_t statusWord;  // 状态字
    int errorCode;       // 错误码

    std::string toString() const {
        std::stringstream ss;
        ss << "Pos: " << position << ", JointPos: " << jointPos
           << ", Vel: " << velocity << ", Acc: " << acceleration
           << ", Curr: " << current << ", Ctrl: 0x" << std::hex << controlWord
           << ", Stat: 0x" << statusWord << std::dec
           << ", ErrCode: " << errorCode;
        return ss.str();
    }
};

class Logger {
public:
    struct Statistics {
        uint64_t rtEnqueued;
        uint64_t rtDropped;
        uint64_t nrtEnqueued;
        uint64_t nrtDropped;
        uint64_t consoleWritten;
        uint64_t consoleDropped;
        uint64_t fileWritten;
    };

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    static Logger& instance();

    void init(const std::string& logDir);

    // 外部宏名不变；这里增加 overload，避免字面量日志在进入 log() 前被强制构造成 std::string。
    void log(LogLevel level, const char* funcName, int lineNum,
             const char* paramStr, const char* customMsg);
    void log(LogLevel level, const char* funcName, int lineNum,
             const std::string& paramStr, const std::string& customMsg);
    void log(LogLevel level, const char* funcName, int lineNum,
             const std::string& paramStr, const char* customMsg);
    void log(LogLevel level, const char* funcName, int lineNum,
             const char* paramStr, const std::string& customMsg);

    // ecatcycle_worker() 入口调用一次。注册后该线程走 RT 专用 SPSC ring。
    void registerRealtimeThread();

    // 如果未来 RT 线程会退出，可在退出前调用；当前 ecatcycle_worker() 是无限循环，可不调用。
    void unregisterRealtimeThread();

    void shutdown();

    // 丢失日志提示函数：返回并清零丢失计数，日志线程会定期写入文件。
    std::string takeDropNotice();

    // 返回进程启动以来的累计统计。用于压力测试前后做差值。
    Statistics getStatistics() const;

private:
    Logger();
    ~Logger();

    struct TextView {
        const char* data;
        size_t size;
    };
    static TextView makeTextView(const char* text);
    static TextView makeTextView(const std::string& text);

    struct LogItem {
        uint64_t seq;
        int64_t epochUs;
        LogLevel level;
        int line;
        std::string func;
        std::string param;
        std::string msg;
    };

    // RT ring 内部记录头。payload 布局为 func + param + msg，均不强制 NUL 结尾。
    struct RtRecordHeader {
        uint32_t magic;
        uint32_t level;
        int32_t line;
        uint32_t funcSize;
        uint32_t paramSize;
        uint32_t msgSize;
        uint64_t totalSize;
        uint64_t seq;
        int64_t epochUs;
    };

    struct CacheLineU64 {
        volatile uint64_t value;
        char padding[64 - sizeof(uint64_t)];
    };

    // 单写单读字节环：写者只能是 ecatcycle，读者只能是日志线程。
    // 不使用 mutex，不使用 std::atomic；用 __atomic_* 发布读写位置。
    class RtSpscByteRing {
    public:
        RtSpscByteRing();
        bool init(size_t capacityBytes);
        bool push(uint64_t seq, int64_t epochUs, LogLevel level, const char* funcName,
                  int lineNum, TextView param, TextView msg);
        size_t drain(std::vector<LogItem>& out, size_t maxItems);
        bool empty() const;

    private:
        static constexpr uint32_t RECORD_MAGIC = 0x484C4F47; // 'HLOG'

        void copyToRing(uint64_t pos, const void* src, size_t len);
        void copyFromRing(uint64_t pos, void* dst, size_t len) const;

        std::vector<char> m_vBuffer_;
        size_t m_sCapacity_;

        // 分离 cache line，降低日志线程读位置更新对 RT 写位置的干扰。
        CacheLineU64 m_stReadPos_;
        CacheLineU64 m_stWritePos_;
    };

    struct NrtNode {
        NrtNode* next;
        LogItem item;
    };

    // NRT 多生产者单消费者队列。仅 NRT 线程入队，日志线程出队。
    // 使用 __atomic_exchange_n 发布节点，不使用 std::atomic。
    class NrtMpscQueue {
    public:
        NrtMpscQueue();
        ~NrtMpscQueue();
        bool init(size_t maxNodes);
        bool push(LogItem&& item);
        size_t drain(std::vector<LogItem>& out, size_t maxItems);
        bool empty() const;
        void clear();

    private:
        NrtNode m_stStub_;
        NrtNode* m_pHead_;
        NrtNode* m_pTail_;
        size_t m_sMaxNodes_;
        volatile uint64_t m_ullApproxSize_;
    };

    void logImpl(LogLevel level, const char* funcName, int lineNum,
                 TextView param, TextView msg);
    bool isRealtimeThread() const;

    static int64_t nowEpochUsec();
    static std::string formatEpochUsec(int64_t epochUs);
    static const char* levelText(LogLevel level);

    void drainQueues(std::vector<LogItem>& out);
    void writeReadyLogs(std::vector<LogItem>& pending, bool forceAll);
    std::string formatLogEntry(const LogItem& item) const;
    void flushFileIfNeeded(bool force);

    void configureConsoleNonBlocking();
    void restoreConsoleFlags();
    void tryConsoleWriteNonBlocking(LogLevel level, int64_t epochUs, const char* funcName,
                                    int lineNum, TextView param, TextView msg);
    void writeDropNoticeToConsole(const std::string& notice);

    std::string m_sCurrentLogPath_;
    std::ofstream m_sLogFile_;
    std::string m_sLogDir_;

    RtSpscByteRing m_stRtRing_;
    NrtMpscQueue m_stNrtQueue_;

    std::thread m_tLogThread_;

    // 不用 std::atomic；跨线程状态用 __atomic_* 访问。
    volatile uint32_t m_uRunning_;
    volatile uint64_t m_ullSeq_;
    volatile uint64_t m_ullRtDropped_;
    volatile uint64_t m_ullNrtDropped_;
    volatile uint64_t m_ullConsoleDropped_;
    volatile uint64_t m_ullRtEnqueuedTotal_;
    volatile uint64_t m_ullRtDroppedTotal_;
    volatile uint64_t m_ullNrtEnqueuedTotal_;
    volatile uint64_t m_ullNrtDroppedTotal_;
    volatile uint64_t m_ullConsoleWrittenTotal_;
    volatile uint64_t m_ullConsoleDroppedTotal_;
    volatile uint64_t m_ullFileWrittenTotal_;

    int m_nConsoleFd_;
    bool m_bConsoleConfigured_;

    std::chrono::steady_clock::time_point m_tLastFileFlush_;

    static thread_local bool m_bRealtimeThread_;


    void rotateLog();
    void cleanupOldLogs();
    std::string getCurrentTime();
    std::string setLogFileName();
    void processLogs();
    void writeToFile(const std::string& logEntry);

};

std::string getProgramDir();

// -------------------------- 核心：辅助函数--------------------------
// 辅助函数：用于检测类型是否为vector
template<typename T>
struct is_vector : std::false_type {};

template<typename T>
struct is_vector<std::vector<T>> : std::true_type {};

// 辅助函数：将vector转换为字符串 - 使用inline避免多重定义
template<typename T>
inline std::string vector_to_string(const std::vector<T>& vec) {
    std::ostringstream oss;
    //oss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << ",";
        oss << vec[i];
    }
    //oss << "]";
    return oss.str();
}

// 辅助函数：根据类型选择输出方式
template<typename T>
inline auto format_param(const T& arg) -> decltype(std::declval<std::ostringstream&>() << arg, std::string()) {
    std::ostringstream oss;
    oss << arg;
    return oss.str();
}

template<typename T>
inline std::string format_param(const std::vector<T>& arg) {
    return vector_to_string(arg);
}

// 主函数：各个参数之间用；分隔，同一个参数是vector之间用，分隔
template <typename... Args>
inline std::string llc_log_get_param_string_1(const char* param_names, Args&&... args) {
    std::ostringstream oss;
    //oss << param_names << " = ";  // 拼接参数名（如 "path, port"）
    
    // C++11 参数包展开：用初始化列表+逗号表达式，遍历所有参数值
    int idx = 0;
    using expander = int[];  // 辅助展开的空数组类型
    (void)expander{  // 强制类型转换避免未使用变量警告
        0,  // 初始化列表第一个元素（避免空参数包报错）
        (   // 每个参数执行的逻辑：
            oss << (idx++ == 0 ? "" : "; ")  // 非第一个参数前加分号分隔
                << format_param(std::forward<Args>(args)),  // 根据类型选择输出方式
                
            0  // 逗号表达式返回 0，填充数组
        )...  // 展开所有参数
    };
    return oss.str();
}
/* 主函数：前两个参数用逗号分隔
          从第二个参数开始，每个参数前加分号和空格
          只有第三个参数显示参数名"results = "
*/
template <typename... Args>
inline std::string llc_log_get_param_string(const char* param_names, Args&&... args) {
    std::ostringstream oss;

    // C++11 参数包展开：用初始化列表+逗号表达式，遍历所有参数值
    int idx = 0;
    using expander = int[];  // 辅助展开的空数组类型
    (void)expander{  // 强制类型转换避免未使用变量警告
        0,  // 初始化列表第一个元素（避免空参数包报错）
        (   // 每个参数执行的逻辑：
            oss << (idx == 0 ? "" : (idx == 1 ? "," : "; "))  // 第一个参数不加分隔符，第二个参数加逗号，其他参数加分号
                << (idx >= 2 ? "results = " : "")  // 只有第三个参数及以后显示参数名
                << format_param(std::forward<Args>(args)),  // 根据类型选择输出方式
            idx++,  // 增加索引
            0  // 逗号表达式返回 0，填充数组
        )...  // 展开所有参数
    };
    return oss.str();
}

// -------------------------- 宏封装：自动传递级别+位置+参数 --------------------------
// 1. LOG_INFO：无参数，直接调用Logger::log，funcName用__func__（GCC7.5.0更兼容）
// 关键：
// - 用__func__替代__FUNCTION__（GCC7.5.0中__func__是标准C++11标识符，展开更稳定），C++预定义的标识符，会自动替换为当前函数名
// - __LINE__ C++预定义的宏，会自动替换为当前代码所在的行号
// - 所有参数直接传递，无中间宏嵌套
// - 每个参数用括号保护，避免语法歧义
#define LLC_LOG_INFO(customMsg) \
    do { \
        Logger::instance().log( \
            (LogLevel::INFO), \
            (__func__),  /* __func__是const char*，GCC7.5.0直接支持，无需转换 */ \
            (__LINE__), \
            (""),  /* 无参数，空字符串 */ \
            (customMsg) \
        ); \
    } while(0)

// 2. LOG_ERROR：与LOG_INFO逻辑一致，仅级别不同
#define LLC_LOG_ERROR(customMsg) \
    do { \
        Logger::instance().log( \
            (LogLevel::ERROR), \
            (__func__), \
            (__LINE__), \
            (""), \
            (customMsg) \
        ); \
    } while(0)

// 3. LOG_INFO_WITH_PARAMS：带参数，直接拼接参数字符串 显式构造参数字符串：参数名 = 参数值 
#define LLC_LOG_INFO_WITH_PARAMS_1(customMsg, ...) \
    do { \
        std::string _param_str = llc_log_get_param_string_1(#__VA_ARGS__, ##__VA_ARGS__); \
        Logger::instance().log( \
            (LogLevel::INFO), \
            (__func__), \
            (__LINE__), \
            (_param_str), \
            (customMsg) \
        ); \
    } while(0)
//CMD格式
#define LLC_LOG_INFO_WITH_PARAMS(customMsg, ...) \
    do { \
        std::string _param_str = llc_log_get_param_string(#__VA_ARGS__, ##__VA_ARGS__); \
        Logger::instance().log( \
            (LogLevel::INFO), \
            (__func__), \
            (__LINE__), \
            (_param_str), \
            (customMsg) \
        ); \
    } while(0)

// 4. LOG_ERROR_WITH_PARAMS：与LOG_INFO_WITH_PARAMS逻辑一致
#define LLC_LOG_ERROR_WITH_PARAMS(customMsg, ...) \
    do { \
        std::string _param_str = llc_log_get_param_string_1(#__VA_ARGS__, ##__VA_ARGS__); \
        Logger::instance().log( \
            (LogLevel::ERROR), \
            (__func__), \
            (__LINE__), \
            (_param_str), \
            (customMsg) \
        ); \
    } while(0)

#endif // LLC_LOGGER_H

