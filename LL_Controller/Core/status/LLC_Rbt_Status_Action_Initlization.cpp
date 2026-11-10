
#include "LLC_Rbt_Status.h"
#include "LLC_Entry.h"

#include "LL_Log.h"


void LLC_Rbt_Status::InitAction_UnInitialized()
{
    //初始化LLC状态机
    AddLLCTrans(UnInitialized, Cmd_UnInitialized, Cmd_Msg, NULL, action_Initlization, ControllerNotConnect);
    AllNoTransLLCCmd(UnInitialized)
}

void LLC_Rbt_Status::InitAction_PreInitializing()
{
    AllNoTransLLCCmd(PreInitializing)
}

void LLC_Rbt_Status::InitAction_ControllerNotConnect()
{
    //主站功能指令
    AddLLCTrans(ControllerNotConnect, Cmd_StartMaster, Cmd_Msg, NULL, action_StartMaster, PreControllerConnecting);
    AddLLCTrans(ControllerNotConnect, Cmd_ScanSlave,Cmd_Msg, NULL, action_ScanSlave,ControllerNotConnect);
    AddLLCTrans(ControllerNotConnect, Cmd_ReloadESI,Cmd_Msg, NULL, action_ReloadESI,ControllerNotConnect);
    AddLLCTrans(ControllerNotConnect, Cmd_ControllerReset, Cmd_Msg, NULL, action_ControllerReset, ControllerNotConnect);

    AllNoTransLLCCmd(ControllerNotConnect)
}
void LLC_Rbt_Status::InitAction_EmergencyStop()
{
    AddLLCTrans(EmergencyStop, Cmd_CloseMaster, Cmd_Msg, NULL, action_CloseMaster, ControllerNotConnect);
    AllNoTransLLCCmd(EmergencyStop)
}
void LLC_Rbt_Status::InitAction_PreControllerConnecting()
{
    //定时状态机错误检测
    AddTrans_CheckErrorTimer(PreControllerConnecting)
    AddLLCTrans(PreControllerConnecting, Cmd_CheckIs_SlaveDrop, Cmd_Timer, guard_SlaveDrop, NULL, ControllerNotConnect);
    AddLLCTrans(PreControllerConnecting, Cmd_CheckIs_SlavesOP, Cmd_Timer, guard_SlavesOP, NULL, StandBy); 
    AllNoTransLLCCmd(PreControllerConnecting)
}
//理论上WatchDog状态仅能跳转到ControllerNotConnect状态
void LLC_Rbt_Status::InitAction_WatchDog()  //在看门狗循环中会运行复位函数，在LLC_Action.c中查看
{
    AddLLCTrans(WatchDog, Cmd_CheckIs_WatchDogHandled, Cmd_Timer, guard_WatchDogHandled, NULL, ControllerNotConnect);
    AddLLCTrans(WatchDog, Cmd_CloseMaster, Cmd_Msg, NULL, action_CloseMaster, ControllerNotConnect);
    AddLLCTrans(WatchDog, Cmd_ControllerReset, Cmd_Msg, NULL, action_ControllerReset, ControllerNotConnect);
    AllNoTransLLCCmd(WatchDog)
}


void LLC_Rbt_Status::InitAction_StandBy()   //使能状态
{
    //定时状态机错误检测
    AddTrans_CheckErrorTimer(StandBy)
    AddLLCTrans(StandBy, Cmd_CheckIs_Moving, Cmd_Timer, guard_Moving, NULL, Moving);


    //ExpandAxisMotion(StandBy, StandBy)

    //触发轨迹执行与控制
    AddLLCTrans(StandBy, Cmd_ControllerStop, Cmd_Msg, NULL, action_ControllerStop, StandBy);
    AddLLCTrans(StandBy, Cmd_AxisStop, Cmd_Msg, NULL, action_AxisStop, StandBy);
    AddLLCTrans(StandBy, Cmd_CloseMaster,Cmd_Msg, NULL, action_CloseMaster, ControllerNotConnect);  
    AddLLCTrans(StandBy, Cmd_ControllerReset, Cmd_Msg, NULL, action_ControllerReset, StandBy); 
    AllNoTransLLCCmd(StandBy) 
    AxisControlCmd(StandBy) 
    StartAxisGroupMotion(StandBy, Moving)
}

