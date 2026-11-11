#include "internal.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <dlfcn.h>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <sched.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <iostream>
namespace HSTR
{
using namespace detail;
using Clock = std::chrono::steady_clock;

// 清理当前Application的历史编译目录。
// 只能在应用停止、旧模块卸载后调用。
// 保留ST、Task、类型文件、.prj以及.trash。
static bool ClearBuildCache(const std::filesystem::path& app)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::directory_iterator it(app, ec);
    const fs::directory_iterator end;

    if (ec)
    {
        std::fprintf(stderr, "[HSTR] 读取缓存目录失败：%s\n", ec.message().c_str());
        return false;
    }

    bool success = true;
    const std::string prefix = ".hstr-build-";

    while (it != end)
    {
        const fs::path path = it->path();

        // 先推进迭代器，再删除当前目录。
        it.increment(ec);
        if (ec)
        {
            std::fprintf(stderr, "[HSTR] 遍历缓存目录失败：%s\n", ec.message().c_str());
            return false;
        }

        const std::string name = path.filename().string();

        // 匹配mkdtemp生成的目录：固定前缀加6位字母数字。
        if (name.size() != prefix.size() + 6 || name.compare(0, prefix.size(), prefix) != 0)
        {
            continue;
        }

        const bool validSuffix = std::all_of(name.begin() + prefix.size(), name.end(), [](unsigned char ch)
        {
            return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
        });

        if (!validSuffix)
        {
            continue;
        }

        const auto status = fs::symlink_status(path, ec);
        if (ec)
        {
            std::fprintf(stderr, "[HSTR] 检查缓存失败：%s，%s\n", path.c_str(), ec.message().c_str());
            ec.clear();
            success = false;
            continue;
        }

        // 不跟随顶层符号链接。
        if (fs::is_symlink(status) || !fs::is_directory(status))
        {
            continue;
        }

        fs::remove_all(path, ec);
        if (ec)
        {
            std::fprintf(stderr, "[HSTR] 删除缓存失败：%s，%s\n", path.c_str(), ec.message().c_str());
            ec.clear();
            success = false;
        }
    }
    return success;
}

/// 获取单调时钟的微秒数，供无时间参数的 Cycle 使用。
static uint64_t nowUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch())
        .count();
}

/// 保存业务错误分类和诊断文本，供调用方捕获后查询。
Error::Error(ErrorCode c, std::string s) : code_(c), message_(std::move(s))
{
}

/// 返回异常对象持有的诊断文本，调用方无需释放。
const char* Error::what() const noexcept
{
    return message_.c_str();
}

/// 返回可供程序判断的业务错误分类。
ErrorCode Error::code() const noexcept
{
    return code_;
}

/// 返回 HSTR 名称及版本的静态字符串。
const char* HSTRVersion() noexcept
{
    return "HSTR 0.2.1 / LLPC Structured Text Runtime System";
}

struct AbortExecution
{
};

struct Runtime::Impl
{
    Options options;
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::string selected;
    std::string lastError;
    std::string diagnostics;
    json project;
    json sourceMap;
    std::map<std::string, std::string> compiledFiles;
    fs::path app;
    int appLock = -1;
    std::atomic<ApplicationStatus> status{ApplicationStatus::NotCompiled};
    void* module = nullptr;
    void (*init)(HSTR_ModuleHost*) = nullptr; ///< 初始化生成模块并注册宿主回调。
    void (*run)(size_t) = nullptr;            ///< 按注册顺序中的程序索引执行一个 PROGRAM。
    void (*setTime)(uint64_t) = nullptr;      ///< 向模块传入当前单调微秒时间。
    size_t (*count)() = nullptr;              ///< 查询模块导出的持久变量数量。
    HSTR_ModuleVariable* (*list)() = nullptr; ///< 获取模块原生变量地址表。
    std::vector<VariableInfo> variables;
    std::vector<unsigned char> memory;

    struct Task
    {
        STTaskConfig config;
        std::vector<size_t> programs;
        TaskStatistics stats;
        uint64_t lastRelease = 0;
        bool released = false;
        bool pending = false;
    };

    std::vector<Task> tasks;
    HardwareCallbacks hardware;
    HSTR_ModuleHost host{};
    std::set<std::pair<std::string, uint32_t>> breakpoints;
    DebugLocation location;
    std::atomic<bool> debug{false}; ///< Cycle 获取锁失败时也需安全判断是否应等待。
    bool job = false;       ///< 调试线程是否仍持有一轮未完成的扫描。
    bool requested = false; ///< 调用方是否提交了新的调试扫描请求。
    bool quit = false;      ///< 请求调试线程退出。
    bool resume = false;    ///< 请求恢复当前暂停的执行栈。
    std::atomic<bool> abortRequested{false};
    std::thread worker;
    std::unique_lock<std::mutex>* executionLock = nullptr;
    int stepMode = 0; ///< 0 继续、1 步过、2 进入。
    int stepDepth = 0;
    uint64_t cycleTime = 0;
    uint64_t lastCycle = 0;
    uint64_t deadline = 0;
    bool hasCycle = false;

