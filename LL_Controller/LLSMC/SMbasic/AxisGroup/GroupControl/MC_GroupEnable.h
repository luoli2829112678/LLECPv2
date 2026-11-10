#include"../../SMbasic.h"
class MC_GroupEnable :public SMbasic
{
private:
    FS_PowerOn m_fsPowerOn;
    void Execute()override;
    bool m_bReady = false;
    // 控制字命令及掩码。
    uint16_t cwShutdown;            // Shutdown 命令
    uint16_t cwSwitchOn;            // Switch On 命令
    uint16_t cwEnableOperation;     // Enable Operation 命令
    uint16_t cwCommandMask;         // 控制字命令掩码

    uint16_t swBaseMask;            // 基础状态掩码
    uint16_t swStateMask;           // 运行状态掩码
    uint16_t swReadyToSwitchOn;     // 准备上电状态
    uint16_t swSwitchedOn;          // 已上电状态
    uint16_t swOperationEnabled;    // 已使能运行状态
    uint16_t swQuickStopActive;     // 快停激活状态
    std::chrono::steady_clock::time_point m_tpEnableStart{};
public:
    MC_GroupEnable(/* args */);
    ~MC_GroupEnable();

    void operator()(AxisGroup* group);
    void operator()(AxisGroup* group,bool bExecute,bool &bBusy,bool &bDone, bool &bError,int &nErrorID);
};
