#pragma once
#include <string>
#include <cstdint>
#include <cstring>
#include <mutex>
#include "LL_AutoLock.h"
using namespace std;
enum EN_NetSendResult { enNetSendRlt_Successed=0,enNetSendRlt_Timeout=-1,enNetSendRlt_SendError=-2,enNetSendRlt_ParamError=-3 };
struct NetSendState { int err_no=0; unsigned bytesToSent=0; void Clear(){err_no=0;bytesToSent=0;} };
class CNethandle {
public:
    virtual ~CNethandle() = default;
    virtual CNethandle* clone()=0;
    virtual int FinishedListen(){return 0;}
    virtual int OnConnect(int)=0;
    virtual int OnDisconnect(int)=0;
    virtual int OnRecv(int,const string&)=0;
    virtual int OnSend(int,string)=0;
    virtual int OnError(int,string)=0;
    virtual int OnTimeout()=0;
    void SetListenSocket(int s){m_nListenSocket=s;}
    bool IsClient()const{return m_bClient;}
    void SetClient(){m_bClient=true;}
    int OnRecv_Event(int s,const string& text){return OnRecv(s,text);}
    char m_szRecvBuf[1024]{};
protected:
    bool m_bClient=false;
    int m_nListenSocket=-1;
};
int LLNet_Listen(int port,CNethandle* callback,string bindIp="0.0.0.0",bool createTimer=false);
int LLNet_Send(int socket,const char* bytes,int size,bool waitComplete=true);
int LLNet_Send(int socket,const char* bytes,int size,NetSendState& state,bool waitComplete=true);
void LLNet_CloseClientConnection(int socket);
void LLNet_StopAll();