    explicit Impl(Options o) : options(std::move(o))
    {
        rejectUnhandledCompilerInvocation();
        options.applicationRoot = fs::absolute(options.applicationRoot).lexically_normal().string();
        if (options.applicationRoot.find_first_of("\"\n\r") != std::string::npos)
        {
            throw Error(ErrorCode::InvalidArgument, "Unsupported characters in root path");
        }
        fs::create_directories(options.applicationRoot);
        host.user = this;
        // 包含空回调检查和上下文传递；新增接口不再编辑构造函数。
#include <HSTR/generated/host_bindings.inc>
        host.trace = [](void* p, const char* file, uint32_t line, int depth, int call)
        {
            static_cast<Impl*>(p)->trace(file, line, depth, call);
        };
    }

    ~Impl()
    {
        if (module)
        {
            dlclose(module);
        }
        if (appLock >= 0)
        {
            close(appLock);
        }
    }

    void requireSelected() const
    {
        if (selected.empty())
        {
            throw Error(ErrorCode::InvalidState, "Select an application first");
        }
    }

    void idle() const
    {
        if (status == ApplicationStatus::Running || status == ApplicationStatus::Breakpoint || job)
        {
            throw Error(ErrorCode::InvalidState,
                        "Stop the application before editing or compiling");
        }
    }

    void snapshot()
    {
        if (!module)
        {
            return;
        }
        auto* v = list();
        for (size_t i = 0; i < variables.size(); ++i)
        {
            std::memcpy(memory.data() + variables[i].offset, v[i].address, v[i].size);
        }
    }

    void unload()
    {
        if (module)
        {
            dlclose(module);
        }
        module = nullptr;
        variables.clear();
        memory.clear();
        tasks.clear();
        compiledFiles.clear();
        sourceMap = json::array();
        breakpoints.clear();
        status = ApplicationStatus::NotCompiled;
        hasCycle = false;
    }

    void save()
    {
        atomicWrite(app / (selected + ".prj"), project.dump(2) + "\n");
    }

    void invalidate()
    {
        unload();
        project["compiled"] = false;
        save();
    }

    /// 仅校验 .prj 的登记信息；源代码和任务内容可以暂时为空，留到编译阶段检查。
    void validateProject(const json& manifest)
    {
        if (manifest.at("schemaVersion") != 2 || manifest.at("name") != selected)
        {
            throw Error(ErrorCode::InvalidArgument, "Invalid project manifest");
        }

        const std::pair<const char*, const char*> categories[] = {
            {"types", ".typ"}, {"globals", ".GVL"}, {"programs", ".st"}, {"functions", ".st.fun"}, {"tasks", ".task"}};
        std::set<std::string> filenames;
        std::set<std::string> sourceNames;
        std::set<std::string> taskNames;
        for (const auto& category : categories)
        {
            const auto& entries = manifest.at(category.first);
            if (!entries.is_array())
            {
                throw Error(ErrorCode::InvalidArgument, "Project file lists must be arrays");
            }
            for (const auto& entry : entries)
            {
                const std::string name = entry.at("name");
                const std::string filename = entry.at("file");
                validateName(name);
                const bool isTask = std::string(category.first) == "tasks";
                auto& names = isTask ? taskNames : sourceNames;
                if (filename != name + category.second ||
                    !filenames.insert(upper(filename)).second || !names.insert(upper(name)).second)
                {
                    throw Error(ErrorCode::InvalidArgument,
                                "Invalid or duplicate file registration");
                }
                if (fs::is_symlink(app / filename))
                {
                    throw Error(ErrorCode::InvalidArgument, "Symlink source files are unsupported");
                }
            }
        }
    }



    /// 只创建空文件并保存登记项，不生成模板，不读写第三方维护的源文件内容。
    void registerFile(const char* category, const std::string& name, const std::string& suffix)
    {
        requireSelected();
        idle();
        validateName(name);

        const std::string filename = name + suffix;
        auto updatedProject = project;
        for (const auto& entry : updatedProject.at(category))
        {
            if (upper(entry.at("name")) == upper(name))
            {
                throw Error(ErrorCode::AlreadyExists, "File already registered: " + filename);
            }
        }
        updatedProject[category].push_back({{"name", name}, {"file", filename}});
        validateProject(updatedProject);

        // O_EXCL 保证既有文件不会因为 HSTR 创建操作而丢失第三方编辑内容。
        createEmptyFile(app / filename);
        project = std::move(updatedProject);
        invalidate();
    }

