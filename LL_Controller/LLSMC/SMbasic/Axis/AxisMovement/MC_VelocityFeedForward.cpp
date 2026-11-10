#include"MC_VelocityFeedForward.h"
MC_VelocityFeedForward::MC_VelocityFeedForward(/* args */)
{
}

MC_VelocityFeedForward::~MC_VelocityFeedForward()
{
}

void MC_VelocityFeedForward::operator()(CIA402Axis* axis)
{
    if(nullptr == axis)
    {
        return;
    }
    m_pCIA402Axis = axis;
}
void MC_VelocityFeedForward::operator()(CIA402Axis* axis,bool bExecute,double dVelocity,bool& bBusy,bool& bError,int& ErrorID)
{
    if(nullptr == axis)
    {
        m_bError = true;
        m_nErrorID = SMEC_INVALID_AXIS;
        return;
    }
    m_pCIA402Axis = axis;
    m_bExecute = bExecute;
    m_dVelocity = dVelocity;
    this->Execute();
    bBusy = m_bBusy;
    bError = m_bError;
    ErrorID = m_nErrorID;
}


void MC_VelocityFeedForward::Execute()
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
        res = m_pCIA402Axis->Axis_SetVFF(m_dVelocity);
    }
    if(AEC_SUCCESSED != res)
    {
        m_bError = true;
    }

    m_bBusy = true;
    return;
}