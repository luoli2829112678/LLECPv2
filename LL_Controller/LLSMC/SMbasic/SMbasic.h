#ifndef SMbasic_H
#define SMbasic_H
#include"../SoftMotion/SoftMotion.h"
#include"../SoftMotion/SMbasicErrorCode.h"
#include"SMbasicDef.h"

class SMbasic
{
    friend class SoftMotion;
protected:
    CIA402Axis* m_pCIA402Axis = nullptr;
    std::vector<CIA402Axis *> m_vCIA402Axis;
    AxisGroup* m_pAxisGroup = nullptr;
    SMCTimer m_Timer;
    SMCTimer m_TimerTimeout;
    //回调函数
    std::function<void (bool, bool, bool, bool, int)> m_fCallback;

    //Move类
    bool m_bExecute;
    double m_dPosition;
    double m_dVelocity;
    double m_dAcceleration;
    double m_dDeceleration;
    double m_dJerk;
    EN_Direction m_enDirection;
    EN_BufferMode m_enBufferMode;
    
    ST_MotionUint m_MotionUint;
    ST_MotionUint m_MotionUint_New;
    ST_GroupMotionUint m_GroupMotionUint;
    ST_GroupMotionUint m_GroupMotionUint_New;

    //通用
    bool m_bBusy;
    bool m_bCommandAborted;
    bool m_bDone;
    bool m_bError;
    int m_nErrorID;
public:
    SMbasic(/* args */);
    ~SMbasic();
    void operator()(CIA402Axis* axis);
    CIA402Axis Axis();
    bool Busy();
    bool Done();
    bool Error();
    bool ErrorID();
    
protected:
    bool m_bBusy_last;
    bool m_bCommandAborted_last;
    bool m_bDone_last;
    bool m_bError_last;
    int m_nErrorID_last;
    int SetFBStatus(bool bBusy,bool bDone,bool bCommandAborted,bool bError,int nErrorID);
    virtual void CheckRunStatus();
    virtual void Execute();
};
#endif // SMbasic_H