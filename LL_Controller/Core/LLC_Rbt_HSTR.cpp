#include "LLC_Rbt.h"
#include "LLC_Rbt_Bridge.h"  // 必须在Adapter之前，声明JSON映射的函数
#include <HSTR/LLPCAdapter.hpp>




int LLC_Rbt::HSTR_Init()
{
    namespace fs = std::filesystem;
    using Json = nlohmann::json;

    // 已初始化，不重复创建Runtime。
    if (m_pHSTRruntime)
    {
        return 0;
    }

    if (m_sApplicationPath.empty())
    {
        std::fprintf(stderr, "[HSTR] Application根目录不能为空\n");
        return -1;
    }

    // 创建Application根目录。
    std::error_code ec;
    fs::create_directories(m_sApplicationPath, ec);
    if (ec)
    {
        std::fprintf(stderr, "[HSTR] 创建Application根目录失败：%s\n", ec.message().c_str());
        return -1;
    }

    // 初始化Application名称规则。
    m_NamePattern = "[A-Za-z_][A-Za-z0-9_]{0,63}";

    HSTR::Options options;
    options.applicationRoot = m_sApplicationPath;

    // 构造函数自动读取work，并加载对应Application的.prj。
    auto runtime = std::make_unique<HSTR::Runtime>(options);

    // 注册llpc.json生成的Bridge回调。
    runtime->SetHardwareCallbacks(HSTR::MakeLLPCHardwareCallbacks());

    const std::string appName = runtime->SelectedApplication();

    // 只转移一次所有权，执行后局部变量runtime为空。
    m_pHSTRruntime = std::move(runtime);

    if (appName.empty())
    {
        std::printf("[HSTR] 初始化成功，尚未选择Application\n");
        return 0;
    }

    std::printf("[HSTR] 初始化成功，已加载Application：%s\n", appName.c_str());

    // 读取工程清单，只打印登记的文件名。
    const fs::path projectPath = fs::path(m_sApplicationPath) / appName / (appName + ".prj");

    std::ifstream projectFile(projectPath);
    if (!projectFile.is_open())
    {
        std::fprintf(stderr, "[HSTR] 无法读取工程清单：%s\n", projectPath.c_str());
        return 0; // 打印失败不改变已完成的初始化结果。
    }

    // JSON解析失败通过返回值判断。
    const Json project = Json::parse(projectFile, nullptr, false);
    if (project.is_discarded() || !project.is_object())
    {
        std::fprintf(stderr, "[HSTR] 工程清单格式错误：%s\n", projectPath.c_str());
        return 0;
    }

    auto printFileList = [&](const char* key, const char* title)
    {
        const auto items = project.find(key);
        if (items == project.end() || !items->is_array())
        {
            return;
        }

        std::printf("[HSTR] %s：\n", title);

        if (items->empty())
        {
            std::printf("  无\n");
            return;
        }

        for (const auto& item : *items)
        {
            if (!item.is_object())
            {
                continue;
            }

            const auto file = item.find("file");
            if (file != item.end() && file->is_string())
            {
                std::printf("  %s\n", file->get_ref<const std::string&>().c_str());
            }
        }
    };

    printFileList("types", "类型文件（枚举/结构体）");
    printFileList("globals", "全局变量文件");
    printFileList("programs", "程序文件");
    printFileList("functions", "函数文件");
    printFileList("tasks", "Task文件");

    return 0;
}

