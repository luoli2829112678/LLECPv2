// MC_GroupInitReset.h
#pragma once

#include "../../SMbasic.h"
#include <chrono>
#include <cstdint>

class MC_GroupInitReset : public SMbasic
{
private:
    bool m_bReady = false;                                    // 本次操作是否已初始化
    uint16_t cwInitReset;                                     // 初始化复位命令，数值为 0x86
    std::chrono::steady_clock::time_point m_tpInitResetStart{}; // 首次写入命令的时间

    void Execute() override;

public:
    MC_GroupInitReset();
    ~MC_GroupInitReset() = default;

    void operator()(AxisGroup* group);
    void operator()(AxisGroup* group, bool bExecute, bool& bBusy, bool& bDone, bool& bError, int& nErrorID);
};