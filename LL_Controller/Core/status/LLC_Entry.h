/**
 * \defgroup LLC status entry interface
 *
 * \author LCC
 * \date 2023.07.10
 *
 * \brief status entry interface
 *
 */

#ifndef _LL_STATUS_ENTRY_H
#define _LL_STATUS_ENTRY_H

#include "LLC_SMBase.h"


#define LLCStateEntry(x) void Entry##x(stateParam);\
void Exit##x(stateParam);

#ifdef __cplusplus
extern "C"
{
#endif



LLCStateEntry(UnInitialized);
LLCStateEntry(PreInitializing);
LLCStateEntry(EmergencyStop);
LLCStateEntry(ControllerNotConnect);
LLCStateEntry(PreControllerConnecting); //5.正在连接控制器状态
LLCStateEntry(WatchDog);
LLCStateEntry(StandBy);
LLCStateEntry(Moving);
LLCStateEntry(ErrorStopping);
LLCStateEntry(Error);
LLCStateEntry(PreResettingError);
LLCStateEntry(HandleEmergencyStop);


#ifdef __cplusplus
}
#endif

#endif