// 创建Application目录和.prj文件，同名目录删除后重建。
// 调用前必须停止并释放使用该Application的Runtime。
// 返回值：0成功，-1失败。
int LLC_Rbt::HSTR_CreateApplication(std::string AppName)
{
    namespace fs = std::filesystem;
    static std::mutex createMutex;
    std::lock_guard<std::mutex> lock(createMutex);
    // 检查名称：只允许英文字母、数字和下划线，不能以数字开头。
    auto isLetter = [](unsigned char ch)
    {
        return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
    };
    if (AppName.empty() || AppName.size() > 64)
    {
        std::fprintf(stderr, "[HSTR] Application名称长度必须为1～64个字符\n");
        return -1;
    }
    if (!isLetter(AppName.front()) && AppName.front() != '_')
    {
        std::fprintf(stderr, "[HSTR] Application名称必须以字母或下划线开头\n");
        return -1;
    }
    for (unsigned char ch : AppName)
    {
        if (!isLetter(ch) && !(ch >= '0' && ch <= '9') && ch != '_')
        {
            std::fprintf(stderr, "[HSTR] Application名称只能包含字母、数字和下划线\n");
            return -1;
        }
    }
    auto toUpper = [](std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::toupper(ch));
        });
        return value;
    };
    const std::string upperName = toUpper(AppName);

    // HSTR前缀由运行库保留，work用于保存当前Application名称。
    if (upperName.rfind("HSTR", 0) == 0 || upperName == "WORK")
    {
        std::fprintf(stderr, "[HSTR] Application名称为保留名称：%s\n", AppName.c_str());
        return -1;
    }

    // 创建Application根目录。
    const fs::path root(m_sApplicationPath);
    if (root.empty())
    {
        std::fprintf(stderr, "[HSTR] Application根目录不能为空\n");
        return -1;
    }
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec)
    {
        std::fprintf(stderr, "[HSTR] 创建根目录失败：%s\n", ec.message().c_str());
        return -1;
    }
    const fs::path app = root / AppName;
    // 拒绝仅大小写不同的名称，保持与HSTR的名称检查一致。
    fs::directory_iterator it(root, ec);
    const fs::directory_iterator end;
    if (ec)
    {
        std::fprintf(stderr, "[HSTR] 读取根目录失败：%s\n", ec.message().c_str());
        return -1;
    }
    while (it != end)
    {
        const std::string existingName = it->path().filename().string();

        if (toUpper(existingName) == upperName && existingName != AppName)
        {
            std::fprintf(stderr, "[HSTR] 已存在仅大小写不同的名称：%s\n", existingName.c_str());
            return -1;
        }

        it.increment(ec);
        if (ec)
        {
            std::fprintf(stderr, "[HSTR] 遍历根目录失败：%s\n", ec.message().c_str());
            return -1;
        }
    }

    // 检查同名路径，不跟随符号链接。
    const fs::file_status status = fs::symlink_status(app, ec);
    if (ec && ec != std::errc::no_such_file_or_directory)
    {
        std::fprintf(stderr, "[HSTR] 检查Application目录失败：%s\n", ec.message().c_str());
        return -1;
    }
    ec.clear();

    if (fs::is_symlink(status))
    {
        std::fprintf(stderr, "[HSTR] Application路径不能是符号链接：%s\n", app.c_str());
        return -1;
    }

    // 同名目录存在时，删除整个Application。
    if (fs::exists(status))
    {
        if (!fs::is_directory(status))
        {
            std::fprintf(stderr, "[HSTR] Application路径已被普通文件占用：%s\n", app.c_str());
            return -1;
        }

        fs::remove_all(app, ec);
        if (ec)
        {
            std::fprintf(stderr, "[HSTR] 删除同名Application失败：%s\n", ec.message().c_str());
            return -1;
        }
    }
    // 创建Application目录。
    const bool created = fs::create_directory(app, ec);
    if (ec)
    {
        std::fprintf(stderr, "[HSTR] 创建Application目录失败：%s\n", ec.message().c_str());
        return -1;
    }

    if (!created)
    {
        std::fprintf(stderr, "[HSTR] Application目录已存在，创建失败：%s\n", app.c_str());
        return -1;
    }

    // 生成与当前HSTR运行库一致的工程清单。
    const fs::path projectFile = app / (AppName + ".prj");
    const fs::path temporaryFile = app / (AppName + ".prj.tmp");

    const std::string projectContent =
        "{\n"
        "  \"schemaVersion\": 2,\n"
        "  \"name\": \"" + AppName + "\",\n"
        "  \"system\": \"LLPC Structured Text Runtime System\",\n"
        "  \"compiled\": false,\n"
        "  \"types\": [],\n"
        "  \"globals\": [],\n"
        "  \"programs\": [],\n"
        "  \"functions\": [],\n"
        "  \"tasks\": []\n"
        "}\n";

    // 写入失败时清理临时文件。
    auto removeTemporaryFile = [&]()
    {
        std::error_code cleanupError;
        fs::remove(temporaryFile, cleanupError);
        if (cleanupError)
        {
            std::fprintf(stderr, "[HSTR] 清理临时文件失败：%s\n", cleanupError.message().c_str());
        }
    };

    // 默认文件流通过状态位报告错误，不启用异常。
    std::ofstream out(temporaryFile, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        std::fprintf(stderr, "[HSTR] 打开工程临时文件失败：%s\n", temporaryFile.c_str());
        return -1;
    }
    out << projectContent;
    out.flush();
    if (!out.good())
    {
        std::fprintf(stderr, "[HSTR] 写入工程临时文件失败：%s\n", temporaryFile.c_str());
        out.close();
        removeTemporaryFile();
        return -1;
    }
    out.close();
    if (out.fail())
    {
        std::fprintf(stderr, "[HSTR] 关闭工程临时文件失败：%s\n", temporaryFile.c_str());
        removeTemporaryFile();
        return -1;
    }
    // 写入完成后，改名为正式.prj文件。
    fs::rename(temporaryFile, projectFile, ec);
    if (ec)
    {
        std::fprintf(stderr, "[HSTR] 生成工程清单失败：%s\n", ec.message().c_str());
        removeTemporaryFile();
        return -1;
    }
    std::printf("[HSTR] Application创建成功：%s\n", app.c_str());
    std::printf("[HSTR] 工程清单：%s\n", projectFile.c_str());
    return 0;
}

