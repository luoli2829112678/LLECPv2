#ifndef _LLC_STATUS_MGR_H
#define _LLC_STATUS_MGR_H

#include <atomic>
#include <string>
using namespace std;


//#include "LLStruct_Robot.h"
#include "LL_SingletonBase.h"
#include "LLC_State.h"
#include "LL_SHMDef.h"
#include "LL_FSMDef.h"

//#include "LL_CmdMsg.h"

#include "LLC_Action.h"
#include "LLC_Action_Initlization.h"
#include "LLC_Action_ConfigHard.h"
#include "LLC_Action_ConfigParam.h"
#include "LLC_Action_General.h"
#include "LLC_Action_MotionSet.h"
#include "LLC_Action_MoveCmd.h"
#include "LLC_Action_HSTR.h"
#include "LLC_CmdMsg.h"

#include "LLC_guard.h"

#define InitLLCState(x) \
m_status[enCDSState_##x].SetState(enCDSState_##x,#x,Entry##x,Exit##x);\
RegisterTrans(m_status[enCDSState_##x],Cmd_CycleHandle,Cmd_Msg,NULL,action_CycleHandle##x,m_status[enCDSState_##x].GetState());

#define GetLLCStateST(x) (m_status[enCDSState_##x].GetState())

#define AddLLCTrans(x,strCmd,nMsgType,pGuard,pAction,nextState) \
RegisterTrans(m_status[enCDSState_##x],strCmd,nMsgType,pGuard,pAction,m_status[enCDSState_##nextState].GetState())

class LLC_Rbt_Status
{
public:
    LLC_Rbt_Status();
    // LLC_Rbt_Status(LLC_Rbt_Status &&) = default;
    // LLC_Rbt_Status(const LLC_Rbt_Status &) = default;
    // LLC_Rbt_Status &operator=(LLC_Rbt_Status &&) = default;
    // LLC_Rbt_Status &operator=(const LLC_Rbt_Status &) = default;
    virtual ~LLC_Rbt_Status();
    void Initialize_LLCStatus();
    struct llsm_state*GetCurState();
    void SetCurLLCFSM(unsigned int unCurCPSFSM,const std::string&stateData);
    unsigned int CurLLCFSM(std::string&stateData);
    int FSM_handleEvent(struct llsm_state * BeforLockCur, struct llsm_event *event);
    bool IsFsmCmdExist(const string& strCmd);
    void dumpInfo();
protected:
    struct llsm_state*GetCurStateChecklist(vector<string>& vecChecklist);
    void StatusCheck();
    SaftLock m_CurLLCFSMLock;
    unsigned int m_unCurCPSFSM;
    string m_strCurCPSFSM;
    

    struct llsm_stateMachine m_fsmStateUnchanged;  //状态无关状态机
    struct llsm_stateMachine m_fsmRbt;             //状态有关状态机
    SaftLock m_FSMLock;

    map<string,int> m_AllCmdMap;
    map<string,int> m_stateUnChangedCmdMap;  //状态无关命令列表
    LL_State m_stateUnchanged;               //状态无关状态
    LL_State m_status[enCDSState_Count];
private:
    void RegisterTrans(LL_State& curState,const string& strCmd,int nMsgType,
            _guardAPI pGuard,_actionAPI pAction,struct llsm_state *nextState);
    void InitAction();
    void InitAction_UnInitialized();
    //1.初始化中状态
    void InitAction_PreInitializing();
    //2.初始化急停状态
    void InitAction_EmergencyStop();
    //3.控制器未连接
    void InitAction_ControllerNotConnect();
    //4.正在连接控制器状态
    void InitAction_PreControllerConnecting();
    //5.看门狗生效状态
    void InitAction_WatchDog();
    //6.准备就绪状态
    void InitAction_StandBy();
    //7.运动中状态
    void InitAction_Moving();
    //8.错误停止处理中状态
    void InitAction_ErrorStopping();
    //9.错误状态
    void InitAction_Error();
    //10.错误恢复中
    void InitAction_PreResettingError();
    //11
    void InitAction_HandleEmergencyStop();
    //注册stateUnChanged命令
    void AddTrans_stateUnChangedCmd(string strCmd, bool (*action)(actionParam));
    //初始化stateUnChanged命令列表
    void InitCmds_stateUnChanged();
};


//-----------------------------------------配置初始化功能指令------------------------------------

//配置初始化功能指令  -仅控制器未连接状态下发
#define SetMasterFuncCmd(curstate) \
AddLLCTrans(curstate, Cmd_SetSimulationType, Cmd_Msg, NULL, action_SetSimulationType, curstate);\
AddLLCTrans(curstate, Cmd_InitMaster, Cmd_Msg, NULL, action_InitMaster, curstate);\
AddLLCTrans(curstate, Cmd_RegistrationSlave, Cmd_Msg, NULL, action_RegistrationSlave, curstate);\
AddLLCTrans(curstate, Cmd_ConfigureSlavePDO, Cmd_Msg, NULL, action_ConfigureSlavePDO, curstate);\
AddLLCTrans(curstate, Cmd_RegistrationAxis, Cmd_Msg, NULL, action_RegistrationAxis, curstate);\
AddLLCTrans(curstate, Cmd_RegistrationIO, Cmd_Msg, NULL, action_RegistrationIO, curstate);



//-----------------------------------------轴控制------------------------------------
//轴组控制指令-UI点击运动控制指令   
#define AxisControlCmd(curstate) \
AddLLCTrans(curstate, Cmd_AxisEnable, Cmd_Msg, NULL, action_AxisEnable, curstate);\
AddLLCTrans(curstate, Cmd_AxisDisable, Cmd_Msg, NULL, action_AxisDisable, curstate);\
AddLLCTrans(curstate, Cmd_AxisReset, Cmd_Msg, NULL, action_AxisReset, curstate);\
AddLLCTrans(curstate, Cmd_AxisSetOffset, Cmd_Msg, NULL, action_AxisSetOffset, curstate);\
AddLLCTrans(curstate, Cmd_AxisSetModesOfOperation , Cmd_Msg, NULL, action_AxisSetModesOfOperation, curstate);\
AddLLCTrans(curstate, Cmd_AxisHome, Cmd_Msg, NULL, action_AxisHome, curstate);

//轴控制指令-自动运动控制指令     -使能或运动中状态下执行
#define StartAxisGroupMotion(curstate,nextstate) 
//运动状态通过监测得到



#define NoTransSet(curstate) \
AddLLCTrans(curstate, Cmd_AxisAbsMove, Cmd_Msg, NULL, action_AxisAbsMove, curstate);\
AddLLCTrans(curstate, Cmd_AxisRelMove, Cmd_Msg, NULL, action_AxisRelMove, curstate);\
AddLLCTrans(curstate, Cmd_AxisBackAndForthMove, Cmd_Msg, NULL, action_AxisBackAndForthMove, curstate);\
AddLLCTrans(curstate, Cmd_AxisContinuousStepMove, Cmd_Msg, NULL, action_AxisContinuousStepMove, curstate);\
AddLLCTrans(curstate, Cmd_AxisVelocityMove, Cmd_Msg, NULL, action_AxisVelocityMove, curstate);\
AddLLCTrans(curstate, Cmd_SetAxisMove, Cmd_Msg, NULL, action_SetAxisMove, curstate);\
AddLLCTrans(curstate, Cmd_WayPointPushFinish, Cmd_Msg, NULL, action_WayPointPushFinish, curstate);\
AddLLCTrans(curstate, Cmd_SetAxisMovePos,  Cmd_Msg, NULL, action_SetAxisMovePos,  curstate);\
AddLLCTrans(curstate, Cmd_SetAxisMovePos,  Cmd_Msg, NULL, action_SetAxisStopParam,  curstate);\
AddLLCTrans(curstate, Cmd_SetAxisMoveVel,  Cmd_Msg, NULL, action_SetAxisMoveVel,  curstate);\
AddLLCTrans(curstate, Cmd_SetAxisMoveAcc,  Cmd_Msg, NULL, action_SetAxisMoveAcc,  curstate);\
AddLLCTrans(curstate, Cmd_SetAxisMoveDec,  Cmd_Msg, NULL, action_SetAxisMoveDec,  curstate);\
AddLLCTrans(curstate, Cmd_SetAxisMoveJerk, Cmd_Msg, NULL, action_SetAxisMoveJerk, curstate);\
AddLLCTrans(curstate, Cmd_InitWayPoint, Cmd_Msg, NULL, action_InitWayPoint, curstate);\
AddLLCTrans(curstate, Cmd_PushWayPoint, Cmd_Msg, NULL, action_PushWayPoint, curstate);\
AddLLCTrans(curstate, Cmd_WayPointPushFinish, Cmd_Msg, NULL, action_WayPointPushFinish, curstate);\
AddLLCTrans(curstate, Cmd_SetAccFFScale, Cmd_Msg, NULL, action_SetAccFFScale, curstate);\
AddLLCTrans(curstate, Cmd_SetVelFFScale, Cmd_Msg, NULL, action_SetVelFFScale, curstate);\
AddLLCTrans(curstate, Cmd_SetJerkFFScale, Cmd_Msg, NULL, action_SetJerkFFScale, curstate);\
AddLLCTrans(curstate, Cmd_WritePDO, Cmd_Msg, NULL, action_LLCWritePDO, curstate);\
AddLLCTrans(curstate, Cmd_FOEwrite, Cmd_Msg, NULL, action_FOEwrite, curstate);\
AddLLCTrans(curstate, Cmd_GetFOEwriteProgress, Cmd_Msg, NULL, action_GetFOEwriteProgress, curstate);\
AddLLCTrans(curstate, Cmd_FOEread, Cmd_Msg, NULL, action_FOEread, curstate);\
AddLLCTrans(curstate, Cmd_GetFOEreadProgress, Cmd_Msg, NULL, action_GetFOEreadProgress, curstate);\
AddLLCTrans(curstate, Cmd_UpdateESI, Cmd_Msg, NULL, action_UpdateESI, curstate);\
AddLLCTrans(curstate, Cmd_SetIO, Cmd_Msg, NULL, action_SetIO, curstate);

#define HSTRcmd(curstate) \
AddLLCTrans(curstate, Cmd_HSTR_CreateApplication, Cmd_Msg, NULL, action_HSTR_CreateApplication, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_SelectedApplication, Cmd_Msg, NULL, action_HSTR_SelectedApplication, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_AddProgram, Cmd_Msg, NULL, action_HSTR_AddProgram, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_DeleteProgram, Cmd_Msg, NULL, action_HSTR_DeleteProgram, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_AddFunction, Cmd_Msg, NULL, action_HSTR_AddFunction, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_DeleteFunction, Cmd_Msg, NULL, action_HSTR_DeleteFunction, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_AddTask, Cmd_Msg, NULL, action_HSTR_AddTask, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_DeleteTask, Cmd_Msg, NULL, action_HSTR_DeleteTask, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_AddTypeFile, Cmd_Msg, NULL, action_HSTR_AddTypeFile, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_DeleteTypeFile, Cmd_Msg, NULL, action_HSTR_DeleteTypeFile, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_AddGlobalVariableFile, Cmd_Msg, NULL, action_HSTR_AddGlobalVariableFile, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_DeleteGlobalVariableFile, Cmd_Msg, NULL, action_HSTR_DeleteGlobalVariableFile, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_CompileApplication, Cmd_Msg, NULL, action_HSTR_CompileApplication, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_RunApplication, Cmd_Msg, NULL, action_HSTR_RunApplication, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_ResetProgram, Cmd_Msg, NULL, action_HSTR_ResetProgram, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_AddBreakpoint, Cmd_Msg, NULL, action_HSTR_AddBreakpoint, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_DebugContinue, Cmd_Msg, NULL, action_HSTR_DebugContinue, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_DebugSingleStep, Cmd_Msg, NULL, action_HSTR_DebugSingleStep, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_DebugStepInto, Cmd_Msg, NULL, action_HSTR_DebugStepInto, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_GetDebugLocation, Cmd_Msg, NULL, action_HSTR_GetDebugLocation, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_GetApplicationStatus, Cmd_Msg, NULL, action_HSTR_GetApplicationStatus, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_RemoveBreakpoint, Cmd_Msg, NULL, action_HSTR_RemoveBreakpoint, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_StopApplication, Cmd_Msg, NULL, action_HSTR_StopApplication, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_ReadVariable, Cmd_Msg, NULL, action_HSTR_ReadVariable, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_WriteVariable, Cmd_Msg, NULL, action_HSTR_WriteVariable, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_GetSelectedApplication, Cmd_Msg, NULL, action_HSTR_GetSelectedApplication, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_GetLastError, Cmd_Msg, NULL, action_HSTR_GetLastError, curstate);\
AddLLCTrans(curstate, Cmd_HSTR_GetCompilerDiagnostics, Cmd_Msg, NULL, action_HSTR_GetCompilerDiagnostics, curstate);

//所有非状态跳转指令状态机
#define AllNoTransLLCCmd(curstate) \
        SetMasterFuncCmd(curstate)\
        NoTransSet(curstate)\
        //HSTRcmd(curstate)
        

//定时器检查
#define AddTrans_CheckErrorTimer(curstate) \
AddLLCTrans(curstate, Cmd_CheckIs_EmergencyStop, Cmd_Timer, guard_EmergencyStop, NULL, HandleEmergencyStop);\
AddLLCTrans(curstate, Cmd_CheckIs_WatchDog, Cmd_Timer, guard_WatchDog, NULL, WatchDog);\
AddLLCTrans(curstate, Cmd_CheckIs_SlaveDrop, Cmd_Timer, guard_SlaveDrop, NULL, Error);\
AddLLCTrans(curstate, Cmd_CheckIs_RobotError, Cmd_Timer, guard_RobotError, NULL, ErrorStopping);


#endif  