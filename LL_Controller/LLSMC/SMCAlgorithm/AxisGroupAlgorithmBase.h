#pragma once
#include"AxisGroupAlgorithmDef.h"
#include"MotionAlgorithm/MotionAlgorithm.h"
#include"../CIA402Axis/CIA402Axis.h"

class AxisGroupAlgorithmBase
{
public:
    // 默认构造函数
    AxisGroupAlgorithmBase() = default;

    // 虚析构函数，保证通过基类指针正确释放派生对象
    virtual ~AxisGroupAlgorithmBase() = default;

    // 禁止复制构造
    AxisGroupAlgorithmBase(const AxisGroupAlgorithmBase&) = delete;

    // 禁止复制赋值
    AxisGroupAlgorithmBase& operator=(const AxisGroupAlgorithmBase&) = delete;

    // ==================== 初始化与配置 ====================

    // 初始化轴组：groupID为轴组ID，model决定轴数与算法，dh为几何参数，cycleTime为控制周期，单位s
    int AxisGroupAlgorithm_Init(ENAxisGroupModel model, const ST_AxisGroupDH& dh, double cycleTime);

    // 读取当前DH几何参数
    int AxisGroupAlgorithm_GetDH(ST_AxisGroupDH& dh) const;

    virtual int AxisGroupAlgorithm_PCS2ACS(ST_AxisGroupPose pcs ,ST_AxisGroupJointPosition ref_acs,ST_AxisGroupJointPosition &acs) const;
    virtual int AxisGroupAlgorithm_ACS2PCS(ST_AxisGroupJointPosition acs,ST_AxisGroupPose &pcs ) const;
protected:
    ST_AxisGroupDH m_stDH;
    ENAxisGroupModel m_enAxisGroupModel;
    double m_CycleTime;
    int m_AxisNum;
};


