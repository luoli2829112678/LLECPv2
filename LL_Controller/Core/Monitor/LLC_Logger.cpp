// src/llc_logger.cpp
#include "LLC_Logger.h"
#include <sys/stat.h>
#include <unistd.h>
//#include <experimental/filesystem>
#include <iostream>
#include <algorithm>
#include <fcntl.h>
#include <sys/uio.h>
#include <errno.h>
#include <new>
#include <limits>

thread_local bool Logger::m_bRealtimeThread_ = false;

#include <dirent.h>    // 用于目录操作
#include <sys/stat.h>  // 用于文件状态


Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : m_uRunning_(0),
      m_ullSeq_(0),
      m_ullRtDropped_(0),
      m_ullNrtDropped_(0),
      m_ullConsoleDropped_(0),
      m_ullRtEnqueuedTotal_(0),
      m_ullRtDroppedTotal_(0),
      m_ullNrtEnqueuedTotal_(0),
      m_ullNrtDroppedTotal_(0),
      m_ullConsoleWrittenTotal_(0),
      m_ullConsoleDroppedTotal_(0),
      m_ullFileWrittenTotal_(0),
      m_nConsoleFd_(-1),
      m_bConsoleConfigured_(false)
{
}

Logger::~Logger()
{
    shutdown();
}

/**
 * @brief 从 C 风格字符串构造 TextView 对象
 * @param text 输入的字符串指针
 * @return TextView 包含字符串数据和长度的视图，空指针时返回空视图
 */
Logger::TextView Logger::makeTextView(const char* text)
{
    if (text == nullptr) {
        return { "", 0 };
    }
    return { text, std::strlen(text) };
}
/**
 * @brief 从 std::string 构造 TextView 对象
 * @param text 输入的字符串引用
 * @return TextView 包含字符串数据和长度的视图
 */
Logger::TextView Logger::makeTextView(const std::string& text)
{
    return { text.data(), text.size() };
}
/**
 * @brief 获取当前时间的微秒级 UNIX 时间戳
 * @return int64_t 自纪元以来的微秒数
 */
int64_t Logger::nowEpochUsec()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000LL + ts.tv_nsec / 1000;
}
/**
 * @brief 将微秒级 UNIX 时间戳格式化为可读的字符串
 * @param epochUs 微秒级时间戳
 * @return std::string 格式化后的时间字符串，如 "YYYY-MM-DD HH:MM:SS.us"
 */
std::string Logger::formatEpochUsec(int64_t epochUs)
{
    time_t sec = static_cast<time_t>(epochUs / 1000000LL);
    int usec = static_cast<int>(epochUs % 1000000LL);

    struct tm tmValue;
    localtime_r(&sec, &tmValue);

    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tmValue);

    char out[96];
    std::snprintf(out, sizeof(out), "%s.%06d", timeBuf, usec);
    return std::string(out);
}
/**
 * @brief 获取日志级别的文本标识
 * @param level 日志级别
 * @return const char* 对应的级别文本，如 "[ERROR] " 或 "[INFO]  "
 */
const char* Logger::levelText(LogLevel level)
{
    return level == LogLevel::ERROR ? "[ERROR] " : "[INFO]  ";
}
/**
 * @brief RT SPSC 字节环构造函数，初始化读写位置和容量
 */
Logger::RtSpscByteRing::RtSpscByteRing()
    : m_sCapacity_(0)
{
    m_stReadPos_.value = 0;
    m_stWritePos_.value = 0;
}
/**
 * @brief 初始化 RT SPSC 字节环，分配内存并重置读写位置
 * @param capacityBytes 环形缓冲区的字节容量
 * @return bool 初始化成功返回 true，容量不足返回 false
 */
bool Logger::RtSpscByteRing::init(size_t capacityBytes)
{
    m_vBuffer_.assign(capacityBytes, 0);
    m_sCapacity_ = m_vBuffer_.size();
    __atomic_store_n(&m_stReadPos_.value, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_stWritePos_.value, 0, __ATOMIC_RELEASE);
    return m_sCapacity_ > sizeof(RtRecordHeader);
}
/**
 * @brief 将数据拷贝到环形缓冲区，处理环绕情况
 * @param pos 写入的起始绝对位置
 * @param src 源数据指针
 * @param len 数据长度
 */
void Logger::RtSpscByteRing::copyToRing(uint64_t pos, const void* src, size_t len)
{
    const char* p = static_cast<const char*>(src);
    size_t idx = static_cast<size_t>(pos % m_sCapacity_);
    size_t first = std::min(len, m_sCapacity_ - idx);
    std::memcpy(&m_vBuffer_[idx], p, first);
    if (len > first) {
        std::memcpy(&m_vBuffer_[0], p + first, len - first);
    }
}
/**
 * @brief 从环形缓冲区拷贝数据，处理环绕情况
 * @param pos 读取的起始绝对位置
 * @param dst 目标数据指针
 * @param len 数据长度
 */
