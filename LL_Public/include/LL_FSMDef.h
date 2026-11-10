#ifndef _LL_FSMDEF_H
#define _LL_FSMDEF_H

//CPS状态机
enum EN_LLCPS_Status
{
    enCPSState_PreInitialize=0,//0
    enCPSState_Initialize,//1 初始化
    //电箱状态机
    enCPSState_ElectricBoxDisconnect,//2 与电箱控制板断开
    enCPSState_ElectricBoxConnecting,//3 连接电箱控制板
    enCPSState_EmergencyStopHandling,//4 急停处理
    enCPSState_EmergencyStop,//5 急停
    enCPSState_Blackouting48V,//6 正在切断本体供电
    enCPSState_Blackout48V,//7 本体供电已切断
    enCPSState_Electrifying48V,//8 正在准备给本体供电
    enCPSState_SafeGuardErrorHandling,//9.安全光幕ErrorHanding
    enCPSState_SafeGuardError,//10.安全光幕Error
    enCPSState_SafeGuardHandling,//11 安全光幕处理
    enCPSState_SafeGuard,//12 安全光幕
    //controller状态机
    enCPSState_ControllerDisconnecting,//13 正在反初始化控制器
    enCPSState_ControllerDisconnect,//14 控制器已处于未初始化状态
    enCPSState_ControllerConnecting,//15 正在初始化控制器
    enCPSState_ControllerVersionError,//16 控制器版本过低错误
    enCPSState_EtherCATError,//17 EtherCAT错误
    enCPSState_ControllerChecking,//18 控制器初始化后检查状态
    //enCPSState_EtherCATResetting,//18 复位EtherCAT错误
    //Robot状态机
    enCPSState_Reseting,//19 复位机器人
    enCPSState_RobotOutofSafeSpace,//20 机器人超出安全空间
    enCPSState_RobotCollisionStop,//21 机器人安全碰撞停车
    enCPSState_Error,//22 机器人错误
    enCPSState_Enabling,//23 机器人使能中
    enCPSState_Disable,//24 机器人去使能
    enCPSState_Moving,//25 机器人运动中
    enCPSState_LongJogMoving,//26 机器人长点动运动中
    enCPSState_RobotStopping,//27 机器人停止运动中
    enCPSState_Disabling,//28 机器人去使能中
    enCPSState_RobotOpeningFreeDriver,//29 机器人正在开启零力示教
    enCPSState_RobotClosingFreeDriver,//30 机器人正在关闭零力示教
    enCPSState_FreeDriver,//31 机器人处于零力示教
    enCPSState_RobotHolding,//32 机器人暂停
    enCPSState_StandBy,//33 机器人就绪
    //script状态机
    enCPSState_ScriptRunning,//34 脚本运行中
    enCPSState_ScriptHoldHandling,//35 脚本暂停处理中
    enCPSState_ScriptHolding,//36 脚本暂停
    enCPSState_ScriptStopping,//37 脚本停止中mNAME_
    enCPSState_ScriptStopped,//38 脚本已停止
    //LLApp
    enCPSState_LLAppDisconnected,//39 LLApp部件断开
    enCPSState_LLAppError,//40 LLApp部件错误
    //负载辨识
    enCPSState_RobotLoadIdentify, //41 负载辨识
    //brake
    enCPSState_Braking,  //42 开关抱闸中
	enCPSState_TemperatureTooLow, //43 温度过低
    //FT
    enCPSState_FTOpeningFreeDriver,//44 机器人正在开启力控零力示教
    enCPSState_FTClosingFreeDriver,//45 机器人正在关闭力控零力示教
    enCPSState_FTFreeDriver,//46 机器人处于力控零力示教
    enCPSState_RobotOSSMoving,//47 机器人处于超出安全空间运动状态
    enCPSState_RobotHoldHandling,//48 机器人暂停处理中
    enCPSState_RobotContinueHandling,//49 机器人继续处理中
    enCPSState_ScriptContinueHandling,//50 机器人脚本继续处理中
    //实时脚本状态机（追加在现有状态之后，避免改变已有状态编号）
    enCPSState_Count
    
};

