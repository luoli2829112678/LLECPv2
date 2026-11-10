#include "LL_ControllerRuntime.h"
#include "LL_CommandCodec.h"
#include "LLC_CmdMsg.h"
#include "HSTR/HSTR.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <chrono>
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int connectTo(int port){
    int fd=socket(AF_INET,SOCK_STREAM,0);sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(static_cast<uint16_t>(port));inet_pton(AF_INET,"127.0.0.1",&address.sin_addr);
    timeval timeout{3,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    if(connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0){close(fd);return -1;}return fd;
}
std::string request(int port,const LL::Command& command){
    int fd=connectTo(port);check(fd>=0,"controller listener missing");const auto text=LL::EncodeCommand(command);
    const auto split=text.size()/2;check(send(fd,text.data(),split,MSG_NOSIGNAL)==static_cast<ssize_t>(split),"fragment send failed");
    check(send(fd,text.data()+split,text.size()-split,MSG_NOSIGNAL)==static_cast<ssize_t>(text.size()-split),"fragment send failed");
    LL::FrameBuffer buffer;std::vector<std::string> frames;
    while(frames.empty()){char bytes[4096];const auto n=recv(fd,bytes,sizeof(bytes),0);check(n>0,"response timeout");check(buffer.Append(std::string(bytes,static_cast<std::size_t>(n)),frames),"invalid response frame");}
    close(fd);return frames.front();
}
}
int main(int argc,char** argv){
    int code=0;if(HSTR::HandleCompilerInvocation(argc,argv,code))return code;
    const auto root=std::filesystem::temp_directory_path()/("llecp-network-"+std::to_string(getpid()));
    try{
        check(LL_InitializeController(root.string()),"controller startup failed");
        check(request(8888,{Cmd_ListMasters,{}})=="ListMasters,OK,0,;","initial registry not empty");
        check(request(8888,{Cmd_RegistrationMaster,{"test","EtherCAT","nonexistent-test-interface","1000"}}).find(",OK")!=std::string::npos,"network registration failed");
        check(request(8888,{Cmd_RegistrationMaster,{"test","EtherCAT","other","1000"}}).find(",Fail")!=std::string::npos,"duplicate master accepted over network");
        check(request(8888,{Cmd_ReadMasterStatus,{"missing"}}).find(",Fail")!=std::string::npos,"unknown master reported success");
        check(request(8890,{Cmd_HSTR_CreateApplication,{"sample"}}).find(",OK")!=std::string::npos,"HSTR application command not connected");
        for(int port:{8889,8891,8893,8894}){int fd=connectTo(port);check(fd>=0,"preserved listener missing");close(fd);}
        int oldEngine=connectTo(8892);check(oldEngine<0,"removed execution engine port still open");
        LL_ShutdownController();std::filesystem::remove_all(root);
        std::cout<<"PASS: controller startup, real TCP command routing, HSTR API and retained ports\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';LL_ShutdownController();std::filesystem::remove_all(root);return 1;}
}
