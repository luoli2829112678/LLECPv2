#pragma once

#include <string>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <chrono>
#include <queue>


#include "LL_Errorcode.h"

using namespace std;
using namespace std::chrono;

struct CmdTask
{
    std::string cmd;              //命令
    std::promise<string> result;  //命令发送或执行的结果
    bool bWaitExeResult;  //是否等命令执行结果，如果“否”：result存放发送结果（0 或 -1）；如果“是”， result存放执行结果
};

//适用于一发一收的TCP Clent，如：CPS 10003的连接
//支持"只发送cmd，不等待执行结果" 和 "发送并等待执行结果"两种使用场景
//支持链路检测、断链重连
class TcpClient
{
public:
    TcpClient(const string& strHost, int nPort);
    virtual ~TcpClient();

    //是否已连接
    bool IsConnected();
    
    //连接服务器，如果失败，会定时重连
    int Connect();

    //断开服务器连接
    int Disconnect();
    void Shutdown();

    //只发送cmd，不等待执行结果。发送成功返回0，否则返回-1
    int SendCmd(const string& strCmd);

    //发送cmd，并等待执行结果。发送并等待成功返回0，结果保存在strRst中；否则返回-1
    int SendCmdAndWaitResult(const string& strCmd, string& strRst);

private:
    int Close(bool bReConnect = true);      //关闭Socket
    void Communication();             //线程函数
    void ChkConnection();             //检查连接状态
    int Send(const string &strCmd);   //发送cmd
    int Receive(string& strRst);      //接收cmd执行结果

private:
    string m_strHost;               //Server host
    int m_nPort;                    //Server侦听端口
    int m_fd;                       //socket fd
    bool m_bReConnect;              //断链是否重连
    bool m_bSetDisconnectFlag;      //设置断开连接标识

    thread* m_pWorkThread;          //Socket工作线程
    bool m_bExitFlag;               //退出子线程标识

    std::queue<CmdTask> m_tasklist; //命令队列

    mutex m_mtx;                    //for m_tasklist，m_bReConnect，m_bExitFlag
    std::condition_variable m_cv;   //for m_tasklist，m_bReConnect，m_bExitFlag
};
