#ifndef _LLC_ACTION_MotionSet_H
#define _LLC_ACTION_MotionSet_H

#include "LLC_SMBase.h"

#ifdef __cplusplus
extern "C"
{
#endif//extern "C"


//Axis
bool action_AxisEnable(actionParam);
bool action_AxisDisable(actionParam);
bool action_AxisReset(actionParam);
bool action_AxisHome(actionParam);
bool action_AxisSetOffset(actionParam);
bool action_AxisSetModesOfOperation(actionParam);


//AxisGroup
bool action_ControllerReset(actionParam);





#ifdef __cplusplus
}
#endif //extern "C"

#endif //_LLC_ACTION_MotionSet_H