void LLC_Rbt_Status::InitAction_Moving()
{
    //定时状态机错误检测
    AddTrans_CheckErrorTimer(Moving)
    AddLLCTrans(Moving, Cmd_CheckIs_Standby, Cmd_Timer, guard_Standby, NULL, StandBy);

    //轴组控制指令-自动运动控制指令
    StartAxisGroupMotion(Moving, Moving)
    AddLLCTrans(Moving, Cmd_CloseMaster,Cmd_Msg, NULL, action_CloseMaster, ControllerNotConnect); 
    AddLLCTrans(Moving, Cmd_AxisStop, Cmd_Msg, NULL, action_AxisStop, Moving);
    AddLLCTrans(Moving, Cmd_ControllerStop, Cmd_Msg, NULL, action_ControllerStop, Moving);
    AddLLCTrans(Moving, Cmd_ControllerReset, Cmd_Msg, NULL, action_ControllerReset, Moving); 
    AllNoTransLLCCmd(Moving)
    AxisControlCmd(Moving) 

}
void LLC_Rbt_Status::InitAction_ErrorStopping()
{
    //定时状态机错误检测
    AddLLCTrans(ErrorStopping, Cmd_CheckIs_ErrorStoppingHandled, Cmd_Timer, guard_ErrorStoppingHandled, NULL, Error);
    AddLLCTrans(ErrorStopping, Cmd_CloseMaster, Cmd_Msg, NULL, action_CloseMaster, ControllerNotConnect);
    AddTrans_CheckErrorTimer(ErrorStopping)

    AllNoTransLLCCmd(ErrorStopping)
    AxisControlCmd(ErrorStopping) 
}
void LLC_Rbt_Status::InitAction_Error()
{
    //定时状态机错误检测
    AddLLCTrans(Error, Cmd_CheckIs_EmergencyStop, Cmd_Timer, guard_EmergencyStop, NULL, HandleEmergencyStop);
    AddLLCTrans(Error, Cmd_CheckIs_WatchDog, Cmd_Timer, guard_WatchDog, NULL, WatchDog);
    AddLLCTrans(Error, Cmd_CheckIs_SlaveDrop, Cmd_Timer, guard_SlaveDrop, NULL, ControllerNotConnect);


    AddLLCTrans(Error, Cmd_CloseMaster, Cmd_Msg, NULL, action_CloseMaster, ControllerNotConnect);
    AddLLCTrans(Error, Cmd_ControllerStop, Cmd_Msg, NULL, action_ControllerStop, Error);
    // AddLLCTrans(Error, Cmd_AxisStop, Cmd_Msg, NULL, action_AxisStop, Error);
    AddLLCTrans(Error, Cmd_ControllerReset, Cmd_Msg, NULL, action_ControllerReset, PreResettingError);
    AddLLCTrans(Error, Cmd_AxisStop, Cmd_Msg, NULL, action_AxisStop, PreResettingError);
    AllNoTransLLCCmd(Error)
    AxisControlCmd(Error) 
}
void LLC_Rbt_Status::InitAction_PreResettingError()
{
    //定时状态机错误检测
    AddTrans_CheckErrorTimer(PreResettingError)
    AllNoTransLLCCmd(PreResettingError)
    AxisControlCmd(PreResettingError) 
    AddLLCTrans(PreResettingError, Cmd_CheckIs_ResetFinish, Cmd_Timer, guard_IsResetFinish, NULL, StandBy);
}

void LLC_Rbt_Status::InitAction_HandleEmergencyStop()
{
    AddLLCTrans(HandleEmergencyStop, Cmd_CheckIs_EmergencyStopHandled, Cmd_Timer, guard_EmergencyStopHandled, NULL, EmergencyStop);
    AllNoTransLLCCmd(HandleEmergencyStop)
}