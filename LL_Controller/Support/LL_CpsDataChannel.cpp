#include "LL_CpsService.h"
#include "LL_netbase.h"
#include "LL_CommandCodec.h"
#include <arpa/inet.h>
#include <sys/socket.h>
std::shared_ptr<LL::DataStream> LL_CpsService::AttachData(const std::string &peer,
                                                          const std::string &session) {
    std::lock_guard<std::mutex> lock(dataMutex_);
    const auto it = dataStreams_.find(peer + "/" + session);
    return it == dataStreams_.end() ? nullptr : it->second;
}
namespace {
class DataHandler : public CNethandle {
    int socket_ = -1;
    LL::FrameBuffer input_;
    std::string peer_;
    std::shared_ptr<LL::DataStream> stream_;
    std::chrono::steady_clock::time_point sent_{};
    std::uint64_t last_ = 0;

  public:
    CNethandle *clone() override {
        return new DataHandler;
    }
    int OnConnect(int fd) override {
        socket_ = fd;
        sockaddr_in address{};
        socklen_t size = sizeof(address);
        char ip[INET_ADDRSTRLEN]{};
        if (getpeername(fd, reinterpret_cast<sockaddr *>(&address), &size) != 0)
            return -1;
        inet_ntop(AF_INET, &address.sin_addr, ip, sizeof(ip));
        peer_ = ip;
        return 0;
    }
    int OnRecv(int, const std::string &bytes) override {
        std::vector<std::string> frames;
        if (!input_.Append(bytes, frames))
            return -1;
        for (const auto &frame : frames) {
            LL::Command command;
            if (!LL::DecodeCommand(frame, command) || command.parameters.size() != 1)
                return -1;
            if (command.name == LL::DataCommand::Attach)
                stream_ = LL_CpsService::Get().AttachData(peer_, command.parameters[0]);
            else if (command.name == LL::DataCommand::Ack && stream_) {
                try {
                    stream_->Ack(std::stoull(command.parameters[0]));
                } catch (...) {
                    return -1;
                }
            } else
                return -1;
            if (!stream_)
                return -1;
        }
        return 0;
    }
    int OnTimeout() override {
        if (!stream_)
            return 0;
        if (stream_->Expired())
            return -1;
        const auto now = std::chrono::steady_clock::now();
        const auto batch = stream_->Peek(last_, now - sent_ >= std::chrono::milliseconds(250));
        if (batch.is_null())
            return 0;
        const auto sequence = batch.at("batch").get<std::uint64_t>();
        if (sequence == last_ && now - sent_ < std::chrono::milliseconds(250))
            return 0;
        const auto frame = LL::EncodeCommand({LL::DataCommand::Batch, {LL::Encode64(LL::SealDataBatch(batch).dump())}});
        if (LLNet_Send(socket_, frame.data(), static_cast<int>(frame.size()), true) != 0)
            return -1;
        last_ = sequence;
        sent_ = now;
        return 0;
    }
    int OnDisconnect(int) override {
        return 0;
    }
    int OnSend(int, std::string) override {
        return 0;
    }
    int OnError(int, std::string) override {
        return 0;
    }
};
} // namespace
CNethandle *LL_CreateCpsDataHandler() {
    return new DataHandler;
}
