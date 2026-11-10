#include "LL_CommandCodec.h"
#include "LLC_CmdMsg.h"
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    if(argc>1&&std::string(argv[1])=="--help") {
        std::cout<<"Usage: LL_CPS [host [port [command [parameter ...]]]]\n";return 0;
    }
    const char* host=argc>1?argv[1]:"127.0.0.1";
    const char* port=argc>2?argv[2]:"8888";
    LL::Command command{argc>3?argv[3]:Cmd_ListMasters,{}};
    for(int i=4;i<argc;++i)command.parameters.emplace_back(argv[i]);
    addrinfo hints{},*addresses=nullptr;hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
    if(getaddrinfo(host,port,&hints,&addresses)!=0){std::cerr<<"Cannot resolve controller\n";return 1;}
    int fd=-1;
    for(auto* item=addresses;item;item=item->ai_next){
        fd=socket(item->ai_family,item->ai_socktype,item->ai_protocol);if(fd<0)continue;
        timeval timeout{5,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
        if(connect(fd,item->ai_addr,item->ai_addrlen)==0)break;
        ::close(fd);fd=-1;
    }
    freeaddrinfo(addresses);
    if(fd<0){std::cerr<<"Cannot connect to controller\n";return 2;}
    const auto frame=LL::EncodeCommand(command);
    std::size_t sent=0;
    while(sent<frame.size()){
        const auto n=send(fd,frame.data()+sent,frame.size()-sent,MSG_NOSIGNAL);
        if(n<=0){::close(fd);return 3;}sent+=static_cast<std::size_t>(n);
    }
    LL::FrameBuffer response;std::vector<std::string> frames;
    while(frames.empty()){
        char buffer[4096];const auto n=recv(fd,buffer,sizeof(buffer),0);
        if(n<=0||!response.Append(std::string(buffer,static_cast<std::size_t>(n)),frames)){::close(fd);return 4;}
    }
    std::cout<<frames.front()<<'\n';::close(fd);
    return frames.front().find(",OK")!=std::string::npos?0:5;
}
