#include"CIA402Axis.h"


void CIA402Axis::PDOsynchronization()
{
    PDO_SetControlword(m_stMirrorPDO.Controlword);
    PDO_SetTargetPosition(m_stMirrorPDO.TargetPosition);
    PDO_SetTargetVelocity(m_stMirrorPDO.TargetVelocity);
    PDO_SetTargetTorque(m_stMirrorPDO.TargetTorque);
    PDO_SetModesOfOperation(m_stMirrorPDO.TargetModesOfOperation);
    PDO_SetVFF(m_stMirrorPDO.AddVelocityValue);
    PDO_SetTFF(m_stMirrorPDO.AddTorqueValue);
    PDO_SetDigitalOutputs(m_stMirrorPDO.DigitalOutputs);

    PDO_ReadStatusWord(m_stMirrorPDO.StatusWord);
    PDO_ReadErrorCode(m_stMirrorPDO.ErrorCode);
    PDO_ReadActualPosition(m_stMirrorPDO.ActualPosition);
    PDO_ReadActualJointPosition(m_stMirrorPDO.JointActPosition);
    PDO_ReadActualVelocity(m_stMirrorPDO.ActualVelocity);
    PDO_ReadActualTorque(m_stMirrorPDO.ActualTorque);
    PDO_ReadModesOfOperation(m_stMirrorPDO.ActualModesOfOperation);
    PDO_ReadDigitalInputs(m_stMirrorPDO.DigitalInputs);
}

void CIA402Axis::VirtualPDOsynchronization()
{
    if(!m_bVirtual)
    {
        return;
    }
    if(m_st_map.pControlword == &m_stPDO_Virtual.Controlword)
    {
        return;
    }
    //轴对象发生了拷贝，同步虚拟PDO
    m_st_map.pControlword             = &m_stPDO_Virtual.Controlword;
    m_st_map.pStatusWord              = &m_stPDO_Virtual.StatusWord;
    m_st_map.pErrorCode               = &m_stPDO_Virtual.ErrorCode;
    m_st_map.pTargetPosition          = &m_stPDO_Virtual.TargetPosition;
    m_st_map.pTargetVelocity          = &m_stPDO_Virtual.TargetVelocity;
    m_st_map.pTargetTorque            = &m_stPDO_Virtual.TargetTorque;
    m_st_map.pTargetModesOfOperation  = &m_stPDO_Virtual.TargetModesOfOperation;
    m_st_map.pActualPosition          = &m_stPDO_Virtual.ActualPosition;
    m_st_map.pJointActPosition        = &m_stPDO_Virtual.JointActPosition;
    m_st_map.pActualVelocity          = &m_stPDO_Virtual.ActualVelocity;
    m_st_map.pActualTorque            = &m_stPDO_Virtual.ActualTorque;
    m_st_map.pActualModesOfOperation  = &m_stPDO_Virtual.ActualModesOfOperation;
    m_st_map.pDigitalInputs           = &m_stPDO_Virtual.DigitalInputs;
    m_st_map.pDigitalOutputs          = &m_stPDO_Virtual.DigitalOutputs;
    return;
}

