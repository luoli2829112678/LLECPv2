#include "LLC_Rbt.h"
#define TimeOut 5000*1000

SMbasic* LLC_Rbt::AxisPTMove(int i, bool bExecute, double dPosition, double dVelocity, double dAcceleration, double dDeceleration, double dJerk, EN_Direction enDirection, EN_BufferMode enBufferMode, bool &bDone, bool &bBusy, bool &bCommandAborted, bool &bError, int & nErrorID)
{
    switch (m_vAxis[i]->Axis_GetActualOperationMode())
    {
    case EN_ModesOfOperation::enModeCyclicSyncPosition:
        m_FB_AxisAbsMove[i](m_vAxis[i],bExecute,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                            m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
        return &m_FB_AxisAbsMove[i];
    case EN_ModesOfOperation::enModeCyclicSyncVelocity:
        m_FB_AxisMoveVelocity[i](m_vAxis[i],bExecute,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                            m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
        return &m_FB_AxisMoveVelocity[i];
    case EN_ModesOfOperation::enModeCyclicSyncTorque:
        m_FB_AxisMoveTorque[i](m_vAxis[i],bExecute,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                            m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
        return &m_FB_AxisMoveTorque[i];
    default:
        printf("AxisPTMoveError!\n");
        break;
    }
    return nullptr;
}

void LLC_Rbt::AxisControlFSM()
{
    AxisInitResetFSM();
    AxisEnableFSM();
    AxisDisableFSM();
    AxisResetFSM();
    AxisHomeFSM();
    AxisStopFSM();
    AxisAbsMoveFSM();
    AxisRelMoveFSM();
    AxisBackAndForthMoveFSM();
    AxisContinuousStepMoveFSM();
    AxisVelocityMove();
    return;
}





void LLC_Rbt::AxisInitResetFSM()
{
    bool bBusy = false;
    bool bDone = false;
    bool bError = false;
    int nErrorID;
    for (uint32_t i = 0; i < m_nConfiguredAxisNum; i++)
    {
        switch (m_enAxisInitResetFSM[i])
        {
        case EN_AxisControlFSM::enAxisControlFSM_Null:
            if(bAxisInitResetCmd[i])
            {
                bAxisInitResetCmd[i] = false;
                m_FB_AxisInitReset[i](m_vAxis[i],false,bBusy,bDone,bError,nErrorID);
                m_TonInitReset[i].Ton(false,TimeOut);
                m_enAxisInitResetFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Execute;
            }
            break;
        case EN_AxisControlFSM::enAxisControlFSM_Execute:
            m_FB_AxisInitReset[i](m_vAxis[i],true,bBusy,bDone,bError,nErrorID);
            if(bDone)
            {
                m_FB_AxisInitReset[i](m_vAxis[i],false,bBusy,bDone,bError,nErrorID);
                m_TonInitReset[i].Ton(false,TimeOut);
                m_enAxisInitResetFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
                break;
            }
            //报错不break，直接滑向下一个状态机
            if(m_TonInitReset[i].Ton(true,TimeOut) || bError)
            {
                m_FB_AxisInitReset[i](m_vAxis[i],false,bBusy,bDone,bError,nErrorID);
                m_TonInitReset[i].Ton(false,TimeOut);
                m_enAxisInitResetFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Error;
            }
            else
            {
                break;
            }
        case EN_AxisControlFSM::enAxisControlFSM_Error:
            m_stRobotControlStatus.bStatus_AxisError = true;
            m_stShmRobotRTStatus.nErrorAxisID = i;
            if(bError)
            {
                m_stShmRobotRTStatus.nErrorID = nErrorID;
            }
            else
            {
                m_stShmRobotRTStatus.nErrorID = REC_ExecResetStatusError;
            }
            m_enAxisInitResetFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
            break;
        default:
            break;
        }
    }
}



void LLC_Rbt::AxisEnableFSM()
{
    bool bBusy = false;
    bool bDone = false;
    bool bError = false;
    int nErrorID;
    for (uint32_t i = 0; i < m_nConfiguredAxisNum; i++)
    {
        switch (m_enAxisEnableFSM[i])
        {
        case EN_AxisControlFSM::enAxisControlFSM_Null:
            if(bAxisEnableCmd[i])
            {
                bAxisEnableCmd[i] = false;
                m_FB_AxisPowerOn[i](m_vAxis[i],false,bBusy,bDone,bError,nErrorID);
                m_TonEnable[i].Ton(false,TimeOut);
                m_enAxisEnableFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Execute;
            }
            break;
        case EN_AxisControlFSM::enAxisControlFSM_Execute:
            m_FB_AxisPowerOn[i](m_vAxis[i],true,bBusy,bDone,bError,nErrorID);
            if(bDone)
            {
                m_FB_AxisPowerOn[i](m_vAxis[i],false,bBusy,bDone,bError,nErrorID);
                m_TonEnable[i].Ton(false,TimeOut);
                m_enAxisEnableFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
                break;
            }
            //报错不break，直接滑向下一个状态机
            if(m_TonEnable[i].Ton(true,TimeOut) || bError)
            {
                m_FB_AxisPowerOn[i](m_vAxis[i],false,bBusy,bDone,bError,nErrorID);
                m_TonEnable[i].Ton(false,TimeOut);
                m_enAxisEnableFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Error;
            }
            else
            {
                break;
            }
        case EN_AxisControlFSM::enAxisControlFSM_Error:
            m_stRobotControlStatus.bStatus_AxisError = true;
            m_stShmRobotRTStatus.nErrorAxisID = i;
            if(bError)
            {
                m_stShmRobotRTStatus.nErrorID = nErrorID;
            }
            else
            {
                m_stShmRobotRTStatus.nErrorID = REC_ExecEnablingTimeOutError;
            }
            m_enAxisEnableFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
            break;
        default:
            break;
        }
    }
}

void LLC_Rbt::AxisDisableFSM()
{
    bool bBusy = false;
    bool bDone = false;
    bool bError = false;
    int nErrorID;
    for (uint32_t i = 0; i < m_nConfiguredAxisNum; i++)
    {
        switch (m_enAxisDisableFSM[i])
        {
        case EN_AxisControlFSM::enAxisControlFSM_Null:
            if(bAxisDisableCmd[i])
            {
                bAxisDisableCmd[i] = false;
                m_FB_AxisPowerOff[i](m_vAxis[i],false,bBusy,bDone,bError,nErrorID);
                m_TonDisable[i].Ton(false,TimeOut);
                m_enAxisDisableFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Execute;
            }
            break;
        case EN_AxisControlFSM::enAxisControlFSM_Execute:
            m_FB_AxisPowerOff[i](m_vAxis[i],true,bBusy,bDone,bError,nErrorID);
            if(bDone)
            {
                m_FB_AxisPowerOff[i](m_vAxis[i],false,bBusy,bDone,bError,nErrorID);
                m_TonDisable[i].Ton(false,TimeOut);
                m_enAxisDisableFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
                break;
            }
            //报错不break，直接滑向下一个状态机
            if(m_TonDisable[i].Ton(true,TimeOut) || bError)
            {
                m_FB_AxisPowerOff[i](m_vAxis[i],false,bBusy,bDone,bError,nErrorID);
                m_TonDisable[i].Ton(false,TimeOut);
                m_enAxisDisableFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Error;
            }
            else
            {
                break;
            }
        case EN_AxisControlFSM::enAxisControlFSM_Error:
            m_stRobotControlStatus.bStatus_AxisError = true;
            m_stShmRobotRTStatus.nErrorAxisID = i;
            if(bError)
            {
                m_stShmRobotRTStatus.nErrorID = nErrorID;
            }
            else
            {
                m_stShmRobotRTStatus.nErrorID = REC_ExecDisablingTimeOutError;
            }
            m_enAxisDisableFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
            break;
        default:
            break;
        }
    }
}

void LLC_Rbt::AxisResetFSM()
{
    bool bBusy = false;
    bool bDone = false;
    bool bError = false;
    int nErrorID;
    for (uint32_t i = 0; i < m_nConfiguredAxisNum; i++)
    {
        switch (m_enAxisResetFSM[i])
        {
        case EN_AxisControlFSM::enAxisControlFSM_Null:
            if(bAxisResetCmd[i])
            {
                bAxisResetCmd[i] = false;
                m_FB_AxisReset[i](m_vAxis[i],false,bBusy,bDone,bError,nErrorID);
                m_TonReset[i].Ton(false,TimeOut);
                m_enAxisResetFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Execute;
            }
            break;
        case EN_AxisControlFSM::enAxisControlFSM_Execute:
            //printf("nCmdControlWord:%d\n",m_vAxis[i].nCmdControlWord);
            m_FB_AxisReset[i](m_vAxis[i],true,bBusy,bDone,bError,nErrorID);
            if(bDone)
            {
                bool anyError=false;
                for(auto* axis:m_vAxis)if(axis && (axis->Axis_GetErrorState()||axis->Axis_GetAxisMotionState()==motionState_errorstop))anyError=true;
                m_stRobotControlStatus.bStatus_AxisError=anyError;
                if(!anyError && m_stShmRobotRTStatus.nErrorAxisID==i)m_stShmRobotRTStatus.nErrorID=0;

                m_FB_AxisReset[i](m_vAxis[i],false,bBusy,bDone,bError,nErrorID);
                m_TonReset[i].Ton(false,TimeOut);
                m_enAxisResetFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
                break;
            }
            //报错不break，直接滑向下一个状态机
            if(m_TonReset[i].Ton(true,TimeOut) || bError)
            {
                m_FB_AxisReset[i](m_vAxis[i],true,bBusy,bDone,bError,nErrorID);
                m_FB_AxisReset[i](m_vAxis[i],false,bBusy,bDone,bError,nErrorID);
                m_TonReset[i].Ton(false,TimeOut);
                m_enAxisResetFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Error;
            }
            else
            {
                break;
            }
        case EN_AxisControlFSM::enAxisControlFSM_Error:
            m_stRobotControlStatus.bStatus_AxisError = true;
            m_stShmRobotRTStatus.nErrorAxisID = i;
            if(bError)
            {
                m_stShmRobotRTStatus.nErrorID = nErrorID;
            }
            else
            {
                m_stShmRobotRTStatus.nErrorID = REC_ExecResetStatusError;
            }
            m_enAxisResetFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
            break;
        default:
            break;
        }
    }
}

void LLC_Rbt::AxisStopFSM()
{
    bool bBusy = false;
    bool bDone = false;
    bool bError = false;
    int nErrorID;
    bool bCommandAborted = false;
    for (uint32_t i = 0; i < m_nConfiguredAxisNum; i++)
    {
        switch (m_enAxisStopFSM[i])
        {
        case EN_AxisControlFSM::enAxisControlFSM_Null:
            if(bAxisStopCmd[i])
            {
                bAxisStopCmd[i] = false;
                m_FB_AxisStop[i](m_vAxis[i],false,m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,bDone,bBusy,bCommandAborted,bError,nErrorID);
                m_enAxisStopFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Execute;
            }
            break;
        case EN_AxisControlFSM::enAxisControlFSM_Execute:
            m_FB_AxisStop[i](m_vAxis[i],true,m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,bDone,bBusy,bCommandAborted,bError,nErrorID);
            if(m_FB_AxisStop[i].Done())
            {
                m_FB_AxisStop[i](m_vAxis[i],false,m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,bDone,bBusy,bCommandAborted,bError,nErrorID);
                m_enAxisStopFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
                break;
            }
            //报错不break，直接滑向下一个状态机
            if(m_FB_AxisStop[i].Error())
            {
                m_FB_AxisStop[i](m_vAxis[i],false,m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,bDone,bBusy,bCommandAborted,bError,nErrorID);
                m_TonStop[i].Ton(false,TimeOut);
                m_enAxisStopFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Error;
            }
            else
            {
                break;
            }
        case EN_AxisControlFSM::enAxisControlFSM_Error:
            m_stRobotControlStatus.bStatus_AxisError = true;
            m_stShmRobotRTStatus.nErrorAxisID = i;
            if(bError)
            {
                m_stShmRobotRTStatus.nErrorID = nErrorID;
            }
            else
            {
                m_stShmRobotRTStatus.nErrorID = REC_ExecStopTimeOutError;
            }
            m_enAxisStopFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
            break;
        default:
            break;
        }
    }
}
void LLC_Rbt::AxisHomeFSM()
{
    for (uint32_t i = 0; i < m_nConfiguredAxisNum; ++i)
    {
        bool bBusy = false; // 当前轴的执行状态
        bool bDone = false; // 当前轴的完成状态
        bool bError = false; // 当前轴的错误状态
        bool bCommandAborted = false; // 当前轴的命令中止状态
        int nErrorID = 0; // 当前轴的错误码

        switch (m_enAxisHomeFSM[i])
        {
            case EN_AxisControlFSM::enAxisControlFSM_Null:
                if (bAxisHomeCmd[i])
                {
                    bAxisHomeCmd[i] = false; // 消费本次回零命令
                    m_FB_AxisHome[i](m_vAxis[i], false, bBusy, bCommandAborted, bDone, bError, nErrorID); // 复位功能块，准备上升沿
                    m_enAxisHomeFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Execute; // 下一周期开始执行
                }
                break;

            case EN_AxisControlFSM::enAxisControlFSM_Execute:
                m_FB_AxisHome[i](m_vAxis[i], true, bBusy, bCommandAborted, bDone, bError, nErrorID); // 回零过程中每个周期持续调用
                if (bError)
                {
                    m_stRobotControlStatus.bStatus_AxisError = true; // 当前周期保存错误状态
                    m_stShmRobotRTStatus.nErrorAxisID = i; // 保存错误轴号
                    m_stShmRobotRTStatus.nErrorID = nErrorID; // 当前周期保存错误码
                    m_enAxisHomeFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Error; // 下一周期复位功能块
                }
                else if (bDone || bCommandAborted)
                {
                    m_FB_AxisHome[i](m_vAxis[i], false, bBusy, bCommandAborted, bDone, bError, nErrorID); // 完成或中止后复位功能块
                    m_enAxisHomeFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null; // 等待下一次命令
                }
                break; // 尚未完成时保持 Execute，下一周期继续调用

            case EN_AxisControlFSM::enAxisControlFSM_Error:
                m_FB_AxisHome[i](m_vAxis[i], false, bBusy, bCommandAborted, bDone, bError, nErrorID); // 清除功能块内部执行状态
                m_enAxisHomeFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
                break;

            default:
                m_enAxisHomeFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
                break;
        }
    }
}

void LLC_Rbt::AxisAbsMoveFSM()
{
    bool bBusy = false;
    bool bDone = false;
    bool bError = false;
    int nErrorID;
    bool bCommandAborted = false;
    for (uint32_t i = 0; i < m_nConfiguredAxisNum; i++)
    {
        switch (m_enAxisAbsMoveFSM[i])
        {
        case EN_AxisControlFSM::enAxisControlFSM_Null:
            if(bAxisAbsMoveCmd[i])
            {
                bAxisAbsMoveCmd[i] = false;
                m_pFB_AxisPTMove[i] = AxisPTMove(i,false,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                    m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);

                m_pFB_AxisPTMove[i] = AxisPTMove(i,true,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                    m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
                m_TonAxisAbsMove[i].Ton(false,TimeOut);
                m_enAxisAbsMoveFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Execute;
            }
            break;
        case EN_AxisControlFSM::enAxisControlFSM_Execute:
            if(m_pFB_AxisPTMove[i]->Done())
            {
                m_pFB_AxisPTMove[i] = AxisPTMove(i,false,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                    m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
                m_TonAxisAbsMove[i].Ton(false,TimeOut);
                m_enAxisAbsMoveFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
                break;
            }
            //报错不break，直接滑向下一个状态机
            if(!m_pFB_AxisPTMove[i]->Busy()|| m_pFB_AxisPTMove[i]->Error())
            {
                m_pFB_AxisPTMove[i] = AxisPTMove(i,false,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                    m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
                m_TonAxisAbsMove[i].Ton(false,TimeOut);
                m_enAxisAbsMoveFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Error;
            }
            else
            {
                break;
            }
        case EN_AxisControlFSM::enAxisControlFSM_Error:
            m_stRobotControlStatus.bStatus_AxisError = true;
            m_stShmRobotRTStatus.nErrorAxisID = i;
            if(bError)
            {
                m_stShmRobotRTStatus.nErrorID = nErrorID;
            }
            else
            {
                m_stShmRobotRTStatus.nErrorID = CEC_AXIS_ERROR_DURING_MOTION;
            }
            m_enAxisAbsMoveFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
            break;
        default:
            break;
        }
    }
}
void LLC_Rbt::AxisRelMoveFSM()
{
    bool bBusy = false;
    bool bDone = false;
    bool bError = false;
    int nErrorID;
    bool bCommandAborted = false;
    for (uint32_t i = 0; i < m_nConfiguredAxisNum; i++)
    {
        switch (m_enAxisRelMoveFSM[i])
        {
        case EN_AxisControlFSM::enAxisControlFSM_Null:
            if(bAxisRelMoveCmd[i])
            {
                bAxisRelMoveCmd[i] = false;
                m_vAxisMtionParam[i].dPos = m_vAxis[i]->Axis_GetActualPosition() + m_vAxisMtionParam[i].dPos;
                m_pFB_AxisPTMove[i] = AxisPTMove(i,false,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                    m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);

                m_pFB_AxisPTMove[i] = AxisPTMove(i,true,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                    m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
                m_TonAxisRelMove[i].Ton(false,TimeOut);
                m_enAxisRelMoveFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Execute;
            }
            break;
        case EN_AxisControlFSM::enAxisControlFSM_Execute:
            if(m_pFB_AxisPTMove[i]->Busy())
            {
                m_pFB_AxisPTMove[i] = AxisPTMove(i,false,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                    m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
                m_TonAxisRelMove[i].Ton(false,TimeOut);
                m_enAxisRelMoveFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
                break;
            }
            //报错不break，直接滑向下一个状态机
            if(!m_pFB_AxisPTMove[i]->Busy()|| m_pFB_AxisPTMove[i]->Error())
            {
                m_pFB_AxisPTMove[i] = AxisPTMove(i,false,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                    m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
                m_TonAxisRelMove[i].Ton(false,TimeOut);
                m_enAxisRelMoveFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Error;
            }
            else
            {
                break;
            }
        case EN_AxisControlFSM::enAxisControlFSM_Error:
            m_stRobotControlStatus.bStatus_AxisError = true;
            m_stShmRobotRTStatus.nErrorAxisID = i;
            if(m_pFB_AxisPTMove[i]->Error())
            {
                m_stShmRobotRTStatus.nErrorID = nErrorID;
            }
            else
            {
                m_stShmRobotRTStatus.nErrorID = CEC_AXIS_ERROR_DURING_MOTION;
            }
            m_enAxisRelMoveFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
            break;
        default:
            break;
        }
    }
}
void LLC_Rbt::AxisBackAndForthMoveFSM()
{
    bool bBusy = false;
    bool bDone = false;
    bool bError = false;
    int nErrorID;
    bool bCommandAborted = false;
    for (uint32_t i = 0; i < m_nConfiguredAxisNum; i++)
    {
        // switch(m_vAxis[i].nActModeOpration)
        // {
        //     case EN_ModesOfOperation::enModeCyclicSyncPosition:
        //         m_pFB = &m_FB_AxisAbsMove[i];
        //         break;
        //     case EN_ModesOfOperation::enModeCyclicSyncVelocity:
        //         m_pFB = &m_FB_AxisMoveVelocity[i];
        //         break;
        //     default:
        //         printf("ModeOprationError!\n");
        //         return;
        // }
        switch (m_enAxisBackAndForthMoveFSM[i])
        {
            case 0:
                if(bAxisBackAndForthMoveCmd[i])
                {
                    bAxisBackAndForthMoveCmd[i] = false;
                    m_enAxisBackAndForthMoveFSM[i] = m_enAxisBackAndForthMoveFSM[i] + 10;
                    m_TonAixsBackAndForthMove[i].Ton(false,m_vAxisMtionWaitTime_Test[0][i] * 1000);
                    //初始化功能块
                    m_pFB_AxisPTMove[i] = AxisPTMove(i,false,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                        m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
                }
                break;
            case 10://运动到pos1
                m_vAxisMtionParam[i] = m_vAxisMtionParam_Test[0][i];
                m_pFB_AxisPTMove[i] = AxisPTMove(i,true,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                    m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
                if(bDone)
                {
                    m_enAxisBackAndForthMoveFSM[i] = m_enAxisBackAndForthMoveFSM[i] + 5;
                    m_TonAixsBackAndForthMove[i].Ton(false,m_vAxisMtionWaitTime_Test[0][i] * 1000);
                }
                break;
            case 15:
                if(m_TonAixsBackAndForthMove[i].Ton(true,m_vAxisMtionWaitTime_Test[0][i] * 1000))
                {
                    m_TonAixsBackAndForthMove[i].Ton(false,m_vAxisMtionWaitTime_Test[0][i] * 1000);
                    m_enAxisBackAndForthMoveFSM[i] = m_enAxisBackAndForthMoveFSM[i] + 5;
                }
                break;
            case 20://运动到pos2
                m_vAxisMtionParam[i] = m_vAxisMtionParam_Test[1][i];
                m_pFB_AxisPTMove[i] = AxisPTMove(i,true,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                    m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
                if(bDone)
                {
                    m_enAxisBackAndForthMoveFSM[i] = m_enAxisBackAndForthMoveFSM[i] + 5;
                    m_TonAixsBackAndForthMove[i].Ton(false,m_vAxisMtionWaitTime_Test[1][i] * 1000);
                }
                break;
            case 25://运动到pos2  
                if(m_TonAixsBackAndForthMove[i].Ton(true,m_vAxisMtionWaitTime_Test[1][i] * 1000))
                {
                    m_TonAixsBackAndForthMove[i].Ton(false,m_vAxisMtionWaitTime_Test[1][i] * 1000);
                    m_enAxisBackAndForthMoveFSM[i] = 10;
                }
                break;
            default:
                m_enAxisBackAndForthMoveFSM[i] = 0;
        }
    }
}

void LLC_Rbt::AxisContinuousStepMoveFSM()
{
    bool bBusy = false;
    bool bDone = false;
    bool bError = false;
    int nErrorID;
    bool bCommandAborted = false;
    for (uint32_t i = 0; i < m_nConfiguredAxisNum; i++)
    {
        switch (m_enAxisContinuousStepMoveFSM[i])
        {
            case 0:
                if(bAxisContinuousStepMoveCmd[i])
                {
                    bAxisContinuousStepMoveCmd[i] = false;
                    m_enAxisContinuousStepMoveFSM[i] = m_enAxisContinuousStepMoveFSM[i] + 10;
                    m_TonAxisContinuousStepMove->Ton(false,m_vAxisMtionWaitTime_Test[0][i] * 1000);
                    //初始化功能块
                    m_vAxisMtionParam[i].dPos = m_vAxisMtionParam_Test[0][i].dPos + m_vAxis[i]->Axis_GetActualPosition();
                    m_pFB_AxisPTMove[i] = AxisPTMove(i,false,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                        m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
                }
                break;
            case 10:
                m_pFB_AxisPTMove[i] = AxisPTMove(i,true,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                    m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
                //等待
                if(bDone)
                {
                    if(m_TonAixsBackAndForthMove[i].Ton(true,m_vAxisMtionWaitTime_Test[0][i] * 1000))
                    {
                        m_vAxisMtionParam[i].dPos = m_vAxisMtionParam_Test[0][i].dPos + m_vAxis[i]->Axis_GetActualPosition();
                        m_TonAixsBackAndForthMove[i].Ton(false,m_vAxisMtionWaitTime_Test[0][i] * 1000);
                        m_pFB_AxisPTMove[i] = AxisPTMove(i,false,m_vAxisMtionParam[i].dPos,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                            m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,EN_Direction::enPositive,EN_BufferMode::enAborting,bDone,bBusy,bCommandAborted,bError,nErrorID);
                    }
                }
                if(bError)
                {
                    m_enAxisContinuousStepMoveFSM[i] = m_enAxisContinuousStepMoveFSM[i] + 10;
                }
                break;
            case 20:
                m_enAxisContinuousStepMoveFSM[i] = 0;
                m_stShmRobotRTStatus.nErrorID = nErrorID;
            default:
                m_enAxisContinuousStepMoveFSM[i] = 0;
                break;
        }
    }

}
void LLC_Rbt::AxisVelocityMove()
{
    bool bBusy = false;
    bool bError = false;
    int nErrorID;
    bool bCommandAborted = false;
    bool bInVelocity = false;
    for (uint32_t i = 0; i < m_nConfiguredAxisNum; i++)
    {
        switch (m_enAxisVelocityMoveFSM[i])
        {
        case EN_AxisControlFSM::enAxisControlFSM_Null:
            if(bAxisVelocityMoveCmd[i])
            {
                bAxisVelocityMoveCmd[i] = false;
                m_vAxisMtionParam = m_vAxisMtionParam_Test[0];
                m_FB_AxisMoveVelocity[i](m_vAxis[i],false,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                    m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,(EN_Direction)m_vAxisMtionParam[i].nDir,bInVelocity,bBusy,bCommandAborted,bError,nErrorID);

                m_FB_AxisMoveVelocity[i](m_vAxis[i],true,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                    m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,(EN_Direction)m_vAxisMtionParam[i].nDir,bInVelocity,bBusy,bCommandAborted,bError,nErrorID);
                m_TonAxisVelocityMove[i].Ton(false,TimeOut);
                m_enAxisVelocityMoveFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Execute;
            }
            break;
        case EN_AxisControlFSM::enAxisControlFSM_Execute:
            //报错不break，直接滑向下一个状态机
            if(!m_FB_AxisMoveVelocity->Busy()|| m_FB_AxisMoveVelocity->Error())
            {
                m_FB_AxisMoveVelocity[i](m_vAxis[i],false,m_vAxisMtionParam[i].dVel,m_vAxisMtionParam[i].dAcc,
                    m_vAxisMtionParam[i].dDec,m_vAxisMtionParam[i].dJerk,(EN_Direction)m_vAxisMtionParam[i].nDir,bInVelocity,bBusy,bCommandAborted,bError,nErrorID);
                m_TonAxisVelocityMove[i].Ton(false,TimeOut);
                m_enAxisVelocityMoveFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Error;
            }
            else
            {
                break;
            }
        case EN_AxisControlFSM::enAxisControlFSM_Error:
            m_stRobotControlStatus.bStatus_AxisError = true;
            m_stShmRobotRTStatus.nErrorAxisID = i;
            if(bError)
            {
                m_stShmRobotRTStatus.nErrorID = nErrorID;
            }
            else
            {
                m_stShmRobotRTStatus.nErrorID = CEC_AXIS_ERROR_DURING_MOTION;
            }
            m_enAxisVelocityMoveFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
            break;
        default:
            break;
        }
    }
}