    /// 编译前一次性读取第三方文件，后续编译和启动检查均使用这份字节快照。
    std::map<std::string, std::string> readCompilationFiles() const
    {
        std::map<std::string, std::string> inputs;
        for (const char* category : {"types", "globals", "functions", "programs", "tasks"})
        {
            for (const auto& entry : project.at(category))
            {
                const std::string filename = entry.at("file");
                try
                {
                    if (!fs::is_regular_file(app / filename))
                    {
                        throw Error(ErrorCode::IOError,
                                    "Registered file is missing or not a regular file");
                    }
                    const std::string content = readFile(app / filename);
                    if (content.find_first_not_of(" \t\r\n") == std::string::npos)
                    {
                        throw Error(
                            ErrorCode::InvalidArgument,
                            "File is empty; fill it using the external editor before compiling");
                    }
                    inputs.emplace(filename, content);
                }
                catch (const std::exception& error)
                {
                    throw Error(ErrorCode::CompileError, filename + ": " + error.what());
                }
            }
        }
        return inputs;
    }

    /// 以 .task 快照为唯一配置来源，编译时检查名字、字段及程序引用关系。
    std::vector<STTaskConfig>
    readTaskConfigurations(const std::map<std::string, std::string>& inputs) const
    {
        std::set<std::string> registeredPrograms;
        for (const auto& program : project.at("programs"))
        {
            registeredPrograms.insert(upper(program.at("name")));
        }

        std::set<std::string> assignedPrograms;
        std::vector<STTaskConfig> configurations;
        for (const auto& entry : project.at("tasks"))
        {
            const std::string filename = entry.at("file");
            const std::string registeredName = entry.at("name");
            try
            {
                STTaskConfig config = parseTask(json::parse(inputs.at(filename)));
                if (upper(config.name) != upper(registeredName))
                {
                    throw Error(ErrorCode::InvalidArgument,
                                "Task name does not match the registered filename");
                }
                config.name = registeredName;
                for (const auto& program : config.programs)
                {
                    if (!registeredPrograms.count(upper(program)))
                    {
                        throw Error(ErrorCode::NotFound, "Task program not registered: " + program);
                    }
                    if (!assignedPrograms.insert(upper(program)).second)
                    {
                        throw Error(ErrorCode::InvalidArgument,
                                    "Program assigned to more than one task: " + program);
                    }
                }
                configurations.push_back(std::move(config));
            }
            catch (const std::exception& error)
            {
                throw Error(ErrorCode::CompileError, filename + ": " + error.what());
            }
        }
        return configurations;
    }

    void deleteSource(const char* category, const std::string& name)
    {
        // 先校验操作状态和名称，不允许在程序运行时删除源文件。
        requireSelected();
        idle();
        validateName(name);

        auto& registeredFiles = project[category];
        const std::string normalizedName = upper(name);
        const auto fileIterator = std::find_if(registeredFiles.begin(),
                                               registeredFiles.end(),
                                               [&](const auto& entry)
                                               {
                                                   return upper(entry.at("name")) == normalizedName;
                                               });
        if (fileIterator == registeredFiles.end())
        {
            throw Error(ErrorCode::NotFound, name);
        }

        // 这里只删除文件登记项；第三方 .task 内的残留程序引用会在编译时报告。
        auto updatedProject = project;
        const size_t fileIndex = static_cast<size_t>(fileIterator - registeredFiles.begin());
        updatedProject[category].erase(fileIndex);
        validateProject(updatedProject);

        // 校验通过后再将文件移入回收目录，方便恢复误删除的源代码。
        const std::string filename = fileIterator->at("file");
        const auto trashPath = app / ".trash" / (std::to_string(nowUs()) + "-" + filename);
        if (fs::exists(app / filename))
        {
            fs::create_directories(trashPath.parent_path());
            fs::rename(app / filename, trashPath);
        }

        // 保存新清单并使原编译模块失效，防止继续运行已删除的程序。
        project = std::move(updatedProject);
        invalidate();
    }

    void reset()
    {
        abortRequested = false;
        hasCycle = false;
        lastCycle = 0;
        deadline = 0;
        stepMode = 0;
        location = {};
        for (auto& t : tasks)
        {
            t.stats = {t.config.name, 0, 0, 0, 0};
            t.released = false;
            t.pending = false;
        }
        if (module)
        {
            init(&host);
            snapshot();
            status = ApplicationStatus::Ready;
        }
        else
        {
            status = ApplicationStatus::NotCompiled;
        }
        lastError.clear();
    }

    bool userFile(const std::string& filename) const
    {
        for (const char* kind : {"programs", "functions"})
        {
            for (auto& p : project.at(kind))
            {
                if (p.at("file") == filename)
                {
                    return true;
                }
            }
        }
        return false;
    }

