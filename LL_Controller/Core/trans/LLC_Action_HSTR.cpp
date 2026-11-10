#include"LLC_Action_HSTR.h"
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <type_traits>
#include <utility>
#include "LLC_Rbt.h"
// HSTR Action内部错误码
static constexpr int HSTR_ACTION_EXCEPTION = -32000;
static constexpr int HSTR_ACTION_NOT_READY = -32001;
static constexpr int HSTR_ACTION_UNSUPPORTED = -32002;

// 统一捕获HSTR异常，避免未捕获异常终止LLC进程
template<typename F>
static int HSTR_ActionInvoke(F&& fn)
{
    try
    {
        return fn(); // 执行HSTR函数并返回原始结果
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSTR] Action异常：" << e.what() << std::endl;
        return HSTR_ACTION_EXCEPTION;
    }
    catch (...)
    {
        std::cerr << "[HSTR] Action发生未知异常" << std::endl;
        return HSTR_ACTION_EXCEPTION;
    }
}

// 二进制数据转换为十六进制字符串
static std::string HSTR_BytesToHex(const uint8_t* data, size_t size)
{
    static const char hex[] = "0123456789ABCDEF";
    std::string result(size * 2, '0');
    for (size_t i = 0; i < size; ++i)
    {
        result[i * 2] = hex[data[i] >> 4];
        result[i * 2 + 1] = hex[data[i] & 0x0F];
    }
    return result;
}

// 十六进制字符串转换为二进制数据
static bool HSTR_HexToBytes(const std::string& text, std::vector<uint8_t>& data)
{
    auto HexValue = [](char c) -> int
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };

    if (text.size() % 2 != 0) return false;
    data.clear();
    data.reserve(text.size() / 2);

    for (size_t i = 0; i < text.size(); i += 2)
    {
        int high = HexValue(text[i]);
        int low = HexValue(text[i + 1]);
        if (high < 0 || low < 0) return false;
        data.push_back(static_cast<uint8_t>((high << 4) | low));
    }
    return true;
}

// 检测DebugLocation中是否存在指定成员，兼容不同结构体字段名称
#define HSTR_DECLARE_DEBUG_FIELD(member) \
template<typename T, typename = void> struct HSTR_Has_##member : std::false_type {}; \
template<typename T> struct HSTR_Has_##member<T, std::void_t<decltype(std::declval<const T&>().member)>> : std::true_type {};

HSTR_DECLARE_DEBUG_FIELD(fileName)
HSTR_DECLARE_DEBUG_FIELD(filename)
HSTR_DECLARE_DEBUG_FIELD(file)
HSTR_DECLARE_DEBUG_FIELD(line)
HSTR_DECLARE_DEBUG_FIELD(lineNumber)
HSTR_DECLARE_DEBUG_FIELD(callDepth)
HSTR_DECLARE_DEBUG_FIELD(depth)
HSTR_DECLARE_DEBUG_FIELD(canStepInto)
HSTR_DECLARE_DEBUG_FIELD(allowStepInto)

#undef HSTR_DECLARE_DEBUG_FIELD

// 将调试位置转换为字符串
template<typename T>
static std::string HSTR_FormatDebugLocation(const T& location)
{
    std::ostringstream out;
    bool first = true;

    auto Append = [&](const char* name, const auto& value)
    {
        if (!first) out << ";";
        out << name << "=" << value;
        first = false;
    };

    if constexpr (HSTR_Has_fileName<T>::value) Append("fileName", location.fileName);
    else if constexpr (HSTR_Has_filename<T>::value) Append("fileName", location.filename);
    else if constexpr (HSTR_Has_file<T>::value) Append("fileName", location.file);

    if constexpr (HSTR_Has_line<T>::value) Append("line", location.line);
    else if constexpr (HSTR_Has_lineNumber<T>::value) Append("line", location.lineNumber);

    if constexpr (HSTR_Has_callDepth<T>::value) Append("callDepth", location.callDepth);
    else if constexpr (HSTR_Has_depth<T>::value) Append("callDepth", location.depth);

    if constexpr (HSTR_Has_canStepInto<T>::value) Append("canStepInto", location.canStepInto);
    else if constexpr (HSTR_Has_allowStepInto<T>::value) Append("canStepInto", location.allowStepInto);

    return out.str();
}


