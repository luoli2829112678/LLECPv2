#pragma once
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include <HSTR/generated/interface_signature.h>

/// LLPC Structured Text Runtime System 的全部公开接口。
namespace HSTR
{
/// 任务触发方式；所有任务均由调用方的 Cycle 驱动。
enum class STTaskType
{
    Cyclic,     ///< 到达 intervalUs 后执行，首次 Cycle 立即执行。
    Event,      ///< TriggerEvent 标记后，在下一次 Cycle 执行一次。
    FreeRunning ///< 每次 Cycle 执行一次，不创建后台忙循环。
};

/// 编译时从第三方维护的 .task 读取的任务配置；HSTR 不负责将此结构写入源文件。
struct STTaskConfig
{
    std::string name;                     ///< 任务名，同时作为 .task 文件名。
    STTaskType type = STTaskType::Cyclic; ///< 默认周期任务。
    uint64_t intervalUs = 1000;           ///< 周期，单位微秒；Cyclic 必须大于 0。
    uint32_t priority = 0;                ///< 数值越小越先执行，同值保持注册顺序。
    uint64_t watchdogUs = 0;              ///< 看门狗阈值，单位微秒；0 关闭，调试模式不检查。
    int32_t cpuAffinity = -1;             ///< -1 不检查；否则要求 Cycle 线程已在该 CPU 上。
    bool enabled = true;                  ///< false 时跳过该任务。
    std::vector<std::string> programs;    ///< 按顺序调用的程序名；一个程序只能分配给一个任务。
};

/// APP 状态；0～3 与用户接口表一致，4 为补充的已编译停止状态。
enum class ApplicationStatus : int
{
    NotCompiled = 0, ///< 未编译，或工程修改后旧模块已失效。
    Running = 1,     ///< 已启动，正在执行或等待下一次 Cycle。
    Error = 2,       ///< 编译/执行失败，详情通过 GetLastError 查询。
    Breakpoint = 3,  ///< 暂停在 ST 语句执行之前。
    Ready = 4        ///< 已编译并初始化，尚未启动或已停止复位。
};

/// HSTR 业务异常分类；文件系统和 JSON 异常也可能以 std::exception 派生类传播。
enum class ErrorCode
{
    InvalidArgument,     ///< 名称、任务参数、变量大小等不合法。
    NotFound,            ///< 指定 APP、源文件、任务或变量不存在。
    AlreadyExists,       ///< APP 或源文件已存在。
    InvalidState,        ///< 当前状态不允许执行此操作。
    CompileError,        ///< ST 编译、原生编译或模块加载失败。
    IOError,             ///< 文件读写失败。
    Watchdog,            ///< 任务执行时间超过配置阈值。
    HardwareUnavailable, ///< 所需硬件回调未注册。
    Unsupported,         ///< 操作不受当前版本支持。
    Busy                 ///< 同一 APP 已被其他 Runtime 占用。
};

/// Runtime 初始化选项；编译器运行于当前可执行文件的内部子模式。
struct Options
{
    std::string applicationRoot = "/home/Application"; ///< APP 根目录，可使用绝对或相对路径。
    std::string cCompiler = "cc"; ///< 系统 C 编译器路径/名称，不支持 shell 命令或附加参数。
};

/// 硬件桥接回调。调用发生在执行 ST 的线程中，应有界、非阻塞，且不得重入 Runtime。
struct HardwareCallbacks
{
    void* user = nullptr; ///< 传给所有回调的上下文，由宿主维护其生命周期。
    // 自动生成强类型字段。新增接口只修改 interfaces/llpc.json。
#include <HSTR/generated/callbacks.inc>
};

/// 持久变量的过程映像描述；数组/结构体按完整聚合值导出。
struct VariableInfo
{
    std::string name;      ///< 完整 IEC 实例路径，不区分大小写。
    std::string type;      ///< matiec 导出的 IEC 类型名称。
    size_t offset = 0;     ///< 相对 GetVariableMemory() 的字节偏移。
    size_t size = 0;       ///< 原生值所占字节数，读写必须完全匹配。
    bool readOnly = false; ///< true 表示常量，ModifyVariable 不允许写入。
};

/// 最近一次调试暂停的位置；程序复位后各字段清空。
struct DebugLocation
{
    std::string file;         ///< 已注册源文件名，例如 Main.st。
    uint32_t line = 0;        ///< 从 1 开始的 ST 语句起始行号。
    int depth = 0;            ///< 原生 POU 调用深度，用于步过/进入控制。
    bool canStepInto = false; ///< 当前语句是否为允许进入的独立函数/FB 调用。
};

/// 单任务执行统计；StopProgram/ResetProgram 后清零。
struct TaskStatistics
{
    std::string name;            ///< 任务名。
    uint64_t executions = 0;     ///< 正常返回的任务执行次数。
    uint64_t missedPeriods = 0;  ///< 周期任务因调用不及时而跳过的周期数。
    uint64_t lastDurationUs = 0; ///< 最近一次执行耗时，单位微秒。
    uint64_t maxDurationUs = 0;  ///< 复位以来的最大执行耗时，单位微秒。
};

/// HSTR 业务异常；可通过 std::exception 统一捕获所有管理接口异常。
class Error : public std::exception
{
  public:
    /// 构造异常，code 为业务错误分类，message 为可读诊断。
    Error(ErrorCode code, std::string message);
    /// 返回异常文本；指针有效期与此异常对象相同，不抛异常。
    const char* what() const noexcept override;
    /// 返回业务错误分类，不抛异常。
    ErrorCode code() const noexcept;

