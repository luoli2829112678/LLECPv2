#include"SoftMotion.h"
void SoftMotion::SoftMotionPlanner_RT()
{
    for(size_t i = 0;i<m_v_Axis.size();i++)
    {
        AxisMotionPlanner(m_v_Axis[i]);
        SoftMotionStateCheck();
    }
    return;
}



int SoftMotion::AxisMotionPlanner(CIA402Axis* pAxis)
{
    //状态校验
    if((pAxis->Axis_ReadAxisState()!=EN_AxisMotionState::motionState_standstill)
            &&(pAxis->Axis_ReadAxisState()!=EN_AxisMotionState::motionState_discrete_motion)
            &&(pAxis->Axis_ReadAxisState()!=EN_AxisMotionState::motionState_continuous_motion)
            &&(pAxis->Axis_ReadAxisState()!=EN_AxisMotionState::motionState_stopping))
            {
                bool bError = (pAxis->Axis_ReadAxisState()==EN_AxisMotionState::motionState_errorstop);
                for(size_t i = 0;i<pAxis->m_stSoftMotionEX.vMotionUint.size();i++)
                {
                    if(bError)//轴在错误状态不支持此操作
                        pAxis->m_stSoftMotionEX.vMotionUint[i].SetFBStauts(false,false,false,true,pAxis->m_nErrorCode);
                    else//正在进行其他运行不支持此运动
                        pAxis->m_stSoftMotionEX.vMotionUint[i].SetFBStauts(false,false,false,true,SMEC_AXIS_STATUS_INTERCEPTION);
                }
                pAxis->m_stSoftMotionEX.vMotionUint.clear();
                return SMEC_SUCCESSED;
            }
    double T;
    if(pAxis->m_stSoftMotionEX.vMotionUint.size() == 0)
    {
        return SMEC_SUCCESSED;
    }
    auto itMotionData = pAxis->m_stSoftMotionEX.vMotionUint.begin();
    while(itMotionData->bMotionDone)
    {
        itMotionData ++;
        //所有点位运动完成
        if(pAxis->m_stSoftMotionEX.vMotionUint.end() == itMotionData)
        {
            //切换状态
            pAxis->Axis_SoftMotionRunDone();
            return SMEC_SUCCESSED;
        }
    }
    //当前迭代器内是正在运动的点位
    ST_PlanningMotionParam stMotionParam = itMotionData->PlanningMotionParam;
    //正在运行当前点位
    if(!pAxis->m_stSoftMotionEX.bReplan)
    {
        //通用规划
        switch (stMotionParam.PlanningMode)
        {
        case EN_PlanningMode::enPositionPlanningMode:
        case EN_PlanningMode::enVelocityPlanningMode:
                //得到当前帧的运动参数
                FifteenSeg_Inter(m_vSoftMotionPlanParams[pAxis->m_nAxisID].stActParam,m_vSoftMotionPlanParams[pAxis->m_nAxisID].trackData,
                                pAxis->m_stSoftMotionEX.dMotionTime,m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData);
                #if DEBUG_TRAJECTORY_PLANNING
                printf("T:%F------Time:%f--------AxisPos:%f---vel:%f\n",T,pAxis->m_stSoftMotionEX.dMotionTime,m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData.P,
                   m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData.V);
                #endif
            break;
        case EN_PlanningMode::enStopPlanningMode:
                stopmotion(stMotionParam.tar,stMotionParam.tar_dot,stMotionParam.ptar_ddot,stMotionParam.ntar_ddot,stMotionParam.tar_dddot,pAxis->m_stSoftMotionEX.dMotionTime,T,m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData);
                #if DEBUG_TRAJECTORY_PLANNING
                printf("StopT:%F------Time:%f--------AxisPos:%f---vel:%f\n",T,pAxis->m_stSoftMotionEX.dMotionTime,m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData.P,
                   m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData.V);
                #endif
                break;     
        default:
            break;
        }
        if (std::isnan(m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData.P)) 
        {
            printf("!!!!!!!!!ERROR!!!!!!!!!!!\n");
            printf("Pos:%f,vel:%f,dSetAcceleration_s:%f,dec:%f,jerk:%f,dMotionTime:%f\n",stMotionParam.tar,stMotionParam.tar_dot,pAxis->Axis_GetSetAcceleration(),stMotionParam.ntar_ddot,stMotionParam.tar_dddot,pAxis->m_stSoftMotionEX.dMotionTime);
            itMotionData->SetFBStauts(false,false,false,true,SMEC_ALGORITHM_PLANNING_ERROR);
            pAxis->CleanSoftMotionData();
            pAxis->Axis_SetAxisState(EN_AxisMotionState::motionState_standstill);
            return SMEC_ALGORITHM_PLANNING_ERROR;
        }
        //设置运行状态
        itMotionData->SetFBStauts(true,false,false,false,SMEC_SUCCESSED);
        //开始运动 
        pAxis->Axis_SetMotionPlanner(m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData);
        //时间帧加
        pAxis->m_stSoftMotionEX.dMotionTime += m_dSoftMotionCycle;
    }
    else//新点位
    {
        pAxis->m_stSoftMotionEX.bReplan = false;
        ST_PlanningMotionParam stMotionParam_Run = stMotionParam;
        ST_PlanParams stsetParam;
        ST_InterParams stInitialParameters;
        ST_PlanParams stNewstActParam;
        m_vSoftMotionPlanParams[pAxis->m_nAxisID].stActParam = stNewstActParam;
        switch (stMotionParam_Run.PlanningMode)
        {
        case EN_PlanningMode::enPositionPlanningMode:
        case EN_PlanningMode::enVelocityPlanningMode:
            //绝对运动做偏移
            if(enRelativMotion == itMotionData->MoveType)
            {
                stMotionParam_Run.tar = pAxis->Axis_GetSetPosition() + stMotionParam_Run.tar;
            }
            pAxis->m_stSoftMotionEX.stSoftMotionMotionParam = stMotionParam;
            pAxis->m_stSoftMotionEX.dMotionTime = 0;
            //初始位置只能使用命令位置，否则会因为位置跟随误差过大导致往回走飞车
            pAxis->Axis_GetMotionInitialParameters(stInitialParameters);
            stsetParam.q0 = stInitialParameters.P;
            stsetParam.q1 = stMotionParam_Run.tar;
            stsetParam.v0 = stInitialParameters.V;
            stsetParam.v1 = 0;
            stsetParam.a0 = 0;
            stsetParam.a1 = 0;
            stsetParam.V_max = stMotionParam_Run.tar_dot *  pAxis->m_stSoftMotionEX.m_stFactor.dVelFactor;
            stsetParam.A_amax = stMotionParam_Run.ptar_ddot *  pAxis->m_stSoftMotionEX.m_stFactor.dAccFactor;
            stsetParam.A_dmax = stMotionParam_Run.ntar_ddot *  pAxis->m_stSoftMotionEX.m_stFactor.dJerkFactor;
            stsetParam.J_max = stMotionParam_Run.tar_dddot;
            stsetParam.S_max = stMotionParam_Run.tar_ddddot;
            stsetParam.enPlanmode = stMotionParam_Run.PlanningMode;
            //无运动检测
            if((abs(stsetParam.q0 - stsetParam.q1)<0.001)&&(abs(stsetParam.v0 - stsetParam.v1)<0.001))
            {
                pAxis->m_stInterParamsData.P = stsetParam.q0;
                pAxis->m_stInterParamsData.V = stsetParam.v0;
                pAxis->m_stInterParamsData.A = stsetParam.v0;
                itMotionData->SetFBStauts(false,true,false,false,SMEC_SUCCESSED);
                return SMEC_SUCCESSED;
            }
            FifteenSeg_plan(stsetParam,m_vSoftMotionPlanParams[pAxis->m_nAxisID].stActParam,m_vSoftMotionPlanParams[pAxis->m_nAxisID].trackData);
            //得到当前帧的运动参数
            FifteenSeg_Inter(m_vSoftMotionPlanParams[pAxis->m_nAxisID].stActParam,m_vSoftMotionPlanParams[pAxis->m_nAxisID].trackData,
                            pAxis->m_stSoftMotionEX.dMotionTime,m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData);
            #if DEBUG_TRAJECTORY_PLANNING
            printf("T:%F------Time:%f--------AxisPos:%f---vel:%f\n",T,pAxis->m_stSoftMotionEX.dMotionTime,m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData.P,
                    m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData.V);
            #endif
            break;
        case EN_PlanningMode::enStopPlanningMode:
            //初始位置只能使用命令位置，否则会因为位置跟随误差过大导致往回走飞车
            //使用停止默认之前是使用softmotion进行了运动
            stMotionParam.tar = pAxis->m_stInterParamsData.P;
            stMotionParam.tar_dot = pAxis->m_stInterParamsData.V;
            stMotionParam.ptar_ddot =  pAxis->m_stInterParamsData.A;
            pAxis->m_stSoftMotionEX.stSoftMotionMotionParam = stMotionParam;
            itMotionData->PlanningMotionParam = stMotionParam;
            pAxis->m_stSoftMotionEX.dMotionTime = 0;
            pAxis->m_stSoftMotionEX.dMotionTime += m_dSoftMotionCycle;
            stopmotion(stMotionParam.tar,
                stMotionParam.tar_dot,
                stMotionParam.ptar_ddot,
                stMotionParam.ntar_ddot,
                stMotionParam.tar_dddot,
                pAxis->m_stSoftMotionEX.dMotionTime,
                T,
                m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData);
            #if DEBUG_TRAJECTORY_PLANNING
            printf("stopT:%F------Time:%f--------AxisPos:%f---vel:%f\n",T,pAxis->m_stSoftMotionEX.dMotionTime,m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData.P,
                m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData.V);
            #endif
            m_vSoftMotionPlanParams[pAxis->m_nAxisID].stActParam,m_vSoftMotionPlanParams[pAxis->m_nAxisID].trackData.T = T;
            break;
        default:
            break;
        }
        if (std::isnan(m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData.P)) 
        {
            printf("!!!!!!!!!ERROR!!!!!!!!!!!\n");
            printf("Pos:%f,vel:%f,dSetAcceleration_s:%f,dec:%f,jerk:%f,dMotionTime:%f\n",stMotionParam.tar,stMotionParam.tar_dot,pAxis->Axis_GetSetAcceleration(),stMotionParam.ntar_ddot,stMotionParam.tar_dddot,pAxis->m_stSoftMotionEX.dMotionTime);
            itMotionData->SetFBStauts(false,false,false,true,SMEC_ALGORITHM_PLANNING_ERROR);
            pAxis->CleanSoftMotionData();
            pAxis->Axis_SetAxisState(EN_AxisMotionState::motionState_standstill);
            return SMEC_ALGORITHM_PLANNING_ERROR;
        }
        //开始运动 
        pAxis->Axis_SetMotionPlanner(m_vSoftMotionPlanParams[pAxis->m_nAxisID].stInterData);
        
        //设置为运行状态
        itMotionData->SetFBStauts(true,false,false,false,SMEC_SUCCESSED);
        //时间帧加
        pAxis->m_stSoftMotionEX.dMotionTime += m_dSoftMotionCycle;
    }


    //运动完成检测
    if((enPositionPlanningMode == itMotionData->PlanningMotionParam.PlanningMode)||(enStopPlanningMode == itMotionData->PlanningMotionParam.PlanningMode))
    {
        //运动总时间大于规划帧时间
        if(pAxis->m_stSoftMotionEX.dMotionTime > m_vSoftMotionPlanParams[pAxis->m_nAxisID].trackData.T)
        {
            itMotionData->SetFBStauts(false,true,false,false,SMEC_SUCCESSED);
        }
    }
    return SMEC_SUCCESSED;
}