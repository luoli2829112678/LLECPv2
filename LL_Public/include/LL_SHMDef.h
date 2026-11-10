#ifndef _LL_SHMDEF_H
#define _LL_SHMDEF_H

#include <stdint.h>
#include "LL_PublicConstant.h"
#include <string.h>
#include <string>
using namespace std;
#define MAX_AXIS_NUM 64
#define MAX_SLAVE_NUM 128
#define MAX_PDO_NUM 1000
#define MAX_IO_NUM 1000
#define MaxAxisNum 64

/***********************************************************
**点动方向
**enJogDirec_Negative=负方向
**enJogDirec_Positive=正方向
***********************************************************/
enum EN_JogDirection
{
	enJogDirec_Negative = 0,
	enJogDirec_Positive,
};

enum EN_CPS_InitStatus
{
	enCPSInit_WaitAutoStart=0,
	//enCPSInit_WaitStartMaster,
	enCPSInit_AutoStartFinished
};
//CPS状态机
enum EN_WayPointState
{
    enWPS_Moving = 0,
    enWPS_Done
};

enum EN_RobotStateIndex2
{
    enRS_Disable = 0,
    enRS_StandBy,
    enRS_Moving,
    enRS_Enabled,
    enRS_Stopping,
    enRS_ErrorStop,
    enRS_Error,
    enRS_WayPointID,
    enRS_WayPointStatus,
    enRS_ResetSafeSpace,
    enRS_CheckDIStopState,
    enRS_FTControlState,
    enRSRobotStatusCnt,
};


enum EN_ControlStateIndex
{
    // Keep the original shared-memory offsets for the LLPC states still in use.
    enCS_Breaking = 4,
    enCS_Holding = 5,
    enCS_ControlStatusCnt = 15
};

enum EN_ECATStateIndex
{
    enECAT_BUS_Running = 0,
    enECAT_LLC_Machine,
    enECAT_SimulationMode,
    enECAT_EtherCAT_Machine,
    enECAT_SlaveDrop,
    enECAT_EtherCatTimeCycle,
    enECAT_EherCATStatusCnt,
};




enum LLPC_EN_AxisStatusMachine
{
    enAxisState_disabled           = 0,  // Disabled
    enAxisState_errorstop           = 1,  // Error Stop
    enAxisState_stopping            = 2,  // Stopping
    enAxisState_StandBy             = 3,  // StandBy
    enAxisState_discrete_motion     = 4,  // 单步运动
    enAxisState_continuous_motion   = 5,  // 连续运动
    enAxisState_synchronized_motion = 6,  // 同步运动
    enAxisState_homing              = 7   // 回零
};




enum LLPC_ModesOfOperation
{
    enManufacturerSpecificMin = -128,// 厂商自定义模式起始值

    enNoModeAssigned = 0,            // 无模式变化/未指定模式
    enProfilePosition = 1,           // 位置模式
    enVelocity = 2,                  // 速度模式
    enProfileVelocity = 3,           // 简单速度模式
    enProfileTorque = 4,             // 扭矩模式
    enReserved5 = 5,                 // 保留
    enHoming = 6,                    // 回零模式
    enInterpolatedPosition = 7,      // 插值位置模式
    enCyclicSyncPosition = 8,        // 同步位置模式（周期）
    enCyclicSyncVelocity = 9,        // 同步速度模式（周期）
    enCyclicSyncTorque = 10,         // 同步扭矩模式（周期）

    enManufacturerSpecificMax = 127  // 厂商自定义最大值
    // 11 到 127 保留
};



//#pragma pack(push,1)
#pragma pack(push,8)

typedef struct _ST_AxisErrorInfo
{
	//轴组状态
	int nAxisGroup_State;				
	//轴组错误码
	int nAxisGroup_ErrorCode;
	//轴通讯状态
	//轴状态
	int nAxis_State[LLPC_MaxAxis];		
	//轴错误码
	int nAxis_ErrorCode[LLPC_MaxAxis];
    _ST_AxisErrorInfo() :nAxisGroup_State(0),nAxisGroup_ErrorCode(0)
    {
        memset(nAxis_State,0,sizeof(nAxis_State));
        memset(nAxis_ErrorCode,0,sizeof(nAxis_ErrorCode));
    }

}ST_AxisErrorInfo;



