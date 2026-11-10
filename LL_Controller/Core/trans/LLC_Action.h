#ifndef _LLC_ACTION_H
#define _LLC_ACTION_H

#include "LLC_SMBase.h"
#include "LL_String.h"


#define LLCStateCycleAction(x) bool action_CycleHandle##x(actionParam);

#define GET_AND_BREAK_NOT_INT(variable, param_str) int variable = 0;\
if(!LL_Str2Int(param_str, variable)){nRet = REC_CmdFormatError; break;}

#define GET_AND_BREAK_NOT_NUM(variable, param_str) double variable = 0;\
if(!LL_Str2Num(param_str, variable)){nRet = REC_CmdFormatError; break;}

#define GET_ST_AND_BREAK_NOT_NUM(struct_member, param_str) \
if(!LL_Str2Num(param_str, struct_member)) {nRet = REC_CmdFormatError; break;}

#ifdef __cplusplus
extern "C"
{
#endif//extern "C"

bool LLCIsAllParamInt(std::vector<std::string>params);

bool action_noTrans(actionParam);

LLCStateCycleAction(UnInitialized);         //0.未初始化状态
LLCStateCycleAction(PreInitializing);         //1.初始化中状态
LLCStateCycleAction(EmergencyStop);           //2.急停状态
LLCStateCycleAction(ControllerNotConnect);    //3.控制器未连接
LLCStateCycleAction(PreControllerConnecting); //4.正在连接控制器状态
LLCStateCycleAction(WatchDog);                //5.看门狗生效状态,同步从站的数据
LLCStateCycleAction(StandBy);                 //6.准备就绪状态
LLCStateCycleAction(Moving);                  //7.运动中状态
LLCStateCycleAction(ErrorStopping);           //8.错误停止处理中状态
LLCStateCycleAction(Error);                   //9.错误状态
LLCStateCycleAction(PreResettingError);       //10.错误恢复中
LLCStateCycleAction(HandleEmergencyStop);     //11.急停处理中);
#ifdef __cplusplus
}
#endif //extern "C"

#endif //_LLC_ACTION_H