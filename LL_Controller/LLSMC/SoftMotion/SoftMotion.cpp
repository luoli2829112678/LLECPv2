#include"SoftMotion.h"

SoftMotion::SoftMotion()
{
}
SoftMotion::~SoftMotion()
{
}

int SoftMotion::RegisterAxis(std::vector<CIA402Axis*> v_Axis)
{
    m_vSoftMotionPlanParams.clear();
    m_v_Axis = v_Axis;
    bMoving = false; bError = false;
    for(size_t i =0;i<v_Axis.size();i++)
    {
        SoftMotionPlanParams data;
        v_Axis[i]->Axis_SetAxisID(i);
        m_vSoftMotionPlanParams.push_back(data);
    }
    return SMEC_SUCCESSED;
}

int SoftMotion::RegisterAxisGroup(std::vector<AxisGroup*> v_AxisGroup)
{
    m_v_AxisGroup = v_AxisGroup;
    return SMEC_SUCCESSED;
}

int SoftMotion::SetSoftMotionCycle(double dCycle)
{
    m_dSoftMotionCycle = dCycle;
    for(size_t i = 0;i<m_v_Axis.size();i++)
    {
        m_v_Axis[i]->Axis_SetControlCycle(dCycle);
    }
    return AEC_SUCCESSED;
}

void SoftMotion::SoftMotionRun()
{
    SoftMotionPlanner_RT();
    AxisRealTime();
    return;
}

void SoftMotion::AxisRealTime()
{
    for(size_t i = 0;i<m_v_Axis.size();i++)
    {
        m_v_Axis[i]->Axis_RT();
    }
}

void SoftMotion::SoftMotionStateCheck()
{
    bMoving = false;
    bError = false;
    for(size_t i = 0;i<m_v_Axis.size();i++)
    {
        if(m_v_Axis[i]->Axis_GetSoftMotionMovingState())
        {
            bMoving = true;
        }
        if(m_v_Axis[i]->Axis_GetErrorState())
        {
            bError = true;
        }
    }
}