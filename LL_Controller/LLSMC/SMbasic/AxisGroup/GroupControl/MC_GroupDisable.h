// MC_GroupDisable.h
#pragma once

#include "../../SMbasic.h"
#include <chrono>
#include <cstdint>

class MC_GroupDisable : public SMbasic
{
private:
    bool m_bReady = false;                                  // 本次操作是否已初始化
    uint16_t swBaseMask;                                    // 基础状态掩码
    std::chrono::steady_clock::time_point m_tpDisableStart{}; // 掉使能开始时间
    uint16_t cwDisableOperation; // 控制字 7
    uint16_t cwShutdown;         // 控制字 6

    uint16_t swStateMask;        // 运行状态掩码
    uint16_t swReadyToSwitchOn;  // 准备上电
    uint16_t swSwitchedOn;       // 已上电、未使能运行
    uint16_t swOperationEnabled; // 已使能运行
    void Execute() override;

public:
    MC_GroupDisable();
    ~MC_GroupDisable() = default;

    void operator()(AxisGroup* group);
    void operator()(AxisGroup* group, bool bExecute, bool& bBusy, bool& bDone, bool& bError, int& nErrorID);
};