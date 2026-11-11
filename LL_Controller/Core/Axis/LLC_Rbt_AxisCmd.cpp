#include "LLC_Rbt.h"
int LLC_Rbt::AxisInitReset(vector<int>AxisList)
{
    for(size_t i = 0;i<AxisList.size();i++)
    {
        bAxisInitResetCmd[AxisList[i]] = true;
    }
    if(0 == AxisList.size())
    {
        for(int i = 0;i<m_nConfiguredAxisNum;i++)
        {
            bAxisInitResetCmd[i] = true;
        }
    }
    return REC_Successed;
}
int LLC_Rbt::AxisEnable(vector<int>AxisList)
{
    if(AxisList.empty())for(std::size_t i=0;i<m_vAxis.size();++i)AxisList.push_back(static_cast<int>(i));
    auto valid=[&](int id){return id>=0&&static_cast<std::size_t>(id)<m_vAxis.size()&&!m_vAxis[id]->Axis_GetErrorState()&&
        (m_vAxis[id]->Axis_GetStatusWord()&0x8)==0&&
        (m_vAxis[id]->Axis_GetAxisMotionState()==motionState_power_off||m_vAxis[id]->Axis_GetAxisMotionState()==motionState_standstill);};
    for(const int id:AxisList)if(!valid(id))return CEC_SDL_INVALID_AXIS_STATE;
    for(const int id:AxisList){
        auto* axis=m_vAxis[id];
        if(axis->Axis_GetAxisMotionState()==motionState_standstill){
            if(axis->Axis_GetVirtualState()||(axis->Axis_GetStatusWord()&0x6f)==0x27)continue;
        }
        bAxisEnableCmd[id]=true;
    }
    return REC_Successed;
}
int LLC_Rbt::AxisDisable(vector<int>AxisList)
{
    for(size_t i = 0;i<AxisList.size();i++)
    {
        int nAxisID = AxisList[i];
        if(EN_AxisMotionState::motionState_standstill !=m_vAxis[nAxisID]->Axis_GetAxisMotionState())
            return CEC_SDL_INVALID_AXIS_STATE;
        bAxisDisableCmd[nAxisID] = true;
    }
    if(0 == AxisList.size())
    {
        for(uint32_t i = 0;i<m_nConfiguredAxisNum;i++)
        {
            if(EN_AxisMotionState::motionState_standstill !=m_vAxis[i]->Axis_GetAxisMotionState())
                break;
            bAxisDisableCmd[i] = true;
        }
    }
    return REC_Successed;
}
int LLC_Rbt::AxisReset(vector<int> axes)
{
    if(axes.empty())for(std::size_t i=0;i<m_vAxis.size();++i)axes.push_back(static_cast<int>(i));
    for(const int id:axes)if(id<0||static_cast<std::size_t>(id)>=m_vAxis.size()||!m_vAxis[id])return REC_AxisIDNotExist;
    for(const int id:axes){
        auto* axis=m_vAxis[id];
        if(axis->Axis_GetAxisMotionState()!=motionState_errorstop&&!axis->Axis_GetErrorState()&&!(axis->Axis_GetStatusWord()&8))continue;
        bAxisAbsMoveCmd[id]=bAxisRelMoveCmd[id]=bAxisBackAndForthMoveCmd[id]=false;
        bAxisContinuousStepMoveCmd[id]=bAxisVelocityMoveCmd[id]=false;
        bAxisEnableCmd[id]=bAxisDisableCmd[id]=bAxisHomeCmd[id]=false;
        m_enAxisAbsMoveFSM[id]=m_enAxisRelMoveFSM[id]=m_enAxisHomeFSM[id]=EN_AxisControlFSM::enAxisControlFSM_Null;
        m_enAxisEnableFSM[id]=m_enAxisDisableFSM[id]=EN_AxisControlFSM::enAxisControlFSM_Null;
        m_enAxisBackAndForthMoveFSM[id]=m_enAxisContinuousStepMoveFSM[id]=m_enAxisVelocityMoveFSM[id]=0;
        bAxisResetCmd[id]=true;
    }
    return REC_Successed;
}