    void trace(const char* file, uint32_t line, int depth, int call)
    {
        if (abortRequested.load(std::memory_order_relaxed))
        {
            throw AbortExecution{};
        }
        if (deadline && nowUs() > deadline)
        {
            throw Error(ErrorCode::Watchdog, "Task watchdog exceeded");
        }
        if (!debug || !file || !executionLock)
        {
            return;
        }
        const auto filename = fs::path(file).filename().string();
        if (!userFile(filename))
        {
            return;
        }
        bool pause = breakpoints.count({filename, line}) || stepMode == 2 ||
                     (stepMode == 1 && depth <= stepDepth);
        if (!pause)
        {
            return;
        }
        location = {filename, line, depth, call != 0, location.pauseCount + 1};
        stepMode = 0;
        resume = false;
        snapshot();
        status = ApplicationStatus::Breakpoint;
        cv.notify_all();
        cv.wait(*executionLock,
                [&]
                {
                    return resume || abortRequested.load();
                });
        if (abortRequested.load())
        {
            throw AbortExecution{};
        }
        status = ApplicationStatus::Running;
    }

    void executeCycle(uint64_t now)
    {
        try
        {
            if (hasCycle && now < lastCycle)
            {
                throw Error(ErrorCode::InvalidArgument, "Cycle time must be monotonic");
            }
            lastCycle = now;
            hasCycle = true;
            setTime(now);
            for (auto& t : tasks)
            {
                auto& c = t.config;
                if (!c.enabled)
                {
                    continue;
                }
                bool due =
                    c.type == STTaskType::FreeRunning || (c.type == STTaskType::Event && t.pending);
                if (c.type == STTaskType::Cyclic)
                {
                    due = !t.released || now - t.lastRelease >= c.intervalUs;
                }
                if (!due)
                {
                    continue;
                }
                if (c.cpuAffinity >= 0 && sched_getcpu() != c.cpuAffinity)
                {
                    throw Error(ErrorCode::InvalidState,
                                "Cycle caller CPU does not match task affinity");
                }
                if (c.type == STTaskType::Cyclic)
                {
                    if (t.released)
                    {
                        auto periods = (now - t.lastRelease) / c.intervalUs;
                        t.stats.missedPeriods += periods - 1;
                        t.lastRelease += periods * c.intervalUs;
                    }
                    else
                    {
                        t.lastRelease = now;
                        t.released = true;
                    }
                }
                t.pending = false;
                uint64_t start = nowUs();
                deadline = (!debug && c.watchdogUs) ? start + c.watchdogUs : 0;
                for (auto index : t.programs)
                {
                    run(index);
                }
                uint64_t duration = nowUs() - start;
                t.stats.lastDurationUs = duration;
                t.stats.maxDurationUs = std::max(t.stats.maxDurationUs, duration);
                ++t.stats.executions;
                if (deadline && nowUs() > deadline)
                {
                    throw Error(ErrorCode::Watchdog, "Task watchdog exceeded");
                }
                deadline = 0;
            }
            snapshot();
        }
        catch (const AbortExecution&)
        {
            status = ApplicationStatus::Ready;
        }
        catch (const std::exception& e)
        {
            lastError = e.what();
            status = ApplicationStatus::Error;
            snapshot();
        }
        catch (...)
        {
            lastError = "Unknown execution error";
            status = ApplicationStatus::Error;
        }
        deadline = 0;
    }

    void startWorker()
    {
        quit = false;
        worker = std::thread(
            [this]
            {
                std::unique_lock<std::mutex> lock(mutex);
                for (;;)
                {
                    cv.wait(lock,
                            [&]
                            {
                                return requested || quit;
                            });
                    if (quit)
                    {
                        break;
                    }
                    requested = false;
                    executionLock = &lock;
                    executeCycle(cycleTime);
                    executionLock = nullptr;
                    job = false;
                    cv.notify_all();
                }
            });
    }

    void resumeDebug(int mode, std::unique_lock<std::mutex>& lock)
    {
        if (status != ApplicationStatus::Breakpoint)
        {
            throw Error(ErrorCode::InvalidState, "Not paused at a breakpoint");
        }
        if (mode == 2 && !location.canStepInto)
        {
            throw Error(ErrorCode::InvalidState,
                        "StepInto requires a standalone function or function-block invocation");
        }
        stepMode = mode;
        stepDepth = location.depth;
        resume = true;
        status = ApplicationStatus::Running;
        cv.notify_all();
        cv.wait(lock,
                [&]
                {
                    return status == ApplicationStatus::Breakpoint || !job;
                });
    }