// 修改当前应用的文件登记，同时创建文件或将文件移入回收目录。
// 必须从管理线程调用，不要从总线实时线程调用。
int LLC_Rbt::HSTR_EditProjectFile(const std::string& name, void (HSTR::Runtime::*operation)(const std::string&), const char* action)
{
    if (!m_pHSTRruntime)
    {
        std::fprintf(stderr, "[HSTR] 尚未初始化\n");
        return -1;
    }

    if (m_pHSTRruntime->SelectedApplication().empty())
    {
        std::fprintf(stderr, "[HSTR] 尚未选择Application\n");
        return -1;
    }

    // 断点暂停仍属于执行过程中，必须先停止。
    const auto status = m_pHSTRruntime->GetApplicationStatus();
    if (status == HSTR::ApplicationStatus::Running || status == HSTR::ApplicationStatus::Breakpoint)
    {
        std::fprintf(stderr, "[HSTR] 请先停止Application，再%s\n", action);
        return -1;
    }

    // 名称只允许字母、数字、下划线，不能以数字开头，不包含扩展名。
    if (name.empty() || name.size() > 64)
    {
        std::fprintf(stderr, "[HSTR] 名称长度必须为1～64：%s\n", name.c_str());
        return -1;
    }

    auto isLetter = [](char ch)
    {
        return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
    };

    if (!isLetter(name.front()) && name.front() != '_')
    {
        std::fprintf(stderr, "[HSTR] 名称必须以字母或下划线开头：%s\n", name.c_str());
        return -1;
    }

    for (char ch : name)
    {
        if (!isLetter(ch) && !(ch >= '0' && ch <= '9') && ch != '_')
        {
            std::fprintf(stderr, "[HSTR] 名称包含非法字符，请勿携带扩展名：%s\n", name.c_str());
            return -1;
        }
    }

    // Runtime负责更新.prj、处理对应文件，并使旧编译结果失效。
    (m_pHSTRruntime.get()->*operation)(name);
    m_ApplicationStatus = m_pHSTRruntime->GetApplicationStatus();

    std::printf("[HSTR] %s成功：%s，请重新编译Application\n", action, name.c_str());
    return 0;
}

// 创建空的name.st，并登记到.prj的programs。
int LLC_Rbt::HSTR_AddProgram(const std::string& name)
{
    return HSTR_EditProjectFile(name, &HSTR::Runtime::AddProgram, "新增程序");
}

// 从programs注销，并将name.st移入.trash。
int LLC_Rbt::HSTR_DeleteProgram(const std::string& name)
{
    return HSTR_EditProjectFile(name, &HSTR::Runtime::DeleteProgram, "删除程序");
}

// 创建空的name.st.fun，并登记到.prj的functions。
int LLC_Rbt::HSTR_AddFunction(const std::string& name)
{
    return HSTR_EditProjectFile(name, &HSTR::Runtime::AddFunction, "新增函数");
}

// 从functions注销，并将name.st.fun移入.trash。
int LLC_Rbt::HSTR_DeleteFunction(const std::string& name)
{
    return HSTR_EditProjectFile(name, &HSTR::Runtime::DeleteFunction, "删除函数");
}

// 创建空的name.task，并登记到.prj的tasks。
// 前端必须填写Task的JSON内容后才能编译。
int LLC_Rbt::HSTR_AddTask(const std::string& name)
{
    return HSTR_EditProjectFile(name, &HSTR::Runtime::AddTask, "新增Task");
}

// 从tasks注销，并将name.task移入.trash。
int LLC_Rbt::HSTR_DeleteTask(const std::string& name)
{
    return HSTR_EditProjectFile(name, &HSTR::Runtime::DeleteTask, "删除Task");
}

// 创建空的name.typ，登记枚举/结构体类型文件。
int LLC_Rbt::HSTR_AddTypeFile(const std::string& name)
{
    return HSTR_EditProjectFile(name, &HSTR::Runtime::AddTypeFile, "新增类型文件");
}

// 注销枚举/结构体类型文件，同时将文件移入.trash。
int LLC_Rbt::HSTR_DeleteTypeFile(const std::string& name)
{
    return HSTR_EditProjectFile(name, &HSTR::Runtime::DeleteTypeFile, "删除类型文件");
}

// 新增全局变量文件。
int LLC_Rbt::HSTR_AddGlobalVariableFile(const std::string& name)
{
    return HSTR_EditProjectFile(name, &HSTR::Runtime::AddGlobalVariableFile, "新增全局变量文件");
}

// 从globals注销，并将name.GVL移入.trash。
int LLC_Rbt::HSTR_DeleteGlobalVariableFile(const std::string& name)
{
    return HSTR_EditProjectFile(name, &HSTR::Runtime::DeleteGlobalVariableFile, "删除全局变量文件");
}

// ==================== 切换应用 ====================

int LLC_Rbt::HSTR_SelectedApplication(std::string AppName)
{
    if (!m_pHSTRruntime)
    {
        std::fprintf(stderr, "[HSTR] 尚未初始化\n");
        return -1;
    }

    if (AppName.empty() || !std::regex_match(AppName, m_NamePattern))
    {
        std::fprintf(stderr, "[HSTR] Application名称非法：%s\n", AppName.c_str());
        return -1;
    }

    const auto status = m_pHSTRruntime->GetApplicationStatus();
    if (status == HSTR::ApplicationStatus::Running || status == HSTR::ApplicationStatus::Breakpoint)
    {
        std::fprintf(stderr, "[HSTR] 请先停止当前应用，再切换Application\n");
        return -1;
    }

    // SelectApplication内部读取<AppName>.prj并加载工程登记信息。
    // 切换到其他应用后，需要重新编译才能运行。
    m_pHSTRruntime->SelectApplication(AppName);

    std::printf("[HSTR] 已选择Application：%s\n", AppName.c_str());
    return 0;
}

// ==================== 编译工程 ====================

