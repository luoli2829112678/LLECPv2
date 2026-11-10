
#include "LLC_Rbt_Status.h"
#include "LLC_Entry.h"
#include "LL_Log.h"
#include "Monitor/LLC_Logger.h"
#include "LLC_Robot_def.h"
#include <dlfcn.h>
#include <fstream>
#include <filesystem>

LLC_Rbt_Status::LLC_Rbt_Status():m_unCurCPSFSM(enCDSState_UnInitialized),m_strCurCPSFSM("")
{
}

LLC_Rbt_Status::~LLC_Rbt_Status()
{
}


void LLC_Rbt_Status::StatusCheck()
{
    struct llsm_event ev;
    ev.type = Cmd_Timer;
    ev.param.clear();
    vector<string> vecChecklist;
    struct llsm_state*curState = GetCurStateChecklist(vecChecklist);
    int nFSMHandleRet=stateM_stateChanged;
    for (size_t i = 0; i < vecChecklist.size(); i++)
    {
        ev.cmd = vecChecklist[i];
        nFSMHandleRet = FSM_handleEvent(curState,&ev);
        //nFSMHandleRet = stateM_handleEvent( &LLStatus::g_rbtfsm, &ev );
        if (nFSMHandleRet!=stateM_noStateChange)
        {
            break;
        }
    }
}
struct llsm_state* LLC_Rbt_Status::GetCurState()
{
    AutoSaftMutex FSMLocker(&m_FSMLock);
    return m_fsmRbt.currentState;
}
void LLC_Rbt_Status::SetCurLLCFSM(unsigned int unCurCPSFSM,const std::string&stateData)
{
    AutoSaftMutex InsLocker(&m_CurLLCFSMLock);
    if(m_unCurCPSFSM != unCurCPSFSM)
    {
        //LL_LOG_Info("LLC FSM state changed from %d(%s) to =======> %d(%s)", m_unCurCPSFSM, m_strCurCPSFSM.c_str(), unCurCPSFSM, stateData.c_str());
       
        LLC_LOG_INFO("LLC FSM state changed from " + std::to_string(m_unCurCPSFSM) + "(" + m_strCurCPSFSM + ") to =======> " + std::to_string(unCurCPSFSM) + "(" + stateData + ")");
       
        m_unCurCPSFSM = unCurCPSFSM;
        m_strCurCPSFSM = stateData;
    }
}
unsigned int LLC_Rbt_Status::CurLLCFSM(std::string&stateData)
{
    AutoSaftMutex InsLocker(&m_CurLLCFSMLock);
    stateData=m_strCurCPSFSM;
    return m_unCurCPSFSM;
}
struct llsm_state* LLC_Rbt_Status::GetCurStateChecklist(vector<string>& vecChecklist)
{
    AutoSaftMutex FSMLocker(&m_FSMLock);
    vecChecklist = m_fsmRbt.currentState->checkCmds;
    return m_fsmRbt.currentState;
}
int LLC_Rbt_Status::FSM_handleEvent(struct llsm_state * BeforLockCur, struct llsm_event *event )
{
    //状态无关命令走状态不变状态机
    if (m_stateUnChangedCmdMap.count(event->cmd) > 0)
    {
        return stateM_handleEvent(&m_fsmStateUnchanged, event, (event->type==Cmd_Msg));
    }

    //其他走普通状态机，不加锁
    //AutoSaftMutex FSMLocker(&m_FSMLock);
    if (BeforLockCur!=nullptr&&(BeforLockCur!=m_fsmRbt.currentState))
    {//被其他线程改变状态，此时Timer消息不再进行处理
        return stateM_stateChanged;
    }
    return stateM_handleEvent(&m_fsmRbt, event,(event->type==Cmd_Msg));
}
void LLC_Rbt_Status::Initialize_LLCStatus()
{
    InitCmds_stateUnChanged();            //注册stateNoChange命令列表

    InitLLCState(UnInitialized);         //0.未初始化状态
    InitLLCState(PreInitializing);         //1.初始化中状态
    InitLLCState(EmergencyStop);           //2.急停状态
    InitLLCState(ControllerNotConnect);    //3.控制器未连接
    InitLLCState(PreControllerConnecting); //4.正在连接控制器状态
    InitLLCState(WatchDog);                //5.看门狗生效状态,同步从站的数据
    InitLLCState(StandBy);                 //6.准备就绪状态
    InitLLCState(Moving);                  //7.运动中状态
    InitLLCState(ErrorStopping);           //8.错误停止处理中状态
    InitLLCState(Error);                   //9.错误状态
    InitLLCState(PreResettingError);       //10.错误恢复中
    InitLLCState(HandleEmergencyStop);     //11.急停处理中);

    stateM_init(&m_fsmRbt, GetLLCStateST(UnInitialized), NULL,action_noTrans);
    stateM_init(&m_fsmStateUnchanged, m_stateUnchanged.GetState(), NULL, NULL);
    InitAction();
}
void LLC_Rbt_Status::RegisterTrans(LL_State& curState,const string& strCmd,int nMsgType,
            _guardAPI pGuard,_actionAPI pAction,struct llsm_state *nextState)
{
    curState.RegisterTrans(strCmd,nMsgType,pGuard,pAction,nextState);
    m_AllCmdMap.insert(std::make_pair(strCmd,0));
}
void LLC_Rbt_Status::InitAction()
{
    InitAction_UnInitialized();
    InitAction_PreInitializing();
    InitAction_EmergencyStop();
    InitAction_ControllerNotConnect();
    InitAction_PreControllerConnecting();
    InitAction_WatchDog();
    InitAction_StandBy();
    InitAction_Moving();
    InitAction_ErrorStopping();
    InitAction_Error();
    InitAction_PreResettingError();
    InitAction_HandleEmergencyStop();
}