    size_t findVariable(const std::string& n) const
    {
        auto name = upper(n);
        size_t match = variables.size();
        for (size_t i = 0; i < variables.size(); ++i)
        {
            const auto& v = variables[i].name;
            if (v == name)
            {
                return i;
            }
            if (v.size() > name.size() &&
                v.compare(v.size() - name.size(), name.size(), name) == 0 &&
                v[v.size() - name.size() - 1] == '.')
            {
                if (match != variables.size())
                {
                    throw Error(ErrorCode::InvalidArgument, "Ambiguous variable name: " + n);
                }
                match = i;
            }
        }
        if (match == variables.size())
        {
            throw Error(ErrorCode::NotFound, "Variable not found: " + n);
        }
        return match;
    }
};

/// 初始化运行环境；若 work 保存了当前 APP，则恢复选择，但仍需重新编译。
Runtime::Runtime(Options options) : p_(std::make_unique<Impl>(std::move(options)))
{
    auto work = fs::path(p_->options.applicationRoot) / "work";
    if (fs::exists(work))
    {
        auto n = readFile(work);
        while (!n.empty() && (n.back() == '\n' || n.back() == '\r'))
        {
            n.pop_back();
        }
        if (!n.empty())
        {
            SelectApplication(n);
        }
    }
}

/// 停止任务并回收调试线程；析构期间不向调用方传播停止异常。
Runtime::~Runtime()
{
    try
    {
        StopProgram();
    }
    catch (...)
    {
    }
}

/// 创建应用目录和文件登记清单 .prj；不生成 ST 源文件或任务配置。
void Runtime::AddApplication(const std::string& name)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    validateName(name);
    for (auto& entry : fs::directory_iterator(p_->options.applicationRoot))
    {
        if (upper(entry.path().filename().string()) == upper(name))
        {
            throw Error(ErrorCode::AlreadyExists, name);
        }
    }
    auto app = fs::path(p_->options.applicationRoot) / name;
    if (!fs::create_directory(app))
    {
        throw Error(ErrorCode::AlreadyExists, name);
    }
    json project = {{"schemaVersion", 2},
                    {"name", name},
                    {"system", "LLPC Structured Text Runtime System"},
                    {"compiled", false},
                    {"types", json::array()},
                    {"globals", json::array()},
                    {"programs", json::array()},
                    {"functions", json::array()},
                    {"tasks", json::array()}};
    atomicWrite(app / (name + ".prj"), project.dump(2) + "\n");
}

/// 在停止状态选中 APP、取得独占锁并更新 work；只处理工程登记，不检查文件内容。
void Runtime::SelectApplication(const std::string& name)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->idle();
    validateName(name);
    if (name == p_->selected)
    {
        return;
    }
    auto app = fs::path(p_->options.applicationRoot) / name;
    if (fs::is_symlink(app))
    {
        throw Error(ErrorCode::InvalidArgument, "Symlink application directory is unsupported");
    }
    auto prj = normalizeProjectManifest(json::parse(readFile(app / (name + ".prj"))));
    int fd = open((app / ".hstr-lock").c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0 || flock(fd, LOCK_EX | LOCK_NB) < 0)
    {
        if (fd >= 0)
        {
            close(fd);
        }
        throw Error(ErrorCode::Busy, "Application is already open in another Runtime");
    }
    auto oldName = p_->selected;
    auto oldApp = p_->app;
    p_->selected = name;
    p_->app = app;
    try
    {
        p_->validateProject(prj);
        atomicWrite(fs::path(p_->options.applicationRoot) / "work", name + "\n");
    }
    catch (...)
    {
        p_->selected = oldName;
        p_->app = oldApp;
        close(fd);
        throw;
    }
    p_->unload();
    if (p_->appLock >= 0)
    {
        close(p_->appLock);
    }
    p_->appLock = fd;
    p_->project = std::move(prj);
    p_->project["compiled"] = false;
    p_->save();
}

/// 返回当前选中 APP 名称的副本，未选择时为空字符串。
std::string Runtime::SelectedApplication() const
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    return p_->selected;
}

/// 创建枚举/结构体类型文件并登记；内容由前端填写，已有文件不覆盖。
void Runtime::AddTypeFile(const std::string& name)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->registerFile("types", name, ".typ");
}

// 删除枚举/结构体类型文件登记，并保留回收副本。
void Runtime::DeleteTypeFile(const std::string& name)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->deleteSource("types", name);
}

// 创建全局变量文件并登记。
void Runtime::AddGlobalVariableFile(const std::string& name)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->registerFile("globals", name, ".GVL");
}

/// 只创建零字节 .st 文件并登记；具体内容由第三方编辑，已有文件绝不覆盖。
void Runtime::AddProgram(const std::string& name)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->registerFile("programs", name, ".st");
}

/// 只创建零字节 .st.fun 文件并登记；具体内容由第三方编辑，已有文件绝不覆盖。
void Runtime::AddFunction(const std::string& name)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->registerFile("functions", name, ".st.fun");
}

