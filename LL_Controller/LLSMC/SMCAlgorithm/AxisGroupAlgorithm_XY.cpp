#include"AxisGroupAlgorithm_XY.h"
// 构造函数实现
AxisGroupAlgorithm_XY_Cartesian::AxisGroupAlgorithm_XY_Cartesian()
{
}

// 析构函数实现
AxisGroupAlgorithm_XY_Cartesian::~AxisGroupAlgorithm_XY_Cartesian()
{
}


int AxisGroupAlgorithm_XY_Cartesian::AxisGroupAlgorithm_ACS2PCS(ST_AxisGroupJointPosition acs, ST_AxisGroupPose& pcs) const
{
    pcs = ST_AxisGroupPose{};                
    pcs.x = m_stDH.d1 + acs.AxisPos[0];     
    pcs.y = m_stDH.d2 + acs.AxisPos[1];      
    return 0;                              
}

// XY逆解：输入应为该模型可实现的平面位姿，此处只计算X、Y对应的关节位置
int AxisGroupAlgorithm_XY_Cartesian::AxisGroupAlgorithm_PCS2ACS(ST_AxisGroupPose pcs, ST_AxisGroupJointPosition ref_acs,ST_AxisGroupJointPosition& acs) const
{
    acs = ST_AxisGroupJointPosition{};       
    acs.AxisPos[0] = pcs.x - m_stDH.d1;     
    acs.AxisPos[1] = pcs.y - m_stDH.d2;      
    return 0;                              
}




// 默认构造函数，DH参数通过基类现有配置接口设置
AxisGroupAlgorithm_XY_LinkRod::AxisGroupAlgorithm_XY_LinkRod() = default;

// 默认析构函数
AxisGroupAlgorithm_XY_LinkRod::~AxisGroupAlgorithm_XY_LinkRod() = default;

// 设置固定逆解分支
int AxisGroupAlgorithm_XY_LinkRod::AxisGroupAlgorithm_SetElbowSign(int elbowSign)
{
    // 只接受+1和-1
    if (elbowSign != 1 && elbowSign != -1)
        return -1;

    // 保存分支选择，不自动切换分支
    m_elbowSign = elbowSign;

    return 0;
}

// 正运动学：关节位置转换为XY位置
int AxisGroupAlgorithm_XY_LinkRod::AxisGroupAlgorithm_ACS2PCS(ST_AxisGroupJointPosition acs, ST_AxisGroupPose& pcs) const
{
    // 读取两根连杆长度
    const double a1 = m_stDH.a1;
    const double a2 = m_stDH.a2;

    // 检查连杆长度及固定角偏置是否合法
    if (!std::isfinite(a1) || !std::isfinite(a2) || a1 <= 0.0 || a2 <= 0.0 || !std::isfinite(m_stDH.theta1) || !std::isfinite(m_stDH.theta2))
        return -1;

    // 检查输入关节角是否合法
    if (!std::isfinite(acs.AxisPos[0]) || !std::isfinite(acs.AxisPos[1]))
        return -1;

    // 实际几何角等于输入关节角加固定角偏置
    const double t1 = acs.AxisPos[0] + m_stDH.theta1;
    const double t2 = acs.AxisPos[1] + m_stDH.theta2;

    // 检查角度相加是否溢出
    if (!std::isfinite(t1) || !std::isfinite(t2) || !std::isfinite(t1 + t2))
        return -1;

    // 分别计算大臂和小臂末端贡献
    const double x = a1 * std::cos(t1) + a2 * std::cos(t1 + t2);
    const double y = a1 * std::sin(t1) + a2 * std::sin(t1 + t2);

    // 检查计算结果是否合法
    if (!std::isfinite(x) || !std::isfinite(y))
        return -1;

    // 仅在计算成功后更新输出，本接口不修改Z及姿态字段
    pcs.x = x;
    pcs.y = y;

    return 0;
}

// 逆运动学：XY位置转换为选定分支的关节角
int AxisGroupAlgorithm_XY_LinkRod::AxisGroupAlgorithm_PCS2ACS(ST_AxisGroupPose pcs, ST_AxisGroupJointPosition ref_acs,ST_AxisGroupJointPosition& acs) const
{
    // 读取两根连杆长度
    const double a1 = m_stDH.a1;
    const double a2 = m_stDH.a2;

    // 检查连杆长度及固定角偏置是否合法
    if (!std::isfinite(a1) || !std::isfinite(a2) || a1 <= 0.0 || a2 <= 0.0 || !std::isfinite(m_stDH.theta1) || !std::isfinite(m_stDH.theta2))
        return -1;

    // 本模型只使用输入位姿的X和Y
    if (!std::isfinite(pcs.x) || !std::isfinite(pcs.y))
        return -1;

    // 使用统一尺度归一化，避免直接计算长度平方导致溢出
    const double scale = std::max(a1, a2);
    const double l1 = a1 / scale;
    const double l2 = a2 / scale;
    const double x = pcs.x / scale;
    const double y = pcs.y / scale;

    // 计算归一化后的目标半径
    const double radius = std::hypot(x, y);

    // 归一化坐标溢出时，目标必然超出当前机构的有限工作范围
    if (!std::isfinite(radius))
        return -1;

    // 设置相对于连杆尺度的浮点容差
    constexpr double tolerance = 1e-12;

    // 二连杆的可达半径范围为两杆长度之差到两杆长度之和
    const double minRadius = std::abs(l1 - l2);
    const double maxRadius = l1 + l2;

    // 检查目标是否超出工作范围，允许边界附近的浮点误差
    if (radius < minRadius - tolerance || radius > maxRadius + tolerance)
        return -1;

    // 等长连杆完全折叠到原点时，第一关节角不唯一，本实现返回奇异状态
    if (radius <= tolerance && minRadius <= tolerance)
        return -1;

    // 计算余弦定理的分母
    const double denominator = 2.0 * l1 * l2;

    // 极端长度比例可能导致浮点下溢
    if (denominator <= 0.0)
        return -1;

    // 根据余弦定理计算第二关节有效角的余弦
    const double rawCosT2 = (radius * radius - l1 * l1 - l2 * l2) / denominator;

    // 检查中间计算结果
    if (!std::isfinite(rawCosT2))
        return -1;

    // 将边界浮点误差限制在合法余弦范围
    const double cosT2 = std::clamp(rawCosT2, -1.0, 1.0);

    // 根据指定分支选择第二关节有效角的正弦符号
    const double sinT2 = static_cast<double>(m_elbowSign) * std::sqrt(std::max(0.0, 1.0 - cosT2 * cosT2));

    // 计算第二关节有效角，范围为[-π,π]
    const double t2 = std::atan2(sinT2, cosT2);

    // 计算第一关节有效角
    const double t1 = std::atan2(y, x) - std::atan2(l2 * sinT2, l1 + l2 * cosT2);

    // 扣除固定角偏置，恢复控制器关节坐标
    const double q1 = t1 - m_stDH.theta1;
    const double q2 = t2 - m_stDH.theta2;

    // 检查最终关节角是否合法
    if (!std::isfinite(q1) || !std::isfinite(q2))
        return -1;

    // 仅在计算成功后修改两个有效关节的输出
    acs.AxisPos[0] = q1;
    acs.AxisPos[1] = q2;

    return 0;
}