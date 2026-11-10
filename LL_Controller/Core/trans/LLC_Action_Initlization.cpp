
#include "LLC_Action_Initlization.h"
#include "LL_Errorcode.h"
#include "LLC_Rbt.h"
 


//plugin_InitLLCMain调用,初始化整体状态机
bool action_Initlization(actionParam)
{
    
    //从站掉线信号在此复位，复位后切换到未初始化状态  
    LLCRbt->m_stRobotControlStatus.bStatus_SlaveDropError = false;
    return true;
}

bool action_StartMaster(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 0){nRet= REC_CmdFormatError;break;}
        LLCRbt->InitBasicFB();
        LLCRbt->ResetMaster();
        LLCRbt->m_stRobotControlStatus.bStartMaster = true;
        LLCRbt->m_stRobotControlStatus.nMasterState = 1;
        nRet = LLCRbt->SetRunControllerState(1);

        printf("StartMaster...nRet:%d\n",nRet);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

bool action_CloseMaster(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        //从站掉线信号在此复位，复位后切换到未初始化状态            
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 0){nRet= REC_CmdFormatError;break;}
        nRet = LLCRbt->SetRunControllerState(0);
        printf("CloseMaster..,nRet:%d\n",nRet);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}


bool action_DumpInfo(actionParam)
{
    LLCRbt->dumpInfo();
    if(event) event->result.push_back(to_string(REC_Successed));
    return true;
}

bool action_ScanSlave(actionParam)
{
    int nRet = REC_Successed;
    std::vector<SlaveInfo> vSlaveInfo;
    do
    {         
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 0){nRet= REC_CmdFormatError;break;}
        nRet = LLCRbt->ScanSlave(vSlaveInfo);
    } while (false);
    event->result.push_back(to_string(nRet));
    event->result.push_back(to_string(vSlaveInfo.size()));
    for (size_t i = 0; i < vSlaveInfo.size(); i++)
    {
        event->result.push_back(vSlaveInfo[i].DriverName);
        event->result.push_back(to_string(vSlaveInfo[i].nProductCode));
        event->result.push_back(to_string(vSlaveInfo[i].nRevisionNo));
    }
    return (nRet==REC_Successed);
}

bool action_ReloadESI(actionParam)
{
    int nRet = REC_Successed;
    do
    {           
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 0){nRet= REC_CmdFormatError;break;}
        nRet = LLCRbt->Initialize_ESI();
        printf("Initialize_ESI..,nRet:%d\n",nRet);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}