#pragma once
#include<vector>
#include <cmath>
#include"../CIA402Axis/CIA402Axis.h"
#include"../AxisGroup/AxisGroup.h"
#include"SMbasicErrorCode.h"
#include"../SMCAlgorithm/AxisGroupAlgorithmBase.h"
#define DEBUG_TRAJECTORY_PLANNING 0
struct SoftMotionPlanParams;


class SoftMotion
{
protected:
    std::vector<CIA402Axis*>m_v_Axis;
    std::vector<AxisGroup*>m_v_AxisGroup;
    uint16_t nAxisCount;
    double m_dSoftMotionCycle = 0.001;
    std::vector<SoftMotionPlanParams> m_vSoftMotionPlanParams;
    //double dMaxSnap = 20000000;
protected:
    void AxisRealTime();
    void SoftMotionPlanner_RT();
    int AxisMotionPlanner(CIA402Axis* pAxis);
    void SoftMotionStateCheck();
public:
    SoftMotion();
    ~SoftMotion();
    int RegisterAxis(std::vector<CIA402Axis*> v_Axis);
    int RegisterAxisGroup(std::vector<AxisGroup*> v_AxisGruop);
    void SoftMotionRun();
    int SetSoftMotionCycle(double dCycle);

    bool bError = false;
    bool bMoving = false;
};


struct SoftMotionPlanParams
{
    //规划参数
    ST_PlanParams stActParam;
    ST_PlanData trackData;

    //当前运动帧参数
    ST_InterParams stInterData;
};