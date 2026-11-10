
#ifndef _LLPC_Slave_def_H
#define _LLPC_Slave_def_H
#include<vector>
#include<string>








enum EN_COE_PDOType
{
    enCOE_PDOType_NULL = 0,

    // RxPDO（主站 -> 从站）
    enCOE_PDOType_Controlword = 1,              // uint16_t
    enCOE_PDOType_TargetPosition,               // int32_t
    enCOE_PDOType_TargetVelocity,               // int32_t
    enCOE_PDOType_TargetTorque,                 // int16_t
    enCOE_PDOType_TargetModesOfOperation,       // uint8_t
    enCOE_PDOType_DigitalOutputs,               // uint32_t
    enCOE_PDOType_TouchProbeFunction,           // uint32_t
    enCOE_PDOType_AddVelocityValue,             // int32_t
    enCOE_PDOType_AddTorqueValue,               // int16_t

    // TxPDO（从站 -> 主站）
    enCOE_PDOType_StatusWord,                   // uint16_t
    enCOE_PDOType_ActualPosition,               // int32_t
    enCOE_PDOType_ActualVelocity,               // int32_t
    enCOE_PDOType_ActualTorque,                 // int16_t
    enCOE_PDOType_ActualModesOfOperation,       // uint8_t
    enCOE_PDOType_TouchProbeStatus,             // uint32_t
    enCOE_PDOType_DigitalInputs,                // uint32_t
    enCOE_PDOType_ErrorCode,                    // uint16_t

    // 双编码器
    enCOE_PDOType_JointActPosition              // uint32_t
};

enum EN_PDO_Dir
{
    enPDO_Dir_NULL,
    enPDO_Dir_TXPDO,
    enPDO_Dir_RXPDO,
};

typedef struct _ST_AxisPDOMap
{
    EN_COE_PDOType enCOE_PDO_Type;
    EN_PDO_Dir enPDO_Dir;
    uint nOffset;
}ST_AxisPDOMap;








#endif