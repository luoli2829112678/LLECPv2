#include"MC_GroupEnable.h"
MC_GroupEnable::MC_GroupEnable(/* args */)
{
    cwShutdown = en_ControlWord_ev | en_ControlWord_qs; // Shutdown，数值为 6
    cwSwitchOn = en_ControlWord_so | en_ControlWord_ev | en_ControlWord_qs; // Switch On，数值为 7
    cwEnableOperation = en_ControlWord_so | en_ControlWord_ev | en_ControlWord_qs | en_ControlWord_eo; // Enable Operation，数值为 15
    cwCommandMask = en_ControlWord_so | en_ControlWord_ev | en_ControlWord_qs | en_ControlWord_eo | en_ControlWord_fr; // 控制字命令掩码

    swBaseMask = en_StatusWord_rtso | en_StatusWord_so | en_StatusWord_oe | en_StatusWord_f | en_StatusWord_sod; // 基础状态掩码
    swStateMask = swBaseMask | en_StatusWord_qs; // 运行状态掩码
    swReadyToSwitchOn = en_StatusWord_rtso | en_StatusWord_qs; // 准备上电
    swSwitchedOn = en_StatusWord_rtso | en_StatusWord_so | en_StatusWord_qs; // 已上电，尚未使能运行
    swOperationEnabled = en_StatusWord_rtso | en_StatusWord_so | en_StatusWord_oe | en_StatusWord_qs; // 已使能运行
    swQuickStopActive = en_StatusWord_rtso | en_StatusWord_so | en_StatusWord_oe; // 快停激活
    m_bExecute =false;
    m_bBusy = false;
    m_bDone = false;
    m_bError = false;
    m_nErrorID = false;
    m_fsPowerOn = ReadyPowerOn;
}

MC_GroupEnable::~MC_GroupEnable()
{
}

void MC_GroupEnable::operator()(AxisGroup* group)
{
    m_pAxisGroup = group;
    this->Execute();
}
void MC_GroupEnable::operator()(AxisGroup* group,bool bExecute,bool &bBusy,bool &bDone, bool &bError,int &nErrorID)
{
    m_pAxisGroup = group;
    m_bExecute = bExecute;
    if(!m_bExecute)
    {
        m_bReady = false;
        m_fsPowerOn = ReadyPowerOn;
        m_bBusy = false;
        m_bError = false;
        m_bDone = false;
        m_nErrorID = SMEC_ABNORMAL_AXIS_CHANGE;
    }
    this->Execute();
    bBusy = m_bBusy;
    bDone = m_bDone;
    bError = m_bError;
    nErrorID = m_nErrorID;
    return;
}