typedef struct _ST_BufferInfo
{
    uint16_t state;
    uint16_t errorCode;
    uint16_t lineID;
    uint16_t cmdID;

    _ST_BufferInfo() : state(0), errorCode(0), lineID(0), cmdID(0) {}

}ST_BufferInfo;


typedef struct _ST_MotionMonitor
{
    int motionStartTime;
    int motionIndex;
    int motionTime;
    double motionTarPos;

    _ST_MotionMonitor() : motionStartTime(0), motionIndex(0), motionTime(0), motionTarPos(0) {}

}ST_MotionMonitor;

typedef struct _ST_LLPC_Axis
{
    int nAxisID;
    int nAxisStatus;
    int nAxisMoveState;
    int nAxisErrorCode;
    int nCmdAxisModesOfOperation;
    int nActAxisModesOfOperation;
	//PDO
    int nCmdPos_PDO;
    int nActPos_PDO;
    int nCmdVel_PDO;
    int nActVel_PDO;
    int nCmdTor_PDO;
    int nActTor_PDO;

    double dCmdPos;
    double dCmdVel;
    double dCmdAcc;
    double dCmdJerk;
    double dCmdTor;

    double dActPos;
    double dActVel;
    double dActAcc;
    double dActJerk;
    double dActTor;
    //PID
    int nKpPos;
    int nKiPos;
    int nKpVel;
    int nKiVel;
    int nKpCur;
    int nKiCur;

    int nMotionMonitorWritingFlag;
    ST_MotionMonitor st_MotionMonitorData[LLPC_MaxMotionGroup];

        _ST_LLPC_Axis():nAxisID(0),nAxisStatus(0),nAxisErrorCode(0),nCmdAxisModesOfOperation(0),nActAxisModesOfOperation(0),
            nCmdPos_PDO(0),nActPos_PDO(0),nCmdVel_PDO(0),nActVel_PDO(0),nCmdTor_PDO(0),nActTor_PDO(0),
            dCmdPos(0),dCmdVel(0),dCmdAcc(0),
            dCmdJerk(0),dCmdTor(0),
            dActPos(0),dActVel(0),dActAcc(0),
            dActJerk(0),dActTor(0),
            nKpPos(0),nKiPos(0),nKpVel(0),
            nKiVel(0),nKpCur(0),nKiCur(0),nMotionMonitorWritingFlag(-1)
        {}


}ST_LLPC_Axis;


typedef struct _ST_LLPC_Slave
{
    int nSlaveState;
    uint16_t nExpectedState;                // 主站期望从站进入的状态
    int32_t nDCSyncDeviationNs;              // DC同步偏差，
    
}ST_LLPC_Slave;

// 单个PDO的数据区域固定为8字节
union alignas(8) UN_PDOValue
{
    uint8_t boolValue;      // BOOL使用uint8_t
    int8_t int8Value;       // INTEGER8
    uint8_t uint8Value;     // UNSIGNED8
    int16_t int16Value;     // INTEGER16
    uint16_t uint16Value;   // UNSIGNED16
    int32_t int32Value;     // INTEGER32
    uint32_t uint32Value;   // UNSIGNED32
    int64_t int64Value;     // INTEGER64
    uint64_t uint64Value;   // UNSIGNED64
    float float32Value;     // REAL32
    double float64Value;    // REAL64
    uint8_t rawBytes[8];    // 最大保存8字节原始数据
};

struct ST_PDO_Unit
{
    uint8_t nSlaveID;
    uint32_t nIndex;
    uint32_t nSubIndex;
    UN_PDOValue unPDOValue;
};



/*************************************************************************
**CRC
*************************************************************************/
typedef struct _ST_CRCHead
{
    int nCRCHead;
    int nReserved;

        _ST_CRCHead():nCRCHead(2018),nReserved(0)
    {}
}ST_CRCHead;

/*************************************************************************
**CRC
*************************************************************************/
typedef struct _ST_CRCTail
{
    int nCRCTail;
    int nReserved;

        _ST_CRCTail():nCRCTail(2020),nReserved(0)
    {}
}ST_CRCTail;



typedef struct _ST_BoxShareInfo
{
    int nIsElectrify;
    int nIsEStop;

    _ST_BoxShareInfo():nIsElectrify(0),nIsEStop(0)
    {
    }

}ST_BoxShareInfo;

