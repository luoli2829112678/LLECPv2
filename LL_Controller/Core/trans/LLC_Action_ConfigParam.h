#ifndef _LLC_ACTION_ConfigParam_H
#define _LLC_ACTION_ConfigParam_H

#include "LLC_SMBase.h"

#ifdef __cplusplus
extern "C"
{
#endif//extern "C"

//设置轴停止参数
bool action_SetAxisStopParam(actionParam);

// 设置轴目标位置
bool action_SetAxisMovePos(actionParam);

// 设置轴运动速度
bool action_SetAxisMoveVel(actionParam);

// 设置轴加速度
bool action_SetAxisMoveAcc(actionParam);

// 设置轴减速度
bool action_SetAxisMoveDec(actionParam);

// 设置轴加加速度（Jerk）
bool action_SetAxisMoveJerk(actionParam);



//前馈设置
bool action_SetAccFFScale(actionParam);
bool action_SetVelFFScale(actionParam);
bool action_SetJerkFFScale(actionParam);

#ifdef __cplusplus
}
#endif //extern "C"

#endif 