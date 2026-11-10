#include "LL_CommandCodec.h"
#include "LL_MasterRegistry.h"
#include "LL_PdoAssignment.h"
#include <cstring>
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
class AssignmentMaster:public FakeMaster {
public:
    AssignmentMaster():FakeMaster({"fixed",LL::BusType::EtherCAT,"test",1000}){}
    std::vector<std::uint16_t> pdos{0x1600};
    bool fixed=true,readable=true,ignoreWrites=false;
    int writes=0;
    int countSize=1;
    int indexSize=2;
    bool requireInt32=false;
    struct Write {std::uint16_t object;std::uint8_t sub;int width;std::uint32_t value;};
    std::vector<Write> accepted;
    int ReadSDO(std::uint16_t,std::uint16_t,std::uint8_t sub,void* data,int& size,bool,int) override {
        if(!readable)return -1;
        if(sub==0){if(size<countSize)return -1;std::memset(data,0,countSize);*static_cast<std::uint8_t*>(data)=static_cast<std::uint8_t>(pdos.size());size=countSize;return 1;}
        if(sub>pdos.size()||size<indexSize)return -1;
        auto* bytes=static_cast<std::uint8_t*>(data);std::memset(bytes,0,indexSize);bytes[0]=pdos[sub-1]&255;bytes[1]=pdos[sub-1]>>8;size=indexSize;return 1;
    }
    int WriteSDO(std::uint16_t,std::uint16_t object,std::uint8_t sub,const void* data,int width,bool,int) override {
        ++writes;if(fixed||(requireInt32&&width!=4))return -1;
        const auto* bytes=static_cast<const std::uint8_t*>(data);
        std::uint32_t value=0;for(int i=0;i<width;++i)value|=static_cast<std::uint32_t>(bytes[i])<<(i*8);
        accepted.push_back({object,sub,width,value});
        if(ignoreWrites)return 1;
        if(sub==0)pdos.resize(bytes[0]);
        else {if(pdos.size()<sub)pdos.resize(sub);pdos[sub-1]=bytes[0]|(bytes[1]<<8);}
        return 1;
    }
};
}
int main(){try{
    AssignmentMaster assignment;
    LL::EnsurePdoAssignment(assignment,1,0x1c12,{0x1600});
    check(assignment.writes==0,"unchanged fixed assignment must not be written");
    for(const int width:{2,4}){assignment.countSize=width;LL::EnsurePdoAssignment(assignment,1,0x1c12,{0x1600});}
    check(assignment.writes==0,"padded count must not cause a fixed assignment write");
    assignment.indexSize=4;LL::EnsurePdoAssignment(assignment,1,0x1c12,{0x1600});
    check(assignment.writes==0,"padded PDO index must not cause a fixed assignment write");
    bool rejected=false;
    try{LL::EnsurePdoAssignment(assignment,1,0x1c12,{0x1601});}catch(const std::exception& e){rejected=std::string(e.what()).find("slave 1")!=std::string::npos;}
    check(rejected,"different fixed assignment must fail with slave diagnostics");
    assignment.fixed=false;
    LL::EnsurePdoAssignment(assignment,1,0x1c12,{0x1601,0x1602});
    check(assignment.pdos==std::vector<std::uint16_t>({0x1601,0x1602}),"configurable assignment not applied");
    assignment.ignoreWrites=true;rejected=false;
    try{LL::EnsurePdoAssignment(assignment,1,0x1c12,{0x1600});}catch(...){rejected=true;}
    check(rejected,"ignored assignment writes must not report success");
    assignment.ignoreWrites=false;
    LL::EnsurePdoAssignment(assignment,1,0x1c12,{});
    check(assignment.pdos.empty(),"empty selection must clear assignment");
    AssignmentMaster legacy;legacy.fixed=false;legacy.readable=false;legacy.requireInt32=true;
    LL::EnsurePdoAssignment(legacy,1,0x1c12,{0x1600},true,false);
    LL::EnsurePdoAssignment(legacy,1,0x1c13,{0x1a00},true,false);
    check(legacy.accepted.size()==6,"legacy initialization did not write both assignments");
    for(std::size_t i=0;i<6;++i){
        const auto& write=legacy.accepted[i];
        check(write.object==(i<3?0x1c12:0x1c13)&&write.width==4,"legacy assignment object/INT32 width differs from reference");
        const auto step=i%3;
        check(write.sub==(step==1?1:0)&&write.value==(step==0?0:step==2?1:i<3?0x1600:0x1a00),"legacy clear/index/count sequence differs from reference");
    }
    AssignmentMaster reinitialize;reinitialize.fixed=false;
    LL::EnsurePdoAssignment(reinitialize,1,0x1c12,{0x1600},true);
    check(reinitialize.accepted.size()==3,"startup skipped initialization because assignment already matched");
    check(reinitialize.accepted[0].width==1&&reinitialize.accepted[1].width==2,"standard PDO assignment widths changed");
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
