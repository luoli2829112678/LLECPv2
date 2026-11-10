#include"MC_GroupStop.h"
MC_GroupStop::MC_GroupStop(/* args */)
{
}

MC_GroupStop::~MC_GroupStop()
{
}

void MC_GroupStop::operator()(AxisGroup* group)
{
    this->Execute();
}

void MC_GroupStop::CheckRunStatus()
{
    //运动完成监测
    if((m_bDone != m_bDone_last)&&(!m_bDone_last))
    {
        m_pAxisGroup->Group_SetGroupState(ENAxisGroupState ::GroupStandby);
    }
    //错误监测
}

void MC_GroupStop::operator()(AxisGroup* group,bool bExecute,double dDeceleration,double dJerk,
                    bool& bDone,bool& bBusy,bool& bCommandAborted,bool& bError,int& ErrorID)
{
    m_pAxisGroup                                   = group;
    m_bExecute                                      = bExecute;
    m_MotionUint_New.fbID                               = this;
    m_MotionUint_New.fCallback                          = m_fCallback;
    this->Execute();
    bDone = m_bDone;
    bBusy = m_bBusy;
    m_dDeceleration = dDeceleration;
    m_dJerk = dJerk;
    bCommandAborted = m_bCommandAborted;
    bError = m_bError;
    ErrorID = m_nErrorID;
}

void MC_GroupStop::Execute()
{
    if(nullptr == m_pAxisGroup)
    {
        m_bError = true;
        m_nErrorID = SMEC_INVALID_AXIS;
        return;
    }
    // 输出参数默认初始化
    m_bDone             = false;
    m_bBusy             = false;
    m_bCommandAborted   = false;
    m_bError            = false;
    m_nErrorID           = 0;
    
    //状态校验
    if(ENAxisGroupState::GroupErrorStop == m_pAxisGroup->Group_ReadGroupState())
    {
        m_bError            = true;
        m_pAxisGroup->Group_CheckError(m_nErrorID);
        return;
    }
    if(m_bExecute)
    {
        if((ENAxisGroupState::GroupMoving!= m_pAxisGroup->Group_ReadGroupState()) &&
              (ENAxisGroupState::GroupStopping!= m_pAxisGroup->Group_ReadGroupState()))
        {
            //未运动直接返回done
            // 输出参数默认初始化
            m_bDone             = true;
            m_bBusy             = false;
            m_bCommandAborted   = false;
            m_bError            = false;
            m_nErrorID           = 0;
            m_Timer.R_TRIG(m_bExecute);
            return;
        }
        if (ENAxisGroupState::GroupErrorStop == m_pAxisGroup->Group_ReadGroupState())
        {
            //轴错误状态调用报错
            m_nErrorID = SMEC_AXIS_STATUS_INTERCEPTION;
            m_bError = true;
            return;
        }
        
    }
    else
    {
        // 输出参数默认初始化
        m_bDone             = false;
        m_bBusy             = false;
        m_bCommandAborted   = false;
        m_bError            = false;
        m_nErrorID           = 0;
        m_Timer.R_TRIG(m_bExecute);
        return;
    }
    int nMotionNum = 0;
    ST_GroupMotionUint  FirstMotionUint;
    //上升沿push
    if(m_Timer.R_TRIG(m_bExecute))
    {
        //未运动直接完成
        if(ENAxisGroupState::GroupStandby == m_pAxisGroup->Group_ReadGroupState())
        {
            m_bDone = true;
            return;
        }
        nMotionNum = m_pAxisGroup->Group_GetMotionUint(FirstMotionUint);
        switch (m_pAxisGroup->Axis_GetActualOperationMode())
        {
        case EN_ModesOfOperation::enModeCyclicSyncPosition:
            m_GroupMotionUint.bIsStoping = true;
            m_GroupMotionUint.PlanningMotionParam.enGroupMotionType = FirstMotionUint.PlanningMotionParam.enGroupMotionType;
            m_GroupMotionUint.PlanningMotionParam.CommandVelocity   = m_pAxisGroup->m_stGroupMotorMotionData.CommandVelocity;
            m_GroupMotionUint.PlanningMotionParam.dJointVelocity    = m_pAxisGroup->m_stGroupMotorMotionData.dJointVelocity;
            m_GroupMotionUint.PlanningMotionParam.dTCPVelocity      = m_pAxisGroup->m_stGroupMotorMotionData.dTCPVelocity;
            m_GroupMotionUint.PlanningMotionParam.CommandAcceleration = m_pAxisGroup->m_stGroupMotorMotionData.CommandAcceleration;
            m_GroupMotionUint.PlanningMotionParam.CommandJerk       = m_pAxisGroup->m_stGroupMotorMotionData.CommandJerk;
            m_GroupMotionUint.PlanningMotionParam.CommandDeceleration = m_pAxisGroup->m_stGroupMotorMotionData.CommandAcceleration;
            m_GroupMotionUint.MoveType                           = enRelativMotion;
            m_MotionUint = m_MotionUint_New;
            break;
        default:
            break;
        }
        m_pAxisGroup->Group_SetGroupState(ENAxisGroupState ::GroupStopping);
        m_pAxisGroup->Group_PushMotionUint(m_GroupMotionUint);
    }
    //参数变更push
    if(m_MotionUint.PlanningMotionParam!=m_MotionUint_New.PlanningMotionParam)
    {
        m_MotionUint = m_MotionUint_New;
        m_pAxisGroup->Group_PushMotionUint(m_GroupMotionUint);
    }

    //点位小于0直接return
    if(nMotionNum < 0)
    {
        m_bCommandAborted = true;
        return;
    }
    //第一个点就是这个点
    if(this == FirstMotionUint.fbID)
    {
        //运动完成监测
        if(FirstMotionUint.bMotionDone)
        {
            m_bDone             = true;
        }
        else
        {
            m_bBusy             = true;
        }
    }
    //move被插其他块
    else
    {
        m_bCommandAborted = true;
        return;
    }
    return;
}