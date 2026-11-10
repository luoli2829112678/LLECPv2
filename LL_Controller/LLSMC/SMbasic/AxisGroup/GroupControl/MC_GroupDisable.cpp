#include "MC_GroupDisable.h"

MC_GroupDisable::MC_GroupDisable()
{
    cwDisableOperation = en_ControlWord_so | en_ControlWord_ev | en_ControlWord_qs; // 数值为 7
    cwShutdown = en_ControlWord_ev | en_ControlWord_qs; // 数值为 6

    swBaseMask = en_StatusWord_rtso | en_StatusWord_so | en_StatusWord_oe | en_StatusWord_f | en_StatusWord_sod; // 基础状态掩码
    swStateMask = swBaseMask | en_StatusWord_qs; // 增加 bit5
    swReadyToSwitchOn = en_StatusWord_rtso | en_StatusWord_qs; // 准备上电
    swSwitchedOn = en_StatusWord_rtso | en_StatusWord_so | en_StatusWord_qs; // 已上电
    swOperationEnabled = en_StatusWord_rtso | en_StatusWord_so | en_StatusWord_oe | en_StatusWord_qs; // 已使能运行

    m_bReady = false;
    m_bExecute = false;
    m_bBusy = false;
    m_bDone = false;
    m_bError = false;
    m_nErrorID = 0;
}

void MC_GroupDisable::operator()(AxisGroup* group)
{
    // 执行过程中禁止切换轴组，原轴组和轴对象必须保持有效。
    if (m_bBusy && group != m_pAxisGroup)
    {
        m_bBusy = false;
        m_bDone = false;
        m_bError = true;
        m_nErrorID = SMEC_ABNORMAL_AXIS_CHANGE;
        m_bReady = false;
        return;
    }

    m_pAxisGroup = group;
    this->Execute(); // 无 Execute 输入的重载：每次调用均推进流程
}

void MC_GroupDisable::operator()(AxisGroup* group, bool bExecute, bool& bBusy, bool& bDone, bool& bError, int& nErrorID)
{
    m_bExecute = bExecute;

    if (!m_bExecute)
    {
        m_bReady = false; // 清除流程，不发送新的驱动命令
        m_bBusy = false;
        m_bDone = false;
        m_bError = false;
        m_nErrorID = 0;
        m_pAxisGroup = group;
    }
    else
    {
        (*this)(group); // 调用上面的重载，检查轴组并推进流程
    }

    bBusy = m_bBusy;
    bDone = m_bDone;
    bError = m_bError;
    nErrorID = m_nErrorID;
}

void MC_GroupDisable::Execute()
{
    // 统一处理错误，结束本次操作。
    const auto SetError = [this](decltype(m_nErrorID) errorID)
    {
        m_bBusy = false;
        m_bDone = false;
        m_bError = true;
        m_nErrorID = errorID;
        m_bReady = false;
    };

    // 完成或出错后保持输出，等待外部复位。
    if (m_bDone || m_bError)
        return;

    // 检查轴组指针。
    if (m_pAxisGroup == nullptr)
    {
        SetError(SMEC_INVALID_GROUP);
        return;
    }

    // 本次操作只初始化一次。
    if (!m_bReady)
    {
        m_vCIA402Axis = m_pAxisGroup->GetAxisList();

        if (m_vCIA402Axis.empty())
        {
            SetError(SMEC_INVALID_GROUP);
            return;
        }

        // 全部检查通过后，才允许写控制字。
        for (auto* pAxis : m_vCIA402Axis)
        {
            if (pAxis == nullptr)
            {
                SetError(SMEC_INVALID_GROUP);
                return;
            }

            const auto motionState = pAxis->Axis_GetAxisMotionState();

            // 保留原来的启动限制：运动中的轴应先停止。
            if (motionState != EN_AxisMotionState::motionState_power_off && motionState != EN_AxisMotionState::motionState_standstill)
            {
                SetError(SMEC_GROUP_STATUS_INTERCEPTION);
                return;
            }
        }

        m_tpDisableStart = std::chrono::steady_clock::now();
        m_bReady = true;
        m_bBusy = true;
        m_nErrorID = 0;
    }

    // 先检查全部轴是否处于允许处理的状态。
    for (auto* pAxis : m_vCIA402Axis)
    {
        const uint16_t nStatusWord = pAxis->Axis_GetStatusWord();
        const uint16_t baseState = static_cast<uint16_t>(nStatusWord & swBaseMask);
        const uint16_t state = static_cast<uint16_t>(nStatusWord & swStateMask);

        // 故障状态不在此处执行复位，也不跳过顺序直接写 0。
        if ((nStatusWord & en_StatusWord_f) != 0U)
        {
            SetError(SMEC_GROUP_STATUS_INTERCEPTION);
            return;
        }

        // 接受正常退出路径，以及未准备好状态。
        if (state == swOperationEnabled || state == swSwitchedOn || state == swReadyToSwitchOn || baseState == en_StatusWord_sod || baseState == 0U)
            continue;

        // 快停等其他状态不在本流程中自动处理。
        SetError(SMEC_GROUP_STATUS_INTERCEPTION);
        return;
    }

    bool allDisabled = true;

    // 根据各轴实际反馈，分别推进去使能步骤。
    for (auto* pAxis : m_vCIA402Axis)
    {
        const uint16_t nStatusWord = pAxis->Axis_GetStatusWord();
        const uint16_t baseState = static_cast<uint16_t>(nStatusWord & swBaseMask);
        const uint16_t state = static_cast<uint16_t>(nStatusWord & swStateMask);

        // 已禁止上电：保持控制字为 0，当前轴完成。
        if (baseState == en_StatusWord_sod)
        {
            pAxis->Axis_PDO_SetControlword(en_ControlWord_Init);
            continue;
        }

        // 当前轴尚未到达最终状态。
        allDisabled = false;

        // 已使能运行：先写 7，等待驱动退出 Operation Enabled。
        if (state == swOperationEnabled)
        {
            pAxis->Axis_PDO_SetControlword(cwDisableOperation);
        }
        // 已上电、未使能运行：再写 6，等待 Ready To Switch On。
        else if (state == swSwitchedOn)
        {
            pAxis->Axis_PDO_SetControlword(cwShutdown);
        }
        // 准备上电：最后写 0，等待 Switch On Disabled。
        else if (state == swReadyToSwitchOn)
        {
            pAxis->Axis_PDO_SetControlword(en_ControlWord_Init);
        }
        // 未准备好：保持 0，等待进入可确认的禁止上电状态。
        else if (baseState == 0U)
        {
            pAxis->Axis_PDO_SetControlword(en_ControlWord_Init);
        }
        else
        {
            SetError(SMEC_GROUP_STATUS_INTERCEPTION);
            return;
        }
    }

    // 全部轴均反馈禁止上电，才报告完成。
    if (allDisabled)
    {
        m_bBusy = false;
        m_bDone = true;
        m_bError = false;
        m_nErrorID = 0;
        m_bReady = false;
        return;
    }

    // 非阻塞超时检查。
    const auto elapsed = std::chrono::steady_clock::now() - m_tpDisableStart;

    if (elapsed >= std::chrono::seconds(5))
    {
        SetError(SMEC_GROUP_STATUS_INTERCEPTION); // 可替换为专用超时错误码
        return;
    }
}