  private:
    ErrorCode code_;
    std::string message_;
};

/**
 * @brief 一个对象管理一个当前选中的 APP。
 * @details 管理接口失败时抛出异常；Cycle 捕获运行错误并返回 Error 状态。
 * 生命周期操作须由一个管理线程串行调用；硬件回调不得重入本对象。
 * 每个宿主 main 必须首先调用 HandleCompilerInvocation，再创建本对象。
 */
class Runtime
{
  public:
    /// 创建运行环境，必要时建立根目录，并恢复 work 中记录的 APP 选择；失败抛异常。
    explicit Runtime(Options options = {});
    /// 停止任务、取消调试等待并释放 APP 锁、运行模块和变量映像。
    ~Runtime();
    /// 禁止复制，避免多个对象共享同一执行栈和 APP 写锁。
    Runtime(const Runtime&) = delete;
    /// 禁止复制赋值；如需独立 APP，请创建独立 Runtime。
    Runtime& operator=(const Runtime&) = delete;

    /// 新建 name 目录及 name.prj；不自动选中；名称非法或已存在时抛异常。
    void AddApplication(const std::string& name);
    /// 选中已有 APP 并写入 work；必须处于停止状态，选中后需要重新编译。
    void SelectApplication(const std::string& name);
    /// 返回当前 APP 名字；尚未选择时返回空字符串。
    std::string SelectedApplication() const;
    /** @brief 创建零字节 name.GVL，并在 .prj 登记名字和文件名。
     *  @param name 不包含扩展名的 IEC 标识符。
     *  @details 只创建空文件；内容由第三方填写，编译时校验。已有文件绝不覆盖。
     */
    void AddGlobalVariableFile(const std::string& name);
    /// 创建空的name.typ并登记到types；支持枚举和STRUCT类型声明，名称不带扩展名。
    /// 类型按登记顺序先于函数和程序编译；依赖的类型文件应先登记。
    void AddTypeFile(const std::string& name);
    /// 注销类型文件并移入.trash，旧编译结果失效；必须先停止应用。
    void DeleteTypeFile(const std::string& name);
    /** @brief 创建零字节 name.st，并登记到当前 APP。
     *  @param name 不包含 .st 的程序名；第三方填写的 PROGRAM 名应与之相同。
     *  @details 不生成 ST 模板，不接收源码参数。必须先停止；已有文件会报错。
     */
    void AddProgram(const std::string& name);
    /** @brief 创建零字节 name.st.fun，并登记到当前 APP。
     *  @param name 不包含 .st.fun 的文件标识符。
     *  @details FUNCTION/FB 文本由第三方维护，HSTR 只在编译阶段读取并校验。
     */
    void AddFunction(const std::string& name);
    /** @brief 创建零字节 name.task，并在 .prj 登记任务文件。
     *  @param name 不包含 .task 的任务名。
     *  @details 不写默认配置；第三方填写 JSON。编译时校验字段、取值及程序引用。
     */
    void AddTask(const std::string& name);
    /// 注销并将 name.GVL 移入 APP 内 .trash；必须先停止程序。
    void DeleteGlobalVariableFile(const std::string& name);
    /// 注销并移走 name.st；Task 中的残留引用由下一次编译检查。
    void DeleteProgram(const std::string& name);
    /// 注销并移走 name.st.fun；残留的 ST 函数引用会在下次编译时报错。
    void DeleteFunction(const std::string& name);
    /// 注销任务并将 .task 移入 .trash，使旧编译失效。
    void DeleteTask(const std::string& name);
    /// 读取第三方维护的源文件/.task，校验后编译并装载；不修改原文件，失败返回 CompileError 异常。
    void CompileProgram();
    /// 校验已编译且源文件未变化后启动；debug=true 启用可暂停调试，后续仍需调用 Cycle。
    void ExecuteProgram(bool debug = false);
    /// 停止并取消暂停栈，复位所有程序/变量/任务统计；不能强行抢占阻塞的硬件回调。
    void StopProgram();
    /** @brief 使用调用方提供的单调时间执行到期任务，时间单位为微秒。
     *  @return 执行后的 APP 状态；普通模式锁忙时返回当前状态并跳过扫描。
     *  @details 首轮周期任务立即执行；时间倒退或执行异常置 Error。
     *  调试模式会等待管理锁，并等待断点暂停或本轮结束，不适用于实时总线线程。
     */
    ApplicationStatus Cycle(uint64_t monotonicNowUs);
    /// 自动获取 steady_clock 微秒时间后调用 Cycle(us)；正常使用时每个总线周期调用一次。
    ApplicationStatus Cycle();
    /// 标记 Event 任务待运行；多次触发合并为下一次 Cycle 的一次执行。
    void TriggerEvent(const std::string& taskName);

