#include"MC_SetPosition.h"
MC_SetPosition::MC_SetPosition(/* args */)
{
}

MC_SetPosition::~MC_SetPosition()
{
}

void MC_SetPosition::operator()(CIA402Axis* axis)
{
    this->Execute();
}
void MC_SetPosition::operator()(CIA402Axis* axis,bool bExecute,double dPosition,bool &bDone, bool &bError,int &nErrorID)
{
    m_bExecute = bExecute;
    m_dPosition = dPosition;
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


void MC_SetPosition::Execute()
{
    if(nullptr == m_pCIA402Axis)
    {
        m_bError = true;
        m_nErrorID = SMEC_INVALID_AXIS;
        return;
    }
    m_bDone = true;
    int nHomePos = m_pCIA402Axis->Axis_GetActualPositionPDO() -static_cast<int>(std::round(m_dPosition *
                                    (m_pCIA402Axis->m_stAxisConfiguration.nEncodeRatio * m_pCIA402Axis->m_stAxisConfiguration.dGearRatio) /
                                    (m_pCIA402Axis->m_stAxisConfiguration.nEncodeDirection * m_pCIA402Axis->m_stAxisConfiguration.enAxisMoveDef)));
    m_pCIA402Axis->Axis_SetEncodeHomePos(nHomePos);
}