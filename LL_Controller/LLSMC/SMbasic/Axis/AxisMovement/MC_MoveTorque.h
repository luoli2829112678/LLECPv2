#include"../../SMbasic.h"
class MC_MoveTorque:public SMbasic
{
private:

    void Execute()override;
    void CheckRunStatus()override;

public:
    void operator()(CIA402Axis* axis);
    void operator()(CIA402Axis* axis,bool bExecute,double dTorque,double dTorque_dot,double dpTorque_ddot,double dnTorque_ddot,double dTorque_dddot,EN_Direction enDirection,EN_BufferMode enBufferMode,
                    bool& bDone,bool& bBusy,bool& bCommandAborted,bool& bError,int& ErrorID);
    MC_MoveTorque(/* args */);
    ~MC_MoveTorque();
};