/// 只创建零字节 .task 文件并登记；具体内容由第三方编辑，已有文件绝不覆盖。
void Runtime::AddTask(const std::string& name)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->registerFile("tasks", name, ".task");
}

/// 注销文件并移入应用回收目录；源码和任务引用的有效性在下次编译时检查。
void Runtime::DeleteGlobalVariableFile(const std::string& name)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->deleteSource("globals", name);
}

/// 注销文件并移入应用回收目录；源码和任务引用的有效性在下次编译时检查。
void Runtime::DeleteProgram(const std::string& name)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->deleteSource("programs", name);
}

/// 注销文件并移入应用回收目录；源码和任务引用的有效性在下次编译时检查。
void Runtime::DeleteFunction(const std::string& name)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->deleteSource("functions", name);
}

/// 注销文件并移入应用回收目录；源码和任务引用的有效性在下次编译时检查。
void Runtime::DeleteTask(const std::string& name)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->deleteSource("tasks", name);
}

/// 读取第三方源文件和 .task 的字节快照，统一校验并编译；不回写这些原文件。
void Runtime::CompileProgram()
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->requireSelected();
    p_->idle();
    p_->unload();
    try
    {
        p_->project =
            normalizeProjectManifest(json::parse(readFile(p_->app / (p_->selected + ".prj"))));
        p_->validateProject(p_->project);
        if (p_->project["programs"].empty() || p_->project["tasks"].empty())
        {
            throw Error(ErrorCode::InvalidArgument, "At least one program and task are required");
        }
        // .prj 仅保存登记；先标记编译失效，再读取第三方维护的源文件和任务配置。
        p_->project["compiled"] = false;
        p_->save();
        const auto inputFiles = p_->readCompilationFiles();
        const auto taskConfigurations = p_->readTaskConfigurations(inputFiles);

        // CompileProgram入口已经检查停止状态，并调用unload()释放旧模块。
        // 在创建本轮构建目录之前清理历史缓存。
        // 清理失败只打印错误，不阻止本轮编译。
        ClearBuildCache(p_->app);

        auto result = detail::build(p_->options, p_->app, p_->project, inputFiles);
        p_->diagnostics = result.diagnostics;
        p_->sourceMap = result.sourceMap;
        p_->module = dlopen(result.module.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!p_->module)
        {
            throw Error(ErrorCode::CompileError, dlerror());
        }
        auto symbol = [&](const char* name)
        {
            auto fn = dlsym(p_->module, name);
            if (!fn)
            {
                throw Error(ErrorCode::CompileError, std::string("Missing module symbol: ") + name);
            }
            return fn;
        };
        auto abi = reinterpret_cast<int (*)()>(symbol("hstr_abi"));
        if (abi() != HSTR_MODULE_ABI)
        {
            throw Error(ErrorCode::CompileError, "Module ABI mismatch");
        }
        auto interfaceSignature =
            reinterpret_cast<const char* (*)()>(symbol("hstr_interface_signature"));
        const char* signature = interfaceSignature();
        if (!signature || std::strcmp(signature, HSTR_INTERFACE_SIGNATURE) != 0)
        {
            throw Error(ErrorCode::CompileError, "Module hardware interface signature mismatch");
        }
        p_->init = reinterpret_cast<void (*)(HSTR_ModuleHost*)>(symbol("hstr_init"));
        p_->run = reinterpret_cast<void (*)(size_t)>(symbol("hstr_run"));
        p_->setTime = reinterpret_cast<void (*)(uint64_t)>(symbol("hstr_set_time"));
        p_->count = reinterpret_cast<size_t (*)()>(symbol("hstr_variable_count"));
        p_->list = reinterpret_cast<HSTR_ModuleVariable* (*)()>(symbol("hstr_variable_list"));
        p_->init(&p_->host);
        size_t offset = 0;
        auto* v = p_->list();
        json exported = json::array();
        for (size_t i = 0; i < p_->count(); ++i)
        {
            offset = (offset + alignof(std::max_align_t) - 1) & ~(alignof(std::max_align_t) - 1);
            p_->variables.push_back({v[i].name, v[i].type, offset, v[i].size, v[i].read_only != 0});
            exported.push_back({{"name", v[i].name},
                                {"type", v[i].type},
                                {"offset", offset},
                                {"size", v[i].size},
                                {"readOnly", v[i].read_only != 0}});
            offset += v[i].size;
        }
        p_->memory.resize(offset);
        for (const auto& config : taskConfigurations)
        {
            Impl::Task t;
            t.config = config;
            t.stats.name = t.config.name;
            for (auto& name : t.config.programs)
            {
                size_t i = 0;
                for (auto& prog : p_->project["programs"])
                {
                    if (upper(prog.at("name")) == upper(name))
                    {
                        break;
                    }
                    ++i;
                }
                t.programs.push_back(i);
            }
            p_->tasks.push_back(std::move(t));
        }
        std::stable_sort(p_->tasks.begin(),
                         p_->tasks.end(),
                         [](auto& a, auto& b)
                         {
                             return a.config.priority < b.config.priority;
                         });
        p_->project["compiled"] = true;
        p_->project["module"] = result.module.string();
        p_->save();
        atomicWrite(p_->app / "variables.json", exported.dump(2));
        // 不回写第三方原文件；精确保留本轮读取的字节，用于 ExecuteProgram 的失效检查。
        p_->compiledFiles = inputFiles;
        p_->compiledFiles[p_->selected + ".prj"] = readFile(p_->app / (p_->selected + ".prj"));
        p_->reset();
    }
    catch (const std::exception& e)
    {
        p_->unload();                                             // 卸载运行资源
        p_->lastError = e.what();                                 // 保存异常信息
        p_->diagnostics = e.what();                               // 保存诊断信息
        p_->status = ApplicationStatus::Error;                     // 设置Application错误状态
        std::cerr << "[HSTR] 编译失败：" << e.what() << std::endl; // 打印异常，不再抛出
    }
}

