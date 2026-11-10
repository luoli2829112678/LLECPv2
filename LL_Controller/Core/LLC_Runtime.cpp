#include "LLC_Rbt.h"
#include "LL_CpsService.h"
#include "LLC_CmdMsg.h"
#include "LL_Process.h"
#include <filesystem>
#include <charconv>
#include <system_error>
#include <limits>
#include <new>
#include <sys/file.h>
#include <cerrno>
#include <cstring>
#include <iostream>
namespace {
unsigned number(const std::string& text, unsigned maximum=std::numeric_limits<unsigned>::max()) {
    unsigned value=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(text.empty()||parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||value>maximum)throw std::invalid_argument("invalid unsigned integer");
    return value;
}
std::string busName(LL::BusType type){
    switch(type){case LL::BusType::EtherCAT:return "EtherCAT";case LL::BusType::Profinet:return "Profinet";case LL::BusType::Modbus:return "Modbus";}
    return {};
}
}
bool LLC_Rbt::InitializeController(const std::string& runtimeRoot) {
    if(m_initialized)return true;
    // Keep one controller per host; compiler subprocesses must not inherit its lock.
    constexpr const char* lockPath="/tmp/llecp-controller.lock";
    m_processLock=open(lockPath,O_CREAT|O_RDWR|O_CLOEXEC,0600);
    if(m_processLock<0) {
        std::cerr<<"[LL_Controller] Cannot open process lock "<<lockPath<<": "<<std::strerror(errno)<<'\n';
        return false;
    }
    if(flock(m_processLock,LOCK_EX|LOCK_NB)!=0) {
        const int error=errno;
        if(error==EWOULDBLOCK||error==EAGAIN) {
            char owner[32]{};
            const auto size=pread(m_processLock,owner,sizeof(owner)-1,0);
            std::cerr<<"[LL_Controller] Another controller is already running";
            if(size>0)std::cerr<<" (lock owner PID "<<std::string(owner,static_cast<std::size_t>(size))<<')';
            std::cerr<<". Stop the existing instance before starting another.\n"
                     <<"Inspect it with: fuser -v "<<lockPath<<'\n';
        } else {
            std::cerr<<"[LL_Controller] Cannot acquire process lock "<<lockPath<<": "<<std::strerror(error)<<'\n';
        }
        close(m_processLock);m_processLock=-1;return false;
    }
    const auto owner=std::to_string(getpid());
    if(ftruncate(m_processLock,0)!=0||pwrite(m_processLock,owner.data(),owner.size(),0)!=static_cast<ssize_t>(owner.size()))
        std::cerr<<"[LL_Controller] Cannot record lock owner PID: "<<std::strerror(errno)<<'\n';
    m_runtimeRoot=runtimeRoot;m_sApplicationPath=(std::filesystem::path(runtimeRoot)/"Application").string();
    for(const auto* sub:{"ESI","Application","logs","dumpInfo"})std::filesystem::create_directories(std::filesystem::path(runtimeRoot)/sub);
    Logger::instance().init((std::filesystem::path(runtimeRoot)/"logs").string());
    if(Initialize_LLCShm()!=REC_Successed){std::cerr<<"[LL_Controller] Shared memory initialization failed: "<<std::strerror(errno)<<'\n';ShutdownController();return false;}
    ResetControllerConfig();
    Initialize_ESI();
    m_stRbtEtherCATParams.bSimulationType=false;
    m_masters.RegisterFactory(LL::BusType::EtherCAT,LL::CreateEtherCATMaster);
    Initialize_LLCStatus();
    llsm_event event{};event.type=Cmd_Msg;event.cmd=Cmd_UnInitialized;
    FSM_handleEvent(nullptr,&event);
    if(HSTR_Init()!=0){std::cerr<<"[LL_Controller] HSTR initialization failed\n";ShutdownController();return false;}
    if(Initialize_LLCIF(m_pLLC_DispatchCmdFunc)!=REC_Successed){std::cerr<<"[LL_Controller] Command listener initialization failed\n";ShutdownController();return false;}
    m_initialized=true;
    if(Initialize_LLCThreads()!=REC_Successed){std::cerr<<"[LL_Controller] Controller thread initialization failed\n";ShutdownController();return false;}
    return true;
}
void LLC_Rbt::ShutdownController() {
    LLNet_StopAll();
    m_controllerRunning=false;
    if(m_controllerThread.joinable())m_controllerThread.join();
    if(m_fileTransfer.valid())m_fileTransfer.wait();
    std::lock_guard<std::recursive_mutex> lock(m_controllerMutex);
    if(m_pHSTRruntime)HSTR_StopApplication();
    CleanSlaveConfig();
    m_masters.CloseAll();
    m_CPSClient.Shutdown();
    if(m_pLLC_ShmRW){munmap(m_pLLC_ShmRW,sizeof(ST_ShareMemory));m_pLLC_ShmRW=nullptr;m_pLLC_CPS2LLCMemory=nullptr;m_pLLC_LLC2CPSMemory=nullptr;}
    LL_CpsService::Get().Shutdown();
    m_initialized=false;m_bMasterState=false;
    Logger::instance().shutdown();
    if(m_processLock>=0){close(m_processLock);m_processLock=-1;}
}
bool LLC_Rbt::BindMaster(const std::string& id) {
    auto master=m_masters.Get(id);if(!master)return false;
    if(id==m_activeMaster)return true;
    if(m_bFoEWorking)return false;
    const auto old=m_masters.Get(m_activeMaster);
    if((old&&old->Status().running)||m_nSetConfiguredSlaveNum||m_nSetConfiguredAxisNum)return false;
    CleanSlaveConfig();m_activeMaster=id;
    ST_EtherCATMasterControl control{};
    control.pScanSlaves=[this,master](std::vector<SlaveInfo>& out){
        out.clear();if(master->Status().running)return false;
        CleanSlaveConfig();
        if(!master->Scan())return false;
        master->Poll();for(const auto& slave:master->Status().slaves)out.push_back({slave.name,slave.productCode,slave.revision});
        return true;
    };
    control.pConnectSlaves=[this]{return ConnectActiveMaster();};
    control.pDisconnectSlaves=[this,master]{master->Stop();m_bMasterState=false;return true;};
    control.pResetSlaves=[master]{return master->Poll();};
    control.pFoEWrite=[this](int slave,const std::string& local,const std::string& remote,std::uint32_t password){return LaunchFileTransfer(true,slave,local,remote,password);};
    control.pFoERead=[this](int slave,const std::string& local,const std::string& remote,std::uint32_t password){return LaunchFileTransfer(false,slave,local,remote,password);};
    control.m_pFoEWriteESI=[this,master](int slave,const std::string& path){
        if(slave<1||slave>65535||m_bFoEWorking)return false;
        if(m_fileTransfer.valid()){if(m_fileTransfer.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;m_fileTransfer.get();}
        master->Stop();m_bMasterState=false;HSTR_StopApplication();CleanSlaveConfig();
        m_bFoEWorking=true;m_bFoEWorkingDone=false;m_dFOEworkProgress=0;
        m_fileTransfer=std::async(std::launch::async,[master,slave,path]{try { return master->WriteSiiFile(static_cast<std::uint16_t>(slave),path); } catch(...) { return -1; }});
        return true;
    };
    SetMaster(control);
    ST_EtherCATInterface callbacks{};
    callbacks.pSDORead=[master](uint16_t slave,uint16_t index,uint8_t sub,uint8_t ca,int* size,void* data,int timeout){
        if(!size)return -1;return master->ReadSDO(slave,index,sub,data,*size,ca!=0,timeout);
    };
    callbacks.pSDOWrite=[master](uint16_t slave,uint16_t index,uint8_t sub,uint8_t ca,int size,const void* data,int timeout){
        return master->WriteSDO(slave,index,sub,data,size,ca!=0,timeout);
    };
    m_slaveInterface=callbacks;
    SetSDOFuncCB(callbacks.pSDORead,callbacks.pSDOWrite);
    return InitMaster(master->Config().endpoint,static_cast<int>(master->Config().cycleUs))==REC_Successed;
}
bool LLC_Rbt::ConnectActiveMaster() {
    auto master=m_masters.Get(m_activeMaster);if(!master)return false;
    if(master->Status().running)return true;
    if(m_stRbtEtherCATParams.bSimulationType) {
        CleanSlaveConfig();
        if(InitSetConfigSlavePdo(m_nSetConfiguredSlaveNum,0,nullptr)!=REC_Successed)return false;
        return InitBasicFB()==REC_Successed&&InitSoftMotion()==REC_Successed&&InitIO()==REC_Successed;
    }
    CleanSlaveConfig();
    if(!master->Scan())return false;
    // Assign PDOs while still in PRE_OP; only then ask SOEM to build its IO map.
    auto assign=[master](std::uint16_t slave,std::uint16_t object,const std::vector<std::uint32_t>& pdos){
        if(pdos.empty())return true; // no override: retain device default assignment
        if(pdos.size()>255)return false;
        std::uint8_t count=0;
        if(master->WriteSDO(slave,object,0,&count,sizeof(count),false,700000)<=0)return false;
        for(std::size_t i=0;i<pdos.size();++i){
            if(pdos[i]==0||pdos[i]>65535)return false;
            const auto pdo=static_cast<std::uint16_t>(pdos[i]);
            if(master->WriteSDO(slave,object,static_cast<std::uint8_t>(i+1),&pdo,sizeof(pdo),false,700000)<=0)return false;
        }
        count=static_cast<std::uint8_t>(pdos.size());
        return master->WriteSDO(slave,object,0,&count,sizeof(count),false,700000)>0;
    };
    for(std::size_t i=0;i<m_vSlaveConfigParam.size();++i){
        const auto& config=m_vSlaveConfigParam[i];
        if(!assign(static_cast<std::uint16_t>(i+1),0x1c12,config.nRxProcessData)||
           !assign(static_cast<std::uint16_t>(i+1),0x1c13,config.nTxProcessData))return false;
    }
    if(!master->Configure())return false;
    for(std::size_t i=0;i<m_vSlaveConfigParam.size();++i){
        const auto& config=m_vSlaveConfigParam[i];
        if(config.bEnableDC){
            const auto shift=static_cast<std::int64_t>(static_cast<std::int32_t>(config.nShiftTime))*1000;
            if(shift<std::numeric_limits<std::int32_t>::min()||shift>std::numeric_limits<std::int32_t>::max()||
               !master->ConfigureSync0(static_cast<std::uint16_t>(i+1),true,master->Config().cycleUs*1000,static_cast<std::int32_t>(shift)))return false;
        }
    }
    if(m_nSetConfiguredSlaveNum) {
        if(!master->VisitSlaves([this](int count,void* data){
            auto* slaves=static_cast<ec_slavet*>(data);
            if(count!=m_nSetConfiguredSlaveNum)return false;
            if(InitSetConfigSlavePdo(count,0,&slaves[0])!=REC_Successed)return false;
            for(int i=1;i<=count;++i)if(InitSetConfigSlavePdo(count,i,&slaves[i])!=REC_Successed){CleanSlaveConfig();return false;}
            return true;
        }))return false;
        if(InitBasicFB()!=REC_Successed||InitSoftMotion()!=REC_Successed||InitIO()!=REC_Successed)return false;
    }
    return master->Start(false);
}
bool LLC_Rbt::AreSlavesConnected() {
    if(LL_CpsService::Get().Attached())return LL_CpsService::Get().Connected();
    if(m_stRbtEtherCATParams.bSimulationType||m_bFoEWorking)return true;
    auto master=m_masters.Get(m_activeMaster);
    return master && master->Status().connected;
}
bool LLC_Rbt::LaunchFileTransfer(bool write,int slave,const std::string& local,const std::string& remote,std::uint32_t password) {
    auto master=m_masters.Get(m_activeMaster);
    if(!master||slave<1||slave>65535||m_bFoEWorking)return false;
    if(m_fileTransfer.valid()) {
        if(m_fileTransfer.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
        m_fileTransfer.get();
    }
    master->Stop();m_bMasterState=false;HSTR_StopApplication();CleanSlaveConfig();
    m_bFoEWorking=true;m_bFoEWorkingDone=false;m_dFOEworkProgress=0;
    m_fileTransfer=std::async(std::launch::async,[master,write,slave,local,remote,password]{
        try { return write?master->WriteFile(static_cast<std::uint16_t>(slave),local,remote,password)
                    :master->ReadFile(static_cast<std::uint16_t>(slave),local,remote,password); } catch(...) { return -1; }
    });
    return true;
}
void LLC_Rbt::ControllerCycle() {
    using Clock=std::chrono::steady_clock;
    auto last=Clock::now(),next=last,nextPoll=last;
    while(m_controllerRunning) {
        try {
            const auto now=Clock::now();
            std::chrono::microseconds period{1000};
            {
                std::lock_guard<std::recursive_mutex> lock(m_controllerMutex);
                auto master=m_masters.Get(m_activeMaster);
                period=std::chrono::microseconds(static_cast<long long>(m_cycle_time*1000.0));
                if(period.count()<100)period=std::chrono::microseconds(1000);
                if(m_fileTransfer.valid()) {
                    if(m_fileTransfer.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
                        const int result=m_fileTransfer.get();m_bFoEWorking=false;m_bFoEWorkingDone=true;
                        m_dFOEworkProgress=result>0?100.0:-1.0;
                    } else if(master) {
                        const auto progress=master->FileStatus();
                        if(progress.totalBytes)m_dFOEworkProgress=100.0*progress.transferredBytes/progress.totalBytes;
                    }
                }
                LL_CpsService::Get().BeforeCycle();
                if(!LL_CpsService::Get().Attached()&&master&&!m_bFoEWorking&&!m_stRbtEtherCATParams.bSimulationType) {
                    if(m_bMasterState)master->Exchange();
                    if(now>=nextPoll){master->Poll();nextPoll=now+std::chrono::milliseconds(100);}
                    const auto status=master->Status();
                    ecat_frame frame{};
                    frame.send_frame_count=static_cast<long long>(status.cycles);
                    frame.frame_lost_count=static_cast<long long>(status.wkcErrors);
                    frame.nEtherCATMasterState=status.slaves.empty()?0:status.slaves.front().state;
                    frame.linkDisconnected=!status.connected;
                    frame.is_error_state=status.actualWkc<status.expectedWkc;
                    frame.foeSendDataPercentage=m_dFOEworkProgress;
                    frame.foeSendDataDone=m_bFoEWorkingDone?1.0:0.0;
                    HandleEcatCheck(frame);
                }
                if(m_fsmRbt.currentState)StatusCheck();
                LLC_ExecHandleNotRealTimeCmd();
                HandleCyclicJob(static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(now-last).count()));
                LL_CpsService::Get().AfterCycle();
            }
            last=now;next+=period;
            if(next<Clock::now())next=Clock::now()+period;
            std::this_thread::sleep_until(next);
        } catch(const std::exception& error) {
            LLC_LOG_ERROR(error.what());
            std::lock_guard<std::recursive_mutex> lock(m_controllerMutex);
            m_stRobotControlStatus.bStatus_ControllerError=true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
bool LLC_Rbt::HandleMasterCommand(const std::string& cmd,const std::vector<std::string>& p,std::vector<std::string>& result) {
    const bool recognized=cmd==Cmd_RegistrationMaster||cmd==Cmd_UnregisterMaster||cmd==Cmd_SelectMaster||cmd==Cmd_ListMasters||
        cmd==Cmd_ReadMasterStatus||cmd==Cmd_ReadSlaveStates||cmd==Cmd_SetSlaveState||cmd==Cmd_StartRegisteredMaster||cmd==Cmd_StopRegisteredMaster;
    if(!recognized)return false;
    result={std::to_string(REC_ParametersError)};
    try {
        if(cmd==Cmd_RegistrationMaster) {
            if(p.size()!=4)return true;
            LL::BusType type;
            if(p[1]=="EtherCAT")type=LL::BusType::EtherCAT;
            else if(p[1]=="Profinet")type=LL::BusType::Profinet;
            else if(p[1]=="Modbus")type=LL::BusType::Modbus;
            else return true;
            if(!m_masters.Register({p[0],type,p[2],number(p[3],1000000)})){result.push_back(m_masters.LastError());return true;}
            if(m_activeMaster.empty()&&!BindMaster(p[0]))return true;
        } else if(cmd==Cmd_ListMasters) {
            if(!p.empty())return true;
            result={"0",std::to_string(m_masters.List().size())};
            for(const auto& config:m_masters.List()){result.push_back(config.id);result.push_back(busName(config.type));result.push_back(config.endpoint);result.push_back(std::to_string(config.cycleUs));}
            return true;
        } else {
            if(p.empty())return true;
            auto master=m_masters.Get(p[0]);if(!master){result.push_back("Master is not registered");return true;}
            if(m_bFoEWorking && p[0]==m_activeMaster){result={std::to_string(REC_TimeOutError),"File transfer is in progress; use progress interface"};return true;}
            if(cmd==Cmd_SelectMaster) {if(p.size()!=1||!BindMaster(p[0]))return true;}
            else if(cmd==Cmd_UnregisterMaster) {
                if(p.size()!=1||master->Status().running||m_bFoEWorking)return true;
                if(p[0]==m_activeMaster){CleanSlaveConfig();ResetControllerConfig();m_activeMaster.clear();SetSDOFuncCB(nullptr,nullptr);SetMaster({});m_slaveInterface={};}
                if(!m_masters.Unregister(p[0]))return true;
            } else if(cmd==Cmd_StartRegisteredMaster) {
                if(p.size()!=1)return true;
                if(p[0]==m_activeMaster){if(!ConnectActiveMaster())return true;m_bMasterState=true;}
                else if(!master->Status().running && (!master->Scan()||!master->Configure()||!master->Start(true)))return true;
            } else if(cmd==Cmd_StopRegisteredMaster) {
                if(p.size()!=1)return true;master->Stop();if(p[0]==m_activeMaster)m_bMasterState=false;
            } else if(cmd==Cmd_SetSlaveState) {
                if(p.size()!=3||!master->ChangeSlaveState(static_cast<uint16_t>(number(p[1],65535)),static_cast<uint16_t>(number(p[2],65535))))return true;
            } else if(cmd==Cmd_ReadMasterStatus) {
                if(p.size()!=1)return true;
                const auto s=master->Status();
                result={"0",s.opened?"1":"0",s.running?"1":"0",s.connected?"1":"0",std::to_string(s.actualWkc),std::to_string(s.expectedWkc),std::to_string(s.cycles),std::to_string(s.wkcErrors),std::to_string(s.missedDeadlines)};
                return true;
            } else if(cmd==Cmd_ReadSlaveStates) {
                if(p.size()!=1)return true;
                if(!m_bFoEWorking)master->Poll();
                const auto status=master->Status();result={"0",std::to_string(status.slaves.size())};
                for(const auto& slave:status.slaves){result.push_back(std::to_string(slave.id));result.push_back(std::to_string(slave.state));result.push_back(slave.connected?"1":"0");result.push_back(std::to_string(slave.alStatusCode));}
                return true;
            }
        }
        result={"0"};
    } catch(const std::exception& ex){result={std::to_string(REC_ParametersError),ex.what()};}
    return true;
}

bool LL_InitializeController(const std::string& root) { return LLCRbt->InitializeController(root); }
void LL_ShutdownController() { LLCRbt->ShutdownController(); }
