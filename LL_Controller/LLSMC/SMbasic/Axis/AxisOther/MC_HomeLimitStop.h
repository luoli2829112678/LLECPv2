#include"../../SMbasic.h"

class MC_HomeLimitStop:public SMbasic
{
private:
    FS_HomeLimitStop m_fsHome;
    ST_RT_MotionPlanPointData* m_stRTMotionPlanPointData = nullptr;
    void Execute()override;
    void CheckRunStatus()override;

public:
    void operator()(CIA402Axis* axis);
    void operator()(CIA402Axis* axis,bool bExecute,double dDeceleration,double dJerk,
                    bool& bDone,bool& bBusy,bool& bCommandAborted,bool& bError,int& ErrorID);
    MC_HomeLimitStop(/* args */);
    ~MC_HomeLimitStop();
};

