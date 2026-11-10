#pragma once

enum ENAxisGroupState
{
    GroupDisabled,   // 轴组协调运动未启用
    GroupStandby,    // 已启用，当前无协调运动
    GroupMoving,     // 执行协调运动
    GroupStopping,   // 执行停止或保持停止锁定
    GroupErrorStop   // 轴组故障停止
};


struct ST_GroupMotionUintParam
{
    EN_GroupMotionType enGroupMotionType;   //运动类型
    ST_GroupPosition CommandPosition;      // 命令位置
    ST_GroupPosition CommandVelocity;      // 命令速度
    ST_GroupPosition CommandAcceleration;  // 命令加速度
    ST_GroupPosition CommandDeceleration;  // 命令加速度
    ST_GroupPosition CommandJerk;          // 命令加加速度
    double dJointVelocity;                 //关节速度
    double dTCPVelocity;                   //TCP速度

};


struct ST_GroupMotionUint
{
    ST_GroupMotionUintParam  PlanningMotionParam;
    EN_BufferMode BufferMode;
    EN_AxisMoveType MoveType;
    bool bIsStoping;
    void* fbID;//功能块指针
    //回调函数
    std::function<void(bool, bool, bool, bool, int)> fCallback;
    bool bMotion;
    bool bMotionDone;
    void SetFBStauts(bool bBusy,bool bDone,bool bCommandAborted,bool bError,int nErrorID)
    {
        bMotionDone = bDone;
        bMotion = bBusy;
        if (fCallback) 
            fCallback(bBusy,bDone,bCommandAborted,bError,nErrorID);
        return;
    }
    ST_GroupMotionUint():bMotionDone(false),bMotion(false){}
};

