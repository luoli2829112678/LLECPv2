#include"AxisGroupAlgorithmBase.h"



int AxisGroupAlgorithmBase::AxisGroupAlgorithm_Init(ENAxisGroupModel model, const ST_AxisGroupDH& dh, double cycleTime)
{
    m_stDH = dh;
    m_enAxisGroupModel = model;
    m_CycleTime = cycleTime;
    return AEC_SUCCESSED;
}
#include "AxisGroupAlgorithmBase.h"

// 读取当前对象保存的DH参数
int AxisGroupAlgorithmBase::AxisGroupAlgorithm_GetDH(ST_AxisGroupDH& dh) const
{
    dh = m_stDH;
    return 0;
}

// 基类不提供具体模型的逆运动学计算
int AxisGroupAlgorithmBase::AxisGroupAlgorithm_PCS2ACS(ST_AxisGroupPose pcs,ST_AxisGroupJointPosition ref_acs, ST_AxisGroupJointPosition& acs) const
{
    (void)pcs;
    (void)acs;
    return -1;
}

// 基类不提供具体模型的正运动学计算
int AxisGroupAlgorithmBase::AxisGroupAlgorithm_ACS2PCS(ST_AxisGroupJointPosition acs, ST_AxisGroupPose& pcs) const
{
    (void)acs;
    (void)pcs;
    return -1;
}