int LLC_Rbt::HSTR_CompileApplication()
{
    if (!m_pHSTRruntime)
    {
        std::fprintf(stderr, "[HSTR] 尚未初始化\n");
        return -1;
    }

    const std::string appName = m_pHSTRruntime->SelectedApplication();
    if (appName.empty())
    {
        std::fprintf(stderr, "[HSTR] 请先选择Application\n");
        return -1;
    }

    const auto status = m_pHSTRruntime->GetApplicationStatus();
    if (status == HSTR::ApplicationStatus::Running || status == HSTR::ApplicationStatus::Breakpoint)
    {
        std::fprintf(stderr, "[HSTR] 请先停止应用，再编译工程\n");
        return -1;
    }

    std::printf("[HSTR] 开始编译Application：%s\n", appName.c_str());

    // 根据工程登记信息读取GVL、程序、函数和Task文件，编译并加载模块。
    m_pHSTRruntime->CompileProgram();

    std::printf("[HSTR] Application编译完成：%s\n", appName.c_str());
    return 0;
}

// ==================== 运行工程 ====================

int LLC_Rbt::HSTR_RunApplication(bool debug)
{
    if (!m_pHSTRruntime)
    {
        std::fprintf(stderr, "[HSTR] 尚未初始化\n");
        return -1;
    }

    const std::string appName = m_pHSTRruntime->SelectedApplication();
    if (appName.empty())
    {
        std::fprintf(stderr, "[HSTR] 请先选择Application\n");
        return -1;
    }

    const auto status = m_pHSTRruntime->GetApplicationStatus();
    if (status != HSTR::ApplicationStatus::Ready)
    {
        std::fprintf(stderr, "[HSTR] 应用未处于就绪状态，当前状态=%d，请先停止或完成编译\n", static_cast<int>(status));
        return -1;
    }

    // 启动选定应用，不创建周期线程。
    // 后续必须由现有周期入口持续调用m_pHSTRruntime->Cycle()。
    m_pHSTRruntime->ExecuteProgram(debug);

    std::printf("[HSTR] Application已启动：%s，调试模式=%d\n", appName.c_str(), static_cast<int>(debug));
    return 0;
}
// 用来结束当前运行，让 ST 程序恢复到初始状态，方便重新运行。
int LLC_Rbt::HSTR_ResetProgram()
{
    if (!m_pHSTRruntime)
    {
        std::fprintf(stderr, "[HSTR] 尚未初始化\n");
        return -1;
    }

    if (m_pHSTRruntime->SelectedApplication().empty())
    {
        std::fprintf(stderr, "[HSTR] 尚未选择Application\n");
        return -1;
    }

    m_pHSTRruntime->ResetProgram();
    m_ApplicationStatus = m_pHSTRruntime->GetApplicationStatus();

    std::printf("[HSTR] Application已复位，当前状态=%d\n", static_cast<int>(m_ApplicationStatus));
    return 0;
}
// ==================== 添加断点 ====================

int LLC_Rbt::HSTR_AddBreakpoint(std::string fileName, uint32_t line)
{
    if (!m_pHSTRruntime)
    {
        std::fprintf(stderr, "[HSTR] 尚未初始化\n");
        return -1;
    }

    if (m_pHSTRruntime->SelectedApplication().empty())
    {
        std::fprintf(stderr, "[HSTR] 请先选择Application\n");
        return -1;
    }

    if (fileName.empty() || line == 0)
    {
        std::fprintf(stderr, "[HSTR] 断点文件名不能为空，行号必须从1开始\n");
        return -1;
    }

    const auto status = m_pHSTRruntime->GetApplicationStatus();
    if (status != HSTR::ApplicationStatus::Ready &&
        status != HSTR::ApplicationStatus::Running &&
        status != HSTR::ApplicationStatus::Breakpoint)
    {
        std::fprintf(stderr, "[HSTR] 请先成功编译Application，再添加断点\n");
        return -1;
    }

    // 行号必须对应编译结果中的可执行ST语句起始行。
    // 文件名示例：AxisProgram.st、EnableTestAxis.st.fun。
    m_pHSTRruntime->AddBreakpoint(fileName, line);

    std::printf("[HSTR] 已添加断点：%s，第%u行\n", fileName.c_str(), static_cast<unsigned>(line));
    return 0;
}

// 调试命令由管理线程串行调用，不要在总线实时线程中调用。
// 调用期间不得切换、销毁Runtime或并发执行其他调试命令。
int LLC_Rbt::HSTR_DebugExecute(void (HSTR::Runtime::*operation)(), const char* action, bool requireStepInto)
{
    if (!m_pHSTRruntime)
    {
        std::fprintf(stderr, "[HSTR] 尚未初始化\n");
        return -1;
    }

    if (m_pHSTRruntime->GetApplicationStatus() != HSTR::ApplicationStatus::Breakpoint)
    {
        std::fprintf(stderr, "[HSTR] %s失败：当前未处于断点暂停状态\n", action);
        return -1;
    }

    if (requireStepInto)
    {
        const auto location = m_pHSTRruntime->GetDebugLocation();
        if (!location.canStepInto)
        {
            std::fprintf(stderr, "[HSTR] 进入函数失败：当前位置不支持进入，file=%s, line=%u\n", location.file.c_str(), static_cast<unsigned>(location.line));
            return -1;
        }
    }

    // Runtime恢复暂停的执行栈，等待下一暂停位置或本轮扫描结束。
    (m_pHSTRruntime.get()->*operation)();
    m_ApplicationStatus = m_pHSTRruntime->GetApplicationStatus();

    if (m_ApplicationStatus == HSTR::ApplicationStatus::Error)
    {
        const auto error = m_pHSTRruntime->GetLastError();
        std::fprintf(stderr, "[HSTR] %s失败：%s\n", action, error.c_str());
        return -1;
    }

    if (m_ApplicationStatus == HSTR::ApplicationStatus::Breakpoint)
    {
        const auto location = m_pHSTRruntime->GetDebugLocation();
        std::printf("[HSTR] %s后暂停：file=%s, line=%u, depth=%d, canStepInto=%d\n", action, location.file.c_str(), static_cast<unsigned>(location.line), location.depth, location.canStepInto ? 1 : 0);
    }
    else
    {
        std::printf("[HSTR] %s完成，当前状态=%d\n", action, static_cast<int>(m_ApplicationStatus));
    }

    return 0;
}

