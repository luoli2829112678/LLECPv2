#ifndef _LLC_ACTION_HSTR_H

#include "LLC_SMBase.h"

#ifdef __cplusplus
extern "C"
{
#endif//extern "C"

// 创建应用
bool action_HSTR_CreateApplication(actionParam);
// 切换应用
bool action_HSTR_SelectedApplication(actionParam);

// 添加Program
bool action_HSTR_AddProgram(actionParam);
// 删除Program
bool action_HSTR_DeleteProgram(actionParam);

// 添加Function
bool action_HSTR_AddFunction(actionParam);
// 删除Function
bool action_HSTR_DeleteFunction(actionParam);

// 添加Task
bool action_HSTR_AddTask(actionParam);
// 删除Task
bool action_HSTR_DeleteTask(actionParam);

// 添加类型文件（枚举、结构体）
bool action_HSTR_AddTypeFile(actionParam);
// 删除类型文件
bool action_HSTR_DeleteTypeFile(actionParam);

// 添加全局变量文件
bool action_HSTR_AddGlobalVariableFile(actionParam);
// 删除全局变量文件
bool action_HSTR_DeleteGlobalVariableFile(actionParam);

// 编译当前应用
bool action_HSTR_CompileApplication(actionParam);

// 运行当前应用，支持Debug模式
bool action_HSTR_RunApplication(actionParam);
// 添加断点
bool action_HSTR_AddBreakpoint(actionParam);
// 从断点继续执行
bool action_HSTR_DebugContinue(actionParam);
// 单步步过
bool action_HSTR_DebugSingleStep(actionParam);
// 单步进入
bool action_HSTR_DebugStepInto(actionParam);
// 获取当前调试暂停位置
bool action_HSTR_GetDebugLocation(actionParam);
// 获取应用运行状态
bool action_HSTR_GetApplicationStatus(actionParam);
// 移除断点
bool action_HSTR_RemoveBreakpoint(actionParam);

// 停止当前应用
bool action_HSTR_StopApplication(actionParam);

// 读取ST变量
bool action_HSTR_ReadVariable(actionParam);
// 写入ST变量
bool action_HSTR_WriteVariable(actionParam);

// 获取所有Task执行统计信息
bool action_HSTR_GetTaskStatistics(actionParam);

// 获取当前选中的应用名称
bool action_HSTR_GetSelectedApplication(actionParam);
// 获取最近一次运行错误
bool action_HSTR_GetLastError(actionParam);
// 获取最近一次编译诊断
bool action_HSTR_GetCompilerDiagnostics(actionParam);

bool action_HSTR_ResetProgram(actionParam);


#ifdef __cplusplus
}
#endif //extern "C"

#endif 