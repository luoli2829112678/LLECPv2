#include"../../SMbasic.h"
class MC_SetModesOfOperation:public SMbasic
{
private:
    void Execute()override;
    double m_nModesOfOperation;
    /* data */
public:
    MC_SetModesOfOperation(/* args */);
    ~MC_SetModesOfOperation();
    void operator()(CIA402Axis* axis);
    void operator()(CIA402Axis* axis,bool bExecute,int nModesOfOperation,bool &bDone, bool &bError,int &nErrorID);
};

