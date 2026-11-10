#pragma once
#include "LL_CpsData.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <atomic>
namespace LL {
class ControllerClient {
  public:
    using Callback = std::function<void(const Json &)>;
    ControllerClient();
    ~ControllerClient();
    void Endpoint(std::string ip);
    std::string Endpoint() const;
    void Request(Json request, Callback callback = {});
    void Upload(const std::string &purpose, const std::string &name, const std::string &bytes,
                Callback callback);
    void Drain();
    std::vector<ScopeSample> DrainSamples();
    void OpenDataStream(const std::string &token, const std::string &session, unsigned port, bool numeric);
    void CloseDataStream(const std::string &token);
    void StopDataStreams();
    Json DrainData(const std::string &token);
    std::string DataError(const std::string &token) const;
    bool DataEnded(const std::string &token) const;
    Json DataStats(const std::string &token) const;
    unsigned UdpPort() const {
        return udpPort_;
    }
    std::uint64_t LostPackets() const {
        return lost_;
    }
    static Json Call(const std::string &ip, const Json &request);

  private:
    struct Job {
        std::string ip;
        Json request;
        Callback callback;
    };
    void Work();
    void Receive();
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Job> jobs_;
    std::deque<std::function<void()>> done_;
    std::vector<ScopeSample> samples_;
    std::string ip_ = "127.0.0.1";
    std::atomic<bool> stop_{false};
    std::thread worker_, receiver_;
    std::intptr_t udp_ = -1;
    unsigned udpPort_ = 0;
    std::atomic<std::uint64_t> lost_{0};
    struct StreamWorker;
    std::map<std::string, std::shared_ptr<StreamWorker>> streams_;
    std::map<std::string, Json> dataLatest_;
    std::map<std::string, std::string> dataErrors_;
    std::map<std::string, Json> dataStats_;
};
} // namespace LL