void Logger::RtSpscByteRing::copyFromRing(uint64_t pos, void* dst, size_t len) const
{
    char* p = static_cast<char*>(dst);
    size_t idx = static_cast<size_t>(pos % m_sCapacity_);
    size_t first = std::min(len, m_sCapacity_ - idx);
    std::memcpy(p, &m_vBuffer_[idx], first);
    if (len > first) {
        std::memcpy(p + first, &m_vBuffer_[0], len - first);
    }
}
/**
 * @brief 向 RT 环形缓冲区推入一条日志记录（仅 RT 线程调用）
 * @param seq 日志序号
 * @param epochUs 微秒级时间戳
 * @param level 日志级别
 * @param funcName 函数名
 * @param lineNum 行号
 * @param param 参数视图
 * @param msg 消息视图
 * @return bool 推入成功返回 true，缓冲区满或记录过大返回 false
 */
bool Logger::RtSpscByteRing::push(uint64_t seq, int64_t epochUs, LogLevel level,
                                  const char* funcName, int lineNum,
                                  TextView param, TextView msg)
{
    if (m_sCapacity_ <= sizeof(RtRecordHeader)) {
        return false;
    }

    TextView func = Logger::makeTextView(funcName);
    uint64_t totalSize = sizeof(RtRecordHeader) + func.size + param.size + msg.size;

    // 不设单条固定长度槽位；但 RT ring 是预分配内存，单条记录不能超过整个 ring。
    if (totalSize > m_sCapacity_) {
        return false;
    }

    uint64_t writePos = m_stWritePos_.value; // 只有 RT 写者修改。
    uint64_t readPos = __atomic_load_n(&m_stReadPos_.value, __ATOMIC_ACQUIRE);
    uint64_t used = writePos - readPos;
    uint64_t freeBytes = static_cast<uint64_t>(m_sCapacity_) - used;

    if (freeBytes < totalSize) {
        return false;
    }

    RtRecordHeader header;
    header.magic = RECORD_MAGIC;
    header.level = static_cast<uint32_t>(level);
    header.line = lineNum;
    header.funcSize = static_cast<uint32_t>(func.size);
    header.paramSize = static_cast<uint32_t>(param.size);
    header.msgSize = static_cast<uint32_t>(msg.size);
    header.totalSize = totalSize;
    header.seq = seq;
    header.epochUs = epochUs;

    uint64_t pos = writePos;
    copyToRing(pos, &header, sizeof(header));
    pos += sizeof(header);
    if (func.size != 0) {
        copyToRing(pos, func.data, func.size);
        pos += func.size;
    }
    if (param.size != 0) {
        copyToRing(pos, param.data, param.size);
        pos += param.size;
    }
    if (msg.size != 0) {
        copyToRing(pos, msg.data, msg.size);
    }

    // release 发布完整记录。日志线程 acquire 读取后才能看到 payload。
    __atomic_store_n(&m_stWritePos_.value, writePos + totalSize, __ATOMIC_RELEASE);
    return true;
}
/**
 * @brief 从 RT 环形缓冲区批量提取日志记录（仅日志线程调用）
 * @param out 输出的日志条目向量
 * @param maxItems 最大提取数量
 * @return size_t 实际提取的记录数量
 */
size_t Logger::RtSpscByteRing::drain(std::vector<LogItem>& out, size_t maxItems)
{
    size_t count = 0;
    uint64_t readPos = m_stReadPos_.value; // 只有日志线程修改。
    uint64_t writePos = __atomic_load_n(&m_stWritePos_.value, __ATOMIC_ACQUIRE);

    while (readPos < writePos && count < maxItems) {
        RtRecordHeader header;
        copyFromRing(readPos, &header, sizeof(header));

        if (header.magic != RECORD_MAGIC ||
            header.totalSize < sizeof(RtRecordHeader) ||
            header.totalSize > m_sCapacity_ ||
            readPos + header.totalSize > writePos) {
            break;
        }

        LogItem item;
        item.seq = header.seq;
        item.epochUs = header.epochUs;
        item.level = static_cast<LogLevel>(header.level);
        item.line = header.line;

        uint64_t pos = readPos + sizeof(RtRecordHeader);
        item.func.resize(header.funcSize);
        if (header.funcSize != 0) {
            copyFromRing(pos, &item.func[0], header.funcSize);
            pos += header.funcSize;
        }
        item.param.resize(header.paramSize);
        if (header.paramSize != 0) {
            copyFromRing(pos, &item.param[0], header.paramSize);
            pos += header.paramSize;
        }
        item.msg.resize(header.msgSize);
        if (header.msgSize != 0) {
            copyFromRing(pos, &item.msg[0], header.msgSize);
        }

        out.push_back(std::move(item));
        readPos += header.totalSize;
        ++count;
    }

    __atomic_store_n(&m_stReadPos_.value, readPos, __ATOMIC_RELEASE);
    return count;
}
/**
 * @brief 检查 RT 环形缓冲区是否为空
 * @return bool 为空返回 true
 */
bool Logger::RtSpscByteRing::empty() const
{
    uint64_t readPos = __atomic_load_n(&m_stReadPos_.value, __ATOMIC_ACQUIRE);
    uint64_t writePos = __atomic_load_n(&m_stWritePos_.value, __ATOMIC_ACQUIRE);
    return readPos == writePos;
}
/**
 * @brief NRT MPSC 队列构造函数，初始化头尾指针到 stub 节点
 */
