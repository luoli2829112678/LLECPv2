#include"MC_SetModesOfOperation.h"
MC_SetModesOfOperation::MC_SetModesOfOperation(/* args */)
{
}

MC_SetModesOfOperation::~MC_SetModesOfOperation()
{
}

void MC_SetModesOfOperation::operator()(CIA402Axis* axis)
{
    this->Execute();
}
void MC_SetModesOfOperation::operator()(CIA402Axis* axis,bool bExecute,int nModesOfOperation,bool &bDone, bool &bError,int &nErrorID)
{
    m_bExecute = bExecute;
    m_nModesOfOperation = nModesOfOperation;
    if((bExecute && m_bExecute)&&(axis != m_pCIA402Axis))
    {
        m_bBusy = false;
        m_bError = true;
        m_bDone = false;
        m_nErrorID = SMEC_ABNORMAL_AXIS_CHANGE;
    }
    m_pCIA402Axis = axis;
    if(!bExecute)
    {
        m_bBusy = false;
        m_bError = false;
        m_bDone = false;
        m_nErrorID = SMEC_SUCCESSED;
        return;
    }
    this->Execute();
    bDone = m_bDone;
    bError = m_bError;
    nErrorID = m_nErrorID;
}


void MC_SetModesOfOperation::Execute()
{
    if(nullptr == m_pCIA402Axis)
    {
        m_bError = true;
        m_nErrorID = SMEC_INVALID_AXIS;
        return;
    }
    m_bBusy = true;
    m_pCIA402Axis->Axis_PDO_SetModesOfOperation(static_cast<EN_ModesOfOperation>(m_nModesOfOperation));
    if(m_nModesOfOperation == m_pCIA402Axis->Axis_GetActualOperationMode())
    {
        m_bDone = true;
    }
    if((m_pCIA402Axis->Axis_GetAxisMotionState() != EN_AxisMotionState::motionState_standstill)&&(m_pCIA402Axis->Axis_GetAxisMotionState() != EN_AxisMotionState::motionState_power_off))
    {
        m_bError = true;
        m_nErrorID = SMEC_AXIS_STATUS_INTERCEPTION;
    }
}