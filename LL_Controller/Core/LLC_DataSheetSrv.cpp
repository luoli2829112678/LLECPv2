#include "LLC_DataSheetSrv.h"
#include "LLC_Rbt.h"
#include "LL_Process.h"
#include "LLStruct_Protocol.h"
#include <cstring>
CNethandle* LLC_DataSheetSrv::clone(){auto* p=new LLC_DataSheetSrv;p->SetSrv(periodUs_,json_);return p;}
int LLC_DataSheetSrv::OnTimeout(){
    const auto now=LL_GetCurUS();if(socket_<0||now-lastSend_<static_cast<unsigned>(periodUs_))return 0;
    lastSend_=now;std::string payload;
    {std::lock_guard<std::mutex> lock(LLCRbt->m_8893dataMutex);payload=LLCRbt->m_sRtData8893;}
    if(payload.empty())return 0;
    ST_LLPackHeaderSend header{MagicNUM,static_cast<unsigned>(sizeof(header)+payload.size()),static_cast<unsigned>(payload.size())};
    std::string frame(reinterpret_cast<const char*>(&header),sizeof(header));frame+=payload;
    return LLNet_Send(socket_,frame.data(),static_cast<int>(frame.size()));
}
void LLC_DataSheetSrv::DataSheetBroadcast(){OnTimeout();}