// 继续执行当前暂停的扫描；后续扫描仍需要周期调用Cycle。
int LLC_Rbt::HSTR_DebugContinue()
{
    return HSTR_DebugExecute(&HSTR::Runtime::Continue, "继续运行", false);
}

// 步过当前语句；被调用函数内部已有的断点仍然有效。
int LLC_Rbt::HSTR_DebugSingleStep()
{
    return HSTR_DebugExecute(&HSTR::Runtime::SingleStep, "单步步过", false);
}

// 进入当前独立函数或功能块调用。
int LLC_Rbt::HSTR_DebugStepInto()
{
    return HSTR_DebugExecute(&HSTR::Runtime::StepInto, "单步进入", true);
}

// 仅在断点暂停状态返回有效位置，避免前端显示过期位置。
int LLC_Rbt::HSTR_GetDebugLocation(HSTR::DebugLocation& location)
{
    location = HSTR::DebugLocation{};

    if (!m_pHSTRruntime)
    {
        std::fprintf(stderr, "[HSTR] 尚未初始化\n");
        return -1;
    }

    if (m_pHSTRruntime->GetApplicationStatus() != HSTR::ApplicationStatus::Breakpoint)
    {
        return -1;
    }

    location = m_pHSTRruntime->GetDebugLocation();
    return 0;
}

// ==================== 获取运行状态 ====================

int LLC_Rbt::HSTR_GetApplicationStatus()
{
    if (!m_pHSTRruntime)
    {
        return -1;
    }
    // -1：尚未初始化
    //  0：NotCompiled，未编译
    //  1：Running，运行中，等待或正在执行Cycle
    //  2：Error，编译或执行错误
    //  3：Breakpoint，断点暂停
    //  4：Ready，已编译且停止
    return static_cast<int>(m_pHSTRruntime->GetApplicationStatus());
}

// 移除指定文件、指定行的断点。
int LLC_Rbt::HSTR_RemoveBreakpoint(std::string fileName, uint32_t line)
{
    if (!m_pHSTRruntime)
    {
        std::fprintf(stderr, "[HSTR] 尚未初始化\n");
        return -1;
    }

    if (m_pHSTRruntime->SelectedApplication().empty())
    {
        std::fprintf(stderr, "[HSTR] 请先选择Application\n");
        return -1;
    }

    if (fileName.empty() || line == 0)
    {
        std::fprintf(stderr, "[HSTR] 断点文件名不能为空，行号必须从1开始\n");
        return -1;
    }

    // 指定断点不存在时，底层接口直接忽略。
    m_pHSTRruntime->RemoveBreakpoint(fileName, line);

    std::printf("[HSTR] 已移除断点设置：%s，第%u行\n", fileName.c_str(), static_cast<unsigned>(line));
    return 0;
}

// HSTR实时周期入口。
// 不在这里编译、读取文件、打印日志或休眠。
int LLC_Rbt::HSTR_RT()
{
    if (!m_pHSTRruntime) { m_ApplicationStatus = HSTR::ApplicationStatus::NotCompiled; return -1; }
    hstrLogicalTimeUs += static_cast<std::uint64_t>(m_cycle_time * 1000.0);
    if (m_pHSTRruntime->GetApplicationStatus() == HSTR::ApplicationStatus::Running)
        m_pHSTRruntime->Cycle(hstrLogicalTimeUs);
    m_ApplicationStatus = m_pHSTRruntime->GetApplicationStatus();
    if (m_pLLC_LLC2CPSMemory)
        m_pLLC_LLC2CPSMemory->st_StatusArea.hstrApplicationStatus = static_cast<int>(m_ApplicationStatus);
    return static_cast<int>(m_ApplicationStatus);
}

// 停止当前应用，复位程序变量和Task统计。
// 仅停止ST执行，不自动停止电机或清除PDO输出。
int LLC_Rbt::HSTR_StopApplication()
{
    if (!m_pHSTRruntime)
    {
        std::fprintf(stderr, "[HSTR] 尚未初始化\n");
        return -1;
    }

    const std::string appName = m_pHSTRruntime->SelectedApplication();
    if (appName.empty())
    {
        std::fprintf(stderr, "[HSTR] 尚未选择Application\n");
        return -1;
    }

    m_pHSTRruntime->StopProgram();

    std::printf("[HSTR] Application已停止：%s\n", appName.c_str());
    return 0;
}

// 获取当前选中的应用名称。
// 未初始化或未选择应用时返回空字符串。
string LLC_Rbt::HSTR_GetSelectedApplication()
{
    if (!m_pHSTRruntime)
    {
        return "";
    }

    return m_pHSTRruntime->SelectedApplication();
}

