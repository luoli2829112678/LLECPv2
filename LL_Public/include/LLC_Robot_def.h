#ifndef _LLC_Robot_def_H
#define _LLC_Robot_def_H

#include <string.h>
#include <vector>
#include <string>
#include <map>
#include"LLSMC/SMbasic/SMbasic_stdafx.h"
using namespace std; 


// 版本号定义
#define Version 80008

// 滤波数据长度
#define Max_FilterData_Count 2
// 波形最大通道数
#define Max_ScopeChannel_Count 36
// 波形最大缓存数
#define Max_ScopeDataBuf_Count 100


// 系统控制板eepid
#define Slave_eep_id_SCB 0x50460200
// E系列从站eepid
#define Slave_eep_id_E 0x50440200
// S系列从站eepid
#define Slave_eep_id_S 0x50450200
// S系列电子皮肤从站eepid
#define Slave_eep_id_S_ESkin 0x50450201
// 总线末端从站eepid
#define Slave_eep_id_EcatIO 0x10003201
// Epro总线末端从站eepid
#define Slave_eep_id_EproIO 0x00000401
// 扩展轴从站eepid(SV630)	786706
#define Slave_eep_id_SV630N 0x000C0112
// 扩展轴从站eepid(NETX)	27
#define Slave_eep_id_NETX 0x0000001B
//人形机械轴老化
#define Slave_Robot_H 0x50500300
//高创Servotronix_CDHD2
#define Slave_eep_id_Servotronix_CDHD2 0x2EC

// 默认固件版本号
#define MCS_Default_SlaveVersion 20250225


// 输出侧减速比
#define OutPutRatio 1

// 微秒转换为毫秒
constexpr double US_TO_MS = 1000.0;
// 毫秒转换为微秒
constexpr double MS_TO_US = 1000.0;
// 毫秒转化为秒
constexpr double MS_TO_SECOND = 1000.0;
// 秒转化为毫秒
constexpr double SECOND_TO_MS = 1000.0;
// 米转化为毫米，高精度型
constexpr double MM_TO_M = 1000.0;
// 米转换为毫米，保留三个小数点型
constexpr int MM_TO_M_INT = 1000;
// 精度误差
constexpr double EPSILON = 0.001;
// 13849 判断魔法数字
constexpr int Is13849 = 100000;
// 判断 E 系列或 S 系列取模
constexpr int IsEOrSForMod = 10;
// ServoEsJ 伺服周期常量
constexpr int servoEsJServoTime = 8;
// 电流转换系数阈值
constexpr double ThresholdOfMortorCurrentScales = 0.1;
// 靠近安全平面
constexpr int SafeAreaTrendency = -1;

#pragma pack(push,1)
class LLCTimer 
{
public:
    bool bInit_IN;
    long nInit_PT = 0;  // 以微秒为单位
    struct timespec nInit_ET = {0, 0};

	bool bLast_CLK = false;
public:
	// 定时器
    bool Ton(bool IN, int PT, int& ET) 
    {
        if (!IN)
        {
            ET = 0;
            bInit_IN = false;
            nInit_PT = 0;
            nInit_ET = {0, 0};
            return false;
        }
        // 检测上升沿
        if (IN && !bInit_IN)
        {
            nInit_PT = PT;
            clock_gettime(CLOCK_MONOTONIC, &nInit_ET);
        }
        if (IN)
        {
            bInit_IN = IN;
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            // 计算经过的时间（微秒）
            long elapsed = (now.tv_sec - nInit_ET.tv_sec) * 1000000 + 
                           (now.tv_nsec - nInit_ET.tv_nsec) / 1000;
            ET = elapsed;
            return ET > nInit_PT;
        }
        return false;
    }