namespace NS_LLCPS_STATE_NAME
{
    constexpr const char* NAME_NOT_INITIALIZED = "NotInitialized";
    constexpr const char* NAME_INITIALIZING = "Initializing";
    constexpr const char* NAME_ELECTRICAL_CONTROLBOX_DISCONNECTED = "ElectriccalControlBoxDisconnected";
    constexpr const char* NAME_CONNECTING_TO_ELECTRICAL_CONTROLBOX = "ConnectingToElectricalControlBox";
    constexpr const char* NAME_EMERGENCY_STOP_HANDLING = "EmergencyStopHandling";
    constexpr const char* NAME_EMERGENCY_STOPPED = "EmergencyStopped";
    constexpr const char* NAME_ROBOT_BLACKING_OUT = "RobotBlackingOut";
    constexpr const char* NAME_ROBOT_BLACK_OUT = "RobotBlackout";
    constexpr const char* NAME_ROBOT_ELECTRIFYING = "RobotElectrifying";
    constexpr const char* NAME_SAFE_GUARD_SIGNAL_ERROR_HANDLING = "SafeguardSignalErrorHandling";
    constexpr const char* NAME_SAFE_GUARD_SIGNAL_ERROR = "SafeguardSignalError";
    constexpr const char* NAME_SAFE_GUARD_HANDLING = "SafeguardHandling";
    constexpr const char* NAME_SAFE_GUARD = "SafeGuard";
    constexpr const char* NAME_DISCONNECTING_CONTROLLER = "DisconnectingController";
    constexpr const char* NAME_CONTROLLER_DISCONNECTED = "ControllerDisconnected";
    constexpr const char* NAME_CONNECTING_TO_CONTROLLER = "ConnectingToController";
    constexpr const char* NAME_CONTROLLER_VERSION_ERROR = "ControllerVersionError";
    constexpr const char* NAME_ETHERCAT_ERROR = "EtherCATError";
    constexpr const char* NAME_CONTROLLER_CHECKING = "ControllerChecking";
    constexpr const char* NAME_ROBOT_RESETTING = "RobotResetting";
    constexpr const char* NAME_ROBOT_OUT_OF_SAFETY_SPACE = "RobotOutofSafetySpace";
    constexpr const char* NAME_ROBOT_COLLISION_STOP = "RobotCollisionStop";
    constexpr const char* NAME_ROBOT_ERROR = "RobotError";
    constexpr const char* NAME_ROBOT_ENABLING = "RobotEnabling";
    constexpr const char* NAME_ROBOT_DISABLE = "RobotDisable";
    constexpr const char* NAME_ROBOT_IN_MOVING = "RobotInMoving";
    constexpr const char* NAME_ROBOT_IN_LONG_JOG_MOVING = "RobotInLongJogMoving";
    constexpr const char* NAME_ROBOT_STOPPING = "RobotStopping";
    constexpr const char* NAME_ROBOT_DISABLING = "RobotDisabling";
    constexpr const char* NAME_ROBOT_STARTING_FREE_DRIVER = "RobotStartingFreeDriver";
    constexpr const char* NAME_ROBOT_CLOSING_FREE_DRIVER = "RobotClosingFreeDriver";
    constexpr const char* NAME_ROBOT_IN_FREE_DRIVER = "RobotInFreeDriver";
    constexpr const char* NAME_ROBOT_PAUSED = "RobotPaused";
    constexpr const char* NAME_ROBOT_STAND_BY = "RobotStandBy";
    constexpr const char* NAME_PROGRAM_RUNNING = "ProgramRunning";
    constexpr const char* NAME_PROGRAM_HOLD_HANDLING = "ProgramHoldHandling";
    constexpr const char* NAME_PROGRAM_HOLDING = "ProgramHolding";
    constexpr const char* NAME_PROGRAM_STOPPING = "ProgramStopping";
    constexpr const char* NAME_PROGRAM_STOPPED = "ProgramStopped";
    constexpr const char* NAME_LLAPP_DISCONNECTED = "LLAppDisconnected";
    constexpr const char* NAME_LLAPP_ERROR = "LLAppError";
    constexpr const char* NAME_ROBOT_LOAD_IDENTIFYING = "RobotLoadIdentifying";
    constexpr const char* NAME_ROBOT_BRAKING = "RobotBraking";
    constexpr const char* NAME_ROBOT_IN_LOW_TEMPERATURE = "RobotInLowTemperature";
    constexpr const char* NAME_ROBOT_STARTING_FT_FREE_DRIVER = "RobotStartingFTFreeDriver";
    constexpr const char* NAME_ROBOT_CLOSING_FT_FREE_DRIVER = "RobotClosingFTFreeDriver";
    constexpr const char* NAME_ROBOT_IN_FT_FREE_DRIVER = "RobotInFTFreeDriver";
    constexpr const char* NAME_ROBOT_OUT_OF_SAFETY_SPACE_MOVING = "RobotOutofSafetySpaceMoving";
    constexpr const char* NAME_ROBOT_HOLD_HANDLING = "RobotHoldHandling";
    constexpr const char* NAME_ROBOT_CONTINUE_HANDLING = "RobotContinueHandling";
    constexpr const char* NAME_PROGRAM_CONTINUE_HANDLING = "ScriptContinueHandling";
}

enum EN_CodesysStatus
{
    enCDSState_UnInitialized=0,         //0.未初始化状态
    enCDSState_PreInitializing,         //1.初始化中状态
    enCDSState_EmergencyStop,           //2.急停状态
    enCDSState_ControllerNotConnect,    //3.控制器未连接
    enCDSState_PreControllerConnecting, //4.正在连接控制器状态
    enCDSState_WatchDog,                //5.看门狗生效状态
    enCDSState_StandBy,                 //6.准备就绪状态
    enCDSState_Moving,                  //7.运动中状态
    enCDSState_ErrorStopping,           //8.错误停止处理中状态
    enCDSState_Error,                   //9.错误状态
    enCDSState_PreResettingError,       //10.错误恢复中
	enCDSState_HandleEmergencyStop,     //11.急停处理
	enCDSState_Count,
};



enum LLPC_EN_StateMachine
{
    enState_ControllerNotConnect=0,         //0.控制器未连接
    enState_PreControllerConnecting,         //1.控制器连接中
    enState_UnInitialized,            //2.未初始化
    enState_PreInitializing,           //3.初始化中
    enState_WatchDog,    //4.看门狗
	enState_AxisPartStandby,//5. 控制器使能状态
	enState_AxisMoving,  //6.轴运动状态
	enState_AxisLinkageMoving,  //7.轴联动运动中
	enState_EmergencyStop,//8.急停状态
	enState_Error//9.错误状态
};


#endif
