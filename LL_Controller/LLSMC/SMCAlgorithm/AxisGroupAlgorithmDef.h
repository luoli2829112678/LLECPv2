#pragma once                                      // 防止头文件重复包含
#include <array>                                  // 固定长度数组
#include <cstdint>                                // 固定宽度整数类型
constexpr std::uint32_t LLC_GROUP_MAX_AXES = 10;             // 项目支持的最大轴组轴数
constexpr std::uint32_t LLC_MAX_DH_LINKS = LLC_GROUP_MAX_AXES;         // DH表最大节数，包含固定节


// 机构类型：描述轴组的机械结构，
enum ENAxisGroupModel
{
    AxisGroupModel_null                 = 0,       // 未配置机构
    AxisGroupModel_xy_LinkRod           = 1,       // 保留原类型，需明确连杆拓扑
    AxisGroupModel_xy_Cartesian         = 2,       // XY直角坐标机构
    AxisGroupModel_xyz_LinkRod          = 3,       // 保留原类型，需明确连杆拓扑
    AxisGroupModel_xyz_Cartesian        = 4,       // XYZ直角坐标机构

    AxisGroupModel_CartesianN           = 10,      // 通用直角坐标机构
    AxisGroupModel_CoreXY               = 11,      // CoreXY耦合机构
    AxisGroupModel_HBot                 = 12,      // H-Bot机构

    AxisGroupModel_Planar2R             = 20,      // 平面二转动关节串联机构
    AxisGroupModel_Planar3R             = 21,      // 平面三转动关节串联机构
    AxisGroupModel_SCARA4               = 22,      // 四轴SCARA机构
    AxisGroupModel_Cylindrical          = 23,      // 圆柱坐标机构
    AxisGroupModel_Polar                = 24,      // 极坐标机构

    AxisGroupModel_Serial5              = 30,      // 五轴串联机构
    AxisGroupModel_Serial6              = 31,      // 六轴串联机器人
    AxisGroupModel_Serial7              = 32,      // 七轴串联机器人
    AxisGroupModel_SerialN              = 33,      // 通用N关节串联机构

    AxisGroupModel_FiveBar              = 40,      // 平面五杆并联机构
    AxisGroupModel_Delta3               = 41,      // 三平移自由度Delta机构
    AxisGroupModel_Delta4               = 42,      // 带末端转动轴的Delta机构
    AxisGroupModel_Stewart6             = 43,      // 六自由度Stewart平台

    AxisGroupModel_Gantry5              = 50,      // 龙门五轴机构
    AxisGroupModel_TableTable5          = 51,      // 双转台五轴机构
    AxisGroupModel_HeadHead5            = 52,      // 双摆头五轴机构
    AxisGroupModel_HeadTable5           = 53,      // 摆头加转台五轴机构

    AxisGroupModel_Custom              = 1000     // 用户自定义机构
};



// DH几何参数：只传参数，不存模型、轴数、轴绑定和运行状态。
// 长度统一使用m，角度统一使用rad；模型不使用的字段保持0。
struct ST_AxisGroupDH
{
    double d1 = 0.0;         // 第1节轴向距离
    double d2 = 0.0;         // 第2节轴向距离
    double d3 = 0.0;         // 第3节轴向距离
    double d4 = 0.0;         // 第4节轴向距离
    double d5 = 0.0;         // 第5节轴向距离
    double d6 = 0.0;         // 第6节轴向距离
    double d7 = 0.0;         // 第7节轴向距离
    double d8 = 0.0;         // 第8节轴向距离
    double d9 = 0.0;         // 第9节轴向距离
    double d10 = 0.0;        // 第10节轴向距离

    double a1 = 0.0;         // 第1节连杆长度参数
    double a2 = 0.0;         // 第2节连杆长度参数
    double a3 = 0.0;         // 第3节连杆长度参数
    double a4 = 0.0;         // 第4节连杆长度参数
    double a5 = 0.0;         // 第5节连杆长度参数
    double a6 = 0.0;         // 第6节连杆长度参数
    double a7 = 0.0;         // 第7节连杆长度参数
    double a8 = 0.0;         // 第8节连杆长度参数
    double a9 = 0.0;         // 第9节连杆长度参数
    double a10 = 0.0;        // 第10节连杆长度参数