Logger::NrtMpscQueue::NrtMpscQueue()
    : m_pHead_(&m_stStub_),
      m_pTail_(&m_stStub_),
      m_sMaxNodes_(0),
      m_ullApproxSize_(0)
{
    m_stStub_.next = nullptr;
}
/**
 * @brief NRT MPSC 队列析构函数，清理所有节点
 */
Logger::NrtMpscQueue::~NrtMpscQueue()
{
    clear();
}
/**
 * @brief 初始化 NRT MPSC 队列
 * @param maxNodes 队列允许的最大节点数
 * @return bool 初始化成功返回 true
 */
bool Logger::NrtMpscQueue::init(size_t maxNodes)
{
    clear();
    m_stStub_.next = nullptr;
    m_pHead_ = &m_stStub_;
    m_pTail_ = &m_stStub_;
    m_sMaxNodes_ = maxNodes;
    __atomic_store_n(&m_ullApproxSize_, 0, __ATOMIC_RELEASE);
    return true;
}
/**
 * @brief 向 NRT 队列推入一条日志记录（多生产者安全）
 * @param item 移动语义的日志条目
 * @return bool 推入成功返回 true，队列满或内存分配失败返回 false
 */
bool Logger::NrtMpscQueue::push(LogItem&& item)
{
    uint64_t oldSize = __atomic_fetch_add(&m_ullApproxSize_, 1, __ATOMIC_ACQ_REL);
    if (m_sMaxNodes_ != 0 && oldSize >= m_sMaxNodes_) {
        __atomic_fetch_sub(&m_ullApproxSize_, 1, __ATOMIC_ACQ_REL);
        return false;
    }

    NrtNode* node = new (std::nothrow) NrtNode;
    if (node == nullptr) {
        __atomic_fetch_sub(&m_ullApproxSize_, 1, __ATOMIC_ACQ_REL);
        return false;
    }

    node->next = nullptr;
    node->item = std::move(item);

    // 多生产者只竞争 head。消费者只沿 tail->next 读取。
    NrtNode* prev = __atomic_exchange_n(&m_pHead_, node, __ATOMIC_ACQ_REL);
    __atomic_store_n(&prev->next, node, __ATOMIC_RELEASE);
    return true;
}
/**
 * @brief 从 NRT 队列批量提取日志记录（仅日志线程调用）
 * @param out 输出的日志条目向量
 * @param maxItems 最大提取数量
 * @return size_t 实际提取的记录数量
 */
size_t Logger::NrtMpscQueue::drain(std::vector<LogItem>& out, size_t maxItems)
{
    size_t count = 0;
    NrtNode* tail = m_pTail_;
    NrtNode* next = __atomic_load_n(&tail->next, __ATOMIC_ACQUIRE);

    while (next != nullptr && count < maxItems) {
        out.push_back(std::move(next->item));

        if (tail != &m_stStub_) {
            delete tail;
        }
        tail = next;
        __atomic_fetch_sub(&m_ullApproxSize_, 1, __ATOMIC_ACQ_REL);
        ++count;

        next = __atomic_load_n(&tail->next, __ATOMIC_ACQUIRE);
    }

    m_pTail_ = tail;
    return count;
}
/**
 * @brief 检查 NRT 队列是否为空
 * @return bool 为空返回 true
 */
bool Logger::NrtMpscQueue::empty() const
{
    uint64_t size = __atomic_load_n(&m_ullApproxSize_, __ATOMIC_ACQUIRE);
    return size == 0;
}
/**
 * @brief 清空 NRT 队列并释放所有动态分配的节点内存
 */
void Logger::NrtMpscQueue::clear()
{
    std::vector<LogItem> ignored;
    while (drain(ignored, 4096) != 0) {
        ignored.clear();
    }
    if (m_pTail_ != &m_stStub_) {
        delete m_pTail_;
    }
    m_stStub_.next = nullptr;
    m_pHead_ = &m_stStub_;
    m_pTail_ = &m_stStub_;
    __atomic_store_n(&m_ullApproxSize_, 0, __ATOMIC_RELEASE);
}
/**
 * @brief 初始化日志系统，创建目录、打开文件、初始化队列并启动后台日志线程
 * @param logDir 日志文件存放目录
 */
