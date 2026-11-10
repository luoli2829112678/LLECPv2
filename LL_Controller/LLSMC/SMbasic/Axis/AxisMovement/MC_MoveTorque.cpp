#include"MC_MoveTorque.h"
MC_MoveTorque::MC_MoveTorque(/* args */)
{
}

MC_MoveTorque::~MC_MoveTorque()
{
}

void MC_MoveTorque::operator()(CIA402Axis* axis)
{
    this->Execute();
}
void MC_MoveTorque::CheckRunStatus()
{
    if((m_bDone != m_bDone_last)&&(!m_bDone_last))
    {
        //完成不切换轴状态机
    }
}

void MC_MoveTorque::operator()(CIA402Axis* axis,bool bExecute,double dTorque,double dTorque_dot,double dpTorque_ddot,double dnTorque_ddot,double dTorque_dddot,EN_Direction enDirection,EN_BufferMode enBufferMode,
                    bool& bDone,bool& bBusy,bool& bCommandAborted,bool& bError,int& ErrorID)
{
    m_pCIA402Axis                                   = axis;
    m_bExecute                                      = bExecute;
    m_MotionUint_New.PlanningMotionParam.tar            = dTorque;
    m_MotionUint_New.PlanningMotionParam.tar_dot            = abs(dTorque_dot);
    m_MotionUint_New.PlanningMotionParam.ptar_ddot            = abs(dpTorque_ddot);
    m_MotionUint_New.PlanningMotionParam.ntar_ddot            = abs(dnTorque_ddot);
    m_MotionUint_New.PlanningMotionParam.tar_dddot           = abs(dTorque_dddot);
    m_MotionUint_New.PlanningMotionParam.Direction      = enDirection;
    m_MotionUint_New.PlanningMotionParam.PlanningMode   = enPositionPlanningMode;
    m_MotionUint_New.BufferMode                         = enBufferMode;
    m_MotionUint_New.MoveType                           = enAbsoluteMotion;
    m_MotionUint_New.fbID                               = this;
    m_MotionUint_New.fCallback                          = m_fCallback;
    this->Execute();
    bDone = m_bDone;
    bBusy = m_bBusy;
    bCommandAborted = m_bCommandAborted;
    bError = m_bError;
    ErrorID = m_nErrorID;
}

void MC_MoveTorque::Execute()
{
    if(nullptr == m_pCIA402Axis)
    {
        m_bError = true;
        m_nErrorID = SMEC_INVALID_AXIS;
        return;
    }
    //状态校验
    if(EN_AxisMotionState::motionState_errorstop == m_pCIA402Axis->Axis_ReadAxisState())
    {
        m_bError            = true;
        m_pCIA402Axis->Axis_CheckError(m_nErrorID);
        return;
    }
    if(m_bExecute)
    {
        if((EN_AxisMotionState::motionState_standstill != m_pCIA402Axis->Axis_ReadAxisState()) &&
             (EN_AxisMotionState::motionState_continuous_motion!= m_pCIA402Axis->Axis_ReadAxisState()) &&
                (EN_AxisMotionState::motionState_discrete_motion!= m_pCIA402Axis->Axis_ReadAxisState()))
        {
            m_nErrorID = SMEC_AXIS_STATUS_INTERCEPTION;
            m_bError = true;
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
    if (m_pCIA402Axis->Axis_GetAxisMotionState() < EN_AxisMotionState::motionState_standstill)
    {
        m_bError            = true;
        m_nErrorID           = SMEC_AXIS_STATUS_INTERCEPTION;
        return;
    }
    //上升沿push
    if(m_Timer.R_TRIG(m_bExecute))
    {   
        m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState::motionState_discrete_motion);
        m_MotionUint = m_MotionUint_New;
        m_pCIA402Axis->Axis_PushMotionUint(m_MotionUint);
        m_bBusy = true;
    }
    //参数变更push
    if(m_MotionUint.PlanningMotionParam!=m_MotionUint_New.PlanningMotionParam)
    {
        m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState::motionState_discrete_motion);
        m_MotionUint = m_MotionUint_New;
        m_pCIA402Axis->Axis_PushMotionUint(m_MotionUint);
        m_bBusy = true;
    }
    //监测去掉，由回调函数处理


    // ST_MotionUint FirstMotionUint;
    // int nMotionNum = m_pCIA402Axis->Axis_GetMotionUint(FirstMotionUint);
    // //点位小于0直接return
    // if(nMotionNum < 0)
    // {
    //     m_bCommandAborted = true;
    //     return;
    // }
    // //第一个点就是这个点
    // if(this == FirstMotionUint.fbID)
    // {
    //     //运动完成监测
    //     if(FirstMotionUint.bMotionDone)
    //     {
    //         m_bDone             = true;
    //     }
    //     else
    //     {
    //         m_bBusy             = true;
    //     }
    // }
    // //move被插其他块
    // else
    // {
    //     m_bCommandAborted = true;
    //     return;
    // }
    return;
}