int LLC_Rbt::AxisHome(vector<int>AxisList)
{
    for(size_t i = 0;i<AxisList.size();i++)
    {
        int nAxisID = AxisList[i];
        if(EN_AxisMotionState::motionState_standstill !=m_vAxis[nAxisID]->Axis_GetAxisMotionState())
            return CEC_SDL_INVALID_AXIS_STATE;
        bAxisHomeCmd[nAxisID] = true;
    }
    if(0 == AxisList.size())
    {
        for(uint32_t i = 0;i<m_nConfiguredAxisNum;i++)
        {
            if(EN_AxisMotionState::motionState_standstill !=m_vAxis[i]->Axis_GetAxisMotionState())
                break;
            bAxisHomeCmd[i] = true;
        }
    }
    return REC_Successed;
}

int LLC_Rbt::AxisSetOffset(int nAxisID,double dOffset)
{
    bool bDone = false;
    bool bError = false;
    int nErrorID = REC_Successed;
    //运动中不允许设置
    switch (m_vAxis[nAxisID]->Axis_GetAxisMotionState())
    {
        case motionState_discrete_motion:
        case motionState_continuous_motion:
        case motionState_synchronized_motion:
            return CEC_SDL_INVALID_AXIS_STATE;
        default:
            m_FB_AxisSetPosition[nAxisID](m_vAxis[nAxisID],false,dOffset,bDone,bError,nErrorID);
            m_FB_AxisSetPosition[nAxisID](m_vAxis[nAxisID],true,dOffset,bDone,bError,nErrorID);
            break;
    }
    if(bDone)
    {
        return REC_Successed;
    }
    return CEC_ExecuteError;
}

int LLC_Rbt::AxisSetModesOfOperation(int nAxisID,int nModesOfOperation)
{
    bool bDone = false;
    bool bError = false;
    int nErrorID = REC_Successed;
    //运动中不允许设置
    if ( motionState_standstill != m_vAxis[nAxisID]->Axis_GetAxisMotionState())
        return CEC_SDL_INVALID_AXIS_STATE;
    m_FB_AxisSetModesOfOperation[nAxisID](m_vAxis[nAxisID],false,nModesOfOperation,bDone,bError,nErrorID);
    m_FB_AxisSetModesOfOperation[nAxisID](m_vAxis[nAxisID],true,nModesOfOperation,bDone,bError,nErrorID);
    //该接口不等待直接返回
    return REC_Successed;
}



int LLC_Rbt::AxisStop(vector<int>AxisList)
{
    for(size_t i = 0;i<AxisList.size();i++)
    {
        int AxisID = AxisList[i];
        bAxisAbsMoveCmd[AxisID] = false;
        bAxisRelMoveCmd[AxisID] = false;
        bAxisBackAndForthMoveCmd[AxisID] = false;
        bAxisContinuousStepMoveCmd[AxisID] = false;
        bAxisVelocityMoveCmd[AxisID] = false;

        m_enAxisAbsMoveFSM[AxisID] = EN_AxisControlFSM::enAxisControlFSM_Null;
        m_enAxisRelMoveFSM[AxisID] = EN_AxisControlFSM::enAxisControlFSM_Null;
        m_enAxisHomeFSM[AxisID] = EN_AxisControlFSM::enAxisControlFSM_Null;
        m_enAxisBackAndForthMoveFSM[AxisID] = 0;
        m_enAxisContinuousStepMoveFSM[AxisID] = 0;
        m_enAxisVelocityMoveFSM[AxisID] = 0;
        if(EN_AxisMotionState::motionState_standstill > m_vAxis[AxisID]->Axis_GetAxisMotionState())
            continue;
        bAxisStopCmd[AxisID] = true;
    }
    return REC_Successed;
}