void MC_GroupEnable::Execute()
{
    // 统一处理错误，结束本次执行。
    const auto SetError = [this](decltype(m_nErrorID) errorID)
    {
        m_bBusy = false;             // 结束执行
        m_bDone = false;             // 未完成使能
        m_bError = true;             // 设置错误标志
        m_nErrorID = errorID;        // 保存错误码
        m_bReady = false;            // 清除初始化标志
        m_fsPowerOn = ReadyPowerOn;  // 恢复原有流程状态
    };

    // 完成或出错后保持输出，等待外部启动逻辑复位。
    if (m_bDone || m_bError)
        return;

    // 检查轴组指针。
    if (m_pAxisGroup == nullptr)
    {
        SetError(SMEC_INVALID_GROUP);
        return;
    }

    // 本次执行只初始化一次。
    if (!m_bReady)
    {
        // 获取并保存本次操作的轴列表。
        m_vCIA402Axis = m_pAxisGroup->GetAxisList();

        // 空轴组不能执行使能。
        if (m_vCIA402Axis.empty())
        {
            SetError(SMEC_INVALID_GROUP);
            return;
        }

        // 先检查全部轴，检查通过后才允许下发控制字。
        for (auto* pAxis : m_vCIA402Axis)
        {
            // 检查轴指针。
            if (pAxis == nullptr)
            {
                SetError(SMEC_INVALID_GROUP);
                return;
            }

            // 读取轴的软件运动状态。
            const auto motionState = pAxis->Axis_GetAxisMotionState();

            // 启动时仅允许掉使能轴和静止轴。
            if (motionState != EN_AxisMotionState::motionState_power_off &&
                motionState != EN_AxisMotionState::motionState_standstill)
            {
                SetError(SMEC_GROUP_STATUS_INTERCEPTION);
                return;
            }
        }

        // 记录使能开始时间。
        m_tpEnableStart = std::chrono::steady_clock::now();

        m_bReady = true;   // 初始化完成
        m_bBusy = true;    // 开始执行
        m_bDone = false;   // 尚未完成
        m_bError = false;  // 清除错误标志
        m_nErrorID = 0;    // 清除错误码
    }

    // 下发控制字前，先检查全部轴是否故障或处于快停。
    for (auto* pAxis : m_vCIA402Axis)
    {
        // 获取当前轴的状态字。
        const uint16_t nStatusWord = pAxis->Axis_GetStatusWord();

        // bit3 为 1：驱动故障或正在进行故障反应。
        if ((nStatusWord & en_StatusWord_f) != 0U)
        {
            SetError(SMEC_GROUP_STATUS_INTERCEPTION);
            return;
        }

        // 快停中的轴不在这里自动恢复使能。
        if ((nStatusWord & swStateMask) == swQuickStopActive)
        {
            SetError(SMEC_GROUP_STATUS_INTERCEPTION);
            return;
        }
    }

    // 只有全部驱动反馈已使能，才允许报告完成。
    bool allEnabled = true;

    // 各轴根据自身状态独立推进使能流程。
    for (auto* pAxis : m_vCIA402Axis)
    {
        // 读取当前轴状态字。
        const uint16_t nStatusWord = pAxis->Axis_GetStatusWord();

        // 已使能的轴直接跳过，不修改控制字。
        if ((nStatusWord & swStateMask) == swOperationEnabled)
            continue;
        // 发现至少一根轴尚未使能。
        allEnabled = false;
        // 初始化本周期的控制命令。
        uint16_t command = en_ControlWord_Init;

        // Not Ready To Switch On：等待驱动初始化完成。
        if ((nStatusWord & swBaseMask) == 0U)
        {
            continue;
        }
        // Switch On Disabled：下发 6。
        else if ((nStatusWord & swBaseMask) == en_StatusWord_sod)
        {
            command = cwShutdown;
        }
        // Ready To Switch On：下发 7。
        else if ((nStatusWord & swStateMask) == swReadyToSwitchOn)
        {
            command = cwSwitchOn;
        }
        // Switched On：下发 15。
        else if ((nStatusWord & swStateMask) == swSwitchedOn)
        {
            command = cwEnableOperation;
        }
        // 未识别的状态：结束本次使能。
        else
        {
            SetError(SMEC_GROUP_STATUS_INTERCEPTION);
            return;
        }
        // 读取当前轴原控制字。
        const uint16_t nControlWord = pAxis->Axis_GetControlWord();
        // 清除原状态机命令位，保留其他控制位。
        const uint16_t preservedControlWord = static_cast<uint16_t>(nControlWord & static_cast<uint16_t>(~cwCommandMask));
        // 将本周期命令写入状态机控制位。
        const uint16_t newControlWord = static_cast<uint16_t>(preservedControlWord | command);
        // 写当前轴，修正原代码写固定 m_pCIA402Axis 的问题。
        pAxis->Axis_PDO_SetControlword(newControlWord);
    }

    // 所有驱动均已反馈 Operation Enabled。
    if (allEnabled)
    {
        m_bBusy = false;   // 结束执行
        m_bDone = true;    // 驱动全部使能完成
        m_bError = false;  // 无错误
        m_nErrorID = 0;    // 清除错误码
        m_bReady = false;  // 清除初始化标志
        return;
    }

    // 计算本次使能已经持续的时间。
    const auto elapsed =std::chrono::steady_clock::now() - m_tpEnableStart;

    // 超过 5 秒仍未全部使能，则报错。
    if (elapsed >= std::chrono::seconds(5))
    {
        // 如果项目有专用超时错误码，应在这里替换。
        SetError(SMEC_GROUP_STATUS_INTERCEPTION);
        return;
    }
    return;
}