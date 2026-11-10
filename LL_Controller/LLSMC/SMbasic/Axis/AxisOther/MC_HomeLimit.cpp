#include"MC_HomeLimit.h"
MC_HomeLimit::MC_HomeLimit(/* args */)
{
    m_bExecute =false;
    m_bBusy = false;
    m_bDone = false;
    m_bError = false;
    m_nErrorID = false;
}

MC_HomeLimit::~MC_HomeLimit()
{
}

void MC_HomeLimit::operator()(CIA402Axis* axis)
{
    this->Execute();
}
void MC_HomeLimit::operator()(CIA402Axis* axis,bool bExecute,bool &bBusy,bool &bCommandAborted,bool &bDone, bool &bError,int &nErrorID)
{
    this->operator()(axis,bExecute,0,bBusy,bCommandAborted,bDone,bError,nErrorID);
}

void MC_HomeLimit::operator()(CIA402Axis* axis,bool bExecute,double dPos,bool &bBusy,bool &bCommandAborted,bool &bDone, bool &bError,int &nErrorID)
{
    if(!bExecute)
    {
        m_fsHome = ReadyHomeLimit;
        m_bBusy = false;
        m_bError = true;
        m_bDone = false;
        m_nErrorID = SMEC_ABNORMAL_AXIS_CHANGE;
    }
    m_pCIA402Axis = axis;
    m_bExecute = bExecute;
    this->Execute();
    bBusy = m_bBusy;
    bDone = m_bDone;
    bError = m_bError;
    nErrorID = m_nErrorID;
}

