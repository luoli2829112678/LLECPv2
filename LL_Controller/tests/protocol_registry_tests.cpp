#include "LL_CommandCodec.h"
#include "LL_MasterRegistry.h"
#include <atomic>
#include <future>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
class FakeMaster:public LL::IMaster {
public:
    explicit FakeMaster(LL::MasterConfig config):config_(std::move(config)){}
    const LL::MasterConfig& Config()const override{return config_;}
    bool Scan()override{return true;}
    bool Configure()override{return true;}
    bool Start(bool=true)override{running=true;return true;}
    void Stop()override{running=false;}
    void Close()override{running=false;}
    int Exchange()override{return 1;}
    bool Poll()override{return true;}
    LL::MasterStatus Status()const override{LL::MasterStatus s;s.running=running;return s;}
    std::string LastError()const override{return {};}
    bool ChangeSlaveState(std::uint16_t,std::uint16_t)override{return true;}
    int ReadSDO(std::uint16_t,std::uint16_t,std::uint8_t,void*,int&,bool,int)override{return 1;}
    int WriteSDO(std::uint16_t,std::uint16_t,std::uint8_t,const void*,int,bool,int)override{return 1;}
    int WriteFile(std::uint16_t,const std::string&,const std::string&,std::uint32_t)override{return 1;}
    int ReadFile(std::uint16_t,const std::string&,const std::string&,std::uint32_t)override{return 1;}
    LL::FileTransferStatus FileStatus()const override{return {};}
private:
    LL::MasterConfig config_;
    std::atomic<bool> running{false};
};
}
int main(){try{
    LL::FrameBuffer buffer;std::vector<std::string> frames;LL::Command command;
    check(buffer.Append("RegistrationMas",frames)&&frames.empty(),"partial packet dispatched");
    check(buffer.Append("ter,m1,EtherCAT,eth0,1000,;ListMasters,;",frames)&&frames.size()==2,"fragment/pipeline framing failed");
    check(LL::DecodeCommand(frames[0],command)&&command.parameters.size()==4&&command.parameters[0]=="m1","request decode incompatible");
    check(LL::EncodeCommand(command)==frames[0],"wire format changed");
    check(LL::EncodeResponse("ListMasters",{"0","2"})=="ListMasters,OK,2,;","success wire response changed");
    check(LL::EncodeResponse("StartMaster",{"123"})=="StartMaster,Fail,123,;","failure wire response changed");
    check(!LL::DecodeCommand("SetSlaveState,m1,,8,;",command),"empty parameter accepted");
    check(!LL::DecodeCommand(std::string("cmd\0x,;",8),command),"embedded NUL accepted");
    LL::FrameBuffer oversized;check(!oversized.Append(std::string(LL::MaxCommandBytes+1,'x'),frames),"oversized request accepted");
    LL::MasterRegistry registry;
    check(!registry.Get("unregistered"),"unregistered master exists");
    check(!registry.Register({"unsupported",LL::BusType::Profinet,"eth1",1000}),"uninstalled protocol silently accepted");
    check(registry.RegisterFactory(LL::BusType::EtherCAT,[](const LL::MasterConfig& c){return std::make_shared<FakeMaster>(c);}),"factory registration failed");
    check(registry.Register({"m1",LL::BusType::EtherCAT,"eth0",1000}),"registration failed");
    check(!registry.Register({"m1",LL::BusType::EtherCAT,"eth1",1000}),"duplicate id accepted");
    check(!registry.Register({"m2",LL::BusType::EtherCAT,"eth0",1000}),"duplicate NIC accepted");
    check(!registry.Register({"m2",LL::BusType::EtherCAT,"eth1",0}),"zero cycle accepted");
    std::vector<std::future<bool>> registrations;
    for(int i=0;i<8;++i)registrations.push_back(std::async(std::launch::async,[&]{return registry.Register({"m2",LL::BusType::EtherCAT,"eth1",1000});}));
    int successes=0;for(auto& result:registrations)if(result.get())++successes;
    check(successes==1&&registry.List().size()==2,"concurrent registration not isolated");
    auto first=registry.Get("m1"),second=registry.Get("m2");
    check(first->Start()&&second->Start(),"parallel backend start failed");
    check(!registry.Unregister("m1"),"running master unregistered");
    first->Stop();check(second->Status().running,"stopping one master affected another");
    check(registry.Unregister("m1")&&!registry.Get("m1"),"unregister failed");
    registry.CloseAll();check(!second->Status().running,"close-all leaked backend");
    std::cout<<"PASS: wire framing, responses, registration validation, concurrent registration and backend isolation\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
