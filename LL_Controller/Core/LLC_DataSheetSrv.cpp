#include "LLC_DataSheetSrv.h"
#include "LL_DataStream.h"
#include "LLC_Rbt.h"
#include "LL_Process.h"
#include "LLStruct_Protocol.h"
#include <cstring>
extern CNethandle* LL_CreateCpsDataHandler();
int LLC_DataSheetSrv::OnConnect(int socket){socket_=socket;connectedAt_=LL_GetCurUS();return 0;}
int LLC_DataSheetSrv::OnRecv(int socket,const std::string& bytes){
    if(batch_)return batch_->OnRecv(socket,bytes);
    if(probe_.size()+bytes.size()>1024)return -1;
    probe_+=bytes;
    if(probe_.rfind(std::string(LL::DataCommand::Attach)+",",0)==0){
        batch_.reset(LL_CreateCpsDataHandler());
        if(batch_->OnConnect(socket)<0)return -1;
        return batch_->OnRecv(socket,probe_);
    }
    return 0;
}
CNethandle* LLC_DataSheetSrv::clone(){auto* p=new LLC_DataSheetSrv;p->SetSrv(periodUs_,json_);return p;}
int LLC_DataSheetSrv::OnTimeout(){
    if(batch_)return batch_->OnTimeout();
    if(LL_GetCurUS()-connectedAt_<100000)return 0;
    const auto now=LL_GetCurUS();if(socket_<0||now-lastSend_<static_cast<unsigned>(periodUs_))return 0;
    lastSend_=now;std::string payload;
    {std::lock_guard<std::mutex> lock(LLCRbt->m_8893dataMutex);payload=LLCRbt->m_sRtData8893;}
    if(payload.empty())return 0;
    ST_LLPackHeaderSend header{MagicNUM,static_cast<unsigned>(sizeof(header)+payload.size()),static_cast<unsigned>(payload.size())};
    std::string frame(reinterpret_cast<const char*>(&header),sizeof(header));frame+=payload;
    return LLNet_Send(socket_,frame.data(),static_cast<int>(frame.size()));
}
void LLC_DataSheetSrv::DataSheetBroadcast(){OnTimeout();}
