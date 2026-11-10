#include "LL_netbase.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <atomic>
#include <thread>
#include <vector>
#include <set>
#include <future>
#include <memory>
#include <chrono>
namespace {
struct Server {
    int fd=-1;
    std::unique_ptr<CNethandle> prototype;
    std::thread thread;
};
struct Runtime {
    std::atomic<bool> stopping{false};
    std::mutex mutex;
    std::vector<std::unique_ptr<Server>> servers;
    std::set<int> clients;
    ~Runtime(){stop();}
    void serve(Server* server) {
        std::vector<std::future<void>> tasks;
        while(!stopping) {
            for(auto it=tasks.begin();it!=tasks.end();) {
                if(it->wait_for(std::chrono::seconds(0))==std::future_status::ready){it->get();it=tasks.erase(it);}else ++it;
            }
            pollfd event{server->fd,POLLIN,0};
            const int result=poll(&event,1,100);
            if(result<=0 || !(event.revents&POLLIN))continue;
            const int fd=accept(server->fd,nullptr,nullptr);
            if(fd<0)continue;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if(stopping || clients.size()>=128){::close(fd);continue;}
                clients.insert(fd);
            }
            auto handler=std::unique_ptr<CNethandle>(server->prototype->clone());
            tasks.push_back(std::async(std::launch::async,[this,fd,handler=std::move(handler)]()mutable {
                timeval timeout{1,0};setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
                int noDelay=1;setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&noDelay,sizeof(noDelay));
                try {
                    handler->OnConnect(fd);
                    while(!stopping) {
                        pollfd event{fd,POLLIN,0};
                        const int ready=poll(&event,1,1);
                        if(ready<0){if(errno==EINTR)continue;break;}
                        if(ready>0) {
                            if(event.revents&(POLLERR|POLLHUP|POLLNVAL))break;
                            char bytes[4096];const auto n=recv(fd,bytes,sizeof(bytes),0);
                            if(n<=0)break;
                            if(handler->OnRecv(fd,std::string(bytes,static_cast<std::size_t>(n)))<0)break;
                        }
                        if(handler->OnTimeout()<0)break;
                    }
                } catch(const std::exception& ex) {handler->OnError(fd,ex.what());}
                handler->OnDisconnect(fd);
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    clients.erase(fd);::close(fd);
                }
            }));
        }
        for(auto& task:tasks)task.get();
    }
    void stop() {
        stopping=true;
        std::vector<std::unique_ptr<Server>> current;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for(auto fd:clients)shutdown(fd,SHUT_RDWR);
            for(auto& server:servers)shutdown(server->fd,SHUT_RDWR);
            current.swap(servers);
        }
        for(auto& server:current){if(server->thread.joinable())server->thread.join();::close(server->fd);}
    }
};
Runtime& runtime(){static Runtime value;return value;}
}
int LLNet_Listen(int port,CNethandle* callback,string bindIp,bool) {
    if(!callback||port<1||port>65535)return EINVAL;
    auto& rt=runtime();
    auto server=std::make_unique<Server>();server->prototype.reset(callback->clone());
    server->fd=socket(AF_INET,SOCK_STREAM,0);if(server->fd<0)return errno;
    int reuse=1;setsockopt(server->fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(static_cast<uint16_t>(port));
    if(inet_pton(AF_INET,bindIp.c_str(),&address.sin_addr)!=1){::close(server->fd);return EINVAL;}
    if(::bind(server->fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0||listen(server->fd,16)<0){const int e=errno;::close(server->fd);return e;}
    server->prototype->SetListenSocket(server->fd);
    server->prototype->FinishedListen();
    std::lock_guard<std::mutex> lock(rt.mutex);
    rt.stopping=false;
    Server* raw=server.get();
    server->thread=std::thread([&rt,raw]{rt.serve(raw);});
    rt.servers.push_back(std::move(server));
    return 0;
}
int LLNet_Send(int socket,const char* bytes,int size,NetSendState& state,bool waitComplete) {
    state.Clear();if(!bytes||size<0)return enNetSendRlt_ParamError;
    while(state.bytesToSent<static_cast<unsigned>(size)) {
        const auto count=send(socket,bytes+state.bytesToSent,static_cast<std::size_t>(size)-state.bytesToSent,MSG_NOSIGNAL);
        if(count>0){state.bytesToSent+=static_cast<unsigned>(count);continue;}
        if(count<0&&errno==EINTR)continue;
        state.err_no=errno;
        if((errno==EAGAIN||errno==EWOULDBLOCK)&&waitComplete)return enNetSendRlt_Timeout;
        return enNetSendRlt_SendError;
    }
    return enNetSendRlt_Successed;
}
int LLNet_Send(int socket,const char* bytes,int size,bool waitComplete){NetSendState state;return LLNet_Send(socket,bytes,size,state,waitComplete);}
void LLNet_CloseClientConnection(int socket){shutdown(socket,SHUT_RDWR);}
void LLNet_StopAll(){runtime().stop();}