typedef struct _ST_KernelCmdArea
{
    ST_BoxShareInfo st_BoxInfo;
    int writeCNT;
	int nReserved;
	_ST_KernelCmdArea():writeCNT(0),nReserved(0)
    {}
}ST_ShmCPS2LLCData;

/********************************************************************
 *All  StatusInfo
*********************************************************************/
typedef struct _ST_SHM_RobotRTStatus
{
	int nVersion;
	int readCNT;
	int nrRobot_Status[enRSRobotStatusCnt];
	int nrControl_Status[enCS_ControlStatusCnt];
	int nrECAT_Status[enECAT_EherCATStatusCnt];
	int nError_AxisID;
	int nError_Code;
	int hstrApplicationStatus;
	ST_AxisErrorInfo st_AxisErrorInfo;
	int nECAT_SendFrameCNT;
	int nECAT_SendFramesPerSecond;
	int nECAT_Reserve;
	int nECAT_TotalLostFrames;
	int nECAT_TxErrorCNT;
	int nECAT_RxErrorCNT;
    double dFOEworkProgress;//foeFOE操作进度百分比
    int nFoeOperationCompleted;//foeFOE操作状态，0未开始FOE操作，1FOE操作中，2FOE操作完成，-1FOE操作失败
	ST_BufferInfo st_bufferInfo[MaxBuffer];
		_ST_SHM_RobotRTStatus(): readCNT(0),nError_AxisID(0),nError_Code(0),
			nECAT_SendFrameCNT(0),nECAT_SendFramesPerSecond(0),nECAT_Reserve(0),
			nECAT_TotalLostFrames(0),nECAT_TxErrorCNT(0),nECAT_RxErrorCNT(0),hstrApplicationStatus(0),dFOEworkProgress(0.0),nFoeOperationCompleted(0)
		{
			memset(nrRobot_Status,0,sizeof(nrRobot_Status));
			memset(nrControl_Status,0,sizeof(nrControl_Status));
			memset(nrECAT_Status,0,sizeof(nrECAT_Status));
		}
		// _ST_SHM_RobotRTStatus& operator=(_ST_SHM_RobotRTStatus& value)
		// {
		//     memcpy(this,&value,sizeof(_ST_SHM_RobotRTStatus));
		//     return *this;
		// }
}ST_SHM_RobotRTStatus;






/**************************************************************************/
typedef struct _ST_ReadCmdArea
{
    ST_SHM_RobotRTStatus st_StatusArea;
	ST_LLPC_Axis stAxisData[MAX_AXIS_NUM];
	ST_LLPC_Slave stSlaveData[MAX_SLAVE_NUM];
    //PDO监视数据
    ST_PDO_Unit stPDOData[MAX_PDO_NUM];
    // LLC writes actual IO values: -1 unavailable, 0/1 valid; global registration ID indexes.
    int8_t ioValue[MAX_IO_NUM];
}ST_ShmLLC2CPSData;

typedef struct _ST_ShareMemory
{
    ST_CRCHead ShmCRCHead;
    ST_ShmCPS2LLCData   ShmCPS2LLC;
    ST_ShmLLC2CPSData  ShmLLC2CPS;
    ST_CRCTail ShmCRCTail;
}ST_ShareMemory;




typedef struct _ST_LLPC_AxisBaseRT
{
    int nAxisStatus;
    int nAxisMoveState;
    int nAxisErrorCode;
    int nCmdAxisModesOfOperation;
    int nActAxisModesOfOperation;

    double dCmdPos;
    double dCmdVel;
    double dCmdAcc;
    double dCmdJerk;
    double dCmdTor;

    double dActPos;
    double dActVel;
    double dActAcc;
    double dActJerk;
    double dActTor;

    _ST_LLPC_AxisBaseRT():nAxisStatus(0),nAxisErrorCode(0),nCmdAxisModesOfOperation(0),nActAxisModesOfOperation(0),
        dCmdPos(0),dCmdVel(0),dCmdAcc(0),
        dCmdJerk(0),dCmdTor(0),
        dActPos(0),dActVel(0),dActAcc(0),
        dActJerk(0),dActTor(0)
    {}


}ST_LLPC_AxisBaseRT;

//8893实时数据上报消息体
typedef struct _RobotRTData
{
	int nStatusMachine;
	int CNT;
	ST_LLPC_AxisBaseRT AxisData[MaxAxisNum];
}RobotRTData;



#pragma pack(pop)

#endif
