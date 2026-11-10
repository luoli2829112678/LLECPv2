
#include "LLC_Rbt.h"
#include "LLC_IF.h"
#include "LL_netplan.h"
#include "LLC_CmdMsg.h"

int LLC_Rbt::OnConnect(int s)
{
    string strLocalifname="";
    uint16_t nLocalPort=0;
    uint16_t nRemotePort=0;
    string strRemoteIP="";
    string strRemoteMAC="";
    LL_GetSocketInfo(s,strLocalifname,nLocalPort,nRemotePort,strRemoteIP,strRemoteMAC);
    printf("client connected [ID:%d IP:%s Port:%d MAC %s] connected[local Port:%d]\n",
                s,strRemoteIP.c_str(),nRemotePort,strRemoteMAC.c_str(),nLocalPort);
    return REC_Successed;
    //return g_pOnConnect(s,nLocalPort,nRemotePort,strRemoteIP,strRemoteMAC);
}
int LLC_Rbt::OnDisconnect(int s)
{
    // if (g_pOnDisconnect==nullptr)
    // {
    //     return REC_Successed;
    // }
    string strLocalifname="";
    uint16_t nLocalPort=0;
    uint16_t nRemotePort=0;
    string strRemoteIP="";
    string strRemoteMAC="";
    LL_GetSocketInfo(s,strLocalifname,nLocalPort,nRemotePort,strRemoteIP,strRemoteMAC);
    printf("client disconnected [ID:%d IP:%s Port:%d MAC %s] disconnected[local Port:%d]\n",
                s,strRemoteIP.c_str(),nRemotePort,strRemoteMAC.c_str(),nLocalPort);
    return REC_Successed;
    //return g_pOnDisconnect(s,nLocalPort,nRemotePort,strRemoteIP,strRemoteMAC);
}



void LLC_Rbt::InitReadCmdList()
{
    // const vector<string>& params,vector<string>& results
    // vector<string> readSDOParams
    LLC_CmdFunc readSDOCmd;
    m_ReadCmdMap.insert(std::make_pair(Cmd_ReadSDO, readSDOCmd));

    LLC_CmdFunc writeSDOCmd;
    m_ReadCmdMap.insert(std::make_pair(Cmd_WriteSDO, writeSDOCmd));

}



int LLC_Rbt::DispatchCmdFunc(int s,const string&strCmd,const vector<string>&params,vector<string>&results,uint16_t nPort)
{
    std::lock_guard<std::recursive_mutex> lock(m_controllerMutex);
    results.clear();
    if (nPort == PORT_NORMALCMD && HandleMasterCommand(strCmd, params, results)) return REC_Successed;
    if (nPort == PORT_NORMALCMD)
    {
        return HandleNormalCmd(strCmd, params, results);
    }
    if (nPort == PORT_FOLLOWMOTIONCMD)
    {
        return HandleFollowMotionCmd(strCmd, params, results);
    }
    else if (nPort == PORT_HSTRCMD)
    {
        return HandleHSTRCmd(strCmd, params, results);
    }   
    else if (nPort == PORT_ASYNCCMD)
    {
        return HandleRbtAsyncCmd(strCmd, params, results);
    }
    else if (nPort == PORT_REPLY)
    {
        return HandleReplyCmd(strCmd, params, results);
    }
    else
    {
        printf("unknown port: %d\n", nPort);
        return REC_Successed;
    }
}

int LLC_Rbt::HandleNormalCmd(const string& strCmd,const vector<string>& params,vector<string>& results)
{
    if (IsFsmCmdExist(strCmd) == 0)
    {
        results.push_back(to_string(REC_InvalidCommandError));
        return REC_InvalidCommandError;
    }
    
    struct llsm_event ev;
    ev.type = Cmd_Msg;
    ev.cmd = strCmd;
    ev.param = params;
    // unsigned long long ullStart = LL_GetCurMS();
    //int nRet = 
    FSM_handleEvent(nullptr,&ev);
    results = ev.result;
    
    LLC_LOG_INFO_WITH_PARAMS("HandleNormalCmd8888", strCmd, params, results);

    return REC_Successed;
}

