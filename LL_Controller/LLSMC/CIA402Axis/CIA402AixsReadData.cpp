#include "CIA402Axis.h"


// --- 控制字 / 状态字 ---
int CIA402Axis::Axis_GetControlWord()
{
    return static_cast<int>(m_stMirrorPDO.Controlword);
}

int CIA402Axis::Axis_GetStatusWord()
{
    return static_cast<int>(m_stMirrorPDO.StatusWord);
}


// --- 目标运动参数 ---
double CIA402Axis::Axis_GetSetPosition()
{
    return m_stMotorMotionData.stCommand.dPosition;
}

int CIA402Axis::Axis_GetTargetPositionPDO()
{
    return static_cast<int>(m_stMirrorPDO.TargetPosition);
}

double CIA402Axis::Axis_GetSetVelocity()
{
    return m_stMotorMotionData.stCommand.dVelocity;
}
double CIA402Axis::Axis_GetSetAcceleration()
{
    return m_stMotorMotionData.stCommand.dAcceleration;
}
double CIA402Axis::Axis_GetSetJerk()
{
    return m_stMotorMotionData.stCommand.dJerk;
}
int CIA402Axis::Axis_GetTargetVelocityPDO()
{
    return static_cast<int>(m_stMirrorPDO.TargetVelocity);
}

double CIA402Axis::Axis_GetSetTorque()
{
    return m_stMotorMotionData.stCommand.dTorque;
}

int CIA402Axis::Axis_GetTargetTorquePDO()
{
    return static_cast<int>(m_stMirrorPDO.TargetTorque);
}


// --- 实际运动参数 ---
double CIA402Axis::Axis_GetActualPosition()
{
    return m_stMotorMotionData.stActual.dPosition;
}

int CIA402Axis::Axis_GetActualPositionPDO()
{
    return static_cast<int>(m_stMirrorPDO.ActualPosition);
}

double CIA402Axis::Axis_GetActualVelocity()
{
    return m_stMotorMotionData.stActual.dVelocity;
}

double CIA402Axis::Axis_GetActualAcceleration()
{
    return m_stMotorMotionData.stActual.dAcceleration;
}

double CIA402Axis::Axis_GetActualJerk()
{
    return m_stMotorMotionData.stActual.dJerk;
}

int CIA402Axis::Axis_GetActualVelocityPDO()
{
    return static_cast<int>(m_stMirrorPDO.ActualVelocity);
}

double CIA402Axis::Axis_GetActualTorque()
{
    return m_stMotorMotionData.stActual.dTorque;
}

int CIA402Axis::Axis_GetActualTorquePDO()
{
    return static_cast<int>(m_stMirrorPDO.ActualTorque);
}

int CIA402Axis::Axis_GetDigitalOutputs()
{
    return static_cast<int>(m_stMirrorPDO.DigitalOutputs);
}

int CIA402Axis::Axis_GetDigitalInputs()
{
    return static_cast<int>(m_stMirrorPDO.DigitalInputs);
}



// --- 模式与错误信息 ---
int CIA402Axis::Axis_GetCommandOperationMode()
{
    return static_cast<int>(m_stMirrorPDO.TargetModesOfOperation);
}

int CIA402Axis::Axis_GetActualOperationMode()
{
    return static_cast<int>(m_stMirrorPDO.ActualModesOfOperation);
}

bool CIA402Axis::Axis_GetErrorState()
{
    return (m_stMirrorPDO.StatusWord & 0x0008) != 0;
}

int CIA402Axis::Axis_GetAxisErrorID()
{
    return static_cast<int>(m_stMirrorPDO.ErrorCode);
}

bool CIA402Axis::Axis_GetBusyState()
{
    return m_bBusy;
}

bool CIA402Axis::Axis_GetMovingState()
{
    return m_enAxisMotionState > motionState_standstill;
}

bool CIA402Axis::Axis_GetSoftMotionMovingState()
{
    return false;
}


// --- 双编码器 ---
ST_AxisDualEncoderConfiguration CIA402Axis::Axis_GetDualEncoderConfiguration()
{
    return m_stDualEncoderConfiguration;
}

ST_AxisDualEncoderData CIA402Axis::Axis_GetDualEncoderData()
{
    return m_stAxisDualEncoderData;
}


// --- 轴状态 ---
bool CIA402Axis::Axis_GetVirtualState()
{
    return m_bVirtual;
}

uint32_t CIA402Axis::Axis_GetAxisID()
{
    return m_nAxisID;
}

EN_AxisMotionState CIA402Axis::Axis_GetAxisMotionState()
{
    return m_enAxisMotionState;
}

bool CIA402Axis::Axis_GetDualEncoderEnableState()
{
    return m_stDualEncoderConfiguration.bDualEncoder;
}


// --- 配置 ---
ST_SMCAxisConfiguration CIA402Axis::Axis_GetAxisConfiguration()
{
    return m_stAxisConfiguration;
}

ST_Feedforward CIA402Axis::Axis_GetFeedforward()
{
    return m_stFeedforward;
}