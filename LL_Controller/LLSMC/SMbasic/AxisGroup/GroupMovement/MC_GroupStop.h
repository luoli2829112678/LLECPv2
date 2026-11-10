// MC_GroupStop.h
#pragma once

#include "../../SMbasic.h"
#include <chrono>
#include <cstdint>

class MC_GroupStop : public SMbasic
{
private:
    bool m_bReady = false;                                // 本次操作是否已初始化
    std::chrono::steady_clock::time_point m_tpResetStart{}; // 复位开始时间

    void Execute() override;
    void CheckRunStatus()override;
public:
    MC_GroupStop();
    ~MC_GroupStop();

    void operator()(AxisGroup* group);
    void operator()(AxisGroup* group,bool bExecute,double dDeceleration,double dJerk,
                    bool& bDone,bool& bBusy,bool& bCommandAborted,bool& bError,int& ErrorID);
};