// ===== 写入（RxPDO） =====
int CIA402Axis::PDO_SetControlword(uint16_t Controlword)
{
    if(NULL == m_st_map.pControlword)
    {
        return -1;
    }
    PDO_CONTROLWORD = Controlword;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_SetTargetPosition(int32_t TargetPosition)
{
    if(NULL == m_st_map.pTargetPosition)
    {
        return -1;
    }
    PDO_TARGETPOSITION = TargetPosition;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_SetTargetVelocity(int32_t TargetVelocity)
{
    if(NULL == m_st_map.pTargetVelocity)
    {
        return -1;
    }
    PDO_TARGETVELOCITY = TargetVelocity;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_SetTargetTorque(int16_t TargetTorque)
{
    if(NULL == m_st_map.pTargetTorque)
    {
        return -1;
    }
    PDO_TARGETTORQUE = TargetTorque;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_SetModesOfOperation(uint8_t mode)
{
    if(NULL == m_st_map.pTargetModesOfOperation)
    {
        return -1;
    }
    PDO_TARGETMODESOFOPERATION = mode;
    return AEC_SUCCESSED;
}

int CIA402Axis::PDO_SetVFF(int32_t VFF)
{
    if(NULL == m_st_map.pAddVelocityValue)
    {
        return -1;
    }
    PDO_TARGET_VFF = VFF;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_SetTFF(int16_t TFF)
{
    if(NULL == m_st_map.pAddTorqueValue)
    {
        return -1;
    }
    PDO_TARGET_TFF = TFF;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_SetDigitalOutputs(uint32_t DigitalOutputs)
{
    if(NULL == m_st_map.pDigitalOutputs)
    {
        return -1;
    }
    PDO_DIGITALOUTPUTS = DigitalOutputs;
    return AEC_SUCCESSED;
}

int CIA402Axis::PDO_ReadControlword(uint16_t& Controlword)
{
    if(NULL == m_st_map.pControlword)
    {
        return -1;
    }
    Controlword = PDO_CONTROLWORD;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_ReadTargetPosition(int32_t& TargetPosition)
{
    if(NULL == m_st_map.pTargetPosition)
    {
        return -1;
    }
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_ReadTargetVelocity(int32_t& TargetVelocity)
{
    if(NULL == m_st_map.pTargetVelocity)
    {
        return -1;
    }
    TargetVelocity = PDO_TARGETVELOCITY;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_ReadTargetTorque(int16_t& TargetTorque)
{
    if(NULL == m_st_map.pTargetTorque)
    {
        return -1;
    }
    TargetTorque = PDO_TARGETTORQUE;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_ReadTargetModesOfOperation(uint8_t& mode)
{
    if(NULL == m_st_map.pTargetModesOfOperation)
    {
        return -1;
    }
    mode = PDO_TARGETMODESOFOPERATION;
    return AEC_SUCCESSED;
}

// ===== 读取（TxPDO） =====
int CIA402Axis::PDO_ReadStatusWord(uint16_t& StatusWord)
{
    if(NULL == m_st_map.pStatusWord)
    {
        return -1;
    }
    StatusWord = PDO_STATUSWORD;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_ReadErrorCode(uint16_t& ErrorCode)
{
    if(NULL == m_st_map.pErrorCode)
    {
        return -1;
    }
    ErrorCode = PDO_ERRORCODE;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_ReadActualPosition(int32_t& ActualPosition)
{
    if(NULL == m_st_map.pActualPosition)
    {
        return -1;
    }
    ActualPosition = PDO_ACTUALPOSITION;
    return AEC_SUCCESSED;
}

int CIA402Axis::PDO_ReadActualJointPosition(int32_t& ActualJointPosition)
{
    if(NULL == m_st_map.pJointActPosition)
    {
        return -1;
    }
    ActualJointPosition = PDO_JOINT_ACTUALPOSITION;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_ReadActualVelocity(int32_t& ActualVelocity)
{
    if(NULL == m_st_map.pActualVelocity)
    {
        return -1;
    }
    ActualVelocity = PDO_ACTVELOCITY;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_ReadActualTorque(int16_t& ActualTorque)
{
    if(NULL == m_st_map.pActualTorque)
    {
        return -1;
    }
    ActualTorque = PDO_ACTUALTORQUE;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_ReadModesOfOperation(uint8_t& mode)
{
    if(NULL == m_st_map.pActualModesOfOperation)
    {
        return -1;
    }
    mode = PDO_ACTUALMODESOFOPERATION;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_ReadDigitalInputs(uint32_t& inputs)
{
    if(NULL == m_st_map.pDigitalInputs)
    {
        return -1;
    }
    inputs = PDO_DIGITALINPUTS;
    return AEC_SUCCESSED;
}
int CIA402Axis::PDO_ReadDigitalOutputs(uint32_t& outputs)
{
    if(NULL == m_st_map.pDigitalOutputs)
    {
        return -1;
    }
    outputs = PDO_DIGITALOUTPUTS;
    return AEC_SUCCESSED;
}





int CIA402Axis::Axis_PDO_SetControlword(uint16_t Controlword)
{
    m_stMirrorPDO.Controlword  = Controlword;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_SetTargetPosition(int32_t TargetPosition)
{
    m_stMirrorPDO.TargetPosition  = TargetPosition;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_SetTargetVelocity(int32_t TargetVelocity)
{
    m_stMirrorPDO.TargetVelocity = TargetVelocity;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_SetTargetTorque(int16_t TargetTorque)
{
    m_stMirrorPDO.TargetTorque = TargetTorque;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_SetVFF(int32_t VFF)
{
    m_stMirrorPDO.AddVelocityValue = VFF;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_SetTFF(int16_t TFF)
{
    m_stMirrorPDO.AddTorqueValue = TFF;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_SetModesOfOperation(EN_ModesOfOperation mode)
{
    int32_t tar;
    int16_t  tarT;
    //位置同步
    switch (mode)
    {
    case EN_ModesOfOperation::enModeCyclicSyncPosition:
        Axis_PDO_ReadActualPosition(tar);
        Axis_PDO_SetTargetPosition(tar);
        break;
    case EN_ModesOfOperation::enModeCyclicSyncVelocity:
        //Axis_PDO_ReadActualVelocity(tar);
        Axis_PDO_SetTargetVelocity(0);
        break;
    case EN_ModesOfOperation::enModeCyclicSyncTorque:
        //Axis_PDO_ReadActualTorque(tarT);
        Axis_PDO_SetTargetTorque(0);
        break;
    case EN_ModesOfOperation::enModeCyclicSyncJointTorque:
        break;
    default:
		printf("Axis_PDO_SetModesOfOperation:%d\n",mode);
        break;
    }
    m_stMirrorPDO.TargetModesOfOperation = mode;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_SetDigitalOutputs(uint32_t DigitalOutputs)
{
    m_stMirrorPDO.DigitalOutputs = DigitalOutputs;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_ReadStatusWord(uint16_t& StatusWord)
{
    StatusWord = m_stMirrorPDO.StatusWord;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_ReadErrorCode(uint16_t& ErrorCode)
{
    ErrorCode = m_stMirrorPDO.ErrorCode;
    return AEC_SUCCESSED;
}
int CIA402Axis:: Axis_PDO_ReadActualPosition(int32_t& ActualPosition)
{
    ActualPosition = m_stMirrorPDO.ActualPosition;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_ReadActualVelocity(int32_t& ActualVelocity)
{
    ActualVelocity = m_stMirrorPDO.ActualVelocity;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_ReadActualTorque(int16_t& ActualTorque)
{
    ActualTorque = m_stMirrorPDO.ActualTorque;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_ReadModesOfOperation(EN_ModesOfOperation& mode)
{
    mode = static_cast<EN_ModesOfOperation>(m_stMirrorPDO.ActualModesOfOperation);
    return AEC_SUCCESSED;
}   
int CIA402Axis::Axis_PDO_ReadDigitalInputs(uint32_t& inputs)
{
    inputs = m_stMirrorPDO.DigitalInputs;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_ReadDigitalOutputs(uint32_t& outputs)
{
    outputs = m_stMirrorPDO.DigitalOutputs;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_ReadControlword(uint16_t& Controlword)
{
    Controlword = m_stMirrorPDO.Controlword;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_ReadTargetPosition(int32_t& TargetPosition)
{
    TargetPosition = m_stMirrorPDO.TargetPosition;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_ReadTargetVelocity(int32_t& TargetVelocity)
{
    TargetVelocity = m_stMirrorPDO.ActualVelocity;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_ReadTargetTorque(int16_t& TargetTorque)
{
    TargetTorque = m_stMirrorPDO.TargetTorque;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_PDO_ReadTargetModesOfOperation(uint8_t& mode)
{
    mode = m_stMirrorPDO.TargetModesOfOperation;
    return AEC_SUCCESSED;
}