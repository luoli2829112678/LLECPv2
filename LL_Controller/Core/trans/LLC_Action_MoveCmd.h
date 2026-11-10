#ifndef _LLC_ACTION_MoveCmd_H
#define _LLC_ACTION_MoveCmd_H

#include "LLC_SMBase.h"

#ifdef __cplusplus
extern "C"
{
#endif//extern "C"
// 控制器停止运动
bool action_ControllerStop(actionParam);
//轴停止运动
bool action_AxisStop(actionParam);

// 轴运动
bool action_AxisAbsMove(actionParam);
bool action_AxisRelMove(actionParam);
bool action_AxisBackAndForthMove(actionParam);
bool action_AxisContinuousStepMove(actionParam);
bool action_AxisVelocityMove(actionParam);

bool action_AxisFollowPosition(actionParam);
bool action_AxisFollowVelocity(actionParam);
bool action_AxisFollowTorque(actionParam);

bool action_SetAxisMove(actionParam);

bool action_InitWayPoint(actionParam);
bool action_PushWayPoint(actionParam);
bool action_WayPointPushFinish(actionParam);

//huayan
bool action_StartAnalogFocusing(actionParam);
bool action_StopAnalogFocusing(actionParam);
bool action_StartPEG(actionParam);
bool action_StopPEG(actionParam);


#ifdef __cplusplus
}
#endif //extern "C"

#endif 