void Logger::init(const std::string& logDir)
{
    if (__atomic_load_n(&m_uRunning_, __ATOMIC_ACQUIRE) != 0) {
        shutdown();
    }

    m_sLogDir_ = logDir;

    struct stat info;
    if (stat(m_sLogDir_.c_str(), &info) != 0 || !(info.st_mode & S_IFDIR)) {
        if (mkdir(m_sLogDir_.c_str(), 0777) != 0) {
            throw std::runtime_error("无法创建日志目录: " + m_sLogDir_);
        }
    }

    m_sCurrentLogPath_ = m_sLogDir_ + "/" + setLogFileName();
    m_sLogFile_.open(m_sCurrentLogPath_, std::ios::app);
    if (!m_sLogFile_.is_open()) {
        throw std::runtime_error("无法打开日志文件: " + m_sCurrentLogPath_);
    }

    if (!m_stRtRing_.init(LLC_LOG_RT_RING_BYTES)) {
        throw std::runtime_error("RT 日志 ring 初始化失败");
    }
    m_stNrtQueue_.init(LLC_LOG_NRT_QUEUE_CAP);

    __atomic_store_n(&m_ullSeq_, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_ullRtDropped_, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_ullNrtDropped_, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_ullConsoleDropped_, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_ullRtEnqueuedTotal_, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_ullRtDroppedTotal_, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_ullNrtEnqueuedTotal_, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_ullNrtDroppedTotal_, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_ullConsoleWrittenTotal_, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_ullConsoleDroppedTotal_, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_ullFileWrittenTotal_, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&m_uRunning_, 1, __ATOMIC_RELEASE);

    configureConsoleNonBlocking();
    m_tLastFileFlush_ = std::chrono::steady_clock::now();

    m_tLogThread_ = std::thread(&Logger::processLogs, this);

    LogItem initItem;
    initItem.seq = __atomic_add_fetch(&m_ullSeq_, 1, __ATOMIC_RELAXED);
    initItem.epochUs = nowEpochUsec();
    initItem.level = LogLevel::INFO;
    initItem.line = 0;
    initItem.func = "Logger::init";
    initItem.msg = "LLC Logger Initialized";
    writeToFile(formatLogEntry(initItem));
    flushFileIfNeeded(true);
}
/**
 * @brief 关闭日志系统，停止后台线程，刷新并关闭文件，恢复终端属性
 */
void Logger::shutdown()
{
    uint32_t wasRunning = __atomic_exchange_n(&m_uRunning_, 0, __ATOMIC_ACQ_REL);
    if (wasRunning == 0) {
        return;
    }

    if (m_tLogThread_.joinable()) {
        m_tLogThread_.join();
    }

    flushFileIfNeeded(true);

    if (m_sLogFile_.is_open()) {
        m_sLogFile_.close();
    }

    restoreConsoleFlags();
    m_stNrtQueue_.clear();
}
/**
 * @brief 将当前线程注册为实时线程
 */
void Logger::registerRealtimeThread()
{
    m_bRealtimeThread_ = true;
}
/**
 * @brief 取消当前线程的实时线程注册
 */
void Logger::unregisterRealtimeThread()
{
    m_bRealtimeThread_ = false;
}
/**
 * @brief 判断当前线程是否为实时线程
 * @return bool 是实时线程返回 true
 */
bool Logger::isRealtimeThread() const
{
    return m_bRealtimeThread_;
}
/**
 * @brief 记录日志（C 风格字符串参数版本）
 */
void Logger::log(LogLevel level, const char* funcName, int lineNum,
                 const char* paramStr, const char* customMsg)
{
    logImpl(level, funcName, lineNum, makeTextView(paramStr), makeTextView(customMsg));
}
/**
 * @brief 记录日志（std::string 参数版本）
 */
void Logger::log(LogLevel level, const char* funcName, int lineNum,
                 const std::string& paramStr, const std::string& customMsg)
{
    logImpl(level, funcName, lineNum, makeTextView(paramStr), makeTextView(customMsg));
}
/**
 * @brief 记录日志（混合参数版本 1）
 */
void Logger::log(LogLevel level, const char* funcName, int lineNum,
                 const std::string& paramStr, const char* customMsg)
{
    logImpl(level, funcName, lineNum, makeTextView(paramStr), makeTextView(customMsg));
}
/**
 * @brief 记录日志（混合参数版本 2）
 */
void Logger::log(LogLevel level, const char* funcName, int lineNum,
                 const char* paramStr, const std::string& customMsg)
{
    logImpl(level, funcName, lineNum, makeTextView(paramStr), makeTextView(customMsg));
}
/**
 * @brief 日志记录的统一内部实现。根据是否为 RT 线程分发到 SPSC 环或 MPSC 队列
 * @param level 日志级别
 * @param funcName 函数名
 * @param lineNum 行号
 * @param param 参数视图
 * @param msg 消息视图
 */
void Logger::logImpl(LogLevel level, const char* funcName, int lineNum,
                     TextView param, TextView msg)
{
    if (__atomic_load_n(&m_uRunning_, __ATOMIC_ACQUIRE) == 0) {
        return;
    }

    // 先取序号和时间戳。文件线程用它们归并排序。
    uint64_t seq = __atomic_add_fetch(&m_ullSeq_, 1, __ATOMIC_RELAXED);
    int64_t epochUs = nowEpochUsec();

    // 终端即时显示：非阻塞，失败不重试，绝不拖住 RT。
    tryConsoleWriteNonBlocking(level, epochUs, funcName, lineNum, param, msg);

    if (isRealtimeThread()) {
        // RT 路径：只写专用 SPSC ring，失败立即丢弃。
        bool ok = m_stRtRing_.push(seq, epochUs, level, funcName, lineNum, param, msg);
        if (!ok) {
            __atomic_fetch_add(&m_ullRtDropped_, 1, __ATOMIC_RELAXED);
            __atomic_fetch_add(&m_ullRtDroppedTotal_, 1, __ATOMIC_RELAXED);
        } else {
            __atomic_fetch_add(&m_ullRtEnqueuedTotal_, 1, __ATOMIC_RELAXED);
        }
        return;
    }

    // NRT 路径：可以构造 std::string 和动态分配节点，不影响 RT。
    LogItem item;
    item.seq = seq;
    item.epochUs = epochUs;
    item.level = level;
    item.line = lineNum;
    item.func.assign(funcName == nullptr ? "" : funcName);
    item.param.assign(param.data, param.size);
    item.msg.assign(msg.data, msg.size);

    if (!m_stNrtQueue_.push(std::move(item))) {
        __atomic_fetch_add(&m_ullNrtDropped_, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&m_ullNrtDroppedTotal_, 1, __ATOMIC_RELAXED);
    } else {
        __atomic_fetch_add(&m_ullNrtEnqueuedTotal_, 1, __ATOMIC_RELAXED);
    }
}
/**
 * @brief 打开独立的非阻塞标准输出句柄，防止日志输出阻塞实时线程
 */
