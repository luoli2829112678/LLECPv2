#include "LLC_Rbt.h"


//FSM
int LLC_Rbt::SetAxisMove(vector<int> v_nAxisID)
{
    for (size_t i = 0; i < v_nAxisID.size(); i++)
    {
        switch (m_vAxis[v_nAxisID[i]]->Axis_GetAxisMotionState())
        {
            case motionState_standstill:
            case motionState_discrete_motion:
            case motionState_continuous_motion:
            case motionState_synchronized_motion:
                m_vAxisMtionParam[v_nAxisID[i]].enMoveType = EN_LLPCAxisMoveType::enLLPCAbsoluteMotion;
                bAxisAbsMoveCmd[v_nAxisID[i]] = true;
                break;;
            default:
                return CEC_SDL_INVALID_AXIS_STATE;
        }
    }
    return REC_Successed;
}

int LLC_Rbt::SetAxisMovePos(int nAxisID, double dPos)
{
    m_vAxisMtionParam[nAxisID].enMoveType = EN_LLPCAxisMoveType::enLLPCAbsoluteMotion;
    m_vAxisMtionParam[nAxisID].dPos = dPos;
    return REC_Successed;
}

int LLC_Rbt::SetAxisStopParam(int nAxisID,double dStopAcc,double dStopJerk)
{
    v_AxisHardParam[nAxisID].dStopAcc = dStopAcc;
    v_AxisHardParam[nAxisID].dStopJerk = dStopJerk;
    return REC_Successed;
}

int LLC_Rbt::SetAxisMoveVel(int nAxisID, double dVel)
{
    m_vAxisMtionParam[nAxisID].dVel = dVel;
    return REC_Successed;
}
int LLC_Rbt::SetAxisMoveAcc(int nAxisID, double dAcc)
{
    m_vAxisMtionParam[nAxisID].dAcc = dAcc;
    return REC_Successed;
}
int LLC_Rbt::SetAxisMoveDec(int nAxisID, double dDec)
{
    m_vAxisMtionParam[nAxisID].dDec = dDec;
    return REC_Successed;
}
int LLC_Rbt::SetAxisMoveJerk(int nAxisID, double dJerk)
{
    m_vAxisMtionParam[nAxisID].dJerk = dJerk;
    return REC_Successed;
}


int LLC_Rbt::SetAccFFScale(int nAxisID,double dScale)
{
    m_stFeedforward[nAxisID].dAccelerationFeedforward = dScale;
    m_vAxis[nAxisID]->Axis_SetFeedforward(m_stFeedforward[nAxisID]);
    return REC_Successed;
}
int LLC_Rbt::SetVelFFScale(int nAxisID,double dScale)
{
    m_stFeedforward[nAxisID].dVelocityFeedforward = dScale;
    m_vAxis[nAxisID]->Axis_SetFeedforward(m_stFeedforward[nAxisID]);
    return REC_Successed;
}
int LLC_Rbt::SetJerkFFScale(int nAxisID,double dScale)
{
    m_stFeedforward[nAxisID].dJerkFeedforward = dScale;
    m_vAxis[nAxisID]->Axis_SetFeedforward(m_stFeedforward[nAxisID]);
    return REC_Successed;
}