	bool Ton(bool IN, int PT) 
    {
		int ET = 0;
        if (!IN)
        {
            bInit_IN = false;
            nInit_PT = 0;
            nInit_ET = {0, 0};
            return false;
        }
        // 检测上升沿
        if (IN && !bInit_IN)
        {
            nInit_PT = PT;
            clock_gettime(CLOCK_MONOTONIC, &nInit_ET);
        }
        if (IN)
        {
            bInit_IN = IN;
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            // 计算经过的时间（微秒）
            long elapsed = (now.tv_sec - nInit_ET.tv_sec) * 1000000 + 
                           (now.tv_nsec - nInit_ET.tv_nsec) / 1000;
            ET = elapsed;
            return ET > nInit_PT;
        }
        return false;
    }
	// 上升沿检测
	bool R_TRIG(bool CLK)
	{
		if (CLK && !bLast_CLK)
		{
			bLast_CLK = CLK;
			return true;
		}
		else
		{
			bLast_CLK = CLK;
			return false;
		}
	}
	// 下降沿检测
    bool F_TRIG(bool CLK)
    {
        if (!CLK && bLast_CLK) // 从 true 变为 false
        {
            bLast_CLK = CLK;
            return true;
        }
        else
        {
            bLast_CLK = CLK;
            return false;
        }
    }
};
struct ST_HomeParam
{
	int nMethod;
	int nLimitVel;
	int nHomingVel;
	int nHomingAcc;
	int nOffset;
};



enum EN_AxisControlFSM
{
	// 未接收到指令
	enAxisControlFSM_Null = 0,
	// 执行操作
	enAxisControlFSM_Execute = 1,
	// //完成操作
	// enAxisControlFSM_Complete = 2,
	//执行错误
	enAxisControlFSM_Error = 2,
};

typedef struct _ST_ST_MCFunState
{
	bool Execute;
	bool Busy;
	bool Done;
	bool Error;
	int ErrorID;
}ST_MCFunState;



// 模拟机器人定义
enum enSimulationType
{
	// 真实机器人
	en_RealRobot = 0,
	// 模拟机器人
	en_SimRobot = 1,
};
// 关节状态
typedef struct _ST_PoseBool
{
	bool X;
	bool Y;
	bool Z;
	bool Rx;
	bool Ry;
	bool Rz;
    _ST_PoseBool() : X(false), Y(false), Z(false), Rx(false), Ry(false), Rz(false)
    {
    }
}ST_PoseBool;

// XYZ定义
typedef struct _ST_PosXYZ
{
	double X;
	double Y;
	double Z;
    _ST_PosXYZ() : X(0), Y(0), Z(0)
    {
    }
}ST_PosXYZ;
// 姿态定义
typedef struct _ST_PosABC
{
	double Rx;
	double Ry;
	double Rz;
    _ST_PosABC() : Rx(0), Ry(0), Rz(0)
    {
    }
}ST_PosABC;

// 连杆参数
typedef struct _ST_KinParams
{
	double d1;
	double d2;
	double d3;
	double d4;
	double d5;
	double d6;
	double a2;
	double a3;
	double a4;
	double a5;
    _ST_KinParams() : d1(0), d2(0), d3(0), d4(0), d5(0), d6(0), a2(0), a3(0), a4(0), a5(0)
    {
    }
}ST_KinParams;




// 机器人状态
typedef struct _ST_RbtRobotStatus
{
    bool bStandBy;
    bool bMoving;
    bool bStopping;
    bool bErrorStop;
    bool bError;
}ST_RbtRobotStatus;
// 控制器总线状态
typedef struct _ST_RbtECATStatus
{
    bool bECATBUSRunning;
    int nECATStatusMachine;
    bool bECATSimulationMode;
}ST_RbtECATStatus;

// 机器人轴组实时数据状态
typedef struct _ST_ShmRobotRTStatus
{
	// 控制器版本号
	int nVersion;
	// 弃用
	int readCNT;
	// 机器人状态
	ST_RbtRobotStatus stRobotStatus;;
	// 总线状态
	ST_RbtECATStatus stECATStatus;
	// 发送帧频率
	int nECAT_SendFrameCNT;
	// 每秒发送帧数量
	int nECAT_SendFramesPerSecond;
	// 备用
	int nECAT_Reserve;
	// 丢帧数量
	int nECAT_TotalLostFrames;
	// Tx错误数量帧
	int nECAT_TxErrorCNT;
	// Rx错误数量帧
	int nECAT_RxErrorCNT;
	int nErrorID;
	int nErrorAxisID;
}ST_ShmRobotRTStatus;


//运动模式
enum EN_LLPCAxisMoveType
{
    enLLPCMoveTypeNull,
    enLLPCAbsoluteMotion,
    enLLPCRelativMotion,
	enLLPCHoming,
	enLLPCBackAndForthMove,
	enLLPCContinuousStepMove,
	enLLPCVelocityMove,
};