void Logger::configureConsoleNonBlocking()
{
    restoreConsoleFlags();

    // 不能直接对 STDOUT_FILENO 设置 O_NONBLOCK：文件状态标志属于 open file
    // description，会连带影响进程内所有 printf/cout。通过 /proc/self/fd/1
    // 重新打开可获得独立状态，只让日志副本非阻塞。
    m_nConsoleFd_ = open("/proc/self/fd/1", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (m_nConsoleFd_ < 0 && isatty(STDOUT_FILENO)) {
        m_nConsoleFd_ = open("/dev/tty", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    }
    if (m_nConsoleFd_ < 0) {
        m_bConsoleConfigured_ = false;
        return;
    }
    m_bConsoleConfigured_ = true;
}
/**
 * @brief 恢复标准输出的原始文件描述符标志
 */
void Logger::restoreConsoleFlags()
{
    if (m_nConsoleFd_ >= 0) {
        close(m_nConsoleFd_);
    }
    m_nConsoleFd_ = -1;
    m_bConsoleConfigured_ = false;
}
/**
 * @brief 尝试非阻塞地向终端写入日志（使用 writev 聚合写入）
 * @param level 日志级别
 * @param epochUs 微秒级时间戳
 * @param funcName 函数名
 * @param lineNum 行号
 * @param param 参数视图
 * @param msg 消息视图
 */
void Logger::tryConsoleWriteNonBlocking(LogLevel level, int64_t epochUs, const char* funcName,
                                        int lineNum, TextView param, TextView msg)
{
    if (!m_bConsoleConfigured_) {
        __atomic_fetch_add(&m_ullConsoleDropped_, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&m_ullConsoleDroppedTotal_, 1, __ATOMIC_RELAXED);
        return;
    }

    // RT 路径不能 localtime/ostringstream，这里用 epoch 秒.us 直接显示。
    char prefix[320];
    long long sec = static_cast<long long>(epochUs / 1000000LL);
    long long usec = static_cast<long long>(epochUs % 1000000LL);
    int prefixLen = std::snprintf(prefix, sizeof(prefix),
        "\033[32m[LOG] %lld.%06lld %s[Fun: %s] [LineNum: %d] ",
        sec, usec, levelText(level), funcName == nullptr ? "" : funcName, lineNum);
    if (prefixLen <= 0) {
        __atomic_fetch_add(&m_ullConsoleDropped_, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&m_ullConsoleDroppedTotal_, 1, __ATOMIC_RELAXED);
        return;
    }
    if (prefixLen >= static_cast<int>(sizeof(prefix))) {
        prefixLen = static_cast<int>(sizeof(prefix)) - 1;
    }

    const char* paramPrefix = "[Param: ";
    const char* paramSuffix = "] ";
    const char* msgPrefix = "[Msg: ";
    const char* msgSuffix = "]\n\033[0m";

    struct iovec iov[8];
    int iovCnt = 0;
    size_t total = 0;

    iov[iovCnt++] = { prefix, static_cast<size_t>(prefixLen) };
    total += static_cast<size_t>(prefixLen);

    if (param.size != 0) {
        iov[iovCnt++] = { const_cast<char*>(paramPrefix), std::strlen(paramPrefix) };
        iov[iovCnt++] = { const_cast<char*>(param.data), param.size };
        iov[iovCnt++] = { const_cast<char*>(paramSuffix), std::strlen(paramSuffix) };
        total += std::strlen(paramPrefix) + param.size + std::strlen(paramSuffix);
    }

    iov[iovCnt++] = { const_cast<char*>(msgPrefix), std::strlen(msgPrefix) };
    iov[iovCnt++] = { const_cast<char*>(msg.data), msg.size };
    iov[iovCnt++] = { const_cast<char*>(msgSuffix), std::strlen(msgSuffix) };
    total += std::strlen(msgPrefix) + msg.size + std::strlen(msgSuffix);

    ssize_t written = writev(m_nConsoleFd_, iov, iovCnt);
    if (written < 0 || static_cast<size_t>(written) != total) {
        // 终端副本丢失不影响文件日志，也不阻塞 RT。
        __atomic_fetch_add(&m_ullConsoleDropped_, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&m_ullConsoleDroppedTotal_, 1, __ATOMIC_RELAXED);
    } else {
        __atomic_fetch_add(&m_ullConsoleWrittenTotal_, 1, __ATOMIC_RELAXED);
    }
}
/**
 * @brief 从后台日志线程低频输出丢弃通知，不占用实时线程
 */
void Logger::writeDropNoticeToConsole(const std::string& notice)
{
    std::string line = "[LOGGER] " + notice + "\n";
    const ssize_t written = write(STDERR_FILENO, line.data(), line.size());
    (void)written;
}
/**
 * @brief 获取并重置各类日志的丢弃数量，生成丢弃通知消息
 * @return std::string 如果有丢弃记录则返回通知字符串，否则返回空串
 */
std::string Logger::takeDropNotice()
{
    uint64_t rtDropped = __atomic_exchange_n(&m_ullRtDropped_, 0, __ATOMIC_ACQ_REL);
    uint64_t nrtDropped = __atomic_exchange_n(&m_ullNrtDropped_, 0, __ATOMIC_ACQ_REL);
    uint64_t consoleDropped = __atomic_exchange_n(&m_ullConsoleDropped_, 0, __ATOMIC_ACQ_REL);

    if (rtDropped == 0 && nrtDropped == 0 && consoleDropped == 0) {
        return std::string();
    }

    std::ostringstream oss;
    oss << "LOGGER_DROP_NOTICE rt_dropped=" << rtDropped
        << ", nrt_dropped=" << nrtDropped
        << ", console_dropped=" << consoleDropped;
    return oss.str();
}
/**
 * @brief 获取日志模块累计统计快照
 */
Logger::Statistics Logger::getStatistics() const
{
    Statistics stats;
    stats.rtEnqueued = __atomic_load_n(&m_ullRtEnqueuedTotal_, __ATOMIC_ACQUIRE);
    stats.rtDropped = __atomic_load_n(&m_ullRtDroppedTotal_, __ATOMIC_ACQUIRE);
    stats.nrtEnqueued = __atomic_load_n(&m_ullNrtEnqueuedTotal_, __ATOMIC_ACQUIRE);
    stats.nrtDropped = __atomic_load_n(&m_ullNrtDroppedTotal_, __ATOMIC_ACQUIRE);
    stats.consoleWritten = __atomic_load_n(&m_ullConsoleWrittenTotal_, __ATOMIC_ACQUIRE);
    stats.consoleDropped = __atomic_load_n(&m_ullConsoleDroppedTotal_, __ATOMIC_ACQUIRE);
    stats.fileWritten = __atomic_load_n(&m_ullFileWrittenTotal_, __ATOMIC_ACQUIRE);
    return stats;
}
/**
 * @brief 分批排空 RT 环形缓冲区和 NRT 队列中的日志到输出向量
 * @param out 输出的日志条目向量
 */
void Logger::drainQueues(std::vector<LogItem>& out)
{
    // 每轮消费必须有界。持续高压下如果强制排到空，后台线程会永远停留在
    // drain 阶段，后续文件写入、flush 和丢弃统计都无法执行。
    m_stRtRing_.drain(out, LLC_LOG_DRAIN_BATCH_ITEMS);
    m_stNrtQueue_.drain(out, LLC_LOG_DRAIN_BATCH_ITEMS);
}
/**
 * @brief 后台日志处理线程的主函数。循环排空队列，定期检查丢弃通知，按序写出日志
 */
void Logger::processLogs()
{
    std::vector<LogItem> pending;
    std::vector<LogItem> batch;
    pending.reserve(8192);
    batch.reserve(4096);

    auto lastDropNotice = std::chrono::steady_clock::now();

    while (__atomic_load_n(&m_uRunning_, __ATOMIC_ACQUIRE) != 0 ||
           !m_stRtRing_.empty() || !m_stNrtQueue_.empty()) {
        batch.clear();
        drainQueues(batch);

        if (!batch.empty()) {
            pending.insert(pending.end(),
                           std::make_move_iterator(batch.begin()),
                           std::make_move_iterator(batch.end()));
        }

        auto nowSteady = std::chrono::steady_clock::now();
        if (nowSteady - lastDropNotice >= std::chrono::seconds(1)) {
            std::string notice = takeDropNotice();
            if (!notice.empty()) {
                writeDropNoticeToConsole(notice);
                LogItem item;
                item.seq = __atomic_add_fetch(&m_ullSeq_, 1, __ATOMIC_RELAXED);
                item.epochUs = nowEpochUsec();
                item.level = LogLevel::ERROR;
                item.line = 0;
                item.func = "Logger";
                item.msg = notice;
                pending.push_back(std::move(item));
            }
            lastDropNotice = nowSteady;
        }

        writeReadyLogs(pending, false);

        if (batch.empty()) {
            std::this_thread::sleep_for(std::chrono::microseconds(LLC_LOG_THREAD_IDLE_SLEEP_US));
        }
    }

    // shutdown 阶段：最后一次 drain，全部按时间排序写出。
    batch.clear();
    drainQueues(batch);
    pending.insert(pending.end(),
                   std::make_move_iterator(batch.begin()),
                   std::make_move_iterator(batch.end()));

    std::string notice = takeDropNotice();
    if (!notice.empty()) {
        writeDropNoticeToConsole(notice);
        LogItem item;
        item.seq = __atomic_add_fetch(&m_ullSeq_, 1, __ATOMIC_RELAXED);
        item.epochUs = nowEpochUsec();
        item.level = LogLevel::ERROR;
        item.line = 0;
        item.func = "Logger";
        item.msg = notice;
        pending.push_back(std::move(item));
    }

    writeReadyLogs(pending, true);
    flushFileIfNeeded(true);
}
/**
 * @brief 将待处理的日志条目按时间排序，将达到水印的日志写入文件
 * @param pending 待处理的日志条目引用
 * @param forceAll 是否强制写出所有日志（通常在关闭时使用）
 */
void Logger::writeReadyLogs(std::vector<LogItem>& pending, bool forceAll)
{
    if (pending.empty()) {
        flushFileIfNeeded(forceAll);
        return;
    }

    std::stable_sort(pending.begin(), pending.end(),
        [](const LogItem& a, const LogItem& b) {
            if (a.epochUs != b.epochUs) {
                return a.epochUs < b.epochUs;
            }
            return a.seq < b.seq;
        });

    int64_t watermark = forceAll
        ? std::numeric_limits<int64_t>::max()
        : nowEpochUsec() - LLC_LOG_ORDER_WINDOW_US;

    size_t readyCount = 0;
    while (readyCount < pending.size() && pending[readyCount].epochUs <= watermark) {
        ++readyCount;
    }

    bool hasError = false;
    for (size_t i = 0; i < readyCount; ++i) {
        if (pending[i].level == LogLevel::ERROR) {
            hasError = true;
        }
        writeToFile(formatLogEntry(pending[i]));
    }

    if (readyCount != 0) {
        pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(readyCount));
    }

    flushFileIfNeeded(forceAll || hasError);
}
/**
 * @brief 将单条日志条目格式化为字符串
 * @param item 日志条目
 * @return std::string 格式化后的日志字符串
 */
std::string Logger::formatLogEntry(const LogItem& item) const
{
    std::ostringstream ss;
    ss << formatEpochUsec(item.epochUs) << " "
       << levelText(item.level)
       << "[Fun: " << item.func << "] "
       << "[LineNum: " << item.line << "] ";

    if (!item.param.empty()) {
        ss << "[Param: " << item.param << "] ";
    }
    ss << "[Msg: " << item.msg << "]\n";
    return ss.str();
}
/**
 * @brief 将日志字符串写入文件，并按间隔检查是否需要轮转或清理旧日志
 * @param logEntry 日志字符串
 */
void Logger::writeToFile(const std::string& logEntry)
{
    static size_t writeCount = 0;

    if (!m_sLogFile_.is_open() || m_sLogFile_.fail() || m_sLogFile_.bad()) {
        return;
    }

    m_sLogFile_ << logEntry;
    if (m_sLogFile_.fail() || m_sLogFile_.bad()) {
        return;
    }
    __atomic_fetch_add(&m_ullFileWrittenTotal_, 1, __ATOMIC_RELAXED);

    if (++writeCount % LOG_FLUSH_INTERVAL == 0) {
        if (m_sLogFile_.good()) {
            std::streampos pos = m_sLogFile_.tellp();
            if (pos != -1 && static_cast<size_t>(pos) > MAX_FILE_SIZE) {
                rotateLog();
            }
            cleanupOldLogs();
        }
    }
}
/**
 * @brief 根据时间间隔或强制标志刷新日志文件缓冲区
 * @param force 是否强制刷新
 */
void Logger::flushFileIfNeeded(bool force)
{
    if (!m_sLogFile_.is_open()) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (force ||
        now - m_tLastFileFlush_ >= std::chrono::milliseconds(LLC_LOG_FILE_FLUSH_INTERVAL_MS)) {
        m_sLogFile_.flush();
        m_tLastFileFlush_ = now;
    }
}

/*std::string Logger::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()) % 1000000;
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S")
       << "." << std::setfill('0') << std::setw(6) << us.count();
    return ss.str();
}*/

/**
 * @brief 根据当前时间生成日志文件名
 * @return std::string 格式如 "llc_log_YYYYMMDD_HHMMSS.log"
 */
std::string Logger::setLogFileName() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    return "llc_log_" + ss.str() + ".log";
}
/**
 * @brief 日志文件轮转。关闭当前文件，将超出大小限制的当前日志文件重命名为归档文件（加_old），并创建新日志文件
 */
