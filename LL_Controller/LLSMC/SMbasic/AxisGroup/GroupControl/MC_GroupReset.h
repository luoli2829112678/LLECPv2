// MC_GroupReset.h
#pragma once

#include "../../SMbasic.h"
#include <chrono>
#include <cstdint>

class MC_GroupReset : public SMbasic
{
private:
    bool m_bReady = false;                                // 本次操作是否已初始化
    std::chrono::steady_clock::time_point m_tpResetStart{}; // 复位开始时间

    void Execute() override;

public:
    MC_GroupReset();
    ~MC_GroupReset() = default;

    void operator()(AxisGroup* group);
    void operator()(AxisGroup* group, bool bExecute, bool& bBusy, bool& bDone, bool& bError, int& nErrorID);
};