#pragma once
#include "LL_netbase.h"
class LLC_DataSheetSrv:public CNethandle {
public:
    void SetSrv(int periodUs,bool json){periodUs_=periodUs;json_=json;}
    void SetCycleTime(double cycle){periodUs_=static_cast<int>(cycle*1000);}
    int FinishedListen() override{return 0;}
    CNethandle* clone() override;
    int OnConnect(int socket) override{socket_=socket;return 0;}
    int OnDisconnect(int) override{socket_=-1;return 0;}
    int OnRecv(int,const std::string&) override{return 0;}
    int OnSend(int,std::string) override{return 0;}
    int OnError(int,std::string) override{return 0;}
    int OnTimeout() override;
    void DataSheetBroadcast();
    void LogDataSheet(std::string&){}
private:
    int socket_=-1,periodUs_=1000;
    bool json_=false;
    unsigned long long lastSend_=0;
};
