#include "LL_MasterRegistry.h"
#include <regex>
namespace LL {
bool MasterRegistry::RegisterFactory(BusType type, MasterFactory factory) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!factory || factories.count(type)) { error = "Invalid or duplicate master factory"; return false; }
    factories.emplace(type, std::move(factory)); return true;
}
bool MasterRegistry::Register(const MasterConfig& config) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!std::regex_match(config.id, std::regex("[A-Za-z_][A-Za-z0-9_-]{0,63}")) ||
        config.endpoint.empty() || config.cycleUs < 100 || config.cycleUs > 1000000) {
        error = "Invalid master id, endpoint or cycle (100..1000000 us)"; return false;
    }
    if (masters.count(config.id)) { error = "Master id already registered"; return false; }
    const auto factory = factories.find(config.type);
    if (factory == factories.end()) { error = "Requested master protocol backend is not installed"; return false; }
    for (const auto& entry : masters) {
        if (config.type == BusType::EtherCAT && entry.second->Config().type == config.type && entry.second->Config().endpoint == config.endpoint) {
            error = "EtherCAT interface already registered"; return false;
        }
    }
    auto master = factory->second(config);
    if (!master) { error = "Master factory failed"; return false; }
    masters.emplace(config.id, std::move(master)); error.clear(); return true;
}
bool MasterRegistry::Unregister(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto it = masters.find(id);
    if (it == masters.end()) { error = "Master is not registered"; return false; }
    if (it->second->Status().running) { error = "Stop master before unregistering"; return false; }
    it->second->Close(); masters.erase(it); error.clear(); return true;
}
std::shared_ptr<IMaster> MasterRegistry::Get(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex);
    const auto it = masters.find(id);
    return it == masters.end() ? nullptr : it->second;
}
std::vector<MasterConfig> MasterRegistry::List() const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<MasterConfig> result;
    for (const auto& entry : masters) result.push_back(entry.second->Config());
    return result;
}
void MasterRegistry::CloseAll() {
    std::vector<std::shared_ptr<IMaster>> snapshot;
    { std::lock_guard<std::mutex> lock(mutex); for (const auto& entry : masters) snapshot.push_back(entry.second); }
    for (const auto& master : snapshot) master->Close();
}
std::string MasterRegistry::LastError() const { std::lock_guard<std::mutex> lock(mutex); return error; }
}