/// 确认所有编译输入未被第三方修改后进入运行态；debug 控制是否创建调试线程。
void Runtime::ExecuteProgram(bool debug)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->idle();
    if (!p_->module || p_->status != ApplicationStatus::Ready)
    {
        throw Error(ErrorCode::InvalidState, "Compile/reset successfully before starting");
    }
    for (auto& entry : p_->compiledFiles)
    {
        if (!fs::exists(p_->app / entry.first) || readFile(p_->app / entry.first) != entry.second)
        {
            p_->unload();
            throw Error(ErrorCode::InvalidState,
                        "Project changed on disk; recompile before starting");
        }
    }
    p_->debug = debug;
    p_->abortRequested = false;
    p_->status = ApplicationStatus::Running;
    if (debug && !p_->worker.joinable())
    {
        try
        {
            p_->startWorker();
        }
        catch (...)
        {
            p_->debug = false;
            p_->status = ApplicationStatus::Ready;
            throw;
        }
    }
}

/// 请求中止当前 ST 执行、结束调试线程，并恢复变量初始值和任务统计。
void Runtime::StopProgram()
{
    p_->abortRequested = true;
    p_->cv.notify_all();
    std::unique_lock<std::mutex> lock(p_->mutex);
    if (p_->worker.joinable())
    {
        p_->quit = true;
        p_->resume = true;
        p_->cv.notify_all();
        lock.unlock();
        p_->worker.join();
        lock.lock();
    }
    p_->job = false;
    p_->requested = false;
    p_->debug = false;
    p_->reset();
}

/// 采用与 StopProgram 相同的停止并复位行为；之后需要显式 ExecuteProgram。
void Runtime::ResetProgram()
{
    StopProgram();
}

/// 普通模式锁忙时跳过扫描；调试模式等待锁，再等待断点或本轮结束。
ApplicationStatus Runtime::Cycle(uint64_t now)
{
    std::unique_lock<std::mutex> lock(p_->mutex, std::try_to_lock);
    if (!lock.owns_lock())
    {
        if (!p_->debug.load(std::memory_order_acquire))
        {
            // 总线正常扫描保持非阻塞，不能等待管理线程。
            return p_->status.load();
        }
        // 调试线程启动时也会短暂持有 mutex。调试扫描必须等到真正提交执行，
        // 否则调用者会把尚未执行的 Running 状态误认为断点没有命中。
        lock.lock();
    }
    if (p_->status != ApplicationStatus::Running || p_->job)
    {
        return p_->status.load();
    }
    if (p_->debug)
    {
        p_->cycleTime = now;
        p_->job = true;
        p_->requested = true;
        p_->cv.notify_all();
        p_->cv.wait(lock,
                    [&]
                    {
                        return p_->status == ApplicationStatus::Breakpoint || !p_->job;
                    });
    }
    else
    {
        p_->executionLock = &lock;
        p_->executeCycle(now);
        p_->executionLock = nullptr;
    }
    return p_->status.load();
}

/// 读取单调时钟后执行一轮调度，供真实总线周期回调调用。
ApplicationStatus Runtime::Cycle()
{
    return Cycle(nowUs());
}

/// 将指定 Event 任务标记为待执行，多次触发合并为下一次 Cycle 的一次运行。
void Runtime::TriggerEvent(const std::string& name)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    for (auto& t : p_->tasks)
    {
        if (upper(t.config.name) == upper(name))
        {
            if (t.config.type != STTaskType::Event)
            {
                throw Error(ErrorCode::InvalidArgument, "Not an event task");
            }
            t.pending = true;
            return;
        }
    }
    throw Error(ErrorCode::NotFound, name);
}