int LLC_Rbt::HandleReplyCmd(const string& strCmd,const vector<string>& params,vector<string>& results)
{
    struct llsm_event eve;
    eve.type = Cmd_Msg;
    eve.cmd = strCmd;
    eve.param = params;
    // 处理ReadCmd请求
    if(strCmd == Cmd_ReadAxisInformation) action_ReadAxisInformation("", &eve, "");
    else
    {
        results.push_back(to_string(REC_InvalidCommandError));
        return REC_InvalidCommandError;
    }
    results = eve.result;
    if(stoi(results[0]) == REC_Successed)
    {
        // results.push_back(to_string(REC_Successed));
        return REC_Successed;
    }
    else{
        // results.push_back(to_string(REC_InvalidCommandError));
        return REC_InvalidCommandError;
    }
    
    LLC_LOG_INFO_WITH_PARAMS("HandleRbtAsyncCmd8892", strCmd, params, results);
    return REC_InvalidCommandError;
}



int LLC_Rbt::HandleFollowMotionCmd(const string& strCmd,const vector<string>& params,vector<string>& results)
{
    struct llsm_event eve;
    eve.type = Cmd_Msg;
    eve.cmd = strCmd;
    eve.param = params;

    // 处理ReadCmd请求
    if(strCmd == Cmd_AxisFollowPosition) action_AxisFollowPosition("", &eve, "");
    else if(strCmd == Cmd_AxisFollowVelocity)action_AxisFollowVelocity("", &eve, "");
    else if(strCmd == Cmd_AxisFollowTorque)action_AxisFollowTorque("", &eve, "");
    else
    {
        results.push_back(to_string(REC_InvalidCommandError));
        return REC_InvalidCommandError;
    }
    results = eve.result;
    if(stoi(results[0]) == REC_Successed)
    {
        return REC_Successed;
    }
    else{
        return REC_InvalidCommandError;
    }
    
    LLC_LOG_INFO_WITH_PARAMS("HandleRbtAsyncCmd8889", strCmd, params, results);
    return REC_InvalidCommandError;
}


