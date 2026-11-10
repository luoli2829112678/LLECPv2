#ifndef _LL_PublicConstant_H
#define _LL_PublicConstant_H

/*************************************************************************
**  枚举类型定义，宏定义，常量定义，都在这个头文件进行定义
*************************************************************************/

// 脚本编译兼容常量：当前脚本协议仍按 6 轴组织机器人和笛卡尔数据。
#define PCS_AxisNum 6
#define DOF_ROBOT 6
#define DOF_CO_CONTROL 6

// Realtime data keys
#define realtimeKey_MsgTitle "MsgTitle"
#define realtimeSubKeyUpdateTime "UpdateTime"
#define realtimeSubKeyStamp "Stamp"
#define realtimeKey_PosAndVel "PosAndVel"
#define realtimeKey_StateAndError "StateAndError"
#define realtimeKey_RobotAuthorization "RobotAuthorization"
#define realtimeKey_Script "Script"
#define realtimeKey_CPSTimeStamp "cpsTimestamp"
#define realtimeKey_curDateTime "curDateTime"
#define realtimeKey_HardLoad "HardLoad"

// MQTT realtime data keys
#define mqtt_RealtimeKey_PosAndVel "posAndVel"
#define mqtt_RealtimeKey_StateAndError "stateAndError"
#define mqtt_RealtimeKey_MsgTitle "msgTitle"
#define mqtt_RealtimeSubKeyStamp "stamp"
#define mqtt_RealtimeSubKeyUpdateTime "updateTime"
#define mqtt_RealtimeKey_RobotAuthorization "robotAuthorization"
#define mqtt_RealtimeKey_Script "script"
#define mqtt_RealtimeKey_HardLoad "hardLoad"

#define Macro_IsSlaveDropCode(x) ((x) == CEC_SlaveDropped) || \
                                 ((x) == CEC_DI_GENERAL_COMMUNICATION_ERROR)

#define ScriptCurVersion 5

// LLPC limits and paths
#define LLPC_MaxAxis 64
#define LLPC_AxisEnableCheckTime 500000
#define MaxBuffer 20
#define LLPC_MaxMotionGroup 4
#define ElectricIO 8





// 无效的轴数量
#define INVALID_AXIS_NUM    -1




#define KCMD_IntCnt 32
#define KCMD_DoubleCnt 32



constexpr const char* CmdEnd = ";";
constexpr const char* CmdSplit = ",";
constexpr const char* OPTOK = "OK";
constexpr const char* OPTFAIL = "Fail";

// LLC 浮点型数据精度
constexpr const int LLC_DECIMAL_PLACES = 0x06;



#define Min_ModbusAddr 0x0000
#define Max_ModbusAddr 0xFFFF

#define Min_ModbusCoil 0x00
#define Max_ModbusCoil 0xFF

#define Min_ModbusHoldingRegister 0x0000
#define Max_ModbusHoldingRegister 0xFFFF

#define MAX_MB_DATA_LENTH 100
#define MB_DATA_BASE_ADDR 900

#define RECOMMENDED_PROGRAM_SIZE        8388608         // 推荐的程序大小 8M
#define WARNING_PROGRAM_SIZE            14680064        // 警告的程序大小 14M
#define ALLOWABLE_PROGRAM_SIZE          20971520        // 最大允许的程序大小 20M   



#define CPS_SETVARINITVALUE_MODEL_NAME "SetVarInitValue"
#define CSP_SETVARINITVALUE_FILE_PATH "/usr/local/bin/Robot/LLCPS/script/SetVarInitValue.py"
#define LLC_FTP_ROBOTAPP_PATH "/home/ftp/RobotApp/"
#define LLC_NODE_DOWNLOAD_FILE_PATH                 "/var/www/html/llpc/downloadfile/"
#define LLC_NODE_DOWNLOAD_FILE_RELPATH              "./downloadfile/"
#define LLC_FTP_PYTASK_PATH "/home/ftp/py_task/"


#define PID_PARAMS_COUNT    0x06

#define CPU_MODEL_J1900 "J1900"
#define CORE_COUNT 4
#define LLC_MAX_CPU 90
#define LLC_START_SH_FILE_PATH "/usr/local/bin/Robot/srvSH/llc.sh"

#define RTCycle4 4
#define RTCycle1 1

#define MaxSendCount2LLC 1      //do not resend



// For network card name define
#define NCN_ENPLO   "lo"
#define NCN_ENP1S0  "enp1s0"
#define NCN_ENP2S0  "enp2s0"
#define NCN_WLP3S0  "wlp3s0"

#define FLOAT_RELATIVE_EPSILON  1.0E-5
#define FLOAT_ABSOLUTE_EPSILON  1.0E-12
#define SCB_13849_COMPARE_TLLESHOLDS FLOAT_RELATIVE_EPSILON


constexpr const char* LLC_PROCESS_NAME = "LLC";
constexpr const char* LLC_EXECUTE_PATH = "/usr/local/bin/Robot/LLCPS/LLC";

enum EN_IFConditionType
{
    enIFConditionType_Modbus=1,
    enIFConditionType_Expression=2
};

enum EN_LoopConditionType
{
    enLoopConditionType_TRUE=0,
    enLoopConditionType_CNT,
    enLoopConditionType_Expression
};

enum EN_WaitConditionType
{
    enWaitConditionType_NO=0,
    enWaitConditionType_MS = 1,
    enWaitConditionType_Expression = 4
};

enum EN_SetIOType
{
    enSetIOType_NO=0,
    enSetIOType_Expand=5,
    enSetIOType_Slave=6
};

enum EN_WayPointType
{
    enWayPointType_Fix=0,
    enWayPointType_Rel,
    enWayPointType_RelVar,
};

enum EN_PYTasks
{
    enPYTasks_ScriptSopped = 0,
};


enum EN_MBSlaveCmd
{
    enMBSlaveCmd_None=0,
    enMBSlaveCmd_StartMaster=1,
    enMBSlaveCmd_CloseMaster=2,
    enMBSlaveCmd_AxisEnable=3,
    enMBSlaveCmd_AxisDisable=4,
    enMBSlaveCmd_AxisStop=5,
    enMBSlaveCmd_Reset=6,
    enMBSlaveCmd_ScriptStart=7,
    enMBSlaveCmd_ScriptPause=8,
    enMBSlaveCmd_ScriptContinue=9,
    enMBSlaveCmd_WriteSDO=11,
    enMBSlaveCmd_WritePDO=12,
};


enum EN_SocketNameType
{
    enSocketNameType_Undefined = 0,
    enSocketNameType_Variable = 1,
    enSocketNameType_String,
};

enum EN_ReceiveAcceptType
{
    enReceiveAcceptType_Undefined = 0,
    enReceiveAcceptType_GlobalString,
    enReceiveAcceptType_SplitString,
};

enum EN_ModbusSignalType
{
    enModbusSignalType_Coil = 0,
    enModbusSignalType_InputCoil,
    enModbusSignalType_HoldingRegister,
    enModbusSignalType_InputRegister,
    enModbusSignalType_Count
};

enum EN_RelMoveType
{
    enRelMoveType_ABS = 0,
    enRelMoveType_Rel
};

#endif