// 获取最近一次运行错误。
// 返回Runtime保存的错误文本，不清除错误记录。
string LLC_Rbt::HSTR_GetLastError()
{
    if (!m_pHSTRruntime)
    {
        return "HSTR尚未初始化";
    }

    return m_pHSTRruntime->GetLastError();
}

// 获取最近一次编译诊断。
// 包括ST编译器和系统C编译器输出的诊断信息。
string LLC_Rbt::HSTR_GetCompilerDiagnostics()
{
    if (!m_pHSTRruntime)
    {
        return "HSTR尚未初始化";
    }

    return m_pHSTRruntime->GetCompilerDiagnostics();
}


// 读取变量快照，不直接访问运行中的变量内存。
int LLC_Rbt::HSTR_ReadVariable(const std::string& variableName, void* data, size_t size)
{
    if (!m_pHSTRruntime)
    {
        std::fprintf(stderr, "[HSTR] 尚未初始化\n");
        return -1;
    }

    if (variableName.empty() || !data || size == 0)
    {
        std::fprintf(stderr, "[HSTR] 读取变量参数错误\n");
        return -1;
    }

    m_pHSTRruntime->ReadVariable(variableName, data, size);
    return 0;
}

// 写入实际变量，ST程序后续执行时可能再次修改该值。
int LLC_Rbt::HSTR_WriteVariable(const std::string& variableName, const void* data, size_t size)
{
    if (!m_pHSTRruntime)
    {
        std::fprintf(stderr, "[HSTR] 尚未初始化\n");
        return -1;
    }

    if (variableName.empty() || !data || size == 0)
    {
        std::fprintf(stderr, "[HSTR] 写入变量参数错误\n");
        return -1;
    }

    m_pHSTRruntime->ModifyVariable(variableName, data, size);
    return 0;
}

// 获取统计快照，供前端查询；无需停止应用。
std::vector<HSTR::TaskStatistics> LLC_Rbt::HSTR_GetTaskStatistics()
{
    if (!m_pHSTRruntime)
    {
        std::fprintf(stderr, "[HSTR] 尚未初始化\n");
        return {};
    }

    return m_pHSTRruntime->GetTaskStatistics();
}

