struct AxisMtionParam
{
	EN_LLPCAxisMoveType enMoveType;
	int nDir;
	double dPos;
	double dVel;
	double dAcc;
	double dDec;
	double dJerk;
	double dSnap;
};




struct ST_LLPCAxisHardParam
{
	int32_t nAxisID;                 // 轴ID
    int32_t nAxisType;                 // 轴类型
    int32_t nHomePos;                  // 零点位置（编码器单位）
    int32_t nEncodeRatio;              // 编码器分辨率
    int32_t nEncodeDirection;          // 编码器方向（+1 / -1）

    double  dGearRatio;                // 减速比
	double dVelocityScale;				//速度系数
	double dTorqueScale;				//电流系数

    double  dSafeSpaceLimitUpper;      // 安全空间上限
    double  dSafeSpaceLimitLower;      // 安全空间下限

    double  dMaxMotionVel;             // 最大速度
    double  dMaxMotionAcc;             // 最大加速度
    double  dMaxMotionJerk;            // 最大加加速度（Jerk）

    double  dTFFScale;   // 力矩前馈系数
    double  dVFFScale; // 速度前馈系数
    double  dJFFScale;     // Jerk 前馈系数
    double  dStopAcc;					//停止加速度
    double  dStopJerk;					//停止加加速度

	//注册PDO
    int32_t nTxPDO;               
    int32_t nRxPDO;                  
};

struct ST_AnalogFocusingParam
{
    uint16_t TargetAnalog;             // STRING_TO_UINT(scmd[2])
    float    AnalogToPositionGain;     // STRING_TO_REAL(scmd[3])
    float    FocusErrorTh;             // STRING_TO_REAL(scmd[4])
    float    LimitPositive;            // STRING_TO_REAL(scmd[5])
    float    LimitNegtive;             // STRING_TO_REAL(scmd[6])  // 注意拼写 Negtive 保持一致
    float    FocusErrorTh1;            // STRING_TO_REAL(scmd[7])
    float    FocusErrorTh2;            // STRING_TO_REAL(scmd[8])
    float    FocusErrorDeadZone;       // STRING_TO_REAL(scmd[9])
    float    AnalogKP1;                // STRING_TO_REAL(scmd[10])
    float    AnalogKI1;                // STRING_TO_REAL(scmd[11])
    float    AnalogKP2;                // STRING_TO_REAL(scmd[12])
    float    AnalogKI2;                // STRING_TO_REAL(scmd[13])
    float    MaxIntegral;              // STRING_TO_REAL(scmd[14])

    float    ADCToVoltageFactor;       // STRING_TO_REAL(scmd[15])
    float    VoltageToPositionFactor;  // STRING_TO_REAL(scmd[16])
    uint16_t EncodeRatio;              // STRING_TO_UINT(scmd[17])
    uint16_t VoltageErrorLimit;        // STRING_TO_UINT(scmd[18])

};
struct ST_PEGParam
{
    int32_t PEG_StartingPosition;        // STRING_TO_UINT(scmd[2])
    int32_t  PEG_EndingPosition;           // STRING_TO_DINT(scmd[3])
    uint16_t PulseHighLevelLength;          // STRING_TO_UINT(scmd[4])
    uint16_t PulseLowLevelLength;           // STRING_TO_UINT(scmd[5])
    uint8_t  WaveformGenerationCtrlSignal; // UINT_TO_BYTE(STRING_TO_UINT(scmd[6]))
    int32_t  PEG_MinPositionLimit;          // STRING_TO_DINT(scmd[7])
    int32_t  PEG_MaxPositionLimit;          // STRING_TO_DINT(scmd[8])

};



// 硬件配置参数
typedef struct _ST_RbtEtherCATParams
{
	// 总线循环周期
	double dEtherCatTimeCycle = 0.001;
	// 模拟机器人状态，false：真机 true：模拟       
	bool bSimulationType;
	//网口
	string sEthernetPort;

}ST_RbtEtherCATParams;


typedef struct _ST_RobotControlStatus
{
	// 控制器错误状态-Cps
	bool bStatus_ControllerError;
	// LL算法错误状态
	bool bStatus_SoftError;
	// 轴错误
	bool bStatus_AxisError;
	bool bStatus_SlaveDropError;
	// 总线运行周期
	double dEtherCatTimeCycle;
	bool bStartMaster = false;
	int nMasterState = 0;
}ST_RobotControlStatus;

