#include"../../SMbasic.h"


class MC_HomeLimit :public SMbasic
{
private:
    FS_HomeLimit m_fsHome;
    ST_RT_MotionPlanPointData* m_stRTMotionPlanPointData = nullptr;
    void Execute()override;
public:
    MC_HomeLimit(/* args */);
    ~MC_HomeLimit();

    void operator()(CIA402Axis* axis);
    void operator()(CIA402Axis* axis,bool bExecute,double dPos,bool &bBusy,bool &bCommandAborted,bool &bDone, bool &bError,int &nErrorID);
    void operator()(CIA402Axis* axis,bool bExecute,bool &bBusy,bool &bCommandAborted,bool &bDone, bool &bError,int &nErrorID);
};