// ============================================================
// HSTR多Task测试
//
// Application目录：/home/Application/<applicationName>
// 同名目录：删除后重新创建
//
// ST函数：
//   EnableTestAxis：调用轴使能接口
//   WriteTestPDO：调用PDO写入接口
//
// Task：
//   IOTask   ：10ms，执行IOProgram，首次执行写入一次PDO
//   AxisTask ：1ms，执行AxisProgram，首次使能，随后读取轴状态
//
// 不创建C++线程，由调用方通过Cycle()统一调度两个Task。
// 本函数执行约2秒后停止ST，并下发轴去使能命令。
//
// 调用要求：
// 1. 在非实时管理线程中调用。
// 2. EtherCAT和轴控制周期已经运行，轴、从站已经配置。
// 3. 没有其他Runtime正在使用同名Application。
// 4. 测试期间不能并发修改轴或从站配置。
// 5. PDO对象不能被其他控制逻辑覆盖，过程映像访问由宿主同步。
//
// 返回：
//   0  测试流程成功
//  -1  测试失败
// ============================================================
int LLC_Rbt::HSTR_TEST()
{
    const std::string& applicationName = "TEST";
    namespace fs = std::filesystem;
    using Clock = std::chrono::steady_clock;

    // ==================== 测试参数 ====================

    const int axis = 0;           // 轴编号，从0开始。
    const int slave = 1;          // 从站编号，从1开始。

    // 必须填写当前从站已经映射到RxPDO的对象。
    // pdoIndex填写实际对象索引，不是0x1600等PDO映射对象。
    const int pdoIndex = 0;       // 必填：0表示尚未配置。
    const int pdoSubindex = 0;    // 对象子索引。
    const int32_t pdoValue = 0;   // 写入值，类型对应ST的DINT。

    // Application统一存放目录。
    const fs::path applicationRoot = "/home/Application";

    // ==================== 防止测试重复运行 ====================

    static std::mutex testMutex;
    std::unique_lock<std::mutex> testLock(testMutex, std::try_to_lock);

    if (!testLock.owns_lock())
    {
        std::fprintf(stderr, "[HSTR_TEST] 已有测试正在运行\n");
        return -1;
    }

    fs::path app;
    std::unique_ptr<HSTR::Runtime> runtime;

    // 开始扫描后，ST可能已经下发使能。
    // 正常或异常退出时，都尝试下发去使能。
    bool enableMayHaveBeenSent = false;

    try
    {
        // ==================== 检查Application名称 ====================

        // 禁止路径分隔符和“..”，避免误删其他目录。
        // 名称最长64个字符，首字符必须为字母或下划线。
        const std::regex namePattern("[A-Za-z_][A-Za-z0-9_]{0,63}");

        if (!std::regex_match(applicationName, namePattern))
        {
            throw std::invalid_argument("Application名称非法：只能包含字母、数字和下划线，不能以数字开头，最长64个字符");
        }

        // ==================== 删除同名Application ====================

        fs::create_directories(applicationRoot);
        app = applicationRoot / applicationName;

        // 使用symlink_status，避免跟随Application路径上的符号链接。
        std::error_code statusError;
        const fs::file_status existingStatus = fs::symlink_status(app, statusError);

        if (statusError && statusError != std::errc::no_such_file_or_directory)
        {
            throw fs::filesystem_error("检查Application目录失败", app, statusError);
        }

        if (fs::is_symlink(existingStatus))
        {
            throw std::runtime_error("Application路径是符号链接，拒绝使用：" + app.string());
        }

        if (fs::exists(existingStatus))
        {
            std::printf("[HSTR_TEST] 删除同名Application：%s\n", app.c_str());

            // 此处删除同名Application的全部内容。
            // 必须确保其他Runtime已经停止并释放该Application。
            fs::remove_all(app);
            fs::create_directories(app);
        }

        // ==================== 创建Runtime ====================

        HSTR::Options options;
        options.applicationRoot = applicationRoot.string();

        runtime = std::make_unique<HSTR::Runtime>(options);

        // 注册由llpc.json生成的全部硬件回调。
        runtime->SetHardwareCallbacks(HSTR::MakeLLPCHardwareCallbacks());

        // AddApplication负责创建目录和工程清单。
        runtime->AddApplication(applicationName);
        runtime->SelectApplication(applicationName);

        // ==================== 注册函数、程序和Task ====================

        // 创建并登记两个ST函数文件。
        runtime->AddFunction("EnableTestAxis");
        runtime->AddFunction("WriteTestPDO");

        // 创建并登记两个ST程序。
        runtime->AddProgram("AxisProgram");
        runtime->AddProgram("IOProgram");

        // 创建并登记两个Task。
        // Task是HSTR逻辑任务，不是操作系统线程。
        runtime->AddTask("AxisTask");
        runtime->AddTask("IOTask");

        std::printf("[HSTR_TEST] Application已创建：%s\n", app.c_str());

        // ==================== 文件写入辅助函数 ====================

        auto writeFile = [](const fs::path& path, const std::string& content)
        {
            std::ofstream out;
            out.exceptions(std::ios::failbit | std::ios::badbit);
            out.open(path, std::ios::binary | std::ios::trunc);
            out << content;
            out.close();
        };

        // ==================== ST函数：轴使能 ====================

        writeFile(app / "EnableTestAxis.st.fun", R"ST(
FUNCTION EnableTestAxis : DINT
VAR_INPUT
    AxisId : DINT;
END_VAR

(* 调用llpc.json注册的使能接口 *)
(* 返回成功表示命令已受理，不代表物理使能已经完成 *)
EnableTestAxis := AXIS_ENABLE_RESULT(AxisId);

END_FUNCTION
)ST");

        // ==================== ST函数：写PDO ====================

        writeFile(app / "WriteTestPDO.st.fun", R"ST(
FUNCTION WriteTestPDO : VOID
VAR_INPUT
    SlaveId : DINT;
    ObjectIndex : DINT;
    ObjectSubindex : DINT;
    OutputValue : DINT;
END_VAR

(* 写入从站RxPDO输出过程映像 *)
(* 写入失败时，Bridge抛出异常，Runtime进入Error *)
SLAVE_PDO_WRITE(SlaveId, ObjectIndex, ObjectSubindex, OutputValue);

END_FUNCTION
)ST");

        // ==================== ST程序：轴控制 ====================

        const std::string axisProgram =
            "PROGRAM AxisProgram\n"
            "VAR\n"
            "    EnableSent : BOOL := FALSE;\n"
            "    EnableResult : DINT := -1;\n"
            "    AxisState : DINT := -1;\n"
            "    Calls : DINT := 0;\n"
            "END_VAR\n"
            "\n"
            "(* 第一次执行时，通过自定义函数下发使能 *)\n"
            "IF NOT EnableSent THEN\n"
            "    EnableResult := EnableTestAxis(" + std::to_string(axis) + ");\n"
            "    EnableSent := TRUE;\n"
            "END_IF;\n"
            "\n"
            "(* 每个轴任务周期读取轴状态 *)\n"
            "AxisState := AXIS_READ_STATE(" + std::to_string(axis) + ");\n"
            "Calls := Calls + 1;\n"
            "\n"
            "END_PROGRAM\n";

        writeFile(app / "AxisProgram.st", axisProgram);

        // ==================== ST程序：PDO输出 ====================

        const std::string ioProgram =
            "PROGRAM IOProgram\n"
            "VAR\n"
            "    WriteDone : BOOL := FALSE;\n"
            "    Calls : DINT := 0;\n"
            "END_VAR\n"
            "\n"
            "(* 第一次执行时，通过自定义函数写入PDO *)\n"
            "IF NOT WriteDone THEN\n"
            "    WriteTestPDO(1, 111, 1, 22);\n"
            "    WriteDone := TRUE;\n"
            "END_IF;\n"
            "\n"
            "(* 统计IO任务执行次数 *)\n"
            "Calls := Calls + 1;\n"
            "\n"
            "END_PROGRAM\n";

        writeFile(app / "IOProgram.st", ioProgram);

        // ==================== AxisTask：1ms ====================

        writeFile(app / "AxisTask.task", R"JSON(
{
    "name": "AxisTask",
    "type": "Cyclic",
    "intervalUs": 1000,
    "priority": 1,
    "watchdogUs": 0,
    "cpuAffinity": -1,
    "enabled": true,
    "programs": ["AxisProgram"]
}
)JSON");

        // ==================== IOTask：10ms ====================

        // priority数值越小，调度顺序越靠前。
        // 首次Cycle会执行两个Task，因此首轮先写PDO，再下发使能。
        // 这里仅保证软件执行顺序，不保证从站已经收到PDO。
        writeFile(app / "IOTask.task", R"JSON(
{
    "name": "IOTask",
    "type": "Cyclic",
    "intervalUs": 10000,
    "priority": 0,
    "watchdogUs": 0,
    "cpuAffinity": -1,
    "enabled": true,
    "programs": ["IOProgram"]
}
)JSON");

        // ==================== 编译ST ====================

        std::printf("[HSTR_TEST] 开始编译Application：%s\n", applicationName.c_str());

        runtime->CompileProgram();

        // false：不启用断点调试。
        runtime->ExecuteProgram(false);

        // ==================== 周期调度两个Task ====================

        const auto deadline = Clock::now() + std::chrono::seconds(2);
        auto nextPrint = Clock::now();

        while (Clock::now() < deadline)
        {
            // Cycle中可能下发使能，异常退出也要尝试去使能。
            enableMayHaveBeenSent = true;

            // Cycle统一检查两个Task是否到期，并依次执行。
            const HSTR::ApplicationStatus status = runtime->Cycle();

            if (status != HSTR::ApplicationStatus::Running)
            {
                throw std::runtime_error("ST执行失败：" + runtime->GetLastError());
            }

            // 使能失败是业务返回码，由ST保存和处理，不因此停止整个Application。
            const int32_t enableResult = runtime->ReadVariable<int32_t>("AXISPROGRAM_INSTANCE.ENABLERESULT");

            // 每100ms输出一次监控信息。
            const auto now = Clock::now();

            if (now >= nextPrint)
            {
                const int32_t state = runtime->ReadVariable<int32_t>("AXISPROGRAM_INSTANCE.AXISSTATE");
                const int32_t axisCalls = runtime->ReadVariable<int32_t>("AXISPROGRAM_INSTANCE.CALLS");
                const int32_t ioCalls = runtime->ReadVariable<int32_t>("IOPROGRAM_INSTANCE.CALLS");
                const bool writeDone = runtime->ReadVariable<bool>("IOPROGRAM_INSTANCE.WRITEDONE");

                std::printf("[HSTR_TEST] app=%s, enableResult=%d, state=%d, axisCalls=%d, ioCalls=%d, pdoWritten=%d\n", applicationName.c_str(), static_cast<int>(enableResult), static_cast<int>(state), static_cast<int>(axisCalls), static_cast<int>(ioCalls), static_cast<int>(writeDone));

                nextPrint = now + std::chrono::milliseconds(100);
            }

            // 测试调用间隔，不保证硬实时1ms。
            // 实际Task执行次数及错过周期数通过统计接口查看。
            ::usleep(1000);
        }

        // ==================== 检查Task执行统计 ====================

        // StopProgram会复位统计，所以必须在停止之前读取。
        const auto taskStatistics = runtime->GetTaskStatistics();

        if (taskStatistics.size() != 2)
        {
            throw std::runtime_error("Task数量不符合预期，应为2个");
        }

        for (const auto& task : taskStatistics)
        {
            std::printf("[Task] name=%s, executions=%llu, missed=%llu, maxDurationUs=%llu\n", task.name.c_str(), static_cast<unsigned long long>(task.executions), static_cast<unsigned long long>(task.missedPeriods), static_cast<unsigned long long>(task.maxDurationUs));

            if (task.executions == 0)
            {
                throw std::runtime_error("Task未实际执行：" + task.name);
            }
        }

        // 确认PDO写入函数已经正常返回。
        if (!runtime->ReadVariable<bool>("IOPROGRAM_INSTANCE.WRITEDONE"))
        {
            throw std::runtime_error("ST程序未完成PDO写入");
        }

        // ==================== 停止ST并下发去使能 ====================

        runtime->StopProgram();

        const int disableResult = LLC_AxisDisable(axis);
        enableMayHaveBeenSent = false;

        std::printf("[HSTR_TEST] 下发轴去使能：axis=%d, result=%d\n", axis, disableResult);

        if (disableResult != REC_Successed)
        {
            throw std::runtime_error("轴去使能命令失败，返回码=" + std::to_string(disableResult));
        }

        // 释放Runtime，但保留Application工程目录。
        runtime.reset();

        std::printf("[HSTR_TEST] PASS，Application保留在：%s\n", app.c_str());
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "[HSTR_TEST] FAIL：%s\n", error.what());

        if (runtime)
        {
            std::fprintf(stderr, "[HSTR_TEST] 编译诊断：\n%s\n", runtime->GetCompilerDiagnostics().c_str());

            // 析构停止执行并释放模块和应用锁。
            runtime.reset();
        }

        // ST可能已经下发使能，异常退出时尝试去使能。
        if (enableMayHaveBeenSent)
        {
            try
            {
                const int result = LLC_AxisDisable(axis);
                std::fprintf(stderr, "[HSTR_TEST] 异常退出，下发去使能返回=%d\n", result);
            }
            catch (const std::exception& disableError)
            {
                std::fprintf(stderr, "[HSTR_TEST] 去使能异常：%s\n", disableError.what());
            }
        }

        // 失败时也不删除Application，方便排查。
        if (!app.empty())
        {
            std::fprintf(stderr, "[HSTR_TEST] Application目录：%s\n", app.c_str());
        }

        return -1;
    }
}
