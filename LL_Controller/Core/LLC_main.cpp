

#include "LLC_main.h"
#include "LLC_Rbt.h"
#include "LLC_SlaveBase.h"
#include "LLC_CmdMsg.h"

#include <string>
#include <iostream>
#include <limits.h>
#include <linux/limits.h>

#ifdef __cplusplus
extern "C"
{
#endif

int plugin_InitLLCMain(LLC_HandlecmdCBFunc callback) {
    LLCRbt->SetCommandCallback(callback);
    return LLCRbt->InitializeController() ? REC_Successed : REC_ControllerNotInit;
}
int plugin_SetMaster(ST_EtherCATMasterControl stEtherCATMasterControl)
{
    LLCRbt->SetMaster(stEtherCATMasterControl);
    return REC_Successed;
}
int plugin_SetEtherCATInterface(ST_EtherCATInterface stEtherCATInterface)
{
    LLCRbt->SetSlaveInterface(stEtherCATInterface);
    for (auto* slave : LLCRbt->m_vSlaves) slave->SetEtherCATFuncCB(stEtherCATInterface);
    LLC_Rbt::SetSDOFuncCB(stEtherCATInterface.pSDORead, stEtherCATInterface.pSDOWrite);
    return REC_Successed;
}

int plugin_SetSlaveInfo(int nSlaveCount,int nSlaveID,void* pSlaveST)
{
    return LLCRbt->InitSetConfigSlavePdo(nSlaveCount,nSlaveID,pSlaveST);
}

int plugin_ReadEtherCATConfig(bool& bIsSimulation, string& sNetPort, float& fCycle,vector<bool>& bEnableDC,vector<double>& dShiftTime)
{
    LLCRbt->ReadEtherCATConfig(bIsSimulation,sNetPort,fCycle,bEnableDC,dShiftTime);
    return REC_Successed;
}


//周期处理ethercat的PDO数据
int plugin_HandleCyclicJob(long nCurTickUSEC)
{
    return LLCRbt->HandleCyclicJob(nCurTickUSEC);
}

int plugin_HandleEcatCheck(ecat_frame& update_ecat_shm)
{
    return LLCRbt->HandleEcatCheck(update_ecat_shm);
}


#ifdef __cplusplus
}
#endif