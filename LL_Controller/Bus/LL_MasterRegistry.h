#pragma once
#include "LL_Master.h"
#include <map>
#include <mutex>
namespace LL {
class MasterRegistry {
public:
    bool RegisterFactory(BusType type, MasterFactory factory);
    bool Register(const MasterConfig& config);
    bool Unregister(const std::string& id);
    std::shared_ptr<IMaster> Get(const std::string& id) const;
    std::vector<MasterConfig> List() const;
    void CloseAll();
    std::string LastError() const;
private:
    mutable std::mutex mutex;
    std::map<BusType, MasterFactory> factories;
    std::map<std::string, std::shared_ptr<IMaster>> masters;
    std::string error;
};
std::shared_ptr<IMaster> CreateEtherCATMaster(const MasterConfig& config);
}
