#include "MC_GroupReset.h"

MC_GroupReset::MC_GroupReset()
{
    m_bExecute = false;
    m_bBusy = false;
    m_bDone = false;
    m_bError = false;
    m_nErrorID = 0;
}

void MC_GroupReset::operator()(AxisGroup* group)
{
    // 执行过程中禁止切换轴组。
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
    this->Execute();
}

void MC_GroupReset::operator()(AxisGroup* group, bool bExecute, bool& bBusy, bool& bDone, bool& bError, int& nErrorID)
{
    m_bExecute = bExecute;

    if (!m_bExecute)
    {
        m_bReady = false; // 清除流程，不继续发送复位命令
        m_bBusy = false;
        m_bDone = false;
        m_bError = false;
        m_nErrorID = 0;
        m_pAxisGroup = group;
    }
    else
    {
        (*this)(group);
    }

    bBusy = m_bBusy;
    bDone = m_bDone;
    bError = m_bError;
    nErrorID = m_nErrorID;
}

void MC_GroupReset::Execute()
{
    const auto SetError = [this](decltype(m_nErrorID) errorID)
    {
        m_bBusy = false;
        m_bDone = false;
        m_bError = true;
        m_nErrorID = errorID;
        m_bReady = false;
    };

    // 生成保留其他位、仅清除 Fault Reset 位的控制字。
    const auto ClearFaultReset = [](uint16_t controlWord) -> uint16_t
    {
        return static_cast<uint16_t>(controlWord & static_cast<uint16_t>(~en_ControlWord_fr));
    };

    if (m_bDone || m_bError)
        return;

    if (m_pAxisGroup == nullptr)
    {
        SetError(SMEC_INVALID_GROUP);
        return;
    }

    if (!m_bReady)
    {
        m_vCIA402Axis = m_pAxisGroup->GetAxisList();

        if (m_vCIA402Axis.empty())
        {
            SetError(SMEC_INVALID_GROUP);
            return;
        }

        // 先验证全部指针，避免验证一半就开始写控制字。
        for (auto* pAxis : m_vCIA402Axis)
        {
            if (pAxis == nullptr)
            {
                SetError(SMEC_INVALID_GROUP);
                return;
            }
        }

        // 先将复位位清零，为下一周期的上升沿做准备。
        for (auto* pAxis : m_vCIA402Axis)
        {
            const uint16_t nControlWord = pAxis->Axis_GetControlWord();
            pAxis->Axis_PDO_SetControlword(ClearFaultReset(nControlWord));
        }

        m_tpResetStart = std::chrono::steady_clock::now();
        m_bReady = true;
        m_bBusy = true;
        m_nErrorID = 0;
        return; // 本周期只输出低电平
    }

    bool allReset = true;

    for (auto* pAxis : m_vCIA402Axis)
    {
        const uint16_t nStatusWord = pAxis->Axis_GetStatusWord();
        const uint16_t nControlWord = pAxis->Axis_GetControlWord();

        // 该轴已经没有故障：清除复位位。
        if ((nStatusWord & en_StatusWord_f) == 0U)
        {
            pAxis->Axis_PDO_SetControlword(ClearFaultReset(nControlWord));
            continue;
        }

        allReset = false;

        // 故障反应尚未结束：bit3、bit2、bit1、bit0 均为 1。
        const uint16_t faultReactionMask = en_StatusWord_f | en_StatusWord_oe | en_StatusWord_so | en_StatusWord_rtso;

        if ((nStatusWord & faultReactionMask) == faultReactionMask)
        {
            // 等待进入 Fault 状态，保持复位位为 0。
            pAxis->Axis_PDO_SetControlword(ClearFaultReset(nControlWord));
            continue;
        }

        // 故障轴置位 bit7，产生复位上升沿，其他控制位保持原值。
        const uint16_t newControlWord = static_cast<uint16_t>(nControlWord | en_ControlWord_fr);
        pAxis->Axis_PDO_SetControlword(newControlWord);
    }

    if (allReset)
    {
        m_bBusy = false;
        m_bDone = true; // 表示驱动故障位已清除，不代表驱动已经使能
        m_bError = false;
        m_nErrorID = 0;
        m_bReady = false;
        return;
    }

    const auto elapsed = std::chrono::steady_clock::now() - m_tpResetStart;

    if (elapsed >= std::chrono::seconds(5))
    {
        // 超时退出前清除复位位，避免持续保持复位请求。
        for (auto* pAxis : m_vCIA402Axis)
        {
            const uint16_t nControlWord = pAxis->Axis_GetControlWord();
            pAxis->Axis_PDO_SetControlword(ClearFaultReset(nControlWord));
        }

        SetError(SMEC_GROUP_STATUS_INTERCEPTION); // 可替换为专用复位超时错误码
        return;
    }
}