void LLC_Rbt_Status::AddTrans_stateUnChangedCmd(string strCmd, bool (*action)(actionParam))
{
    RegisterTrans(m_stateUnchanged, strCmd, enCmdType::Cmd_Msg, NULL, action, m_stateUnchanged.GetState());
    m_stateUnChangedCmdMap.insert(std::make_pair(strCmd, 0));
}

void LLC_Rbt_Status::InitCmds_stateUnChanged()
{
    //此处添加不走普通状态机的命令
    //格式：AddTrans_stateUnChangedCmd(cmd, action_cmd);
    AddTrans_stateUnChangedCmd(Cmd_DumpInfo, action_DumpInfo);
}

bool LLC_Rbt_Status::IsFsmCmdExist(const string& strCmd)
{
    if (m_AllCmdMap.count(strCmd) != 0 || m_stateUnChangedCmdMap.count(strCmd) != 0)
        return true;
    return false;
}

void LLC_Rbt_Status::dumpInfo()
{
   vector<string> statusTab;
   vector<string> statetransTab;
   string strState="状态号,状态名,进入函数,退出函数";
   statusTab.push_back(strState);
   string strTrans="状态号,状态名,命令,跳转状态";
   statetransTab.push_back(strTrans);
   for(int i=EN_CodesysStatus::enCDSState_UnInitialized; i<EN_CodesysStatus::enCDSState_Count; ++i)
   {
      string strName = m_status[i].GetState()->strData;
      string strEntryFun="";
      string strExityFun="";
      strState=to_string(m_status[i].GetState()->unStateCode);
      strState.append(",");
      strState.append(strName);
      
      Dl_info info;
      if (dladdr((void*)m_status[i].GetState()->entryAction, &info)) {
        strEntryFun = info.dli_sname;
      }
      strState.append(",");
      strState.append(strEntryFun);
      if (dladdr((void*)m_status[i].GetState()->exitAction, &info)) {
        strExityFun = info.dli_sname;
      }
      strState.append(",");
      strState.append(strExityFun);

      statusTab.push_back(strState);
    //   if(m_status[i].GetState()->unStateCode==0)
    //   {
    //     printf("%d\n",i);
    //   }

      for (auto it = m_status[i].GetState()->transitions.begin();
            it != m_status[i].GetState()->transitions.end(); ++it) 
      {
         string strNextState="";
         if (it->second.nextState != nullptr)
         {
            strNextState = it->second.nextState->strData;
         }
         
         strTrans=to_string(m_status[i].GetState()->unStateCode);
         strTrans.append(",");
         strTrans.append(strName);
         strTrans.append(",");
         strTrans.append(it->second.condition);
         strTrans.append(",");
         strTrans.append(strNextState);

         statetransTab.push_back(strTrans);
      }
   }
   string packageName=to_string(Version);
   //LL_PackageVersiong(packageName);
   std::filesystem::create_directories("runtime/dumpInfo");

   string strRbtFSM_listFile = "runtime/dumpInfo/RbtFSM_list.LLC_";
   strRbtFSM_listFile.append(packageName);
   strRbtFSM_listFile.append(".csv");

   string strRbtFSM_TransFile = "runtime/dumpInfo/RbtFSM_Trans.LLC_";
   strRbtFSM_TransFile.append(packageName);
   strRbtFSM_TransFile.append(".csv");

   ofstream ofWriteFSM(strRbtFSM_listFile.c_str(), ios::out);
   for(std::size_t i=0; i<statusTab.size(); ++i)
   {
      ofWriteFSM <<statusTab[i]<< endl;
   }
   ofWriteFSM.close();

   ofstream ofWriteTrans(strRbtFSM_TransFile.c_str(), ios::out);
   for(std::size_t i=0; i<statetransTab.size(); ++i)
   {
      ofWriteTrans <<statetransTab[i]<< endl;
   }
   ofWriteTrans.close();
}