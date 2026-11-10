#include"CIA402Axis.h"

int CIA402Axis::Axis_SetAxisType(EN_AxisMoveDef enAxisMoveDef)
{
    m_stAxisConfiguration.enAxisMoveDef = enAxisMoveDef;
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_SetGearRatio(double GearRatio)
{
    m_stAxisConfiguration.dGearRatio = GearRatio;
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_SetEncodeRatio(uint32_t EncodeRatio)
{
    m_stAxisConfiguration.nEncodeRatio = EncodeRatio;
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_SetEncodeDirection(int EncodeDirection)
{
    m_stAxisConfiguration.nEncodeDirection = EncodeDirection;
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_SetEncodeHomePos(int32_t EncodeHomePos)
{
    m_stAxisConfiguration.nEncodeHomePos = EncodeHomePos;
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_SetTorqueScale(double TorqueScale)
{
    m_stAxisConfiguration.dTorqueScale = TorqueScale;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_SetVelocityScale(double VelocityScale)
{
    m_stAxisConfiguration.dVelocityScale = VelocityScale;
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_SetTFFScale(double TFFScales)
{
    m_stFeedforward.dJerkFeedforward = TFFScales;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_SetVFFScale(double VFFScale)
{
    m_stFeedforward.dVelocityFeedforward = VFFScale;
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_SetTorqueDirection(int TorqueDirection)
{
    m_stAxisConfiguration.nTorqueDirection = TorqueDirection;
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_SetSWLimit(double Positive,double Negative)
{
    m_stAxisConfiguration.dPositive = Positive;
    m_stAxisConfiguration.dNegative = Negative;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_SetControlCycle(double dControlCycle)
{
    m_dControlCycle = dControlCycle;
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_SetDriveErrorCode(int ErrorCode)
{
    m_nErrorCode = ErrorCode;
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_SetFeedforward(ST_Feedforward stfw)
{
    m_stFeedforward = stfw;
    return AEC_SUCCESSED;
}