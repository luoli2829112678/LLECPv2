#include "LLC_Rbt.h"

bool LLC_Rbt::guard_CheckIsError()
{
    bool bWatchDogTimeOut = CheckCPSWatchDogTimeOut();
    bool bSlaveDropError = CheckSlaveDropError();
    bool bEmergencyStop = CheckEmergencyStop();
    bool bLLPCError =false;
    if(m_fsmRbt.currentState->unStateCode >= enCDSState_WatchDog)
    {
        bLLPCError = CheckLLPCError();
    } 
    bool IsError = bWatchDogTimeOut || bSlaveDropError || bEmergencyStop || bLLPCError;
    if(IsError)
        return true;
    else
        return false;
}


bool  LLC_Rbt::guard_CheckIsSlaveDrop()
{
    if(m_bFoEWorking)
        return false;
    if (!AreSlavesConnected())
    {
        m_stRobotControlStatus.bStatus_SlaveDropError = true;
        return true;
    }
    if(m_stRobotControlStatus.bStatus_SlaveDropError)
    {
        m_stRobotControlStatus.bStatus_SlaveDropError = true;
        return true;
    }
    return false;
}


bool  LLC_Rbt::guard_CheckIsMoveDone()
{
    if(LLCSoftMotion.bMoving)return false;
    for (size_t i = 0; i < m_vAxis.size(); i++)
    {
        if(m_vAxis[i]->Axis_GetMovingState())
        {
            return false;
        }
    }
    if(CheckIsScriptMoving())
    {
        return false;
    }
    if(m_ApplicationStatus == HSTR::ApplicationStatus::Running)
    {
        return false;
    }
    return true;
}


bool  LLC_Rbt::guard_CheckResetFinish()
{
    if(LLCSoftMotion.bError)return false;
    for (size_t i = 0; i < m_vAxis.size(); i++)
    {
        if(m_vAxis[i]->Axis_GetErrorState())
        {
            return false;
        }
    }
    return true;
}

bool LLC_Rbt::guard_CheckIsSlaveConfigDone()
{
    return m_nConfiguredSlaveNum == m_nSetConfiguredSlaveNum;
}







bool LLC_Rbt::CheckLLPCError()
{
    //监测轴错误
    for (size_t i = 0; i < m_vAxis.size(); i++)
    {
        if(m_vAxis[i]->Axis_GetErrorState())
        {
            return true;
        }
    }
    if(LLCSoftMotion.bError)
    {
        return true;
    }
    return false;
}