//区分是否归档：高频日志场景：在一秒内可能产生多个日志文件;并发写入：多个线程或进程可能同时创建日志文件;文件完整性：归档文件表示已完成写入，不应被修改
void Logger::rotateLog() {
    if (m_sLogFile_.is_open()) {
        m_sLogFile_.close();
    }
    
    //获取当前文件完整路径--老文件
    DIR* dir;
    struct dirent* dentry;
    std::string scurrentPath;
    if ((dir = opendir(m_sLogDir_.c_str())) != nullptr){
        while((dentry = readdir(dir))!=nullptr){
            std::string sfilename = dentry->d_name;
            if(sfilename.find("llc_log_") == 0 && sfilename.find(".log") != std::string::npos &&
                sfilename.find("_old") == std::string::npos){//以"llc_log_"开头，结尾包含log，不包含"_old"的文件
                     // 添加文件大小检查
                    std::string fullPath = m_sLogDir_ + "/" + sfilename;
                    struct stat file_stat;
                    if (stat(fullPath.c_str(), &file_stat) == 0) {
                        // 只有文件大小超过限制才进行轮转
                        if (static_cast<size_t>(file_stat.st_size) > MAX_FILE_SIZE) {
                            scurrentPath = fullPath;
                            break;
                        }
                    }
                } 
        }
        closedir(dir);

    }

    //重命名已满老文件----加old---证明已归档
    if(scurrentPath.size() > 0){
        std::string soldPath = scurrentPath;
        size_t pos = soldPath.find(".log");
        if (pos != std::string::npos) {
            soldPath.replace(pos, 4, "_old.log");
        }
        
        if(rename(scurrentPath.c_str(), soldPath.c_str()) != 0){
            std::cerr << "警告: 无法重命名日志文件从 " << scurrentPath 
                      << " 到 " << soldPath << std::endl;
        }

    }

    //生成新文件名,完整路径
    std::string snewFileName = setLogFileName();
    std::string snewFilePath = m_sLogDir_ + "/" +snewFileName;
    
    // 创建新的日志文件
    m_sLogFile_.open(snewFilePath, std::ios::app);
    
    if (m_sLogFile_.is_open() == false) {
        throw std::runtime_error("无法打开新的日志文件: " + snewFilePath);
}
}

