#ifndef _LL_RTINFO_STRUCT_H
#define _LL_RTINFO_STRUCT_H

#include "LL_SHMDef.h"
#include <cstdint>
#include <string>
#include <vector>

typedef struct _ST_AxisIO
{
    int nCI[ElectricIO];
    int nCO[ElectricIO];
} ST_AxisIO;

typedef struct _ST_RTInfo_AxisIO
{
    ST_AxisIO stAxisIOs[LLPC_MaxAxis];
} ST_RTInfo_AxisIO;

typedef struct _ST_RTInfo_AxisPos
{
    int cmdAxisPosPDO[LLPC_MaxAxis];
    int actAxisPosPDO[LLPC_MaxAxis];
    int cmdAxisVelPDO[LLPC_MaxAxis];
    int actAxisVelPDO[LLPC_MaxAxis];
    int cmdAxisTorPDO[LLPC_MaxAxis];
    int actAxisTorPDO[LLPC_MaxAxis];

    double dCmdPos[LLPC_MaxAxis];
    double dCmdVel[LLPC_MaxAxis];
    double dCmdAcc[LLPC_MaxAxis];
    double dCmdJerk[LLPC_MaxAxis];
    double dCmdTor[LLPC_MaxAxis];

    double dActPos[LLPC_MaxAxis];
    double dActVel[LLPC_MaxAxis];
    double dActAcc[LLPC_MaxAxis];
    double dActJerk[LLPC_MaxAxis];
    double dActTor[LLPC_MaxAxis];
} ST_RTInfo_AxisPos;

typedef struct _ST_RTInfo_EthercatSlaveState{
    int nSlaveId;
    int nCurrentState;
    uint16_t nExpectedState;
} ST_RTInfo_EcatSlaveState;

typedef struct _ST_RTInfo_Ethercat{
    int llcState;
    int etherCatState;
    int nSlaveCount;
    ST_RTInfo_EcatSlaveState slaveState[MAX_SLAVE_NUM];
    ST_PDO_Unit pdoData[MAX_PDO_NUM];
} ST_RTInfo_Ethercat;

typedef struct _ST_RTInfo_HardLoad
{
    int nECAT_SendFrameCNT;
    int nECAT_SendFramesPerSecond;
    int nECAT_TotalLostFrames;
    int nECAT_TxErrorCNT;
    int nECAT_RxErrorCNT;
} ST_RTInfo_HardLoad;

// MQTT PDO静态布局，仅用于组织推送字段，不属于ST_RTInfo实时数据。
struct ST_MqttPdoEntry
{
    int shmSlot = 0;
    std::uint32_t index = 0;
    std::uint32_t subIndex = 0;
    int bitLen = 0;
    std::string name;
    std::string dataType;
};

struct ST_MqttPdoGroup
{
    std::uint32_t groupIndex = 0;
    bool isUsedByAxis = false;
    std::vector<ST_MqttPdoEntry> entries;
};

struct ST_MqttSlavePdo
{
    std::uint8_t slaveId = 0;
    std::vector<ST_MqttPdoGroup> rxPdos;
    std::vector<ST_MqttPdoGroup> txPdos;
};

struct ST_MqttPdoPublish
{
    std::vector<ST_MqttSlavePdo> slaves;
    int actualPdoNum = 0;
};

typedef struct _ST_LLPC_Pid
{
    int pp;
    int pi;
    int vp;
    int vi;
    int cp;
    int ci;

    _ST_LLPC_Pid() : pp(0), pi(0), vp(0), vi(0), cp(0), ci(0) {}
} ST_LLPC_Pid;

typedef struct _ST_RTInfo_LLPCStateAndError
{
    int axisISMovingState[LLPC_MaxAxis];
    int axisState[LLPC_MaxAxis];
    int axisErrorCode[LLPC_MaxAxis];
    ST_LLPC_Pid axisPid[LLPC_MaxAxis];
    int cmdAxisModesOfOperation[LLPC_MaxAxis];
    int actAxisModesOfOperation[LLPC_MaxAxis];
    int nCCStatus;
    int nRbtBlendingDone;
    int errorCode;
} ST_RTInfo_LLPCStateAndError;

typedef struct _ST_RTInfo
{
    ST_RTInfo_AxisPos m_RT_AxisPos;
    ST_RTInfo_LLPCStateAndError m_RT_LLPCStateAndError;
    ST_RTInfo_HardLoad m_RT_HardLoad;
    ST_RTInfo_AxisIO m_RT_AxisIO;
    ST_RTInfo_Ethercat m_RT_Ethercat;
} ST_RTInfo;

#endif