int LLC_Rbt::HandleHSTRCmd(const string& strCmd, const vector<string>& params, vector<string>& results)
{
    struct llsm_event eve{};
    eve.type = Cmd_Msg;
    eve.cmd = strCmd;
    eve.param = params;

    // Application管理
    if (strCmd == Cmd_HSTR_CreateApplication) action_HSTR_CreateApplication("", &eve, "");
    else if (strCmd == Cmd_HSTR_SelectedApplication) action_HSTR_SelectedApplication("", &eve, "");

    // Program管理
    else if (strCmd == Cmd_HSTR_AddProgram) action_HSTR_AddProgram("", &eve, "");
    else if (strCmd == Cmd_HSTR_DeleteProgram) action_HSTR_DeleteProgram("", &eve, "");

    // Function管理
    else if (strCmd == Cmd_HSTR_AddFunction) action_HSTR_AddFunction("", &eve, "");
    else if (strCmd == Cmd_HSTR_DeleteFunction) action_HSTR_DeleteFunction("", &eve, "");

    // Task管理
    else if (strCmd == Cmd_HSTR_AddTask) action_HSTR_AddTask("", &eve, "");
    else if (strCmd == Cmd_HSTR_DeleteTask) action_HSTR_DeleteTask("", &eve, "");

    // 类型文件管理
    else if (strCmd == Cmd_HSTR_AddTypeFile) action_HSTR_AddTypeFile("", &eve, "");
    else if (strCmd == Cmd_HSTR_DeleteTypeFile) action_HSTR_DeleteTypeFile("", &eve, "");

    // 全局变量文件管理
    else if (strCmd == Cmd_HSTR_AddGlobalVariableFile) action_HSTR_AddGlobalVariableFile("", &eve, "");
    else if (strCmd == Cmd_HSTR_DeleteGlobalVariableFile) action_HSTR_DeleteGlobalVariableFile("", &eve, "");

    // Application编译、运行、复位
    else if (strCmd == Cmd_HSTR_CompileApplication) action_HSTR_CompileApplication("", &eve, "");
    else if (strCmd == Cmd_HSTR_RunApplication) action_HSTR_RunApplication("", &eve, "");
    else if (strCmd == Cmd_HSTR_ResetProgram) action_HSTR_ResetProgram("", &eve, "");

    // 断点调试
    else if (strCmd == Cmd_HSTR_AddBreakpoint) action_HSTR_AddBreakpoint("", &eve, "");
    else if (strCmd == Cmd_HSTR_DebugContinue) action_HSTR_DebugContinue("", &eve, "");
    else if (strCmd == Cmd_HSTR_DebugSingleStep) action_HSTR_DebugSingleStep("", &eve, "");
    else if (strCmd == Cmd_HSTR_DebugStepInto) action_HSTR_DebugStepInto("", &eve, "");
    else if (strCmd == Cmd_HSTR_GetDebugLocation) action_HSTR_GetDebugLocation("", &eve, "");
    else if (strCmd == Cmd_HSTR_GetApplicationStatus) action_HSTR_GetApplicationStatus("", &eve, "");
    else if (strCmd == Cmd_HSTR_RemoveBreakpoint) action_HSTR_RemoveBreakpoint("", &eve, "");

    // 停止Application
    else if (strCmd == Cmd_HSTR_StopApplication) action_HSTR_StopApplication("", &eve, "");

    // ST变量读写
    else if (strCmd == Cmd_HSTR_ReadVariable) action_HSTR_ReadVariable("", &eve, "");
    else if (strCmd == Cmd_HSTR_WriteVariable) action_HSTR_WriteVariable("", &eve, "");

    // Application信息查询
    else if (strCmd == Cmd_HSTR_GetSelectedApplication) action_HSTR_GetSelectedApplication("", &eve, "");
    else if (strCmd == Cmd_HSTR_GetLastError) action_HSTR_GetLastError("", &eve, "");
    else if (strCmd == Cmd_HSTR_GetCompilerDiagnostics) action_HSTR_GetCompilerDiagnostics("", &eve, "");

    // 未知命令
    else
    {
        results.clear();
        results.push_back(to_string(REC_InvalidCommandError));
        return REC_InvalidCommandError;
    }

    // 获取Action执行结果
    results = eve.result;

    // 防止结果为空导致数组越界
    if (results.empty())
    {
        results.push_back(to_string(REC_InvalidCommandError));
        return REC_InvalidCommandError;
    }

    // 打印命令执行日志
    LLC_LOG_INFO_WITH_PARAMS("HandleHSTRCmd", strCmd, params, results);

    // 解析Action返回的错误码
    try
    {
        int nRet = stoi(results[0]);
        if (nRet == REC_Successed)
        {
            return REC_Successed;
        }
        else
        {
            return REC_InvalidCommandError;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSTR] 命令返回值解析失败：" << e.what() << std::endl;
        return REC_InvalidCommandError;
    }
}


int LLC_Rbt::HandleRbtAsyncCmd(const string& strCmd,const vector<string>& params,vector<string>& results)
{
    struct llsm_event eve;
    eve.type = Cmd_Msg;
    eve.cmd = strCmd;
    eve.param = params;
    if (this->m_ReadCmdMap.count(strCmd) == 0)
    {
        results.push_back(to_string(REC_InvalidCommandError));
        return REC_InvalidCommandError;
    }
    else
    {
        // 处理ReadCmd请求
        if(strCmd == Cmd_ReadSDO) action_LLCReadSDO("", &eve, "");
        else if(strCmd == Cmd_WriteSDO)action_LLCWriteSDO("", &eve, "");
        else
        {
            results.push_back(to_string(REC_InvalidCommandError));
            return REC_InvalidCommandError;
        }
        results = eve.result;
        if(stoi(results[0]) == REC_Successed)
        {
            // results.push_back(to_string(REC_Successed));
            return REC_Successed;
        }
        else{
            // results.push_back(to_string(REC_InvalidCommandError));
            return REC_InvalidCommandError;
        }
    }
    
    LLC_LOG_INFO_WITH_PARAMS("HandleRbtAsyncCmd8891", strCmd, params, results);
    return REC_InvalidCommandError;
}
