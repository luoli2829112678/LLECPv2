
#include"../../SMbasic.h"
class MC_FollowJointTorque:public SMbasic
{
private:
    double m_dTorque;
    void Execute()override;
public:
    MC_FollowJointTorque(/* args */);
    ~MC_FollowJointTorque();

    void operator()(CIA402Axis* axis);
    void operator()(CIA402Axis* axis,bool bExecute,double dTorque,bool& bBusy,bool& bError,int& ErrorID);
};