// ============================================================================
// 1. 创建应用
// ============================================================================
bool action_HSTR_CreateApplication(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 1 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_CreateApplication(event->param[0]); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 2. 切换应用
// ============================================================================
bool action_HSTR_SelectedApplication(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 1 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_SelectedApplication(event->param[0]); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 3. 添加Program
// ============================================================================
bool action_HSTR_AddProgram(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 1 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_AddProgram(event->param[0]); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 4. 删除Program
// ============================================================================
bool action_HSTR_DeleteProgram(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 1 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_DeleteProgram(event->param[0]); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 5. 添加Function
// ============================================================================
bool action_HSTR_AddFunction(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 1 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_AddFunction(event->param[0]); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 6. 删除Function
// ============================================================================
bool action_HSTR_DeleteFunction(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 1 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_DeleteFunction(event->param[0]); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 7. 添加Task
// ============================================================================
bool action_HSTR_AddTask(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 1 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_AddTask(event->param[0]); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 8. 删除Task
// ============================================================================
bool action_HSTR_DeleteTask(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 1 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_DeleteTask(event->param[0]); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 9. 添加类型文件
// ============================================================================
bool action_HSTR_AddTypeFile(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 1 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_AddTypeFile(event->param[0]); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 10. 删除类型文件
// ============================================================================
bool action_HSTR_DeleteTypeFile(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 1 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_DeleteTypeFile(event->param[0]); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 11. 添加全局变量文件
// ============================================================================
bool action_HSTR_AddGlobalVariableFile(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 1 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_AddGlobalVariableFile(event->param[0]); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 12. 删除全局变量文件
// ============================================================================
bool action_HSTR_DeleteGlobalVariableFile(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 1 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_DeleteGlobalVariableFile(event->param[0]); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 13. 编译当前Application
// ============================================================================
bool action_HSTR_CompileApplication(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 0) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_CompileApplication(); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 14. 运行当前Application
// debug=0正常运行，debug=1启用断点调试
// ============================================================================
bool action_HSTR_RunApplication(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() > 1) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        bool bDebug = false;

        if (event->param.size() == 1)
        {
            GET_AND_BREAK_NOT_INT(nDebug, event->param[0])
            if (nDebug != 0 && nDebug != 1) {nRet = REC_CmdFormatError;break;}
            bDebug = (nDebug == 1);
        }

        if (nRet != REC_Successed) break;
        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_RunApplication(bDebug); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 15. 复位当前Application
// ============================================================================
bool action_HSTR_ResetProgram(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 0) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_ResetProgram(); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 16. 添加断点
// 参数：文件名、行号
// ============================================================================
bool action_HSTR_AddBreakpoint(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 2 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        GET_AND_BREAK_NOT_INT(nLine, event->param[1])
        if (nLine <= 0) {nRet = REC_CmdFormatError;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_AddBreakpoint(event->param[0], static_cast<uint32_t>(nLine)); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 17. 从断点继续执行
// ============================================================================
bool action_HSTR_DebugContinue(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 0) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_DebugContinue(); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 18. 单步步过
// ============================================================================
bool action_HSTR_DebugSingleStep(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 0) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_DebugSingleStep(); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 19. 单步进入
// ============================================================================
bool action_HSTR_DebugStepInto(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 0) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_DebugStepInto(); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 20. 获取当前调试暂停位置
// ============================================================================
bool action_HSTR_GetDebugLocation(actionParam)
{
    int nRet = REC_Successed;
    std::string value;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 0) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() -> int
        {
            HSTR::DebugLocation location{};
            int ret = LLCRbt->HSTR_GetDebugLocation(location);
            if (ret != REC_Successed) return ret;
            value = HSTR_FormatDebugLocation(location);
            return value.empty() ? HSTR_ACTION_UNSUPPORTED : REC_Successed;
        });
    } while (false);
    event->result.push_back(to_string(nRet));
    if (nRet == REC_Successed) event->result.push_back(value);
    return (nRet == REC_Successed);
}


// ============================================================================
// 21. 获取Application状态
// 状态：-1未初始化，0未编译，1运行，2错误，3断点暂停，4就绪
// ============================================================================
bool action_HSTR_GetApplicationStatus(actionParam)
{
    int nRet = REC_Successed;
    int nStatus = -1;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 0) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() -> int
        {
            nStatus = LLCRbt->HSTR_GetApplicationStatus();
            return REC_Successed;
        });
    } while (false);
    event->result.push_back(to_string(nRet));
    if (nRet == REC_Successed) event->result.push_back(to_string(nStatus));
    return (nRet == REC_Successed);
}


// ============================================================================
// 22. 移除断点
// 参数：文件名、行号
// ============================================================================
bool action_HSTR_RemoveBreakpoint(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 2 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        GET_AND_BREAK_NOT_INT(nLine, event->param[1])
        if (nLine <= 0) {nRet = REC_CmdFormatError;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_RemoveBreakpoint(event->param[0], static_cast<uint32_t>(nLine)); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 23. 停止Application
// ============================================================================
bool action_HSTR_StopApplication(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 0) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_StopApplication(); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 24. 读取ST变量
// 参数：变量名、字节数
// 返回：错误码、十六进制数据
// ============================================================================
bool action_HSTR_ReadVariable(actionParam)
{
    int nRet = REC_Successed;
    std::string value;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 2 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        GET_AND_BREAK_NOT_INT(nSize, event->param[1])
        if (nSize <= 0 || nSize > 1048576) {nRet = REC_CmdFormatError;break;}

        nRet = HSTR_ActionInvoke([&]() -> int
        {
            std::vector<uint8_t> data(static_cast<size_t>(nSize), 0);
            int ret = LLCRbt->HSTR_ReadVariable(event->param[0], data.data(), data.size());
            if (ret == REC_Successed) value = HSTR_BytesToHex(data.data(), data.size());
            return ret;
        });
    } while (false);
    event->result.push_back(to_string(nRet));
    if (nRet == REC_Successed) event->result.push_back(value);
    return (nRet == REC_Successed);
}


// ============================================================================
// 25. 写入ST变量
// 参数：变量名、字节数、十六进制数据
// ============================================================================
bool action_HSTR_WriteVariable(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 3 || event->param[0].empty()) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        GET_AND_BREAK_NOT_INT(nSize, event->param[1])
        if (nSize <= 0 || nSize > 1048576) {nRet = REC_CmdFormatError;break;}
        if (event->param[2].size() != static_cast<size_t>(nSize) * 2) {nRet = REC_CmdFormatError;break;}

        std::vector<uint8_t> data;
        if (!HSTR_HexToBytes(event->param[2], data)) {nRet = REC_CmdFormatError;break;}

        nRet = HSTR_ActionInvoke([&]() { return LLCRbt->HSTR_WriteVariable(event->param[0], data.data(), data.size()); });
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// ============================================================================
// 26. 获取当前选中的Application名称
// ============================================================================
bool action_HSTR_GetSelectedApplication(actionParam)
{
    int nRet = REC_Successed;
    std::string value;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 0) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() -> int
        {
            value = LLCRbt->HSTR_GetSelectedApplication();
            return REC_Successed;
        });
    } while (false);
    event->result.push_back(to_string(nRet));
    if (nRet == REC_Successed) event->result.push_back(value);
    return (nRet == REC_Successed);
}


// ============================================================================
// 27. 获取最近一次运行错误
// ============================================================================
bool action_HSTR_GetLastError(actionParam)
{
    int nRet = REC_Successed;
    std::string value;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 0) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() -> int
        {
            value = LLCRbt->HSTR_GetLastError();
            return REC_Successed;
        });
    } while (false);
    event->result.push_back(to_string(nRet));
    if (nRet == REC_Successed) event->result.push_back(value);
    return (nRet == REC_Successed);
}


// ============================================================================
// 28. 获取最近一次编译诊断
// ============================================================================
bool action_HSTR_GetCompilerDiagnostics(actionParam)
{
    int nRet = REC_Successed;
    std::string value;
    do
    {
        if (event == nullptr) return false;
        if (event->param.size() != 0) {nRet = REC_CmdFormatError;break;}
        if (LLCRbt == nullptr) {nRet = HSTR_ACTION_NOT_READY;break;}

        nRet = HSTR_ActionInvoke([&]() -> int
        {
            value = LLCRbt->HSTR_GetCompilerDiagnostics();
            return REC_Successed;
        });
    } while (false);
    event->result.push_back(to_string(nRet));
    if (nRet == REC_Successed) event->result.push_back(value);
    return (nRet == REC_Successed);
}