void MC_HomeLimit::Execute()
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
    bool bUpperLimitIO = false;
    bool bLowerLimitIO = false;
    bool bExpandAxisHome = false;
    uint32_t nDigitalInputs = 0;
    m_pCIA402Axis->Axis_PDO_ReadDigitalInputs(nDigitalInputs);
    bUpperLimitIO = (bool)((nDigitalInputs & 0x400) >> 10);
    bLowerLimitIO = (bool)((nDigitalInputs & 0x800) >> 11);
    bExpandAxisHome = (bool)((nDigitalInputs & 0x1000) >> 12);
    double dCurTime =0;
    double dCmdPostion =0;
    int nRet = 0;

    m_bBusy = false;
    m_bError = false;
    m_bDone = false;
    m_nErrorID = SMEC_SUCCESSED;
    if(!m_bExecute)
    {
       m_Timer.R_TRIG(m_bExecute);
       m_fsHome = ReadyHomeLimit;
       return;
    }
    //检测复位中报错
    if(m_pCIA402Axis->Axis_CheckError())
    {
        m_fsHome = HomeLimitError;
        //反馈错误状态
        m_bError = true;
        m_Timer.Ton(false, SMC_TIME_OUT);
    } 
    //printf("=======MC_HomeLimit->m_fsHome = %d\n",m_fsHome);
    switch (m_fsHome)
    {
        case ReadyHomeLimit:
            if(m_Timer.R_TRIG(m_bExecute))
            {
                //检测轴是否为使能状态
                if(motionState_standstill != m_pCIA402Axis->Axis_ReadAxisState())
                {
                    //此状态不允许找零，也不切状态机，功能块置为error
                    m_fsHome = HomeLimitError;
                    //切换到错误状态
                    m_bError = true;
                    m_Timer.Ton(false, SMC_TIME_OUT);
                    break;
                }
                //切换轴状态
                m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState::motionState_homing);
                //切换到找零模式
                m_pCIA402Axis->Axis_PDO_SetModesOfOperation(EN_ModesOfOperation ::enModeCyclicSyncPosition);
                //虚拟轴找零完成
                if(m_pCIA402Axis->Axis_GetVirtualState())
                {
                    //反馈完成状态
                    m_bDone = true;
                    //同步状态机
                    m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState::motionState_standstill);
                    m_fsHome = HomeLimitFinish;
                    m_Timer.Ton(false, SMC_TIME_OUT);
                    break;
                }

                //设置找原目标位置、最大速度、加速度、加加速度
                m_stRTMotionPlanPointData->dStartTime = 0;
                m_stRTMotionPlanPointData->dMaxVel = 20;
                m_stRTMotionPlanPointData->dMaxAcc = 1000;
                m_stRTMotionPlanPointData->dJerk = 1000;
                m_stRTMotionPlanPointData->dEcatCycle = m_pCIA402Axis->m_dControlCycle;
                //printf("=======m_stRTMotionPlanPointData->dEcatCycle = %f\n",m_stRTMotionPlanPointData->dEcatCycle);

                m_stRTMotionPlanPointData->dStartPostion = m_pCIA402Axis->Axis_GetActualPosition();
                m_stRTMotionPlanPointData->dStartVelocity =0;
                m_stRTMotionPlanPointData->dTargetPostion = m_stRTMotionPlanPointData->dStartPostion - 2000;
                m_stRTMotionPlanPointData->dTargetVelocity = -20;
                m_stRTMotionPlanPointData->nMotionFrameCounts = 0;
                
                m_stRTMotionPlanPointData->stMotionFrame= s_curve_generator_RT(m_stRTMotionPlanPointData->dStartPostion,
                                            m_stRTMotionPlanPointData->dTargetPostion,
                                            m_stRTMotionPlanPointData->dStartVelocity,
                                            m_stRTMotionPlanPointData->dTargetVelocity, 
                                            m_stRTMotionPlanPointData->dMaxVel,
                                            m_stRTMotionPlanPointData->dMaxAcc,
                                            m_stRTMotionPlanPointData->dJerk,
                                            m_stRTMotionPlanPointData->dStartTime,
                                            m_stRTMotionPlanPointData->dEcatCycle,
                                            dCurTime);

                //初始化控制字
                m_fsHome = StartHomeLimit;
                m_bBusy = true;
                m_Timer.Ton(false, SMC_TIME_OUT);
                break;
            }
            break;
        case StartHomeLimit:
            m_bBusy = true;  
            if(bUpperLimitIO || bLowerLimitIO)
            {
                m_stRTMotionPlanPointData->dStartPostion = m_stRTMotionPlanPointData->stMotionFrame.q;
                m_stRTMotionPlanPointData->dStartVelocity = m_stRTMotionPlanPointData->stMotionFrame.v;
                printf("=======m_stRTMotionPlanPointData->dStartPostion = %f\n",m_pCIA402Axis->Axis_GetActualPosition());
                printf("=======m_stRTMotionPlanPointData->stMotionFrame.q = %f\n",m_stRTMotionPlanPointData->stMotionFrame.q);
                //printf("=======m_stRTMotionPlanPointData->dCmdVelocity = %f\n",m_stRTMotionPlanPointData->stMotionFrame.v);
                m_stRTMotionPlanPointData->dTargetPostion = m_stRTMotionPlanPointData->dStartPostion + 2000;
                m_stRTMotionPlanPointData->dTargetVelocity = 20;
                m_stRTMotionPlanPointData->nMotionFrameCounts = 0;
                m_fsHome = LimitIOTrigger;
                break;
            }


            //调用S型速度点位规划接口，获取点位列表
            dCurTime = m_stRTMotionPlanPointData->dStartTime + m_stRTMotionPlanPointData->dEcatCycle * m_stRTMotionPlanPointData->nMotionFrameCounts;
            m_stRTMotionPlanPointData->stMotionFrame= s_curve_generator_RT(m_stRTMotionPlanPointData->dStartPostion,
                                        m_stRTMotionPlanPointData->dTargetPostion,
                                        m_stRTMotionPlanPointData->dStartVelocity,
                                        m_stRTMotionPlanPointData->dTargetVelocity, 
                                        m_stRTMotionPlanPointData->dMaxVel,
                                        m_stRTMotionPlanPointData->dMaxAcc,
                                        m_stRTMotionPlanPointData->dJerk,
                                        m_stRTMotionPlanPointData->dStartTime,
                                        m_stRTMotionPlanPointData->dEcatCycle,
                                        dCurTime);
            //printf("StartHomeVel=======m_stRTMotionPlanPointData->stMotionFrame.q = %f\n",m_stRTMotionPlanPointData->stMotionFrame.q);
            //printf("StartHomeVel=======dActPosition = %f\n",this->dActPosition);
            //printf("StartHomeVel=======m_stRTMotionPlanPointData->stMotionFrame.v = %f\n",m_stRTMotionPlanPointData->stMotionFrame.v);
            m_stRTMotionPlanPointData->nMotionFrameCounts ++;

            dCmdPostion=m_stRTMotionPlanPointData->stMotionFrame.q;
            //printf("MC_SetTargetPosition = %f\n",dCmdPostion);
            m_pCIA402Axis->Axis_SetTargetPosition(dCmdPostion);
            break;
        case LimitIOTrigger:
            m_bBusy = true;
            if(bExpandAxisHome)
            {
                m_stRTMotionPlanPointData->dStartPostion = m_stRTMotionPlanPointData->stMotionFrame.q;
                m_stRTMotionPlanPointData->dStartVelocity = m_stRTMotionPlanPointData->stMotionFrame.v;
                printf("=======m_stRTMotionPlanPointData->dStartPostion = %f\n",m_pCIA402Axis->Axis_GetActualPosition());
                printf("=======m_stRTMotionPlanPointData->stMotionFrame.q = %f\n",m_stRTMotionPlanPointData->stMotionFrame.q);
                //printf("========m_stRTMotionPlanPointData->dCmdVelocity = %f\n",m_stRTMotionPlanPointData->stMotionFrame.v);
                m_stRTMotionPlanPointData->dTargetPostion = m_stRTMotionPlanPointData->dStartPostion + 5;
                m_stRTMotionPlanPointData->dTargetVelocity = 0;
                m_stRTMotionPlanPointData->nMotionFrameCounts = 0;
                m_fsHome = HomeIOTrigger;
                break;
            }

            //调用S型速度点位规划接口，获取点位列表
            dCurTime = m_stRTMotionPlanPointData->dStartTime + m_stRTMotionPlanPointData->dEcatCycle * m_stRTMotionPlanPointData->nMotionFrameCounts;
            m_stRTMotionPlanPointData->stMotionFrame= s_curve_generator_RT(m_stRTMotionPlanPointData->dStartPostion,
                                        m_stRTMotionPlanPointData->dTargetPostion,
                                        m_stRTMotionPlanPointData->dStartVelocity,
                                        m_stRTMotionPlanPointData->dTargetVelocity, 
                                        m_stRTMotionPlanPointData->dMaxVel,
                                        m_stRTMotionPlanPointData->dMaxAcc,
                                        m_stRTMotionPlanPointData->dJerk,
                                        m_stRTMotionPlanPointData->dStartTime,
                                        m_stRTMotionPlanPointData->dEcatCycle,
                                        dCurTime);
            m_stRTMotionPlanPointData->nMotionFrameCounts ++;

            dCmdPostion=m_stRTMotionPlanPointData->stMotionFrame.q;
            //printf("MC_SetTargetPosition = %f\n",dCmdPostion);
            //printf("LowerIOTrigger:dActPosition = %f\n",this->dActPosition);
            //printf("LowerIOTrigger:_stRTMotionPlanPointData.stMotionFrame.v = %f\n",m_stRTMotionPlanPointData->stMotionFrame.v);
            //printf("LowerIOTrigger:_stRTMotionPlanPointData.stMotionFrame.q = %f\n",m_stRTMotionPlanPointData->stMotionFrame.q);
            m_pCIA402Axis->Axis_SetTargetPosition(dCmdPostion);
            break;
        case HomeIOTrigger:
            m_bBusy = true;
            if(m_stRTMotionPlanPointData->stMotionFrame.r_t < 0)
            {
                m_stRTMotionPlanPointData->nMotionFrameCounts = 0;
                double dCurrentPostion = m_pCIA402Axis->Axis_GetActualPosition();
                printf("MC_SetPosition(dCurrentPostion) = %f\n",dCurrentPostion);
                m_pCIA402Axis->Axis_SetEncodeHomePos(m_pCIA402Axis->Axis_GetActualPositionPDO());
                m_fsHome = HomeLimitFinish;
                break;
            }

            //调用S型速度点位规划接口，获取点位列表
            dCurTime = m_stRTMotionPlanPointData->dStartTime + m_stRTMotionPlanPointData->dEcatCycle * m_stRTMotionPlanPointData->nMotionFrameCounts;
            m_stRTMotionPlanPointData->stMotionFrame= s_curve_generator_RT(m_stRTMotionPlanPointData->dStartPostion,
                                        m_stRTMotionPlanPointData->dTargetPostion,
                                        m_stRTMotionPlanPointData->dStartVelocity,
                                        m_stRTMotionPlanPointData->dTargetVelocity, 
                                        m_stRTMotionPlanPointData->dMaxVel,
                                        m_stRTMotionPlanPointData->dMaxAcc,
                                        m_stRTMotionPlanPointData->dJerk,
                                        m_stRTMotionPlanPointData->dStartTime,
                                        m_stRTMotionPlanPointData->dEcatCycle,
                                        dCurTime);
            m_stRTMotionPlanPointData->nMotionFrameCounts ++;

            dCmdPostion=m_stRTMotionPlanPointData->stMotionFrame.q;
            //printf("MC_SetTargetPosition = %f\n",dCmdPostion);
            //printf("HomeIOTrigger:m_stRTMotionPlanPointData->dEcatCycle = %f\n",m_stRTMotionPlanPointData->dEcatCycle);
            //printf("HomeIOTrigger:dCurTime = %f\n",dCurTime);
            //printf("HomeIOTrigger:dActPosition = %f\n",this->dActPosition);
            //printf("HomeIOTrigger:_stRTMotionPlanPointData.stMotionFrame.v = %f\n",m_stRTMotionPlanPointData->stMotionFrame.v);
            //printf("HomeIOTrigger:_stRTMotionPlanPointData.stMotionFrame.q = %f\n",m_stRTMotionPlanPointData->stMotionFrame.q);
             m_pCIA402Axis->Axis_SetTargetPosition(dCmdPostion);
            break;
        case HomeLimitFinish:
            m_bDone = true;
            m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState::motionState_standstill);
            m_fsHome = ReadyHomeLimit;
            break;
        case HomeLimitError:
            m_bError = true;
            if(m_Timer.F_TRIG(m_bExecute))
            {
                m_nErrorID = SMEC_SUCCESSED;
                m_fsHome = ReadyHomeLimit;
                break;
            }
            break;
        default:
            break;
    }
}