/**
 * @brief 清理旧日志文件。当日志总大小超过 MAX_TOTAL_SIZE 时，按修改时间和归档状态排序，优先删除最旧及已归档的文件
 */
void Logger::cleanupOldLogs() {
    try {
        DIR* dir;
        struct dirent* entry;
        std::vector<std::pair<std::string, size_t>> allLogFiles; // 所有日志文件
        size_t totalSize = 0;

        if ((dir = opendir(m_sLogDir_.c_str())) != nullptr) {
            while ((entry = readdir(dir)) != nullptr) {
                std::string sfilename = entry->d_name;
                // 跳过当前目录和上级目录
                if (sfilename == "." || sfilename == "..") continue;
                
                std::string sfullPath = m_sLogDir_ + "/" + sfilename;
                struct stat stat_buf;

                if(stat(sfullPath.c_str(), &stat_buf) == 0){
                    size_t sfilesize = stat_buf.st_size;

                    // 只处理日志文件
                    if(sfilename.find("llc_log_") == 0 && sfilename.find(".log") != std::string::npos){
                        allLogFiles.emplace_back(sfullPath, sfilesize);
                        totalSize += sfilesize;
                    }
                }
            }
            closedir(dir);
        }

        // 如果总大小超过限制，按修改时间和归档状态排序并删除最旧的文件
        if (totalSize > MAX_TOTAL_SIZE) {
            // 按修改时间和归档状态排序
            std::sort(allLogFiles.begin(), allLogFiles.end(),
                [](const auto&a,const auto&b) {
                    struct stat stat_a, stat_b;
                    stat(a.first.c_str(), &stat_a);
                    stat(b.first.c_str(), &stat_b);
                    
                    // 获取文件名，检查是否已归档
                    std::string name_a = a.first.substr(a.first.find_last_of('/') + 1);
                    std::string name_b = b.first.substr(b.first.find_last_of('/') + 1);
                    bool a_is_archived = (name_a.find("_old.log") != std::string::npos);
                    bool b_is_archived = (name_b.find("_old.log") != std::string::npos);
                    
                    // 如果修改时间相同，已归档文件优先被删除
                    if (stat_a.st_mtime == stat_b.st_mtime) {
                        return a_is_archived && !b_is_archived;
                    }
                    return stat_a.st_mtime < stat_b.st_mtime;
                });

            // 删除最旧的文件直到总大小在限制内
            for (const auto& file : allLogFiles) {
                if (totalSize <= MAX_TOTAL_SIZE) break;
                
                // 获取文件名，检查是否已归档
                std::string filename = file.first.substr(file.first.find_last_of('/') + 1);
                bool is_archived = (filename.find("_old.log") != std::string::npos);
                
                if (remove(file.first.c_str()) == 0) {
                    totalSize -= file.second;
                    std::cout << "已删除日志文件: " << file.first 
                              << " (大小: " << file.second << " 字节)" 
                              << (is_archived ? " [已归档]" : " [未归档]") << std::endl;
                }
            }
        }
       
    } catch (const std::exception& e) {
        std::cerr << "警告: 日志清理失败: " << e.what() << std::endl;
    }
}

/**
 * @brief 获取当前可执行程序所在的根目录（向上寻找到 build 目录的父级）
 * @return std::string 程序根目录路径，找不到则返回 "."
 */
std::string getProgramDir()
{
    char cBuf[PATH_MAX];//PATH_MAX是一个系统常量，用于表示文件系统路径的最大长度
    ssize_t sLen = readlink("/proc/self/exe",cBuf,sizeof(cBuf)-1);
    if(sLen == -1)
    {
        throw std::runtime_error("无法获取程序路径");
    }
    cBuf[sLen]='\0';

    std::string sPath(cBuf);
    // 假设程序在build文件夹下，总文件夹是build的父目录
    size_t sPos = sPath.find("/build/");
    if (sPos == std::string::npos) {
        // 找不到build目录，默认使用当前目录（兼容处理）
        return ".";
    }
    // 截取总文件夹路径（build的父目录）
    std::string sParentDir = sPath.substr(0, sPos);
    //printf("总文件夹路径: %s\n", parentDir.c_str());  // 调试输出
    return sParentDir;
}
