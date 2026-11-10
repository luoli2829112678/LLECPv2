#include "LLC_IF.h"
#include "LLC_Rbt.h"
#include "LL_Errorcode.h"
#include <chrono>
CNethandle* LLCIFHandle::clone(){return new LLCIFHandle(m_port);}
int LLCIFHandle::OnConnect(int socket){return LLCRbt->OnConnect(socket);}
int LLCIFHandle::OnDisconnect(int socket){
    std::lock_guard<std::recursive_mutex> lock(LLCRbt->ControllerMutex());
    auto* original=LLCRbt->GetIFHandle(m_port);
    if(original&&original->m_ifhandle==this)original->m_ifhandle=nullptr;
    return LLCRbt->OnDisconnect(socket);
}
int LLCIFHandle::OnRecv(int socket,const std::string& bytes){
    static std::mutex portSerial[16];
    std::lock_guard<std::mutex> serial(portSerial[m_port % 16]);
    std::vector<std::string> frames;
    if(!m_frames.Append(bytes,frames))return -1;
    for(const auto& frame:frames){
        LL::Command command;std::vector<std::string> result;
        if(!LL::DecodeCommand(frame,command)){result={std::to_string(REC_CmdFormatError)};SendRes(socket,command.name,result);continue;}
        {
            std::lock_guard<std::recursive_mutex> gate(LLCRbt->ControllerMutex());
            if(auto* original=LLCRbt->GetIFHandle(m_port))original->m_ifhandle=this;
            {std::lock_guard<std::mutex> lock(reply->mutex);reply->ready=false;reply->values.clear();}
            try{LLCRbt->DispatchCmdFunc(socket,command.name,command.parameters,result,m_port);}
            catch(const std::exception&){result={std::to_string(REC_ParametersError)};}
        }
        if(result.empty()){
            std::unique_lock<std::mutex> lock(reply->mutex);
            if(reply->cv.wait_for(lock,std::chrono::seconds(1),[this]{return reply->ready;}))result=reply->values;
            else result={std::to_string(REC_TimeOutError)};
        }
        if(SendRes(socket,command.name,result)!=0)return -1;
    }
    return 0;
}
int LLCIFHandle::SendRes(int socket,const std::string& command,std::vector<std::string>& result){
    const auto frame=LL::EncodeResponse(command,result);
    return LLNet_Send(socket,frame.data(),static_cast<int>(frame.size()));
}
void LLCIFHandle::SetCmdResult(const std::vector<std::string>& result){
    {std::lock_guard<std::mutex> lock(reply->mutex);reply->values=result;reply->ready=true;}
    reply->cv.notify_all();
}
