#pragma once
#include "LL_CpsData.h"
#include "LL_Master.h"
#include "LL_DataStream.h"
#include <chrono>
#include <future>
#include <map>
#include <set>
#include <netinet/in.h>
class LL_CpsService {
  public:
    static LL_CpsService &Get();
    LL::Json Handle(int socket, const std::vector<std::string> &parameters);
    void BeforeCycle();
    void AfterCycle();
    void Forces();
    void Shutdown();
    std::shared_ptr<LL::DataStream> AttachData(const std::string& peer,const std::string& session);
    bool Attached() const {
        return project_.is_object();
    }
    bool Connected() const;
    void InvalidateAxes() {
        prepared_ = false;
        axisIndex_.clear();
        axisMaster_.clear();
        readyAxes_.clear();
        virtualPdos_.clear();
    }
    int ResolveAxis(int id) const {
        if (!Attached())
            return id;
        const auto it = axisIndex_.find(id);
        return it != axisIndex_.end() && readyAxes_.count(id) ? it->second : -1;
    }
    bool IsBuilding() const {
        return buildJob_.valid();
    }
    bool Emergency() const {
        return emergency_;
    }

  private:
    using Json = LL::Json;
    using Clock = std::chrono::steady_clock;
    struct Transfer {
        std::string purpose, name, owner;
        LL::fs::path path;
        std::size_t size = 0, offset = 0;
        bool committed = false;
        Clock::time_point touched;
    };
    struct Subscription {
        sockaddr_in address{};
        std::vector<std::string> channels;
        Clock::time_point expires, next;
        int intervalMs = 10;
        std::uint64_t sequence = 0;
        std::uint64_t missedSamples = 0;
        std::shared_ptr<LL::DataStream> stream;
        std::vector<Json> metadata;
    };
    Json Dispatch(const Json &, const std::string &peer);
    Json Status();
    Json Variables();
    Json Read(const std::string &);
    Json ReadResolved(const Json&);
    Json Metadata(const std::string &);
    Json Pdo(const Json& request, bool write);
    void RefreshReadyAxes();
    void Neutralize(int);
    void Apply(const Json &);
    void Prepare();
    void Download(const Json &);
    void StopMaster(const std::string &);
    void SetState(unsigned, const std::string &);
    std::shared_ptr<LL::IMaster> Master(const std::string &);
    const Json &Config(const std::string &) const;
    bool Running(const std::string &) const;
    std::future<Json> buildJob_;
    Json buildResult_;
    std::string buildingApp_;
    std::string commandError_;
    Json project_;
    bool prepared_ = false, emergency_ = false;
    std::map<std::string, bool> simulation_;
    std::map<int, int> axisIndex_;
    std::map<int, std::string> axisMaster_;
    std::set<int> readyAxes_;
    struct VirtualPdoImage {std::vector<std::uint8_t> rx,tx;};
    std::map<std::string,VirtualPdoImage> virtualPdos_;
    std::map<std::string, std::vector<std::uint8_t>> forces_;
    std::map<std::string, Transfer> transfers_;
    std::map<std::string, Subscription> subscriptions_;
    std::mutex dataMutex_;
    std::map<std::string,std::shared_ptr<LL::DataStream>> dataStreams_;
    std::string fileToken_;
    int udp_ = -1;
    Clock::time_point nextPoll_{};
    std::future<int> fileJob_;
    int fileResult_ = 0;
    bool fileDone_ = false;
};