    /// 添加文件/行断点；行号从 1 开始，必须对应编译结果中的实际 ST 语句起始行。
    void AddBreakpoint(const std::string& filename, uint32_t line);
    /// 移除指定断点；断点不存在时不报错。
    void RemoveBreakpoint(const std::string& filename, uint32_t line);
    /// 在暂停态执行当前语句，停于相同/较浅调用深度的下一语句；函数内部断点仍有效。
    void SingleStep();
    /// 在暂停的独立函数/FB 调用语句处进入；赋值等其他语句调用本接口会抛异常。
    void StepInto();
    /// 从断点恢复本次扫描，等待下一个断点或扫描结束；不会自动驱动未来的 Cycle。
    void Continue();
    /// 返回最近一次暂停的文件、行号、调用深度及是否允许进入。
    DebugLocation GetDebugLocation() const;

    /// 将 size 字节从 data 写入实际变量并刷新映像；名称支持唯一后缀，常量/大小不符拒绝。
    void ModifyVariable(const std::string& name, const void* data, size_t size);
    /// 将变量快照复制到 data；size 必须精确等于变量大小，缓冲区由调用方提供。
    void ReadVariable(const std::string& name, void* data, size_t size) const;

    /// 按类型返回变量快照；T 必须可平凡复制且符合 IEC 原生布局，例如 DINT 对应 int32_t。
    template <class T> T ReadVariable(const std::string& name) const
    {
        static_assert(std::is_trivially_copyable<T>::value,
                      "Use an IEC-compatible plain data type");
        T value{};
        ReadVariable(name, &value, sizeof(value));
        return value;
    }

    /// 按类型写入变量；T 的布局及 sizeof(T) 必须匹配变量，不支持直接传入 std::string。
    template <class T> void ModifyVariable(const std::string& name, const T& value)
    {
        static_assert(std::is_trivially_copyable<T>::value,
                      "Use an IEC-compatible plain data type");
        ModifyVariable(name, &value, sizeof(value));
    }

    /// 返回持久变量描述副本；包含 GVL/PROGRAM/FB 及聚合值，不包含 FUNCTION 栈临时变量。
    std::vector<VariableInfo> GetVariableList() const;
    /// 返回连续只读映像首地址；有效到重新编译/选择/析构；访问须与 Cycle 同步，禁止直接写入。
    const void* GetVariableMemory() const;
    /// 返回过程映像字节长度；没有加载模块时为 0。
    size_t GetVariableMemorySize() const;
    /// 与 StopProgram 相同：先停止，再恢复初始值，不自动重新启动。
    void ResetProgram();
    /// 返回当前 APP 状态；原子读取，不等待执行锁。
    ApplicationStatus GetApplicationStatus() const;

    /// 兼容原接口表中的 Stauts 拼写；行为与 GetApplicationStatus 完全相同。
    ApplicationStatus GetApplicationStauts() const
    {
        return GetApplicationStatus();
    }

    /// 返回最近一次编译/运行错误文本；复位后清空，不代表每个管理异常都会写入此字段。
    std::string GetLastError() const;
    /// 返回最近一次编译诊断，包括 matiec/系统 C 编译器的输出。
    std::string GetCompilerDiagnostics() const;
    /// 返回按实际调度顺序排列的任务统计副本。
    std::vector<TaskStatistics> GetTaskStatistics() const;
    /// 在停止状态注册硬件回调；未注册某回调而实际调用时会报 HardwareUnavailable。
    /// 默认签名来自宿主编译时的头文件；不同版本会在读取回调结构之前报错。
    void SetHardwareCallbacks(const HardwareCallbacks& callbacks,
                              const char* interfaceSignature = HSTR_INTERFACE_SIGNATURE);

    // 宿主直接调用的方法与 ST 内置函数来自同一份定义，包含历史别名。
#include <HSTR/generated/runtime_declarations.inc>

  private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

/** @brief 在宿主 main 的第一步处理当前可执行文件的内部编译模式。
 *  @param argc main 收到的参数数量。
 *  @param argv main 收到的原始参数数组；编译模式会将后续参数交给 matiec。
 *  @param exitCode 编译模式结束码；普通调用时保持不变。
 *  @return true 表示已处理编译模式，宿主应立即 return exitCode；false 表示继续普通逻辑。
 *  @details 必须在创建 Runtime、线程或设备之前调用。库使用 posix_spawn 启动同一个可执行
 *  文件的 --hstr-compiler 子模式；该子进程可由 matiec 直接 exit，不影响父进程。
 */
bool HandleCompilerInvocation(int argc, char** argv, int& exitCode);

/// 返回库名称与版本的静态字符串，不需要释放，且不抛异常。
const char* HSTRVersion() noexcept;
} // namespace HSTR
