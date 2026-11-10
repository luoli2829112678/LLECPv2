#include"SMbasic.h"


SMbasic::SMbasic(/* args */)
{
    m_fCallback = [this](bool bBusy,bool bDone,bool bCommandAborted,bool bError,int nErrorID){this->SetFBStatus(bBusy,bDone,bCommandAborted,bError,nErrorID);};
}

SMbasic::~SMbasic()
{
}

void SMbasic::Execute()
{
    return;
}
void SMbasic::CheckRunStatus()
{
    return;
}

void SMbasic::operator()(CIA402Axis* axis)
{
    m_pCIA402Axis = axis;
}

CIA402Axis  SMbasic::Axis()
{
    return m_pCIA402Axis;
}
bool SMbasic::Busy()
{
    return m_bBusy;
}
bool SMbasic::Done()
{
    return m_bDone;
}
bool SMbasic::Error()
{
    return m_bError;
}
bool SMbasic::ErrorID()
{
    return m_nErrorID;
}
int SMbasic::SetFBStatus(bool bBusy,bool bDone,bool bCommandAborted,bool bError,int nErrorID)
{
    m_bBusy = bBusy;
    m_bDone = bDone;
    m_bCommandAborted = bCommandAborted;
    m_bError = bError;
    m_nErrorID = nErrorID;
    CheckRunStatus();
    // printf("Busy=%d Done=%d Aborted=%d Error=%d ErrorID=%d\n",
    //     m_bBusy,
    //     m_bDone,
    //     m_bCommandAborted,
    //     m_bError,
    //     m_nErrorID);
    m_bBusy_last = m_bBusy;
    m_bCommandAborted_last = m_bCommandAborted;
    m_bDone_last = m_bDone;
    m_bError_last = m_bError;
    m_nErrorID_last = m_nErrorID;
    return SMEC_SUCCESSED;
}