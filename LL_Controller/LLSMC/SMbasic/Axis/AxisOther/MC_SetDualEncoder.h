#include"../../SMbasic.h"
class MC_SetDualEncoder:public SMbasic
{
private:
    void Execute()override;
    double m_dPosition;
    ST_AxisDualEncoderConfiguration m_stAxisDualEncoderConfiguration;
    /* data */
public:
    MC_SetDualEncoder(/* args */);
    ~MC_SetDualEncoder();
    void operator()(CIA402Axis* axis);
    void operator()(CIA402Axis* axis,bool bExecute,
        ST_AxisDualEncoderConfiguration stAxisDualEncoderConfiguration,
        bool &bDone, bool &bError,int &nErrorID);
};

