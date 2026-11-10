#include"MC_FollowTorque.h"
MC_FollowTorque::MC_FollowTorque(/* args */)
{
}

MC_FollowTorque::~MC_FollowTorque()
{
}

void MC_FollowTorque::operator()(CIA402Axis* axis)
{
    if(nullptr == axis)
    {
        return;
    }
    m_pCIA402Axis = axis;
}
void MC_FollowTorque::operator()(CIA402Axis* axis,bool bExecute,double dTorque,bool& bBusy,bool& bError,int& ErrorID)
{
    if(nullptr == axis)
    {
        m_bError = true;
        m_nErrorID = SMEC_INVALID_AXIS;
        return;
    }
    //printf("MC_FollowTorque:%f\n",dTorque);
    m_pCIA402Axis = axis;
    m_bExecute = bExecute;
    m_dTorque = dTorque;
    this->Execute();
    bBusy = m_bBusy;
    bError = m_bError;
    ErrorID = m_nErrorID;
}


void MC_FollowTorque::Execute()
{
    if(nullptr == m_pCIA402Axis)
    {
        m_bError = true;
        m_nErrorID = SMEC_INVALID_AXIS;
        return;
    }
    //输出初始化
    m_bBusy             = false;
    m_bError            = false;
    m_nErrorID           = SMEC_SUCCESSED;
    int res = AEC_SUCCESSED;
    if(m_bExecute)
    {
        if(EN_ModesOfOperation::enModeCyclicSyncTorque== m_pCIA402Axis->Axis_GetActualOperationMode())
        {
            m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState::motionState_synchronized_motion);
            m_pCIA402Axis->Axis_SetCyclicSyncMotion(); 
        }
        res = m_pCIA402Axis->Axis_SetTargetTorque(m_dTorque);
    }
    if(AEC_SUCCESSED != res)
    {
        m_bError = true;
    }

    m_bBusy = true;
    return;
}