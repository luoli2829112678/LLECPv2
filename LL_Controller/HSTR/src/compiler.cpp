#include "internal.hpp"
#include "embedded.hpp"
#include <HSTR/generated/hardware_st.hpp>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <regex>
#include <sstream>
#include <thread>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
extern char** environ;
// matiec 原始 main 已在构建时重命名，并链接在 HSTR.so 内。
extern int hstr_matiec_main(int argc, char** argv);

namespace HSTR
{
/// 识别同一可执行文件的编译子模式并进入库内 matiec；普通启动直接返回 false。
bool HandleCompilerInvocation(int argc, char** argv, int& exitCode)
{
    if (argc < 2 || !argv || !argv[1] || std::strcmp(argv[1], "--hstr-compiler") != 0)
    {
        return false;
    }
    // 此分支仅用于当前可执行文件重新启动后的编译进程，不构造 Runtime。
    if (argc < 3)
    {
        exitCode = 2;
        return true;
    }
    exitCode = hstr_matiec_main(argc - 1, argv + 1);
    return true;
}
} // namespace HSTR

namespace HSTR::detail
{
/// 宿主遗漏编译分流时立即报错，避免反复创建 Runtime 和递归启动自身。
void rejectUnhandledCompilerInvocation()
{
    // 宿主若忘记在 main 最前面分流，必须报错，避免再次启动自身递归编译。
    std::ifstream command("/proc/self/cmdline", std::ios::binary);
    std::string argument;
    std::getline(command, argument, '\0');
    if (std::getline(command, argument, '\0') && argument == "--hstr-compiler")
    {
        throw Error(ErrorCode::InvalidState,
                    "Call HSTR::HandleCompilerInvocation at the beginning of main before "
                    "constructing Runtime");
    }
}

/// 将 IEC 标识符转换为大写副本，用于不区分大小写的名称比较。
std::string upper(std::string s)
{
    for (auto& c : s)
    {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

void validateName(const std::string& n)
{
    if (!std::regex_match(n, std::regex("[A-Za-z_][A-Za-z0-9_]{0,63}")))
    {
        throw Error(ErrorCode::InvalidArgument, "Invalid IEC identifier: " + n);
    }
    if (upper(n).rfind("HSTR", 0) == 0)
    {
        throw Error(ErrorCode::InvalidArgument, "HSTR prefix is reserved: " + n);
    }
}

std::string readFile(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f)
    {
        throw Error(ErrorCode::IOError, "Cannot read " + p.string());
    }
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

void atomicWrite(const fs::path& p, const std::string& text)
{
    fs::create_directories(p.parent_path());
    std::string temp = p.string() + ".tmp-XXXXXX";
    int fd = mkstemp(temp.data());
    if (fd < 0)
    {
        throw Error(ErrorCode::IOError, "Cannot create " + p.string());
    }
    size_t done = 0;
    while (done < text.size())
    {
        auto n = write(fd, text.data() + done, text.size() - done);
        if (n < 0 && errno == EINTR)
        {
            continue;
        }
        if (n <= 0)
        {
            close(fd);
            unlink(temp.c_str());
            throw Error(ErrorCode::IOError, "Write failed: " + p.string());
        }
        done += static_cast<size_t>(n);
    }
    if (fsync(fd) != 0)
    {
        close(fd);
        unlink(temp.c_str());
        throw Error(ErrorCode::IOError, "fsync failed");
    }
    close(fd);
    if (rename(temp.c_str(), p.c_str()) != 0)
    {
        unlink(temp.c_str());
        throw Error(ErrorCode::IOError, "Rename failed: " + p.string());
    }
}

/// 创建空文件的唯一底层入口：排他打开防止覆盖已由第三方写入的文件。
void createEmptyFile(const fs::path& path)
{
    const int descriptor = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (descriptor < 0)
    {
        const ErrorCode code = errno == EEXIST ? ErrorCode::AlreadyExists : ErrorCode::IOError;
        throw Error(code, "Cannot create empty file: " + path.string());
    }
    if (close(descriptor) != 0)
    {
        throw Error(ErrorCode::IOError, "Cannot close new file: " + path.string());
    }
}

/// 兼容上一版将任务配置放入 .prj 的结构；实际参数始终从原 .task 文件读取。
json normalizeProjectManifest(json project)
{
    if (project.at("schemaVersion") == 1)
    {
        json taskFiles = json::array();
        for (const auto& task : project.at("tasks"))
        {
            const std::string name = task.at("name");
            validateName(name);
            taskFiles.push_back({{"name", name}, {"file", name + ".task"}});
        }
        project["tasks"] = std::move(taskFiles);
        project["schemaVersion"] = 2;
    }
    // 兼容旧工程；已有types字段的格式由validateProject校验。
    if (!project.contains("types"))
    {
        project["types"] = json::array();
    }
    return project;
}

/// 读取非负整数，不允许负数、浮点数或超范围值被 JSON 库隐式转换。
static uint64_t unsignedTaskField(const json& task, const char* field, uint64_t maximum)
{
    const auto& value = task.at(field);
    if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<int64_t>() < 0))
    {
        throw Error(ErrorCode::InvalidArgument,
                    std::string(field) + " must be a nonnegative integer");
    }
    const uint64_t result = value.get<uint64_t>();
    if (result > maximum)
    {
        throw Error(ErrorCode::InvalidArgument, std::string(field) + " is out of range");
    }
    return result;
}

/// .task 是任务配置的唯一来源；这里只做解析和校验，不回写或补全原文件。
STTaskConfig parseTask(const json& task)
{
    if (!task.is_object())
    {
        throw Error(ErrorCode::InvalidArgument, "Task content must be a JSON object");
    }

    STTaskConfig config;
    config.name = task.at("name").get<std::string>();
    validateName(config.name);
    const std::string type = task.at("type").get<std::string>();
    if (type == "Cyclic")
    {
        config.type = STTaskType::Cyclic;
    }
    else if (type == "Event")
    {
        config.type = STTaskType::Event;
    }
    else if (type == "FreeRunning")
    {
        config.type = STTaskType::FreeRunning;
    }
    else
    {
        throw Error(ErrorCode::InvalidArgument, "Invalid task type: " + type);
    }

    const uint64_t maximumTime = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    config.intervalUs = unsignedTaskField(task, "intervalUs", maximumTime);
    config.watchdogUs = unsignedTaskField(task, "watchdogUs", maximumTime);
    config.priority = static_cast<uint32_t>(
        unsignedTaskField(task, "priority", std::numeric_limits<uint32_t>::max()));
    if (config.type == STTaskType::Cyclic && config.intervalUs == 0)
    {
        throw Error(ErrorCode::InvalidArgument, "Cyclic intervalUs must be positive");
    }

    const auto& affinity = task.at("cpuAffinity");
    if (!affinity.is_number_integer())
    {
        throw Error(ErrorCode::InvalidArgument, "cpuAffinity must be an integer");
    }
    if (affinity.is_number_unsigned() &&
        affinity.get<uint64_t>() > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
    {
        throw Error(ErrorCode::InvalidArgument, "cpuAffinity is out of range");
    }
    const int64_t cpu = affinity.get<int64_t>();
    if (cpu < -1 || cpu > std::numeric_limits<int32_t>::max())
    {
        throw Error(ErrorCode::InvalidArgument, "cpuAffinity must be -1 or a valid CPU number");
    }
    config.cpuAffinity = static_cast<int32_t>(cpu);

    if (!task.at("enabled").is_boolean())
    {
        throw Error(ErrorCode::InvalidArgument, "enabled must be a JSON boolean");
    }
    config.enabled = task.at("enabled").get<bool>();
    config.programs = task.at("programs").get<std::vector<std::string>>();
    if (config.programs.empty())
    {
        throw Error(ErrorCode::InvalidArgument, "Task must reference at least one program");
    }
    for (const auto& name : config.programs)
    {
        validateName(name);
    }
    return config;
}

static int process(const std::vector<std::string>& args, const fs::path& log)
{
    std::vector<char*> argv;
    for (auto& a : args)
    {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(
        &actions, STDOUT_FILENO, log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attr, 0);
    pid_t pid = 0;
    int rc = posix_spawnp(&pid, argv[0], &actions, &attr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    if (rc)
    {
        throw Error(ErrorCode::CompileError, "Cannot start " + args[0] + ": " + std::strerror(rc));
    }
    int status = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
    for (;;)
    {
        auto result = waitpid(pid, &status, WNOHANG);
        if (result == pid)
        {
            break;
        }
        if (result < 0 && errno != EINTR)
        {
            throw Error(ErrorCode::CompileError, "waitpid failed");
        }
        if (std::chrono::steady_clock::now() > deadline)
        {
            kill(-pid, SIGKILL);
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
            {
            };
            throw Error(ErrorCode::CompileError, "Compiler timed out");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

static std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> out;
    std::stringstream in(s);
    std::string item;
    while (std::getline(in, item, delimiter))
    {
        out.push_back(item);
    }
    return out;
}

// ST 声明、输入参数和 C pragma 转发都由接口清单生成。
static const char* builtins = hstr_detail::hardware_st;

/// 使用本轮已读取的第三方文件快照完成原生模块编译，所有产物写入私有构建目录。
BuildResult build(const Options& o,
                  const fs::path& app,
                  const json& project,
                  const std::map<std::string, std::string>& inputFiles)
{
    std::string temporary = (app / ".hstr-build-XXXXXX").string();
    if (!mkdtemp(temporary.data()))
    {
        throw Error(ErrorCode::IOError, "Cannot create build directory");
    }
    fs::path dir = temporary;
    for (auto& r : hstr_detail::resources)
    {
        atomicWrite(dir / r.name, std::string(reinterpret_cast<const char*>(r.data), r.size));
    }
    // 私有构建副本与第三方原文件分开；同一份字节同时用于解析和启动前失效检查。
    for (const auto& input : inputFiles)
    {
        atomicWrite(dir / "sources" / input.first, input.second);
    }
    atomicWrite(dir / "hardware.st", builtins);
    std::string source;
    std::vector<std::pair<std::string, uint32_t>> origins{{"", 0}};
    auto appendSource = [&](const std::string& filename, std::string text)
    {
        if (text.empty() || text.back() != '\n')
        {
            text += '\n';
        }
        uint32_t line = 1;
        for (char c : text)
        {
            if (c == '\n')
            {
                origins.push_back({filename, line++});
            }
        }
        source += text;
    };
    // 枚举、结构体及其依赖类型按.prj登记顺序加载，必须先于函数/程序。
    for (const auto& item : project.at("types"))
    {
        const std::string filename = item.at("file");
        appendSource(filename, inputFiles.at(filename));
    }
    appendSource("hardware.st", builtins);
    for (const char* category : {"functions", "programs"})
    {
        for (const auto& item : project.at(category))
        {
            std::string filename = item.at("file");
            const auto& text = inputFiles.at(filename);
            appendSource(filename, text);
        }
    }
    appendSource("<configuration>", "CONFIGURATION HSTRCONFIG\n");
    for (const auto& item : project.at("globals"))
    {
        std::string filename = item.at("file");
        const auto& text = inputFiles.at(filename);
        appendSource(filename, text);
    }
    appendSource(
        "<configuration>",
        "RESOURCE HSTRRESOURCE ON PLC\nTASK HSTRCLOCK(INTERVAL := T#1ms, PRIORITY := 0);\n");
    // One persistent instance per registered program; HSTR schedules body calls itself.
    for (const auto& p : project.at("programs"))
    {
        std::string name = p.at("name");
        appendSource("<configuration>",
                     "PROGRAM " + name + "_INSTANCE WITH HSTRCLOCK : " + name + ";\n");
    }
    appendSource("<configuration>", "END_RESOURCE\nEND_CONFIGURATION\n");
    atomicWrite(dir / "project.st", source);
    auto rc = process({"/proc/self/exe",
                       "--hstr-compiler",
                       "-e",
                       "-b",
                       "-i",
                       "-p",
                       "-l",
                       "-I",
                       (dir / "lib").string(),
                       "-T",
                       dir.string(),
                       (dir / "project.st").string()},
                      dir / "matiec.log");
    std::string diagnostics = readFile(dir / "matiec.log");
    // matiec诊断包含单行位置或行列范围，均映射回原始.typ/.st文件。
    const std::string diagnosticPrefix = (dir / "project.st").string() + ":";
    const std::regex diagnosticPosition(R"(([0-9]+)(?:-([0-9]+)\.\.([0-9]+)-([0-9]+))?:)");
    size_t diagnosticAt = 0;
    while ((diagnosticAt = diagnostics.find(diagnosticPrefix, diagnosticAt)) != std::string::npos)
    {
        const size_t positionAt = diagnosticAt + diagnosticPrefix.size();
        std::smatch match;
        const std::string remaining = diagnostics.substr(positionAt);
        if (!std::regex_search(remaining, match, diagnosticPosition, std::regex_constants::match_continuous))
        {
            diagnosticAt = positionAt;
            continue;
        }
        const size_t firstLine = std::stoul(match[1].str());
        if (firstLine >= origins.size())
        {
            diagnosticAt = positionAt;
            continue;
        }
        std::string mappedPosition = origins[firstLine].first + ":" + std::to_string(origins[firstLine].second);
        if (match[2].matched)
        {
            const size_t lastLine = std::stoul(match[3].str());
            if (lastLine < origins.size() && origins[lastLine].first == origins[firstLine].first)
            {
                mappedPosition += "-" + match[2].str() + ".." + std::to_string(origins[lastLine].second) + "-" + match[4].str();
            }
        }
        mappedPosition += ":";
        diagnostics.replace(diagnosticAt, diagnosticPrefix.size() + match.length(), mappedPosition);
        diagnosticAt += mappedPosition.size();
    }
    if (rc)
    {
        throw Error(ErrorCode::CompileError,
                    "matiec failed (" + std::to_string(rc) + "):\n" + diagnostics);
    }
    // Depth frames use GCC C cleanup so return/exit and exception unwinding balance them.
    std::string pous = readFile(dir / "POUS.c");
    // Translate compiler source positions back to registered files.
    std::regex originalTrace("HSTR_TRACE\\(\"([^\"]+)\", ([0-9]+), ([01])\\)");
    std::string mapped;
    size_t previous = 0;
    for (std::sregex_iterator it(pous.begin(), pous.end(), originalTrace), end; it != end; ++it)
    {
        auto line = std::stoul((*it)[2]);
        mapped += pous.substr(previous, it->position() - previous);
        if ((*it)[1].str() == (dir / "project.st").string() && line < origins.size())
        {
            mapped += "HSTR_TRACE(" + json(origins[line].first).dump() + ", " +
                      std::to_string(origins[line].second) + ", " + (*it)[3].str() + ")";
        }
        else
        {
            mapped += it->str();
        }
        previous = it->position() + it->length();
    }
    mapped += pous.substr(previous);
    pous = std::move(mapped);
    // Only function definitions (column zero), never if/while blocks.
    pous = std::regex_replace(
        pous,
        std::regex("(^|\\n)((?:static )?[A-Za-z_][A-Za-z_0-9 *]*\\([^;{}]*\\)\\s*\\{)"),
        "$1$2\nHSTR_FRAME;\n");
    // Poll loop back edges, including empty loops, for watchdog and stop requests.
    pous = std::regex_replace(pous, std::regex("\\bwhile\\s*\\("), "while (HSTR_POLL(), ");
    pous = std::regex_replace(pous, std::regex("(for\\s*\\([^;]*;)([^;]*;)"), "$1 HSTR_POLL(), $2");
    atomicWrite(dir / "POUS.c",
                "#ifndef HSTR_POUS_IMPL\n#define HSTR_POUS_IMPL\n" + pous + "\n#endif\n");
    json sourceMap = json::array();
    std::regex trace("HSTR_TRACE\\(\"([^\"]+)\", ([0-9]+), ([01])\\)");
    for (std::sregex_iterator it(pous.begin(), pous.end(), trace), end; it != end; ++it)
    {
        auto filename = fs::path((*it)[1].str()).filename().string();
        bool registered = false;
        for (const char* kind : {"programs", "functions"})
        {
            for (const auto& f : project.at(kind))
            {
                if (f.at("file") == filename)
                {
                    registered = true;
                }
            }
        }
        if (registered)
        {
            sourceMap.push_back(
                {{"file", filename}, {"line", std::stoul((*it)[2])}, {"call", (*it)[3] == "1"}});
        }
    }
    std::string wrapper = R"C(
#include "hstr_module.h"
#include <string.h>
#include <math.h>
#define DISABLE_EN_ENO_PARAMETERS
#include "iec_std_lib.h"
/* Used by IEC ties-to-even REAL/LREAL -> integer conversions. */
double iec_lib_fmod(double x, double y) { return fmod(x, y); }
static HSTR_ModuleHost hstr_host;
static int hstr_depth;
static void hstr_leave(int *v) { (void)v; --hstr_depth; }
#define HSTR_FRAME int hstr_frame __attribute__((cleanup(hstr_leave),unused)) = (++hstr_depth)
#define HSTR_TRACE(f,l,c) hstr_host.trace(hstr_host.user,f,l,hstr_depth,c)
#define HSTR_POLL() hstr_host.trace(hstr_host.user,0,0,hstr_depth,0)
TIME __CURRENT_TIME;
BOOL __DEBUG = 0;
)C";
    // Located variables get owned backing memory. Physical PDO access is explicit via callbacks.
    wrapper += R"C(
#define __LOCATED_VAR(type,name,...) static type name##_storage; type *name=&name##_storage;
#include "LOCATED_VARIABLES.h"
#undef __LOCATED_VAR
#include "HSTRCONFIG.c"
#include "HSTRRESOURCE.c"
)C";
    std::vector<std::vector<std::string>> vars;
    bool section = false;
    std::istringstream csv(readFile(dir / "VARIABLES.csv"));
    std::string line;
    while (std::getline(csv, line))
    {
        if (line == "// Variables")
        {
            section = true;
            continue;
        }
        if (line.rfind("//", 0) == 0)
        {
            section = false;
            continue;
        }
        if (!section || line.empty())
        {
            continue;
        }
        auto v = split(line, ';');
        if (v.size() < 6 || v[1] == "FB")
        {
            continue;
        }
        vars.push_back(v);
    }
    wrapper += "static HSTR_ModuleVariable hstr_variables[" +
               std::to_string(std::max<size_t>(vars.size(), 1)) + "];\n";
    wrapper +=
        "int hstr_abi(void) { return HSTR_MODULE_ABI; }\n"
        "const char* hstr_interface_signature(void) { return HSTR_INTERFACE_SIGNATURE; }\n"
        "void hstr_init(HSTR_ModuleHost *host) { "
        "hstr_host=*host; hstr_depth=0; __CURRENT_TIME=(TIME){0,0}; config_init__();\n";
    size_t index = 0;
    for (auto& v : vars)
    {
        auto path = split(v[3], '.');
        std::string expr;
        if (path.size() == 2)
        {
            expr = upper(path[0]) + "__" + upper(path[1]);
        }
        else
        {
            // matiec variable list: CONFIG.RESOURCE.INSTANCE.FIELD[.FBFIELD...]
            if (path.size() < 4)
            {
                throw Error(ErrorCode::CompileError, "Unsupported variable path: " + v[3]);
            }
            expr = upper(path[1]) + "__" + upper(path[2]);
            for (size_t i = 3; i < path.size(); ++i)
            {
                expr += "." + upper(path[i]);
            }
        }
        const bool pointer = v[1] == "EXT" || v[1] == "IN" || v[1] == "OUT" || v[1] == "MEM";
        std::string value = expr + ".value";
        std::string address = pointer ? value : "&" + value;
        std::string size = pointer ? "sizeof(*" + value + ")" : "sizeof(" + value + ")";
        std::string display = upper(v[2]);
        std::string type = v[1] == "ARRAY" ? "ARRAY OF " + v[4] : v[4];
        std::string readOnly = v.size() > 7 && v[7] == "1" ? "1" : "0";
        wrapper += "hstr_variables[" + std::to_string(index++) + "]=(HSTR_ModuleVariable){" +
                   json(display).dump() + "," + json(type).dump() + ",(void*)(" + address + ")," +
                   size + "," + readOnly + "};\n";
    }
    wrapper += "}\nsize_t hstr_variable_count(void){return " + std::to_string(vars.size()) +
               ";}\nHSTR_ModuleVariable* hstr_variable_list(void){return hstr_variables;}\n";
    wrapper +=
        "void hstr_set_time(uint64_t "
        "us){__CURRENT_TIME.tv_sec=us/1000000;__CURRENT_TIME.tv_nsec=(us%1000000)*1000;}\nvoid "
        "hstr_run(size_t program) { switch(program) {\n";
    index = 0;
    for (const auto& p : project.at("programs"))
    {
        std::string name = upper(p.at("name"));
        wrapper += "case " + std::to_string(index++) + ": " + name + "_body__(&HSTRRESOURCE__" +
                   name + "_INSTANCE); break;\n";
    }
    wrapper += "default: break; }}\n";
    atomicWrite(dir / "module.c", wrapper);
    rc = process({o.cCompiler,
                  "-std=gnu11",
                  "-shared",
                  "-fPIC",
                  "-fexceptions",
                  "-O0",
                  "-g",
                  "-Wl,-Bsymbolic",
                  "-Wno-unused",
                  "-I" + dir.string(),
                  "-I" + (dir / "lib/C").string(),
                  (dir / "module.c").string(),
                  "-o",
                  (dir / "program.so").string(),
                  "-lm"},
                 dir / "cc.log");
    diagnostics += readFile(dir / "cc.log");
    if (rc)
    {
        throw Error(ErrorCode::CompileError, "Native compilation failed:\n" + diagnostics);
    }
    atomicWrite(dir / "source-map.json", sourceMap.dump(2));
    return {dir / "program.so", diagnostics, sourceMap};
}
} // namespace HSTR::detail