/// 在已编译的源行映射中校验文件和行号，再登记执行前断点。
void Runtime::AddBreakpoint(const std::string& file, uint32_t line)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    bool found = false;
    for (auto& l : p_->sourceMap)
    {
        if (l.at("file") == file && l.at("line") == line)
        {
            found = true;
        }
    }
    if (!found)
    {
        throw Error(ErrorCode::InvalidArgument,
                    "No executable statement at " + file + ":" + std::to_string(line));
    }
    p_->breakpoints.insert({file, line});
}

/// 移除指定源行断点；不存在时保持当前断点集合。
void Runtime::RemoveBreakpoint(const std::string& filename, uint32_t line)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->breakpoints.erase({filename, line});
}

/// 从当前暂停点步过一条语句，等待相同或较浅调用层的下一暂停点。
void Runtime::SingleStep()
{
    std::unique_lock<std::mutex> lock(p_->mutex);
    p_->resumeDebug(1, lock);
}

/// 从独立函数或功能块调用语句进入内部；不允许在赋值等其他语句上调用。
void Runtime::StepInto()
{
    std::unique_lock<std::mutex> lock(p_->mutex);
    p_->resumeDebug(2, lock);
}

/// 恢复当前暂停扫描，等待下一个断点或本轮结束；后续扫描仍由 Cycle 驱动。
void Runtime::Continue()
{
    std::unique_lock<std::mutex> lock(p_->mutex);
    p_->resumeDebug(0, lock);
}

/// 返回最近一次调试暂停位置的副本，包括源文件、行号与调用深度。
DebugLocation Runtime::GetDebugLocation() const
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    return p_->location;
}

/// 校验名称、字节大小和可写属性，写入 PLC 实际变量后刷新全部过程映像别名。
void Runtime::ModifyVariable(const std::string& name, const void* data, size_t size)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    size_t index = p_->findVariable(name);
    auto& v = p_->variables[index];
    if (!data || size != v.size || v.readOnly)
    {
        throw Error(ErrorCode::InvalidArgument, "Variable size/read-only mismatch");
    }
    std::memcpy(p_->list()[index].address, data, size);
    p_->snapshot();
}

/// 将指定变量的快照复制到调用方缓冲区；缓冲区大小必须与变量精确一致。
void Runtime::ReadVariable(const std::string& name, void* data, size_t size) const
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    auto& v = p_->variables[p_->findVariable(name)];
    if (!data || size != v.size)
    {
        throw Error(ErrorCode::InvalidArgument, "Variable size mismatch");
    }
    std::memcpy(data, p_->memory.data() + v.offset, size);
}

/// 返回变量名称、类型、大小和过程映像偏移的列表副本。
std::vector<VariableInfo> Runtime::GetVariableList() const
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    return p_->variables;
}

/// 返回只读过程映像地址；调用方须与扫描同步，且不能跨重新编译持有旧地址。
const void* Runtime::GetVariableMemory() const
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    return p_->memory.data();
}

/// 返回当前过程映像总字节数；未装载程序时为零。
size_t Runtime::GetVariableMemorySize() const
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    return p_->memory.size();
}

/// 原子读取 APP 状态，无需等待执行锁。
ApplicationStatus Runtime::GetApplicationStatus() const
{
    return p_->status.load();
}

/// 返回最近一次编译或运行错误的诊断副本。
std::string Runtime::GetLastError() const
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    return p_->lastError;
}

/// 返回最近编译的输入校验、matiec 或系统 C 编译器诊断。
std::string Runtime::GetCompilerDiagnostics() const
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    return p_->diagnostics;
}

/// 按调度顺序返回任务执行次数、漏周期及耗时统计的副本。
std::vector<TaskStatistics> Runtime::GetTaskStatistics() const
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    std::vector<TaskStatistics> s;
    for (auto& t : p_->tasks)
    {
        s.push_back(t.stats);
    }
    return s;
}

/// 在停止状态安装硬件适配回调；回调上下文由调用方维持有效。
void Runtime::SetHardwareCallbacks(const HardwareCallbacks& callbacks, const char* interfaceSignature)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    p_->idle();
    // 先校验再复制，防止新旧头文件与 HSTR.so 混用时读取错误布局。
    if (!interfaceSignature || std::strcmp(interfaceSignature, HSTR_INTERFACE_SIGNATURE) != 0)
    {
        throw Error(ErrorCode::InvalidArgument,
                    "Hardware interface signature mismatch; rebuild the host with matching HSTR headers");
    }
    p_->hardware = callbacks;
}

// 包含所有 Runtime 硬件方法及别名，锁和转发逻辑统一生成。
#include <HSTR/generated/runtime_definitions.inc>
} // namespace HSTR
