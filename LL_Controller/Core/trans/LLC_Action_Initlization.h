#ifndef _LLC_ACTION_Initlization_H
#define _LLC_ACTION_Initlization_H

#include "LLC_SMBase.h"

#ifdef __cplusplus
extern "C"
{
#endif//extern "C"

bool action_Initlization(actionParam);
bool action_StartMaster(actionParam);
bool action_CloseMaster(actionParam);
bool action_ScanSlave(actionParam);
bool action_ReloadESI(actionParam);
bool action_DumpInfo(actionParam);
#ifdef __cplusplus
}
#endif //extern "C"

#endif //_LLC_ACTION_Initlization_H