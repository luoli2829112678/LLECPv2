#pragma once
#include"AxisGroupAlgorithmBase.h"

// XY直角坐标算法类，公开继承轴组算法基类
class AxisGroupAlgorithm_XY_Cartesian : public AxisGroupAlgorithmBase
{
public:
    // 构造函数
    AxisGroupAlgorithm_XY_Cartesian();

    // 析构函数，覆盖基类虚析构函数
    ~AxisGroupAlgorithm_XY_Cartesian() override;
    // 笛卡尔位姿转换为关节位置
    int AxisGroupAlgorithm_PCS2ACS(ST_AxisGroupPose pcs,ST_AxisGroupJointPosition ref_acs, ST_AxisGroupJointPosition& acs) const override;

    // 关节位置转换为笛卡尔位姿
    int AxisGroupAlgorithm_ACS2PCS(ST_AxisGroupJointPosition acs, ST_AxisGroupPose& pcs) const override;

};

// XY平面二连杆串联机构，关节角单位rad，长度单位与DH参数保持一致
class AxisGroupAlgorithm_XY_LinkRod : public AxisGroupAlgorithmBase
{
public:
    // 默认采用第二关节有效角非负的逆解分支
    AxisGroupAlgorithm_XY_LinkRod();

    // 虚析构函数
    ~AxisGroupAlgorithm_XY_LinkRod() override;

    // 设置逆解分支：+1选择有效第二关节角非负的解，-1选择非正的解，仅允许静止时修改
    int AxisGroupAlgorithm_SetElbowSign(int elbowSign);

    // 输入XY位置，输出选定分支的关节角，失败时不修改输出
    int AxisGroupAlgorithm_PCS2ACS(ST_AxisGroupPose pcs, ST_AxisGroupJointPosition ref_acs,ST_AxisGroupJointPosition& acs) const override;

    // 输入关节角，输出XY位置，失败时不修改输出
    int AxisGroupAlgorithm_ACS2PCS(ST_AxisGroupJointPosition acs, ST_AxisGroupPose& pcs) const override;

private:
    // 第二关节有效角的符号，用于选择逆解分支
    int m_elbowSign = 1;
};