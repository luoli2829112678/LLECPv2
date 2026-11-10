#include"../../SMbasic.h"
class MC_Home :public SMbasic
{
private:
    FS_Home m_fsHome;
    void Execute()override;
public:
    MC_Home(/* args */);
    ~MC_Home();

    void operator()(CIA402Axis* axis);
    void operator()(CIA402Axis* axis,bool bExecute,double dPos,bool &bBusy,bool &bCommandAborted,bool &bDone, bool &bError,int &nErrorID);
    void operator()(CIA402Axis* axis,bool bExecute,bool &bBusy,bool &bCommandAborted,bool &bDone, bool &bError,int &nErrorID);
};
