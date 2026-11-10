#include "LLC_Rbt.h"

// 获取IF
LLCIFHandle* LLC_Rbt::GetIFHandle(uint16_t nPort)
{
    for (auto& it : m_IFHandleMap)
    {
        if (it.first == nPort)
            return it.second.GetIfHandle();
    }
    return nullptr;
}
// 获取CPS客户端
TcpClient* LLC_Rbt::GetCPSClient()
{
    return &m_CPSClient;
}
// 发送命令到CPS端口
int LLC_Rbt::SendCmd2CPS()
{
    // 发送消息到10003端口
    if (m_bSendCmd2CPS10003)
    {
        m_bSendCmd2CPS10003 = false;
        return m_CPSClient.SendCmd(m_sSendCmd2CPS);
    }
    // 返回消息到CPS8888端口
    if (m_bSendCmd2CPS8888)
    {
        m_bSendCmd2CPS8888 = false;
        vector<string> vsResult;
        vsResult.push_back(to_string(m_nSendResult2CPS8888));
        if (m_IFHandleMap[PORT_NORMALCMD].m_ifhandle) m_IFHandleMap[PORT_NORMALCMD].m_ifhandle->SetCmdResult(vsResult);
    }
    // 返回消息到CPS8889端口
    if (m_bSendCmd2CPS8889)
    {
        m_bSendCmd2CPS8889 = false;
        vector<string> vsResult;
        vsResult.push_back(to_string(m_nSendResult2CPS8889));
        if (m_IFHandleMap[PORT_FOLLOWMOTIONCMD].m_ifhandle) m_IFHandleMap[PORT_FOLLOWMOTIONCMD].m_ifhandle->SetCmdResult(vsResult);
    }
    return REC_Successed;
}

// 发送命令到CPS
void LLC_Rbt::SendCmd2CPS10003(string sCmd)
{
    // 发送命令到CPS消息
    m_sSendCmd2CPS = sCmd;
     // 发送命令到CPS标志
    m_bSendCmd2CPS10003 = true;
    return;
}
// 发送命令到CPS
void LLC_Rbt::SendCmd2CPS8888(int nResult)
{
    // 发送命令到CPS消息
    m_nSendResult2CPS8888 = nResult;
     // 发送命令到CPS标志
    m_bSendCmd2CPS8888 = true;
    return;
}
// 发送命令到CPS
void LLC_Rbt::SendCmd2CPS8889(int nResult)
{
    // 发送命令到CPS消息
    m_nSendResult2CPS8889 = nResult;
     // 发送命令到CPS标志
    m_bSendCmd2CPS8889 = true;
    return;
}

// 发送命令到CPS端口并等待返回结果
int LLC_Rbt::SendCmd2CPSAndWaitResult(const string& strCmd, string& strRst)
{
    return m_CPSClient.SendCmdAndWaitResult(strCmd, strRst);
}
// 发送命令执行结果到8888端口
void LLC_Rbt::SendResult2CPSNormalPort(const int nResult)
{
    vector<string> vsResult;
    vsResult.push_back(to_string(nResult));
    if (m_IFHandleMap[PORT_NORMALCMD].m_ifhandle) m_IFHandleMap[PORT_NORMALCMD].m_ifhandle->SetCmdResult(vsResult);
}