int LLC_Rbt::AxisAbsMove(int nAxisID,double dPos,double dVel,double dAcc,double dDec,double dJerk)
{
    switch (m_vAxis[nAxisID]->Axis_GetAxisMotionState())
    {
        case motionState_standstill:
        case motionState_discrete_motion:
        case motionState_continuous_motion:
        case motionState_synchronized_motion:
            m_vAxisMtionParam[nAxisID].enMoveType = EN_LLPCAxisMoveType::enLLPCAbsoluteMotion;
            m_vAxisMtionParam[nAxisID].dPos = dPos;
            m_vAxisMtionParam[nAxisID].dVel = dVel;
            m_vAxisMtionParam[nAxisID].dAcc = dAcc;
            m_vAxisMtionParam[nAxisID].dDec = dDec;
            m_vAxisMtionParam[nAxisID].dJerk = dJerk;
            bAxisAbsMoveCmd[nAxisID] = true;
            return REC_Successed;
        default:
            return CEC_SDL_INVALID_AXIS_STATE;
    }
    return REC_Successed;
}
int LLC_Rbt::AxisRelMove(int nAxisID,double dPos,double dVel,double dAcc,double dDec,double dJerk)
{
    switch (m_vAxis[nAxisID]->Axis_GetAxisMotionState())
    {
        case motionState_standstill:
        case motionState_discrete_motion:
        case motionState_continuous_motion:
        case motionState_synchronized_motion:
            m_vAxisMtionParam[nAxisID].enMoveType = EN_LLPCAxisMoveType::enLLPCRelativMotion;
            m_vAxisMtionParam[nAxisID].dPos = dPos;
            m_vAxisMtionParam[nAxisID].dVel = dVel;
            m_vAxisMtionParam[nAxisID].dAcc = dAcc;
            m_vAxisMtionParam[nAxisID].dDec = dDec;
            m_vAxisMtionParam[nAxisID].dJerk = dJerk;
            bAxisRelMoveCmd[nAxisID] = true;
            return REC_Successed;
        default:
            return CEC_SDL_INVALID_AXIS_STATE;
    }
    return REC_Successed;
}

int LLC_Rbt::AxisBackAndForthMove(int nAxisID,double dPos1,double dPos2,double dVel1,double dVel2,double dAcc1,double dAcc2,double dDec1,double dDec2,double dJerk1,double dJerk2,double dWaitTime)
{
    switch (m_vAxis[nAxisID]->Axis_GetAxisMotionState())
    {
        case motionState_standstill:
            m_vAxisMtionParam[nAxisID].enMoveType = EN_LLPCAxisMoveType::enLLPCBackAndForthMove;
            m_vAxisMtionParam_Test[0][nAxisID].enMoveType = EN_LLPCAxisMoveType::enLLPCBackAndForthMove;
            m_vAxisMtionParam_Test[1][nAxisID].enMoveType = EN_LLPCAxisMoveType::enLLPCBackAndForthMove;
            m_vAxisMtionParam_Test[0][nAxisID].dPos = dPos1;
            m_vAxisMtionParam_Test[1][nAxisID].dPos = dPos2;
            m_vAxisMtionParam_Test[0][nAxisID].dVel = dVel1;
            m_vAxisMtionParam_Test[1][nAxisID].dVel = dVel2;
            m_vAxisMtionParam_Test[0][nAxisID].dAcc = dAcc1;
            m_vAxisMtionParam_Test[1][nAxisID].dAcc = dAcc2;
            m_vAxisMtionParam_Test[0][nAxisID].dDec = dDec1;
            m_vAxisMtionParam_Test[1][nAxisID].dDec = dDec2;
            m_vAxisMtionParam_Test[0][nAxisID].dJerk = dJerk1;
            m_vAxisMtionParam_Test[1][nAxisID].dJerk = dJerk2;
            m_vAxisMtionWaitTime_Test[0][nAxisID] = dWaitTime;
            m_vAxisMtionWaitTime_Test[1][nAxisID] = dWaitTime;
            bAxisBackAndForthMoveCmd[nAxisID] = true;
            return REC_Successed;
        default:
            return CEC_SDL_INVALID_AXIS_STATE;
    }
    return REC_Successed;
}
int LLC_Rbt::AxisContinuousStepMove(int nAxisID,double dPos,double dVel,double dAcc,double dDec,double dJerk,double dWaitTime)
{
    switch (m_vAxis[nAxisID]->Axis_GetAxisMotionState())
    {
        case motionState_standstill:
            m_vAxisMtionParam[nAxisID].enMoveType = EN_LLPCAxisMoveType::enLLPCContinuousStepMove;
            m_vAxisMtionParam_Test[0][nAxisID].enMoveType = EN_LLPCAxisMoveType::enLLPCContinuousStepMove;
            m_vAxisMtionParam_Test[0][nAxisID].dPos = dPos;
            m_vAxisMtionParam_Test[0][nAxisID].dVel = dVel;
            m_vAxisMtionParam_Test[0][nAxisID].dAcc = dAcc;
            m_vAxisMtionParam_Test[0][nAxisID].dDec = dAcc;
            m_vAxisMtionParam_Test[0][nAxisID].dJerk = dJerk;
            m_vAxisMtionWaitTime_Test[0][nAxisID] = dWaitTime;
            bAxisContinuousStepMoveCmd[nAxisID] = true;
            return REC_Successed;
        default:
            return CEC_SDL_INVALID_AXIS_STATE;
    }
    return REC_Successed;
}
int LLC_Rbt::AxisVelocityMove(int nAxisID,int nDir,double dVel,double dAcc,double dDec,double dJerk)
{
    switch (m_vAxis[nAxisID]->Axis_GetAxisMotionState())
    {
        case motionState_standstill:
            m_vAxisMtionParam[nAxisID].enMoveType = EN_LLPCAxisMoveType::enLLPCVelocityMove;
            m_vAxisMtionParam_Test[0][nAxisID].enMoveType = EN_LLPCAxisMoveType::enLLPCVelocityMove;
            m_vAxisMtionParam_Test[0][nAxisID].nDir = nDir;
            m_vAxisMtionParam_Test[0][nAxisID].dVel = dVel;
            m_vAxisMtionParam_Test[0][nAxisID].dAcc = dAcc;
            m_vAxisMtionParam_Test[0][nAxisID].dDec = dAcc;
            m_vAxisMtionParam_Test[0][nAxisID].dJerk = dJerk;
            bAxisVelocityMoveCmd[nAxisID] = true;
            return REC_Successed;
        default:
            return CEC_SDL_INVALID_AXIS_STATE;
    }
    return REC_Successed;
}


