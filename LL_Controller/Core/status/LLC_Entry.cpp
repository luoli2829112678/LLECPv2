#include <unistd.h>
#include <iostream>
using namespace std;

#include "LL_Log.h"

#include "LLC_Entry.h"

#include "LL_Errorcode.h"
#include "LL_Process.h"

#include "../LLC_Rbt.h"

void EntryState(unsigned int unStateCode,const std::string&stateData)
{
    LLCRbt->SetCurLLCFSM(unStateCode,stateData);
}
string ExitReason(struct llsm_transition* sTrans)
{
    string strCondition="";
    if(sTrans!=nullptr)
    {
        //strCondition="[";
        strCondition.append(sTrans->condition);
        //strCondition.append("]");
    }
    return strCondition;
}
//////////////////////////////////////////////////////////////////////////////////////
void EntryUnInitialized(stateParam)
{
    EntryState(unStateCode,stateData);
}
void ExitUnInitialized(stateParam)
{
    //string strCondition=ExitReason(sTrans);
    //LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //pyTask_StartTask(enPYTasks_AutoStart);
    //RbtAgent->Electrify48VOUT();
}
//////////////////////////////////////////////////////////////////////////////////////
void EntryPreInitializing(stateParam)
{
    EntryState(unStateCode,stateData);
}
void ExitPreInitializing(stateParam)
{
    //string strCondition=ExitReason(sTrans);
    //LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //pyTask_StartTask(enPYTasks_AutoStart);
    //RbtAgent->Electrify48VOUT();
}
//////////////////////////////////////////////////////////////////////////////////////
void EntryNotElectrify(stateParam)
{
    EntryState(unStateCode,stateData);
}
void ExitNotElectrify(stateParam)
{
    //string strCondition=ExitReason(sTrans);
    //LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //pyTask_StartTask(enPYTasks_AutoStart);
    //RbtAgent->Electrify48VOUT();
}
//////////////////////////////////////////////////////////////////////////////////////
void EntryEmergencyStop(stateParam)
{
    EntryState(unStateCode,stateData);
}
void ExitEmergencyStop(stateParam)
{
    //string strCondition=ExitReason(sTrans);
    //LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //pyTask_StartTask(enPYTasks_AutoStart);
    //RbtAgent->Electrify48VOUT();
}
//////////////////////////////////////////////////////////////////////////////////////
void EntryControllerNotConnect(stateParam)
{    
    LLCRbt->CleanSlaveConfig();
    LLCRbt->CleanIOConfig();
    LLCRbt->CleanAxisConfig();
    LLCRbt->CleanGroupConfig();
    EntryState(unStateCode,stateData);
    system("sudo ethtool -C enp2s0 rx-usecs 3");
}
void ExitControllerNotConnect(stateParam)
{

}
//////////////////////////////////////////////////////////////////////////////////////
void EntryPreControllerConnecting(stateParam)
{
    EntryState(unStateCode,stateData);
}
void ExitPreControllerConnecting(stateParam)
{
    //string strCondition=ExitReason(sTrans);
    //LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //pyTask_StartTask(enPYTasks_AutoStart);
    //RbtAgent->Electrify48VOUT();

}
//////////////////////////////////////////////////////////////////////////////////////
void EntryWatchDog(stateParam)
{
    EntryState(unStateCode,stateData);
    //LLCRbt->SyncSlavesOPRxPDO();
}
void ExitWatchDog(stateParam)
{
    string strCondition=ExitReason(sTrans);
    LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //LLCRbt->PrintStatus();
}


//////////////////////////////////////////////////////////////////////////////////////
void EntryStandBy(stateParam)
{
    EntryState(unStateCode,stateData);
}
void ExitStandBy(stateParam)
{
    //string strCondition=ExitReason(sTrans);
    //LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //pyTask_StartTask(enPYTasks_AutoStart);
    //RbtAgent->Electrify48VOUT();
}

//////////////////////////////////////////////////////////////////////////////////////
void EntryMoving(stateParam)
{
    EntryState(unStateCode,stateData);
}
void ExitMoving(stateParam)
{
    //string strCondition=ExitReason(sTrans);
    //LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //pyTask_StartTask(enPYTasks_AutoStart);
    //RbtAgent->Electrify48VOUT();
}
//////////////////////////////////////////////////////////////////////////////////////
void EntryStopping(stateParam)
{
    EntryState(unStateCode,stateData);
}
void ExitStopping(stateParam)
{
    //string strCondition=ExitReason(sTrans);
    //LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //pyTask_StartTask(enPYTasks_AutoStart);
    //RbtAgent->Electrify48VOUT();
}
//////////////////////////////////////////////////////////////////////////////////////
void EntryPausing(stateParam)
{
    EntryState(unStateCode,stateData);
}
void ExitPausing(stateParam)
{
    //string strCondition=ExitReason(sTrans);
    //LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //pyTask_StartTask(enPYTasks_AutoStart);
    //RbtAgent->Electrify48VOUT();
}
void EntryHandlePausing(stateParam)
{
    EntryState(unStateCode,stateData);
}
void ExitHandlePausing(stateParam)
{
    //string strCondition=ExitReason(sTrans);
    //LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //pyTask_StartTask(enPYTasks_AutoStart);
    //RbtAgent->Electrify48VOUT();
}
void EntryHandleContinue(stateParam)
{
    EntryState(unStateCode,stateData);
}
void ExitHandleContinue(stateParam)
{
    //string strCondition=ExitReason(sTrans);
    //LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //pyTask_StartTask(enPYTasks_AutoStart);
    //RbtAgent->Electrify48VOUT();
}
//////////////////////////////////////////////////////////////////////////////////////
void EntryErrorStopping(stateParam)
{
    EntryState(unStateCode,stateData);
}
void ExitErrorStopping(stateParam)
{
    //string strCondition=ExitReason(sTrans);
    //LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //pyTask_StartTask(enPYTasks_AutoStart);
    //RbtAgent->Electrify48VOUT();
}
//////////////////////////////////////////////////////////////////////////////////////
void EntryError(stateParam)
{
    LLCRbt->ControllerStop();
    EntryState(unStateCode,stateData);
}
void ExitError(stateParam)
{
    string strCondition=ExitReason(sTrans);
    LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
}
//////////////////////////////////////////////////////////////////////////////////////
void EntryPreResettingError(stateParam)
{
    EntryState(unStateCode,stateData);
}
void ExitPreResettingError(stateParam)
{
    string strCondition=ExitReason(sTrans);
    LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
}
void EntrySafeGuard(stateParam)
{
    EntryState(unStateCode,stateData);
}
void ExitSafeGuard(stateParam)
{
    //string strCondition=ExitReason(sTrans);
    //LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //pyTask_StartTask(enPYTasks_AutoStart);
    //RbtAgent->Electrify48VOUT();
}
void EntryHandleEmergencyStop(stateParam)
{
    EntryState(unStateCode,stateData);
}

void ExitHandleEmergencyStop(stateParam)
{
    //string strCondition=ExitReason(sTrans);
    //LL_LOG_Info("Exit reason : [%s]",strCondition.c_str());
    //pyTask_StartTask(enPYTasks_AutoStart);
    //RbtAgent->Electrify48VOUT();
}
