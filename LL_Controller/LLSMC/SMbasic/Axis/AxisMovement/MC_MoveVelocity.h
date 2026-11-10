#include"../../SMbasic.h"
class MC_MoveVelocity:public SMbasic
{
private:
    void CheckRunStatus()override;
    void Execute()override;
    bool m_bInVelocity;
public:
    void operator()(CIA402Axis* axis);
    //CSP
    void operator()(CIA402Axis* axis,bool bExecute,double dVelocity,double dAcceleration,double dDeceleration,double dJerk,EN_Direction enDirection,
                    bool& bInVelocity,bool& bBusy,bool& bCommandAborted,bool& bError,int& ErrorID);
    //CSV
    void operator()(CIA402Axis* axis,bool bExecute,double dVelocity,double dAcceleration,double dPJerk,double dNJerk,double dSnap,EN_Direction enDirection,EN_BufferMode enBufferMode,
                    bool& bDone,bool& bBusy,bool& bCommandAborted,bool& bError,int& ErrorID);
    MC_MoveVelocity(/* args */);
    ~MC_MoveVelocity();
};