typedef struct _ST_RbtAppParams
{
	// 任务开始时间
	int64_t nSystemTime_TaskStart;
	// 上一次任务开始时间
	int64_t nSystemTime_LastTaskStart;
	// 任务执行间隔时间
	int64_t nSystemTime_TaskCycleTime;
	// 最大任务执行间隔时间
	int64_t nSystemTime_MaxTaskCycleTime;
	// 最小任务执行间隔时间
	int64_t nSystemTime_MinTaskCycleTime;
	// 任务结束间时
	int64_t nSystemTime_TaskEnd;
	// 任务执行时间
	int64_t nSystemTime_TaskExecTime;
	// 最大任务执行时间
	int64_t nSystemTime_MaxTaskExecTime;
	// 最小任务执行时间
	int64_t nSystemTime_MinTaskExecTime;
}ST_RbtAppParams;

//8893实时数据上报消息头
typedef struct _RobotRTDataHead
{
	//数据头,魔术数据,一直保持为0x5242544C
	uint32_t nMagicNumber = 0x5242544C;
	//消息长度,整个数据包的字节数
	uint32_t nMsgSize;
	//数据长度,消息数据的字节数
	uint32_t nDataSize;
	//发送周期, 默认为1ms，单位：ms
	float fCycleTime = 1.0;
}RobotRTDataHead;







typedef struct _ST_SDOWorkState
{
	bool bExecute;
	bool bBusy;
	bool bDone;
	bool bError;
}ST_SDOWorkState;



// SDO读写数据长度
enum EN_SDOType
{
	enum_SDOType_STRING = 0,
	//8bit
	enum_SDOType_BYTE = 1,
	//16bit
	enum_SDOType_WORD = 2,
	//32bit
	enum_SDOType_DWORD = 3,
	//64
	enum_SDOType_LWORD = 4,
	//32
	enum_SDOType_REAL = 5,
	//
	enum_SDOType_LREAL = 6,
};


// 状态类型
enum _ActStatusValue
{
	// 0状态
	enum_StatusLow = 0,
	// 1状态
	enum_StatusHigh = 1,
};

// 读写共享内存
enum _LLCWriteScope
{
	enum_LLCWriteVar = 0,
	enum_LLCWriteVarLock = 1,
};

// 摆焊参数
typedef struct _ST_WeaveParams
{
	int nWeaveType;
	double dFrequency;
	double dAmplitude;
	int nWeaveFrameType;
	double dRightDwell;
	double dLeftDwell;
	double dCentreDwell;
	double dLPatternAngle;
	double dElevation;
	double dAzimuth;
	double dCentreRise;
	double dRadius;
    _ST_WeaveParams()
    {
    }
}ST_WeaveParams;

enum EN_WayPointRunState
{
    enum_WayPointUninitialized 	= 0,  // 未初始化
	enum_WayPointInitialized 	= 1,  // 未初始化
    enum_WayPointRunning       	= 2,  // 运行中
    enum_WayPointFinished      	= 3,   // 运行完成
	enum_WayPointError      	= 4   // 运行完成
};

enum EN_MoveBaseType
{
	enum_MoveBaseTypeNULL		= 0,
	enum_MoveBaseTypeAxis		= 1,
	enum_MoveBaseTypeAxisGroup	= 1,
};

struct ST_AxisGroupWayPointUnit
{
	int nAxisGroupID;
};

//wayPoint 1个点位中轴的运动参数
struct ST_AxisWayPointUnit
{
	int nAxisID;
    AxisMtionParam stAxisMtionParam;
    int AxisMtionStauts;
	SMbasic* pFunBasic;
	ST_AxisWayPointUnit():nAxisID(-1),AxisMtionStauts(-1),pFunBasic(nullptr){}
};

//wayPoint 1个点位
struct ST_AxisWayPoint_Point
{
	EN_MoveBaseType enMoveBaseType;//轴运动指令/轴组运动指令
    std::vector<ST_AxisWayPointUnit> AxisPoints;
	std::vector<ST_AxisGroupWayPointUnit> AxisGroupPoints;
    int WayPointID;
	EN_WayPointRunState PointStauts;
	ST_AxisWayPoint_Point() : enMoveBaseType(enum_MoveBaseTypeAxis),PointStauts(enum_WayPointUninitialized) ,WayPointID(-1){}
};



#pragma pack(pop)

#endif //_LLC_Slave_def_H