#ifndef _LLSTATE_GUARD_H
#define _LLSTATE_GUARD_H

#include "LLC_SMBase.h"

#ifdef __cplusplus
extern "C"
{
#endif//extern "C"

//重构
bool guard_SlavesOP(guardParam);
// 检测是否触发急停
bool guard_EmergencyStop(guardParam);
// 检测是否急停处理完成
bool guard_EmergencyStopHandled(guardParam);
// 检测是否看门狗超时
bool guard_WatchDog(guardParam);
// 检测是否看门狗超时处理完成
bool guard_WatchDogHandled(guardParam);
// 检测是否从站掉线
bool guard_SlaveDrop(guardParam);
// 检测是否报错
bool guard_RobotError(guardParam);
// 检测是否错误处理完成
bool guard_ErrorStoppingHandled(guardParam);
//监测复位完成
bool guard_IsResetFinish(guardParam);

// 检测是否处于运动状态
bool guard_Moving(guardParam);
// 检测是否处于准备就绪状态
bool guard_Standby(guardParam);


#ifdef __cplusplus
}
#endif //extern "C"

#endif //_LLSTATE_GUARD_H