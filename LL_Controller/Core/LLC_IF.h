#pragma once
#include "LL_netbase.h"
#include "LLC_Def.h"
#include "LL_CommandCodec.h"
#include <memory>
#include <mutex>
#include <condition_variable>
class LLCIFHandle:public CNethandle {
public:
    explicit LLCIFHandle(uint16_t port=0):m_port(port){}
    CNethandle* clone() override;
    int OnConnect(int socket) override;
    int OnDisconnect(int socket) override;
    int OnRecv(int socket,const std::string& bytes) override;
    int OnSend(int,std::string) override{return 0;}
    int OnError(int,std::string) override{return 0;}
    int OnTimeout() override{return 0;}
    void SetCmdResult(const std::vector<std::string>& result);
    static int SendRes(int socket,const std::string& command,std::vector<std::string>& result);
    LLCIFHandle* GetIfHandle(){return m_ifhandle;}
    LLCIFHandle* m_ifhandle=nullptr;
private:
    struct Reply {std::mutex mutex;std::condition_variable cv;std::vector<std::string> values;bool ready=false;};
    uint16_t m_port;
    LL::FrameBuffer m_frames;
    std::shared_ptr<Reply> reply=std::make_shared<Reply>();
};
