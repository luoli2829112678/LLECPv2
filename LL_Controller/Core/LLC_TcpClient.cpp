#include "LLC_TcpClient.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>

TcpClient::TcpClient(const string& strHost, int nPort) : m_strHost(strHost), m_nPort(nPort)
{
    m_fd = -1;
    m_bReConnect = false;
    m_bSetDisconnectFlag = false;
    m_bExitFlag = false;
    m_pWorkThread = nullptr;
}

TcpClient::~TcpClient() { Shutdown(); }
void TcpClient::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_bExitFlag=true;m_bReConnect=false;
        if(m_fd>=0)shutdown(m_fd,SHUT_RDWR);
    }
    m_cv.notify_all();
    if(m_pWorkThread){if(m_pWorkThread->joinable())m_pWorkThread->join();delete m_pWorkThread;m_pWorkThread=nullptr;}
    Close(false);
}

int TcpClient::Connect()
{
    if (m_fd != -1) return 0;
    m_bExitFlag = false;
    int err = 0;
    do
    {
        // 创建服务端地址
        struct sockaddr_in ip_addr;
        ip_addr.sin_family = AF_INET;
        ip_addr.sin_port = htons(m_nPort);
        if (inet_pton(AF_INET, m_strHost.c_str(), &(ip_addr.sin_addr)) <= 0)
        {
            printf("Invalid IP Address!\n");
            err = -1;
            break;
        }

        // 创建新的套接字
        m_fd = socket(AF_INET, SOCK_STREAM, 0);  // -1 for errors
        if (m_fd == -1)
        {
            printf("Create Socket Error!\n");
            err = -1;
            break;
        }

        timeval timeout{1,0};
        setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        // 保活机制, 保持 TCP 连接
        int optval = 1;
        setsockopt(m_fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));

        // 连接到服务器端
        err = connect(m_fd, (struct sockaddr *)&ip_addr, sizeof(ip_addr));
        if (err == -1)
        {
            printf("Socket connect failed!\n");
            ::close(m_fd);
            m_fd = -1;
        }
        else
        {
            printf("Socket connect success!\n");
        }
    } while (false);

    //创建工作线程
    if (!m_pWorkThread)
    {
        m_pWorkThread = new thread(&TcpClient::Communication, this);
        
    }

    m_bReConnect = true;
    return err;
}

int TcpClient::Disconnect()
{
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_bSetDisconnectFlag = true;
    }
    m_cv.notify_one();
    return REC_Successed;
}

int TcpClient::Close(bool bReConnect/*  = true */)
{
    int err = 0;
    if (m_fd != -1)
    {
        err = ::close(m_fd);
    }
    m_fd = -1;
    m_bReConnect = bReConnect;
    return err;
}

bool TcpClient::IsConnected()
{
    return (m_fd != -1);
}

void TcpClient::ChkConnection()
{
    string strCmd = "IsSimulation,;";
    string strRst;
    if (Send(strCmd))
    {
        return;
    }
    Receive(strRst);
}

int TcpClient::Send(const string &strCmd)
{
    if (m_fd == -1)
        return -1;

    // 发送消息到服务器
    int send_count = ::send(m_fd, strCmd.c_str(), strCmd.size(), MSG_NOSIGNAL);
    if (send_count < 0)
    {
        printf("Send message failed!\n");
        Close();
        return -1;
    }
    return REC_Successed;
}

int TcpClient::Receive(string& strRst)
{
    if (m_fd == -1)
        return -1;

    char buffer[1024] = {0}; // 缓冲区大小

    // 从套接字读取数据
    int recv_count = ::recv(m_fd, buffer, sizeof(buffer), 0);
    if (recv_count <= 0)
    {
        printf("Receive message failed!\n");
        Close();
        return -1;
    }
    strRst.assign(buffer, static_cast<std::size_t>(recv_count));
    return REC_Successed;
}

void TcpClient::Communication()
{
    signal(SIGPIPE, SIG_IGN);
    int nInterval = 5000;  //5s检测一次连接状态或断链重连
    auto _timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(nInterval);
    while (true) 
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        bool bWait = m_cv.wait_until(lock, _timeout, [this]() { return (!m_tasklist.empty() || m_bExitFlag || m_bSetDisconnectFlag); });
        if (bWait)
        {
            if (m_fd == -1 && m_bReConnect)
                Connect();

            while (m_tasklist.size())
            {
                CmdTask task = std::move(m_tasklist.front());
                m_tasklist.pop();

                //发送
                int err = Send(task.cmd);
                if (!task.bWaitExeResult)
                    task.result.set_value(to_string(err));

                //接收
                string strResult;
                Receive(strResult);
                if (task.bWaitExeResult)
                    task.result.set_value(strResult);
            }

            if (m_bExitFlag)
            {
                printf("Communication thread exit.\n");
                break;
            }

            if (m_bSetDisconnectFlag)
            {
                printf("Disconnect to server.\n");
                m_bSetDisconnectFlag = false;
                Close(false);
            }
        }
        // else  //等待超时
        // {
        //     if (m_fd != -1)
        //         ChkConnection(); //检测连接状态
        //     else if (m_bReConnect)
        //         Connect(); //重连
        // }
        _timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(nInterval);  //重置超时时间
    }
}

int TcpClient::SendCmd(const string& strCmd)
{
    CmdTask task;
    task.cmd = strCmd;
    task.bWaitExeResult = false;
    std::future<string> future = task.result.get_future();
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_tasklist.push(std::move(task));
    }
    m_cv.notify_one();
    string strSendRst = future.get();
    return ((strSendRst == "0") ? 0 : -1);
}

int TcpClient::SendCmdAndWaitResult(const string& strCmd, string& strRst)
{
    CmdTask task;
    task.cmd = strCmd;
    task.bWaitExeResult = true;
    std::future<string> future = task.result.get_future();
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_tasklist.push(std::move(task));
    }
    m_cv.notify_one();
    strRst = future.get();
    return (strRst.size() ? 0 : -1);
}