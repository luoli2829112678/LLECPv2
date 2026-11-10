#ifndef _LLC_ACTION_ConfigHard_H
#define _LLC_ACTION_ConfigHard_H

#include "LLC_SMBase.h"

#ifdef __cplusplus
extern "C"
{
#endif//extern "C"
// 设置模拟机器人
bool action_SetSimulationType(actionParam);
// 设置主站
bool action_InitMaster(actionParam);
//注册从站
bool action_RegistrationSlave(actionParam);
//配置从站数据通道
bool action_ConfigureSlavePDO(actionParam);
//注册轴
bool action_RegistrationAxis(actionParam);
//注册IO
bool action_RegistrationIO(actionParam);
#ifdef __cplusplus
}
#endif //extern "C"

#endif 