    double alpha1 = 0.0;     // 第1节连杆扭转角
    double alpha2 = 0.0;     // 第2节连杆扭转角
    double alpha3 = 0.0;     // 第3节连杆扭转角
    double alpha4 = 0.0;     // 第4节连杆扭转角
    double alpha5 = 0.0;     // 第5节连杆扭转角
    double alpha6 = 0.0;     // 第6节连杆扭转角
    double alpha7 = 0.0;     // 第7节连杆扭转角
    double alpha8 = 0.0;     // 第8节连杆扭转角
    double alpha9 = 0.0;     // 第9节连杆扭转角
    double alpha10 = 0.0;    // 第10节连杆扭转角

    double theta1 = 0.0;     // 第1关节固定角偏置，不是实时关节角
    double theta2 = 0.0;     // 第2关节固定角偏置
    double theta3 = 0.0;     // 第3关节固定角偏置
    double theta4 = 0.0;     // 第4关节固定角偏置
    double theta5 = 0.0;     // 第5关节固定角偏置
    double theta6 = 0.0;     // 第6关节固定角偏置
    double theta7 = 0.0;     // 第7关节固定角偏置
    double theta8 = 0.0;     // 第8关节固定角偏置
    double theta9 = 0.0;     // 第9关节固定角偏置
    double theta10 = 0.0;    // 第10关节固定角偏置
};



// 位姿：表示参考坐标系下的TCP位置和姿态。
struct ST_AxisGroupPose
{
    double x = 1.0;                                       
    double y = 0.0;                                      
    double z = 0.0;                                       
    double rx = 0.0;        
    double ry = 0.0;  
    double rz = 0.0;                                
};

// 关节位置：仅前模型轴数个元素有效，轴数由模型决定。
struct ST_AxisGroupJointPosition
{
    double AxisPos[LLC_GROUP_MAX_AXES]{0};             // 转动关节rad，移动关节m
};

struct ST_GroupPosition
{
    ST_AxisGroupPose PCS;
    ST_AxisGroupJointPosition ACS;
};

enum EN_GroupMotionType
{
    GroupMotionType_Joint,     // 关节运动
    GroupMotionType_Line,      // 直线运动
    GroupMotionType_Arc        // 圆弧运动
};

struct ST_GroupMotorMotionData
{
    EN_GroupMotionType enGroupMotionType;   //运动类型
    ST_GroupPosition CommandPosition;      // 命令位置
    ST_GroupPosition ActualPosition;       // 实际位置
    ST_GroupPosition CommandVelocity;      // 命令速度
    ST_GroupPosition ActualVelocity;       // 实际速度
    ST_GroupPosition CommandAcceleration;  // 命令加速度
    ST_GroupPosition ActualAcceleration;   // 实际加速度 
    ST_GroupPosition CommandJerk;          // 命令加加速度
    ST_GroupPosition ActualJerk;           // 实际加加速度
    double dJointVelocity;                 //关节速度
    double dTCPVelocity;                   //TCP速度

};
// 单关节限制：按关节下标配置，不按硬件轴ID配置。
struct ST_AxisGroupJointLimit
{
    bool positionEnabled = false;                          // 是否启用位置限制
    double minPosition = 0.0;                              // 位置下限，rad或m
    double maxPosition = 0.0;                              // 位置上限，rad或m

    double maxVelocity = 0.0;                              // 速度上限，rad/s或m/s
    double maxAcceleration = 0.0;                          // 加速度上限
    double maxDeceleration = 0.0;                          // 减速度上限，填写正数
    double maxJerk = 0.0;                                  // 加加速度上限
};

// 关节运动参数：模型轴数个关节同步到达目标。
struct ST_AxisGroupJointMotion
{
    double velocityRatio = 0.1;                            // 速度比例，范围(0,1]
    double accelerationRatio = 0.1;                        // 加速度比例，范围(0,1]
    double decelerationRatio = 0.1;                        // 减速度比例，范围(0,1]
    double jerkRatio = 0.1;                                // 加加速度比例，范围(0,1]
};

// 笛卡尔运动参数：平移和旋转约束同时生效。
struct ST_AxisGroupPathMotion
{
    double velocity = 0.0;                                 // TCP平移速度上限，m/s
    double acceleration = 0.0;                             // TCP平移加速度上限，m/s²
    double deceleration = 0.0;                             // TCP平移减速度上限，m/s²
    double jerk = 0.0;                                     // TCP平移加加速度上限，m/s³

    double angularVelocity = 0.0;                          // TCP角速度上限，rad/s
    double angularAcceleration = 0.0;                      // TCP角加速度上限，rad/s²
    double angularDeceleration = 0.0;                      // TCP角减速度上限，rad/s²
    double angularJerk = 0.0;                              // TCP角加加速度上限，rad/s³
};

