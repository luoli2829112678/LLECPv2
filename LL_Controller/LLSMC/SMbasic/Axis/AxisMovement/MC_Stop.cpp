#include"MC_Stop.h"
MC_Stop::MC_Stop(/* args */)
{
}

MC_Stop::~MC_Stop()
{
}

void MC_Stop::operator()(CIA402Axis* axis)
{
    this->Execute();
}

void MC_Stop::CheckRunStatus()
{
    //运动完成监测
    if((m_bDone != m_bDone_last)&&(!m_bDone_last))
    {
        m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState ::motionState_standstill);
    }
    //错误监测
}

void MC_Stop::operator()(CIA402Axis* axis,bool bExecute,double dDeceleration,double dJerk,
                    bool& bDone,bool& bBusy,bool& bCommandAborted,bool& bError,int& ErrorID)
{
    m_pCIA402Axis                                   = axis;
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

void MC_Stop::Execute()
{
    if(nullptr == m_pCIA402Axis)
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
    if(EN_AxisMotionState::motionState_errorstop == m_pCIA402Axis->Axis_ReadAxisState())
    {
        m_bError            = true;
        m_pCIA402Axis->Axis_CheckError(m_nErrorID);
        return;
    }
    if(m_bExecute)
    {
        if((EN_AxisMotionState::motionState_continuous_motion!= m_pCIA402Axis->Axis_ReadAxisState()) &&
              (EN_AxisMotionState::motionState_stopping!= m_pCIA402Axis->Axis_ReadAxisState()) &&
                (EN_AxisMotionState::motionState_discrete_motion!= m_pCIA402Axis->Axis_ReadAxisState()))
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
        if (EN_AxisMotionState::motionState_errorstop == m_pCIA402Axis->Axis_ReadAxisState())
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
    //上升沿push
    if(m_Timer.R_TRIG(m_bExecute))
    {
        //未运动直接完成
        if(motionState_standstill == m_pCIA402Axis->Axis_ReadAxisState())
        {
            m_bDone = true;
            return;
        }
        switch (m_pCIA402Axis->Axis_GetActualOperationMode())
        {
        case EN_ModesOfOperation::enModeCyclicSyncPosition:
            m_MotionUint_New.PlanningMotionParam.PlanningMode = EN_PlanningMode::enStopPlanningMode;
            m_MotionUint_New.PlanningMotionParam.tar            = m_pCIA402Axis->Axis_GetSetPosition();
            m_MotionUint_New.PlanningMotionParam.tar_dot            = m_pCIA402Axis->Axis_GetSetVelocity();//此处使用命令速度，待优化
            m_MotionUint_New.PlanningMotionParam.ptar_ddot            = m_pCIA402Axis->Axis_GetSetAcceleration();
            m_MotionUint_New.PlanningMotionParam.ntar_ddot      = m_MotionUint_New.PlanningMotionParam.ntar_ddot;
            m_MotionUint_New.PlanningMotionParam.ntar_ddot            = m_dDeceleration;
            m_MotionUint_New.PlanningMotionParam.tar_dddot           = m_dJerk;
            m_MotionUint_New.MoveType                           = enRelativMotion;
            m_MotionUint = m_MotionUint_New;
            break;
        case EN_ModesOfOperation::enModeCyclicSyncTorque:
            m_MotionUint_New.PlanningMotionParam.PlanningMode = EN_PlanningMode::enPositionPlanningMode;
            m_MotionUint_New.PlanningMotionParam.tar            = 0;
            m_MotionUint_New.PlanningMotionParam.tar_dot        = m_pCIA402Axis->m_stSoftMotionEX.vMotionUint.begin()->PlanningMotionParam.tar_dot;
            m_MotionUint_New.PlanningMotionParam.ptar_ddot      = m_pCIA402Axis->m_stSoftMotionEX.vMotionUint.begin()->PlanningMotionParam.ptar_ddot;
            m_MotionUint_New.PlanningMotionParam.ntar_ddot      = m_pCIA402Axis->m_stSoftMotionEX.vMotionUint.begin()->PlanningMotionParam.ntar_ddot;
            m_MotionUint_New.PlanningMotionParam.tar_dddot      = m_pCIA402Axis->m_stSoftMotionEX.vMotionUint.begin()->PlanningMotionParam.tar_dddot;
            m_MotionUint_New.PlanningMotionParam.tar_ddddot     = m_pCIA402Axis->m_stSoftMotionEX.vMotionUint.begin()->PlanningMotionParam.tar_ddddot;
            m_MotionUint_New.MoveType = enAbsoluteMotion;
            m_MotionUint = m_MotionUint_New;
            break;
        case EN_ModesOfOperation::enModeCyclicSyncVelocity:
            m_MotionUint_New.PlanningMotionParam.PlanningMode = EN_PlanningMode::enPositionPlanningMode;
            m_MotionUint_New.PlanningMotionParam.tar            = 0;
            m_MotionUint_New.PlanningMotionParam.tar_dot        = m_pCIA402Axis->m_stSoftMotionEX.vMotionUint.begin()->PlanningMotionParam.tar_dot;
            m_MotionUint_New.PlanningMotionParam.ptar_ddot      = m_pCIA402Axis->m_stSoftMotionEX.vMotionUint.begin()->PlanningMotionParam.ptar_ddot;
            m_MotionUint_New.PlanningMotionParam.ntar_ddot      = m_pCIA402Axis->m_stSoftMotionEX.vMotionUint.begin()->PlanningMotionParam.ntar_ddot;
            m_MotionUint_New.PlanningMotionParam.tar_dddot      = m_pCIA402Axis->m_stSoftMotionEX.vMotionUint.begin()->PlanningMotionParam.tar_dddot;
            m_MotionUint_New.PlanningMotionParam.tar_ddddot     = m_pCIA402Axis->m_stSoftMotionEX.vMotionUint.begin()->PlanningMotionParam.tar_ddddot;
            m_MotionUint_New.MoveType = enAbsoluteMotion;
            m_MotionUint = m_MotionUint_New;
            break;
        default:
            break;
        }
        m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState::motionState_stopping);
        m_pCIA402Axis->Axis_PushMotionUint(m_MotionUint);
    }
    //参数变更push
    if(m_MotionUint.PlanningMotionParam!=m_MotionUint_New.PlanningMotionParam)
    {
        m_MotionUint = m_MotionUint_New;
        m_pCIA402Axis->Axis_PushMotionUint(m_MotionUint);
    }
    ST_MotionUint FirstMotionUint;
    int nMotionNum = m_pCIA402Axis->Axis_GetMotionUint(FirstMotionUint);
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