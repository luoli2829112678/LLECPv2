#include"CIA402Axis.h"

int CIA402Axis::Axis_SetMotionPlanner(ST_InterParams stMotionPlanner)
{
    //printf("Set Motion Planner: P=%f\n", stMotionPlanner.P);
    Axis_SetCyclicSyncMotion();
    m_bSoftMotionSignal = true;
    m_stInterParamsData = stMotionPlanner;
    switch (m_stMirrorPDO.ActualModesOfOperation)
    {
    case EN_ModesOfOperation::enModeCyclicSyncPosition:
        m_stMotorMotionData.stCommand.dPosition = stMotionPlanner.P;
        m_stMotorMotionData.stCommand.dVelocity = stMotionPlanner.V;
        m_stMotorMotionData.stCommand.dAcceleration = stMotionPlanner.A;
        m_stMotorMotionData.stCommand.dJerk = stMotionPlanner.J;
        m_stMotorMotionData.stCommand.dSnap = stMotionPlanner.S;
        Axis_SetTargetPosition(stMotionPlanner.P);
        break;
    case EN_ModesOfOperation::enModeCyclicSyncVelocity:
        m_stMotorMotionData.stCommand.dPosition = m_stMotorMotionData.stCommand.dPosition + stMotionPlanner.P / m_dControlCycle;
        m_stMotorMotionData.stCommand.dVelocity = stMotionPlanner.P;
        m_stMotorMotionData.stCommand.dAcceleration = stMotionPlanner.V;
        m_stMotorMotionData.stCommand.dJerk = stMotionPlanner.A;
        m_stMotorMotionData.stCommand.dSnap = stMotionPlanner.J;
        Axis_SetTargetVelocity(stMotionPlanner.P);
        break;
    case EN_ModesOfOperation::enModeCyclicSyncTorque:
        m_stMotorMotionData.stCommand.dPosition = m_stMotorMotionData.stActual.dPosition;
        m_stMotorMotionData.stCommand.dTorque = stMotionPlanner.P;
        m_stMotorMotionData.stCommand.dTorqueDot = stMotionPlanner.V;
        m_stMotorMotionData.stCommand.dTorqueDdot = stMotionPlanner.A;
        m_stMotorMotionData.stCommand.dTorqueDddot = stMotionPlanner.J;
        m_stMotorMotionData.stCommand.dTorqueDdddot = stMotionPlanner.S;
        Axis_SetTargetTorque(stMotionPlanner.P);
        break;
    default:
        printf("m_stMirrorPDO.ActualModesOfOperationError%d\n",m_stMirrorPDO.ActualModesOfOperation);
        break;
    }
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_SetTargetPosition(double TargetPosition)
{
    //printf("CmdPosition:%f\n",TargetPosition);
    int32_t count = m_stAxisConfiguration.nEncodeDirection *  (TargetPosition * m_stAxisConfiguration.nEncodeRatio * m_stAxisConfiguration.dGearRatio / m_stAxisConfiguration.enAxisMoveDef) + 
                                    m_stAxisConfiguration.nEncodeHomePos;
    Axis_PDO_SetTargetPosition(count);  
    // printf("CmdPositionpdo:%d\n",count);
    m_stMotorMotionData.stCommand.dPosition = TargetPosition;
    return AEC_SUCCESSED;   
}

int CIA402Axis::Axis_SetTargetTorque(double TargetTorque)
{
    int32_t count = TargetTorque * m_stAxisConfiguration.nTorqueDirection * m_stAxisConfiguration.dTorqueScale;
    Axis_PDO_SetTargetTorque(count);  
    return AEC_SUCCESSED;  
}

int CIA402Axis::Axis_SetTargetVelocity(double TargetVelocity)
{
    int32_t count = TargetVelocity * m_stAxisConfiguration.dVelocityScale;
    //printf("Axis_SetTargetVelocity:%d\n",count);
    Axis_PDO_SetTargetVelocity(count);  
    return AEC_SUCCESSED;  
}
void CIA402Axis::Axis_SetCyclicSyncMotion()
{
    m_bCyclicSyncMotion = true;
}

int CIA402Axis::Axis_ReadActPos(double& pos)
{
    pos = m_stAxisConfiguration.nEncodeDirection * 
                                        double(m_stMirrorPDO.ActualPosition - m_stAxisConfiguration.nEncodeHomePos) * m_stAxisConfiguration.enAxisMoveDef /
                                        double(m_stAxisConfiguration.nEncodeRatio * m_stAxisConfiguration.dGearRatio);
    return AEC_SUCCESSED;  
}

int CIA402Axis::Axis_SetVFF(double dVelocity)
{
    Axis_PDO_SetVFF(m_stFeedforward.dVelocityFeedforward * dVelocity); 
    return AEC_SUCCESSED; 
}
int CIA402Axis::Axis_SetTFF(double dTorque)
{
    Axis_PDO_SetTFF(m_stFeedforward.dAccelerationFeedforward * dTorque); 
    return AEC_SUCCESSED; 
}