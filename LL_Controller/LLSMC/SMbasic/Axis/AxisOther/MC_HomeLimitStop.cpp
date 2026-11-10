#include"MC_HomeLimitStop.h"
MC_HomeLimitStop::MC_HomeLimitStop(/* args */)
{
}

MC_HomeLimitStop::~MC_HomeLimitStop()
{
}

void MC_HomeLimitStop::operator()(CIA402Axis* axis)
{
    this->Execute();
}

void MC_HomeLimitStop::CheckRunStatus()
{
    //运动完成监测
    if((m_bDone != m_bDone_last)&&(!m_bDone_last))
    {
        m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState ::motionState_standstill);
    }
    //错误监测
}

void MC_HomeLimitStop::operator()(CIA402Axis* axis,bool bExecute,double dDeceleration,double dJerk,
                    bool& bDone,bool& bBusy,bool& bCommandAborted,bool& bError,int& ErrorID)
{
    m_pCIA402Axis                                   = axis;
    m_bExecute                                      = bExecute;
    this->Execute();
    bDone = m_bDone;
    bBusy = m_bBusy;
    m_dDeceleration = dDeceleration;
    m_dJerk = dJerk;
    bCommandAborted = m_bCommandAborted;
    bError = m_bError;
    ErrorID = m_nErrorID;
}

void MC_HomeLimitStop::Execute()
{
    if(nullptr == m_pCIA402Axis)
    {
        m_bError = true;
        m_nErrorID = SMEC_INVALID_AXIS;
        return;
    }
    if(nullptr == m_stRTMotionPlanPointData)
    {
        m_stRTMotionPlanPointData = &m_pCIA402Axis->m_stRTMotionPlanPointData;
    }
    int nRet = SMEC_SUCCESSED;
    double dCurTime =0;
    double dCmdPostion =0;
    m_bBusy = false;
    m_bError = false;
    m_bDone = false;
    m_nErrorID = SMEC_SUCCESSED;
    if(!m_bExecute)
    {
       m_Timer.R_TRIG(m_bExecute);
       m_fsHome = ReadyHomeLimitStop;
       return;
    }
    //检测复位中报错
    uint16_t unStatuBit;
    if(m_pCIA402Axis->Axis_CheckError())
    {
        m_fsHome = HomeLimitStopError;
        //反馈错误状态
        m_bError = true;
        m_Timer.Ton(false, SMC_TIME_OUT);
    } 
    switch (m_fsHome)
    {
        case ReadyHomeLimitStop:
            if(m_Timer.R_TRIG(m_bExecute))
            {
                //检测轴是否为使能状态
                if(motionState_standstill != m_pCIA402Axis->Axis_ReadAxisState())
                {
                    //此状态不允许找零，也不切状态机，功能块置为error
                    m_fsHome = HomeLimitStopError;
                    //切换到错误状态
                    m_bError = true;
                    m_Timer.Ton(false, SMC_TIME_OUT);
                    break;
                }

                //虚拟轴找零完成
                if(m_pCIA402Axis->Axis_GetVirtualState())
                {
                    //反馈完成状态
                    m_bDone = true;
                    //同步状态机
                    m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState::motionState_standstill);
                    m_fsHome = HomeLimitStopFinish;
                    m_Timer.Ton(false, SMC_TIME_OUT);
                    break;
                }

                //设置目标位置、最大速度、加速度、加加速度
                m_stRTMotionPlanPointData->dMaxAcc = 1000;
                m_stRTMotionPlanPointData->dJerk = 1000;
                m_stRTMotionPlanPointData->dEcatCycle = m_pCIA402Axis->m_dControlCycle;

                m_stRTMotionPlanPointData->dStartPostion = m_stRTMotionPlanPointData->stMotionFrame.q;
                m_stRTMotionPlanPointData->dStartVelocity =m_stRTMotionPlanPointData->stMotionFrame.v;
                m_stRTMotionPlanPointData->nMotionFrameCounts = 0;

                nRet = stopmotion(m_stRTMotionPlanPointData->dStartPostion,
                    m_stRTMotionPlanPointData->dStartVelocity,
                    0, 
                    m_stRTMotionPlanPointData->dMaxAcc,
                    m_stRTMotionPlanPointData->dJerk,
                    dCurTime,
                    m_stRTMotionPlanPointData->dStopMotionTime,
                    m_stRTMotionPlanPointData->stStopMotionFrame);
                break;

                //初始化控制字
                m_fsHome = StartHomeLimitStop;
                m_bBusy = true;
                m_Timer.Ton(false, SMC_TIME_OUT);
                break;
            }
            break;
        case StartHomeLimitStop:
            m_bBusy = true;  
            m_stRTMotionPlanPointData->nMotionFrameCounts ++;
            dCurTime = m_stRTMotionPlanPointData->dEcatCycle * m_stRTMotionPlanPointData->nMotionFrameCounts;
            //printf("MC_SetTargetPosition = %f\n",dCmdPostion);
            if(dCurTime > m_stRTMotionPlanPointData->dStopMotionTime)
            {
                m_stRTMotionPlanPointData->nMotionFrameCounts = 0;
                double dCurrentPostion = m_pCIA402Axis->Axis_GetActualPosition();
                //printf("MC_SetPosition(dCurrentPostion) = %f\n",dCurrentPostion);
                m_fsHome = HomeLimitStopFinish;
                break;
            }
            //printf("\n");
            //printf("--------------------------------------------------------------------------------");
            //printf("HomeStop: m_stRTMotionPlanPointData->dStartPostion = %f\n",m_stRTMotionPlanPointData->dStartPostion);
            //printf("HomeStop: m_stRTMotionPlanPointData->dStartVelocity = %f\n",m_stRTMotionPlanPointData->dStartVelocity);
            //printf("HomeStop: m_stRTMotionPlanPointData->dMaxAcc = %f\n",m_stRTMotionPlanPointData->dMaxAcc);
            //printf("HomeStop: m_stRTMotionPlanPointData->dJerk = %f\n",m_stRTMotionPlanPointData->dJerk);
            //printf("HomeStop: dCurTime = %f\n",dCurTime);
            nRet = stopmotion(m_stRTMotionPlanPointData->dStartPostion,
                m_stRTMotionPlanPointData->dStartVelocity,
                0, 
                m_stRTMotionPlanPointData->dMaxAcc,
                m_stRTMotionPlanPointData->dJerk,
                dCurTime,
                m_stRTMotionPlanPointData->dStopMotionTime,
                m_stRTMotionPlanPointData->stStopMotionFrame);
            //调用S型速度点位规划接口，获取点位列表
            dCmdPostion=m_stRTMotionPlanPointData->stStopMotionFrame.P;
            m_pCIA402Axis->Axis_SetTargetPosition(dCmdPostion);

            break;
        case HomeLimitStopFinish:
            m_bDone = true;
            m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState::motionState_standstill);
            m_fsHome = ReadyHomeLimitStop;
            break;
        case HomeLimitStopError:
            m_bError = true;
            if(m_Timer.F_TRIG(m_bExecute))
            {
                m_nErrorID = SMEC_SUCCESSED;
                m_fsHome = ReadyHomeLimitStop;
                break;
            }
            break;
        default:
            break;
    }
}
