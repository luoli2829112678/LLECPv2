#include"AxisGroup.h"
AxisGroup::AxisGroup(/* args */)
{
    m_enModesOfOperation = EN_ModesOfOperation::enModeCyclicSyncPosition;
    m_enGroupState = GroupDisabled;
}

AxisGroup::~AxisGroup()
{
}

void AxisGroup::AxisGruop_RT()
{
    return;
}
int AxisGroup::ResetError()
{
    return AEC_SUCCESSED;
}
int AxisGroup::Group_CheckError(int nErrorID)
{
    return 0;
}

int AxisGroup::InitGroup(ENAxisGroupModel enAlgorithmModel,std::vector<CIA402Axis*>vpAxis,ST_AxisGroupDH dh, double cycleTime)
{
    m_enAlgorithmModel = enAlgorithmModel;
    m_vpAxis = vpAxis;
    m_DH = dh;
    m_CycleTime = cycleTime;
    //初始化算法
    m_pAxisGroupAlgorithm = nullptr;
    m_pAxisGroupAlgorithm = GenerativeModelAlgorithm(m_enAlgorithmModel,dh);
    if(nullptr == m_pAxisGroupAlgorithm)
    {
        printf("运动学模型加载失败\n");
        return -1;
    }
    int nRet = m_pAxisGroupAlgorithm->AxisGroupAlgorithm_Init(m_enAlgorithmModel,m_DH,m_CycleTime);
    if(nRet < 0 )
    {
        printf("运动学模型初始化失败\n");
        return -1;
    }
    return AEC_SUCCESSED;
}

std::vector<CIA402Axis*> AxisGroup::GetAxisList()
{
    return m_vpAxis;
}

int AxisGroup::Group_PCS2ACS(ST_AxisGroupPose pcs ,ST_AxisGroupJointPosition ref_acs,ST_AxisGroupJointPosition &acs)
{
    return m_pAxisGroupAlgorithm->AxisGroupAlgorithm_PCS2ACS(pcs,ref_acs,acs);
}
int AxisGroup::Group_ACS2PCS(ST_AxisGroupJointPosition acs,ST_AxisGroupPose &pcs )
{
    return m_pAxisGroupAlgorithm->AxisGroupAlgorithm_ACS2PCS(acs,pcs);
}

ENAxisGroupState AxisGroup::Group_ReadGroupState()
{
    return m_enGroupState;
}

EN_ModesOfOperation  AxisGroup::Axis_GetActualOperationMode()
{
    return m_enModesOfOperation;
}
int AxisGroup::Group_SetGroupState(ENAxisGroupState state)
{
    m_enGroupState = state;
}

void AxisGroup::AxisGroupStateCange(ENAxisGroupState enGroupState)
{
    m_enGroupState = enGroupState;
}


int AxisGroup::Group_PushMotionUint(ST_GroupMotionUint stGroupMotionUint)
{
    m_vGroupMotorMotionBuffer.push_back(stGroupMotionUint);
}

int AxisGroup::Group_GetMotionUint(ST_GroupMotionUint& stGroupMotionUint)
{
    stGroupMotionUint = m_vGroupMotorMotionBuffer.at(0);
}

ST_GroupPosition AxisGroup::Axis_GetSetPosition() const
{
    return m_stGroupMotorMotionData.CommandPosition;      // 返回各电机的命令位置
}

ST_GroupPosition AxisGroup::Axis_GetSetVelocity() const
{
    return m_stGroupMotorMotionData.CommandVelocity;      // 返回各电机的命令速度
}

ST_GroupPosition AxisGroup::Axis_GetSetAcceleration() const
{
    return m_stGroupMotorMotionData.CommandAcceleration;  // 返回各电机的命令加速度
}


// 周期检查全部成员轴状态，汇总轴组运行状态
void AxisGroup::AxisGroupStateCheck()
{
    bool bHasDisabledAxis = false;    // 是否存在未使能或无效成员轴
    bool bHasErrorAxis = false;       // 是否存在故障轴
    bool bHasStoppingAxis = false;    // 是否存在正在停止的轴
    bool bHasMovingAxis = false;      // 是否存在正在运动的轴

    // 没有成员轴时，轴组为禁用状态
    if (m_vpAxis.empty())
    {
        m_enGroupState = GroupDisabled;
        return;
    }

    // 遍历成员轴，每个周期对每根轴读取一次状态
    for (CIA402Axis* pAxis : m_vpAxis)
    {
        // 空指针按无效成员轴处理，继续检查其他轴是否存在故障
        if (pAxis == nullptr)
        {
            bHasDisabledAxis = true;
            continue;
        }

        // 读取当前成员轴的运动状态
        const EN_AxisMotionState enAxisState = pAxis->Axis_GetAxisMotionState();

        // 根据单轴状态累计轴组状态标志
        switch (enAxisState)
        {
        case motionState_power_off:
            // 任意轴未使能，轴组不满足全使能条件
            bHasDisabledAxis = true;
            break;

        case motionState_errorstop:
            // 任意轴故障，轴组优先进入故障状态
            bHasErrorAxis = true;
            break;

        case motionState_stopping:
            // 任意轴处于停止过程，轴组标记为停止中
            bHasStoppingAxis = true;
            break;

        case motionState_standstill:
            // 该轴已使能且静止，不增加其他标志
            break;

        case motionState_discrete_motion:
        case motionState_continuous_motion:
        case motionState_synchronized_motion:
        case motionState_homing:
            // 单步、连续、同步运动及回零均视为正在运动
            bHasMovingAxis = true;
            break;

        default:
            // 未知状态不允许被判断为正常待机
            bHasErrorAxis = true;
            break;
        }
    }

    // 最高优先级：任意成员轴故障或状态异常
    if (bHasErrorAxis)
    {
        m_enGroupState = GroupErrorStop;
        return;
    }

    // 任意成员轴掉使能或绑定无效，轴组进入禁用状态
    if (bHasDisabledAxis)
    {
        m_enGroupState = GroupDisabled;
        return;
    }

    // 全部轴有效且未掉使能，任意轴停止中则进入停止状态
    if (bHasStoppingAxis)
    {
        m_enGroupState = GroupStopping;
        return;
    }

    // 无故障、无掉使能、无停止过程，任意轴运动则进入运动状态
    if (bHasMovingAxis)
    {
        m_enGroupState = GroupMoving;
        return;
    }

    // 所有成员轴均处于使能静止状态
    m_enGroupState = GroupStandby;
}

int AxisGroup::AxisGroupFollowACS(ST_AxisGroupJointPosition acs)
{
    for (size_t i = 0; i < m_vpAxis.size(); i++)
    {
        if(EN_ModesOfOperation::enModeCyclicSyncPosition== m_vpAxis[i]->Axis_GetActualOperationMode())
        {
            m_vpAxis[i]->Axis_SetAxisState(EN_AxisMotionState::motionState_synchronized_motion);
            m_vpAxis[i]->Axis_SetCyclicSyncMotion();
        }
        m_vpAxis[i]->Axis_SetTargetPosition(acs.AxisPos[i]);
    }
    return AEC_SUCCESSED;
}

int AxisGroup::AxisGroupFollowPCS(ST_AxisGroupPose pcs)
{
    ST_AxisGroupJointPosition acs;
    int ret = Group_PCS2ACS(pcs,m_stPosition_Acs,acs);
    if(ret != AEC_SUCCESSED)
    {
        AxisGroupStateCange(ENAxisGroupState::GroupErrorStop);
        return -1;
    }
    return AxisGroupFollowACS(acs);
}