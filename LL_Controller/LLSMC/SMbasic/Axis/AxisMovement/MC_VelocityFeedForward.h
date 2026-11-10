
#include"../../SMbasic.h"
class MC_VelocityFeedForward:public SMbasic
{
private:
    double m_dVel;
    void Execute()override;
public:
    MC_VelocityFeedForward(/* args */);
    ~MC_VelocityFeedForward();

    void operator()(CIA402Axis* axis);
    void operator()(CIA402Axis* axis,bool bExecute,double delocity,bool& bBusy,bool& bError,int& ErrorID);
};