int LLC_Rbt::AxisReadState(int nAxisID)
{
    if(nAxisID > m_vAxis.size())return -1;
    return m_vAxis[nAxisID]->Axis_GetAxisMotionState();
}


int LLC_Rbt::StartAnalogFocusing(int nAxisID)
{
    return REC_Successed;
}
int LLC_Rbt::StopAnalogFocusing(int nAxisID)
{
    return REC_Successed;
}
int LLC_Rbt::StartPEG(int nAxisID)
{
    return REC_Successed;
}
int LLC_Rbt::StopPEG(int nAxisID)
{
    return REC_Successed;
}


int LLC_Rbt::AxisFollowPosition(int nAxisID,double dPos)
{
    if((size_t)nAxisID > m_vAxis.size())return REC_ParametersError;
    bool bBusy;
    bool bError;
    int ErrorID;
    m_FB_FollowPosition[nAxisID](m_vAxis[nAxisID],true,dPos,bBusy,bError,ErrorID);
    if(bError)
        return ErrorID;
    return REC_Successed;
}
int LLC_Rbt::AxisFollowVelocity(int nAxisID,double dVel)
{
    if((size_t)nAxisID > m_vAxis.size())return REC_ParametersError;
    bool bBusy;
    bool bError;
    int ErrorID;
    m_FB_FollowVelocity[nAxisID](m_vAxis[nAxisID],true,dVel,bBusy,bError,ErrorID);
    if(bError)return ErrorID;
    return REC_Successed;
}
int LLC_Rbt::AxisFollowTorque(int nAxisID,double dTor)
{
    if((size_t)nAxisID > m_vAxis.size())return REC_ParametersError;
    bool bBusy;
    bool bError;
    int ErrorID;
    m_FB_FollowTorque[nAxisID](m_vAxis[nAxisID],true,dTor,bBusy,bError,ErrorID);
    if(bError)return ErrorID;
    return REC_Successed;
}
