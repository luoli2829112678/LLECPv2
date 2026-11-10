#include "LL_MasterRegistry.h"
#include "EtherCAT_Master.h"
#include <mutex>
#include <cstring>
extern "C" {
#include "soem/soem.h"
}
namespace LL {
class EtherCATBackend final : public IMaster {
public:
    explicit EtherCATBackend(MasterConfig config) : config_(std::move(config)) {}
    const MasterConfig& Config() const override { return config_; }
    bool Scan() override {
        if (master_.IsRunning()) return false;
        descriptors_.clear();
        if (!master_.IsOpen() && !master_.Open(config_.endpoint.c_str())) return false;
        return master_.ScanSlaves() > 0;
    }
    bool Configure() override {
        if (!master_.GetMonitorStatus().mapped && master_.ConfigureProcessData() <= 0) return false;
        return master_.VisitNativeSlaves([this](int count, void* native) {
            const auto* first = static_cast<const ec_slavet*>(native);
            descriptors_.assign(first, first + count + 1);
            inputs_.resize(count+1);outputs_.resize(count+1);
            for(int i=1;i<=count;++i){auto& d=descriptors_[i];
                inputs_[i].assign((d.Ibits+d.Istartbit+7u)/8u,0);outputs_[i].assign((d.Obits+d.Ostartbit+7u)/8u,0);
                if(!inputs_[i].empty()){std::memcpy(inputs_[i].data(),d.inputs,inputs_[i].size());d.inputs=inputs_[i].data();}
                if(!outputs_[i].empty()){std::memcpy(outputs_[i].data(),d.outputs,outputs_[i].size());d.outputs=outputs_[i].data();}
            }
            return true;
        });
    }
    bool Start(bool background) override {
        if (master_.IsRunning()) return true;
        if (!master_.IsOpen() || master_.GetSlaveCount() <= 0) return false;
        if (!master_.GetMonitorStatus().mapped && !Configure()) return false;
        EtherCAT_Master::CycleOptions options;
        options.period = std::chrono::microseconds(config_.cycleUs);
        options.background = background;
        background_=background;Exchange();return master_.Start(options);
    }
    void Stop() override { master_.Stop(); }
    void Close() override { master_.Close(); descriptors_.clear(); }
    int Exchange() override {
        for(std::size_t i=1;i<descriptors_.size();++i)
            if(!outputs_[i].empty())master_.WriteOutputs(static_cast<std::uint16_t>(i),outputs_[i].data(),outputs_[i].size());
        const int wkc=master_.IsRunning()&&background_?master_.GetMonitorStatus().actualWkc:master_.ExchangeProcessData();
        RefreshInputs();
        return wkc;
    }
    bool RefreshInputs() override {
        for(std::size_t i=1;i<descriptors_.size();++i) {
            const auto input=master_.ReadInputs(static_cast<std::uint16_t>(i));
            if(input.size()!=inputs_[i].size())return false;
            if(!input.empty())std::memcpy(inputs_[i].data(),input.data(),input.size());
        }
        return !descriptors_.empty();
    }
    bool Poll() override {
        const bool result = master_.PollMonitor();
        const auto slaves = master_.GetSlaves();
        for (const auto& slave : slaves) if (slave.id < descriptors_.size()) {
            descriptors_[slave.id].state = slave.state;
            descriptors_[slave.id].ALstatuscode = slave.alStatusCode;
            descriptors_[slave.id].islost = slave.lost ? TRUE : FALSE;
        }
        return result;
    }
    MasterStatus Status() const override {
        const auto status = master_.GetMonitorStatus();
        MasterStatus result;
        result.opened = status.opened; result.running = status.running;
        result.expectedWkc = status.expectedWkc; result.actualWkc = status.actualWkc;
        result.cycles = status.cycles; result.wkcErrors = status.wkcErrors; result.missedDeadlines = status.missedDeadlines;
        result.sentFrames=status.sentFrames;result.sendErrors=status.sendErrors;
        result.receiveErrors=status.receiveErrors;result.framesPerSecond=status.running?status.framesPerSecond:0;
        result.connected = status.opened && status.slaveCount > 0;
        for (const auto& slave : master_.GetSlaves()) {
            const bool connected = !slave.lost && slave.state != EC_STATE_NONE;
            result.connected = result.connected && connected;
            result.slaves.push_back({slave.id,slave.state,slave.alStatusCode,slave.productCode,slave.revision,slave.name,connected,slave.vendorId});
        }
        return result;
    }
    std::string LastError() const override { return master_.GetLastError(); }
    bool ConfigureSync0(std::uint16_t id,bool enabled,std::uint32_t cycle,std::int32_t shift) override { return master_.ConfigureSync0(id,enabled,cycle,shift); }
    bool ConfigureSync01(std::uint16_t id,std::uint32_t cycle0,std::uint32_t cycle1,std::int32_t shift) override { return master_.ConfigureSync01(id,cycle0,cycle1,shift); }
    bool ChangeSlaveState(std::uint16_t slave, std::uint16_t state) override {
        const bool result = master_.ChangeSlaveState(slave, static_cast<EtherCAT_Master::State>(state));
        Poll(); return result;
    }
    int ReadSDO(std::uint16_t slave, std::uint16_t index, std::uint8_t sub, void* data, int& size, bool ca, int timeout) override {
        return master_.ReadSDO(slave,index,sub,data,size,ca,timeout);
    }
    int WriteSDO(std::uint16_t slave, std::uint16_t index, std::uint8_t sub, const void* data, int size, bool ca, int timeout) override {
        return master_.WriteSDO(slave,index,sub,data,size,ca,timeout);
    }
    int WriteFile(std::uint16_t slave, const std::string& local, const std::string& remote, std::uint32_t password) override {
        master_.Stop(); EtherCAT_Master::FoEOptions options; options.password=password; options.bootstrap=true;
        return master_.FoEWriteFile(slave,local,remote,options);
    }
    int ReadFile(std::uint16_t slave, const std::string& local, const std::string& remote, std::uint32_t password) override {
        master_.Stop(); EtherCAT_Master::FoEOptions options; options.password=password; options.bootstrap=true;
        return master_.FoEReadFile(slave,local,remote,256u*1024u*1024u,options);
    }
    int WriteSiiFile(std::uint16_t id, const std::string& path) override { master_.Stop(); return master_.WriteSiiFile(id,path); }
    FileTransferStatus FileStatus() const override {
        const auto status=master_.GetFoEStatus();
        return {status.active,status.succeeded,status.transferredBytes,status.totalBytes,status.result};
    }
    bool VisitSlaves(const std::function<bool(int,void*)>& visitor) override {
        if (!visitor || descriptors_.empty()) return false;
        return visitor(static_cast<int>(descriptors_.size())-1,descriptors_.data());
    }
private:
    MasterConfig config_;
    EtherCAT_Master master_;
    // Legacy axes retain descriptors from this stable snapshot, never from SOEM's mutable context.
    std::vector<ec_slavet> descriptors_;
    std::vector<std::vector<std::uint8_t>> inputs_,outputs_;
    bool background_=false;
};
std::shared_ptr<IMaster> CreateEtherCATMaster(const MasterConfig& config) { return std::make_shared<EtherCATBackend>(config); }
}
