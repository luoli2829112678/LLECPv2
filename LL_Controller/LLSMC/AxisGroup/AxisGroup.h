#pragma once
#include"../CIA402Axis/CIA402Axis.h"
#include "AxisGroupFriendLists.h"
#include"../SMCAlgorithm/AxisGroupAlgorithStdafx.h"
#include"AxisGroupDef.h"
#include<vector>
class AxisGroup
{
    // 声明轴组功能块为友元
    AXISGROUP_FRIEND_CLASSES
public:
    AxisGroup(/* args */);
    ~AxisGroup();
    int InitGroup(ENAxisGroupModel pAlgorithmModel,std::vector<CIA402Axis*>vpAxis,ST_AxisGroupDH dh, double cycleTime);
    void AxisGruop_RT();
    int ResetError();
    int Group_CheckError(int nErrorID);
    int Group_PCS2ACS(ST_AxisGroupPose pcs ,ST_AxisGroupJointPosition ref_acs,ST_AxisGroupJointPosition &acs);
    int Group_ACS2PCS(ST_AxisGroupJointPosition acs,ST_AxisGroupPose &pcs );
    EN_ModesOfOperation Axis_GetActualOperationMode();
    ENAxisGroupState Group_ReadGroupState();
    std::vector<CIA402Axis*> GetAxisList();
    ST_GroupPosition Axis_GetSetPosition() const;      // 获取命令位置
    ST_GroupPosition Axis_GetSetVelocity() const;      // 获取命令速度
    ST_GroupPosition Axis_GetSetAcceleration() const;  // 获取命令加速度
public:
    ST_AxisGroupJointPosition m_AxisGroupJointPosition;
    ST_AxisGroupPose m_AxisGroupPose;
    ENAxisGroupState m_enGroupState;
    //当前位置
    ST_AxisGroupJointPosition m_stPosition_Acs;
    ST_AxisGroupJointPosition m_stPose_Pcs;
    EN_ModesOfOperation m_enModesOfOperation;
protected:
    //轴列表
    std::vector<CIA402Axis*>m_vpAxis;
    //运动学模型
    ENAxisGroupModel m_enAlgorithmModel;
    //DH参数
    ST_AxisGroupDH m_DH;
    //控制周期
    double m_CycleTime;
    //算法
    AxisGroupAlgorithmBase* m_pAxisGroupAlgorithm;
    //实时运动参数
    ST_GroupMotorMotionData m_stGroupMotorMotionData;
    //规划器缓存
    std::vector<ST_GroupMotionUint> m_vGroupMotorMotionBuffer;
    
protected:
    void AxisGroupStateCheck();
    void AxisGroupStateCange(ENAxisGroupState enGroupState);
    //功能块运动控制命令
    int AxisGroupFollowACS(ST_AxisGroupJointPosition acs);
    int AxisGroupFollowPCS(ST_AxisGroupPose pcs);
    int Group_SetGroupState(ENAxisGroupState state);
    int Group_PushMotionUint(ST_GroupMotionUint stGroupMotionUint);
    int Group_GetMotionUint(ST_GroupMotionUint& stGroupMotionUint);
};

