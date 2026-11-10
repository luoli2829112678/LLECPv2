#include "MC_GroupInitReset.h"

MC_GroupInitReset::MC_GroupInitReset()
{
    cwInitReset = en_ControlWord_ev | en_ControlWord_qs | en_ControlWord_fr; // 数值为 0x86

    m_bExecute = false;
    m_bBusy = false;
    m_bDone = false;
    m_bError = false;
    m_nErrorID = 0;
}

void MC_GroupInitReset::operator()(AxisGroup* group)
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

void MC_GroupInitReset::operator()(AxisGroup* group, bool bExecute, bool& bBusy, bool& bDone, bool& bError, int& nErrorID)
{
    m_bExecute = bExecute;

    if (!m_bExecute)
    {
        m_bReady = false; // 清除流程，停止继续写入
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

void MC_GroupInitReset::Execute()
{
    const auto SetError = [this](decltype(m_nErrorID) errorID)
    {
        m_bBusy = false;
        m_bDone = false;
        m_bError = true;
        m_nErrorID = errorID;
        m_bReady = false;
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

        // 只检查对象有效性，不检查状态字和运动状态。
        for (auto* pAxis : m_vCIA402Axis)
        {
            if (pAxis == nullptr)
            {
                SetError(SMEC_INVALID_GROUP);
                return;
            }
        }

        // 首次向全部轴写入 0x86。
        for (auto* pAxis : m_vCIA402Axis)
        {
            pAxis->Axis_PDO_SetControlword(cwInitReset);
        }

        // 从首次写入完成后开始计算持续时间。
        m_tpInitResetStart = std::chrono::steady_clock::now();
        m_bReady = true;
        m_bBusy = true;
        m_nErrorID = 0;
        return;
    }

    // 每个调用周期继续写入完整控制字 0x86。
    for (auto* pAxis : m_vCIA402Axis)
    {
        pAxis->Axis_PDO_SetControlword(cwInitReset);
    }

    // 只计算时间差，不阻塞等待。
    const auto elapsed = std::chrono::steady_clock::now() - m_tpInitResetStart;

    if (elapsed >= std::chrono::milliseconds(500))
    {
        m_bBusy = false;
        m_bDone = true; // 仅表示写入持续时间达到要求
        m_bError = false;
        m_nErrorID = 0;
        m_bReady = false;
        return;
    }
}