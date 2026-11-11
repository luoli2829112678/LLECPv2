#include "LL_CpsService.h"
#include "LLC_Rbt.h"
#include "LL_PdoAssignment.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#include <cmath>
using LL::Json;
namespace fs = std::filesystem;
LL_CpsService &LL_CpsService::Get() {
    static LL_CpsService service;
    return service;
}
namespace {
void require(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}
std::vector<std::uint32_t> selected(const Json &slave, const char *key) {
    std::vector<std::uint32_t> result;
    for (const auto &p : slave.at(key))
        if (p.value("selected", false))
            result.push_back(p.at("index"));
    return result;
}
void check(int result, const char *message) {
    require(result == REC_Successed, std::string(message) + ": " + std::to_string(result));
}
} // namespace
std::shared_ptr<LL::IMaster> LL_CpsService::Master(const std::string &id) {
    auto result = LLCRbt->m_masters.Get(id);
    require(result != nullptr, "Master is not registered");
    return result;
}
const Json &LL_CpsService::Config(const std::string &id) const {
    for (const auto &m : project_.at("masters"))
        if (m.value("id", "") == id)
            return m;
    throw std::runtime_error("Master is not in the active project");
}
bool LL_CpsService::Running(const std::string &id) const {
    const auto sim = simulation_.find(id);
    if (sim != simulation_.end())
        return sim->second;
    auto master = LLCRbt->m_masters.Get(id);
    return master && master->Status().running;
}
bool LL_CpsService::Connected() const {
    for (const auto &m : LLCRbt->m_masters.List())
        if (Running(m.id) && !Config(m.id).value("simulation", false) &&
            !LLCRbt->m_masters.Get(m.id)->Status().connected)
            return false;
    return true;
}
void LL_CpsService::SetState(unsigned code, const std::string &name) {
    auto *c = LLCRbt;
    c->m_fsmRbt.currentState = c->m_status[code].GetState();
    c->SetCurLLCFSM(code, name);
}
Json LL_CpsService::Handle(int socket, const std::vector<std::string> &parameters) {
    try {
        require(parameters.size() == 1, "Expected one encoded JSON parameter");
        sockaddr_in address{};
        socklen_t len = sizeof(address);
        require(getpeername(socket, reinterpret_cast<sockaddr *>(&address), &len) == 0,
                "Cannot identify peer");
        char ip[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &address.sin_addr, ip, sizeof(ip));
        auto result = Dispatch(Json::parse(LL::Decode64(parameters[0])), ip);
        result["ok"] = true;
        return result;
    } catch (const std::exception &e) {
        std::cerr<<"[CPS] "<<e.what()<<'\n';
        if(parameters.size()==1)try {
            const auto op=Json::parse(LL::Decode64(parameters[0])).value("op",std::string{});
            if(op==LL::CpsOp::MasterStart||op==LL::CpsOp::Apply||op==LL::CpsOp::Scan)commandError_=e.what();
        }catch(...){}
        return {{"ok", false},
                {"error", e.what()},
                {"diagnostics", buildJob_.valid() ? "" : LLCRbt->HSTR_GetCompilerDiagnostics()}};
    }
}
void LL_CpsService::Apply(const Json &project) {
    require(!LLCRbt->m_bFoEWorking && !fileJob_.valid(), "File transfer in progress");
    require(project.is_object() && project.value("schemaVersion", 0) == 1, "Unsupported project schema");
    LL::ValidateIdentifier(project.at("id"));
    require(project.at("masters").is_array() && project["masters"].size() <= 16,
            "At most 16 masters are supported");
    // Unchanged bus configurations are safe to reuse while other masters are running.
    if (Attached() && project_.at("masters") == project.at("masters")) {
        project_ = project;
        return;
    }
    for (auto &m : LLCRbt->m_masters.List())
        require(!Running(m.id), "Stop all masters before changing bus configuration");
    std::set<std::string> ids, nics;
    std::set<int> axes;
    unsigned slaveCount = 0;
    auto catalog = LL::LoadDeviceCatalog(LL::CpsDataRoot() / "ESI");
    for (const auto &m : project["masters"]) {
        auto id = m.at("id").get<std::string>();
        LL::ValidateIdentifier(id);
        require(ids.insert(id).second, "Duplicate master id");
        const auto cycle = m.at("cycleUs").get<int>();
        require(cycle >= 100 && cycle <= 1000000, "Bus cycle must be 100..1000000 us");
        if (!m.value("simulation", false))
            require(nics.insert(m.at("nic").get<std::string>()).second,
                    "Physical masters cannot share a network interface");
        require(m["slaves"].size() <= 128, "Too many slaves");
        int local = 0;
        for (const auto &d : m.at("slaves")) {
            require(d.at("id").get<int>() == ++local, "Slave ids must follow topology order");
            ++slaveCount;
            Json definition;
            for (const auto &item : catalog)
                if (!item.contains("error") && item.value("productCode", 0u) == d.value("productCode", 0u) &&
                    item.value("revision", 0u) == d.value("revision", 0u) &&
                    item.value("vendorId", 0u) == d.value("vendorId", 0u)) {
                    definition = item;
                    break;
                }
            require(!definition.is_null(), "Missing matching ESI device");
            for (const auto *key : {"rxPdos", "txPdos"}) {
                require(d.at(key).size() == definition.at(key).size(),
                        "PDO catalog was modified; re-add device from ESI");
                for (std::size_t i = 0; i < d[key].size(); ++i) {
                    auto actual = d[key][i], expected = definition[key][i];
                    actual.erase("selected");
                    expected.erase("selected");
                    require(actual == expected, "PDO definition differs from ESI");
                    if (definition[key][i].value("mandatory", false))
                        require(d[key][i].value("selected", false), "Mandatory PDO disabled");
                }
                auto copy = d;
                for (const auto &p : d[key])
                    if (p.value("selected", false))
                        LL::SelectPdo(copy, key == std::string("rxPdos"), p.at("index"), true);
                require(copy[key] == d[key], "Conflicting PDO selections");
            }
            const auto shift = d.at("dc").at("shiftUs").get<long long>();
            require(shift >= -2147483 && shift <= 2147483, "DC shift exceeds nanosecond integer range");
            for (const auto &a : d.at("axes")) {
                const int axis = a.at("id");
                require(axis >= 0 && axes.insert(axis).second, "Axis ids must be nonnegative and unique");
                if (!a.value("enabled", false))
                    continue;
                for (const auto &f : LL::AxisFields()) {
                    const auto &map = a.at("mapping").at(f.name);
                    if (map.is_null()) {
                        require(std::string(f.name) != "Controlword" && std::string(f.name) != "StatusWord",
                                "Enabled axis requires controlword and statusword");
                        continue;
                    }
                    bool found = false;
                    for (const auto &p : d.at(f.rx ? "rxPdos" : "txPdos")) {
                        if (!p.value("selected", false) || p.at("index") != map.at("pdo"))
                            continue;
                        unsigned bit = 0;
                        for (const auto &e : p.at("entries")) {
                            if (e.at("index") == map.at("index") && e.at("subIndex") == map.at("subIndex") &&
                                e.at("bits").get<int>() == f.bits && !e.value("padding", false) &&
                                bit % 8 == 0)
                                found = true;
                            bit += e.at("bits").get<unsigned>();
                        }
                    }
                    require(found, "Invalid or unaligned axis PDO mapping: " + std::string(f.name));
                }
                auto p = a.at("parameters");
                require(p.at("encoder").get<int>() > 0 && std::abs(p.at("encoderDirection").get<int>()) == 1,
                        "Invalid encoder settings");
                for (const auto *key : {"gearRatio", "velocityScale", "torqueScale", "maxVelocity",
                                        "maxAcceleration", "maxJerk", "stopAcceleration", "stopJerk"})
                    require(p.at(key).get<double>() > 0 && std::isfinite(p.at(key).get<double>()),
                            "Axis scale/limit must be positive");
                require(p.at("softMin").get<double>() < p.at("softMax").get<double>(), "Invalid soft limits");
            }
        }
    }
    require(slaveCount <= MAX_SLAVE_NUM && axes.size() <= MaxAxisNum,
            "Controller axis/slave capacity exceeded");
    auto *c = LLCRbt;
    c->HSTR_StopApplication();
    forces_.clear();
    c->CleanSlaveConfig();
    c->ResetControllerConfig();
    c->m_activeMaster.clear();
    for (auto &m : c->m_masters.List())
        c->m_masters.Unregister(m.id);
    simulation_.clear();
    axisIndex_.clear();
    axisMaster_.clear();
    prepared_ = false;
    for (const auto &m : project["masters"]) {
        require(c->m_masters.Register({m.at("id"), LL::BusType::EtherCAT,
                                       m.value("simulation", false)
                                           ? "simulation:" + m.at("id").get<std::string>()
                                           : m.at("nic").get<std::string>(),
                                       m.at("cycleUs")}),
                c->m_masters.LastError());
        if (m.value("simulation", false))
            simulation_[m.at("id")] = false;
    }
    project_ = project;
    LL::ProjectRepository().Save(project);
    c->Initialize_ESI();
    SetState(enCDSState_ControllerNotConnect, "ControllerNotConnect");
}
void LL_CpsService::Prepare() {
    if (prepared_)
        return;
    auto *c = LLCRbt;
    c->CleanSlaveConfig();
    c->ResetControllerConfig();
    axisIndex_.clear();
    axisMaster_.clear();
    c->m_cycle_time = 1000.0;
    unsigned globalSlave = 0;
    try {
        for (const auto &config : project_.at("masters")) {
            const auto id = config.at("id").get<std::string>();
            auto master = Master(id);
            const bool sim = config.value("simulation", false);
            c->m_cycle_time = std::min(c->m_cycle_time, config.at("cycleUs").get<double>() / 1000.0);
            if (!sim) {
                if(!master->Scan())throw std::runtime_error("Scan failed for " + id + ": " + master->LastError());
                auto physical = master->Status().slaves;
                require(physical.size() == config["slaves"].size(), "Slave count differs from project");
                for (std::size_t i = 0; i < physical.size(); ++i) {
                    require(physical[i].vendorId == config["slaves"][i].at("vendorId").get<unsigned>() &&
                                physical[i].productCode ==
                                    config["slaves"][i].at("productCode").get<unsigned>() &&
                                physical[i].revision == config["slaves"][i].at("revision").get<unsigned>(),
                            "Slave identity differs from project");
                    const auto& slaveConfig=config["slaves"][i];
                    const ESI_Parse::Device* esi=nullptr;
                    for(const auto& parser:c->m_vESI_Parse)
                        if(parser->getVendorId()==slaveConfig.at("vendorId").get<unsigned>()&&
                           (esi=parser->findDeviceConfig(slaveConfig.at("productCode"),slaveConfig.at("revision"))))break;
                    require(esi!=nullptr,"Matching ESI was not loaded");
                    for (const auto &direction : std::vector<std::pair<const char *, std::uint16_t>>{
                             {"rxPdos", 0x1c12}, {"txPdos", 0x1c13}}) {
                        auto list = selected(config["slaves"][i], direction.first);
                        const int width=LL::EnsurePdoAssignment(*master,static_cast<std::uint16_t>(i+1),direction.second,list,true,esi->supportsPdoUpload);
                        std::cout<<"[CPS] Master "<<id<<", slave "<<(i+1)<<": PDO assignment 0x"<<std::hex
                                 <<direction.second<<std::dec<<" count="<<list.size()<<", "
                                 <<(width==4?"INT32 initialized":width==1?"UINT8/UINT16 initialized":"unchanged read-only assignment verified")<<'\n';
                    }
                }
                if(!master->Configure())throw std::runtime_error("Process data mapping failed for "+id+": "+master->LastError());
                require(master->VisitSlaves([&](int count,void* data){
                    require(count==static_cast<int>(physical.size()),"Slave count changed during PDO mapping");
                    const auto* native=static_cast<const ec_slavet*>(data);
                    for(int i=1;i<=count;++i)for(const bool rx:{true,false}) {
                        unsigned bits=0;
                        for(const auto& p:config["slaves"][i-1].at(rx?"rxPdos":"txPdos"))
                            if(p.value("selected",false))for(const auto& entry:p.at("entries"))bits+=entry.at("bits").get<unsigned>();
                        const unsigned actual=rx?native[i].Obits:native[i].Ibits;
                        require(bits==actual,"Master "+id+", slave "+std::to_string(i)+" "+(rx?"RxPDO":"TxPDO")+
                                " size differs: ESI/project="+std::to_string(bits)+" bits, device="+std::to_string(actual)+" bits; check ESI and device firmware");
                    }
                    return true;
                }),"Cannot validate mapped PDO sizes");
            }
            for (const auto &d : config.at("slaves")) {
                ++globalSlave;
                const auto local = d.at("id").get<std::uint16_t>();
                const auto dc = d.at("dc");
                if (!sim) {
                    bool enable = dc.value("enabled", false);
                    if (enable) {
                        bool found = false;
                        for (const auto &mode : d.at("dcModes"))
                            if (mode.at("name") == dc.at("mode")) {
                                std::uint16_t activate = mode.at("assignActivate");
                                require(activate == 0x300 || activate == 0x700,
                                        "Selected DC mode is not SYNC0/SYNC1");
                                const auto cycle0 = mode.value("cycleTimeSync0", 0) > 0
                                                        ? mode.at("cycleTimeSync0").get<unsigned>()
                                                        : config.at("cycleUs").get<unsigned>() * 1000;
                                const auto shift =
                                    static_cast<std::int32_t>(dc.at("shiftUs").get<long long>() * 1000);
                                require(activate == 0x700
                                            ? master->ConfigureSync01(local, cycle0,
                                                                      mode.value("cycleTimeSync1", 0u), shift)
                                            : master->ConfigureSync0(local, true, cycle0, shift),
                                        "DC configuration failed");
                                found = true;
                                break;
                            }
                        require(found, "Select a DC operation mode");
                    }
                }
                check(c->RegistrationSlave(globalSlave, d.at("productCode"), d.at("revision"),
                                           dc.value("enabled", false),
                                           static_cast<std::uint32_t>(dc.at("shiftUs").get<std::int32_t>())),
                      "Register slave");
                check(c->ConfigureSlavePDO(globalSlave, selected(d, "txPdos"), selected(d, "rxPdos")),
                      "Configure PDO");
                Json overrides = Json::array();
                for (const auto &a : d.at("axes"))
                    if (a.value("enabled", false)) {
                        const int index = static_cast<int>(c->v_AxisHardParam.size());
                        const auto p = a.at("parameters");
                        check(c->RegistrationAxis(
                                  index, globalSlave, a.at("txPdo"), a.at("rxPdo"), 0, 0, p.at("encoder"),
                                  p.at("encoderDirection"), p.at("gearRatio"), p.at("velocityScale"),
                                  p.at("torqueScale"), p.at("softMax"), p.at("softMin"), p.at("maxVelocity"),
                                  p.at("maxAcceleration"), p.at("maxJerk"), 0, 0, 0, p.at("stopAcceleration"),
                                  p.at("stopJerk")),
                              "Register axis");
                        axisIndex_[a.at("id")] = index;
                        axisMaster_[a.at("id")] = id;
                        overrides.push_back(a);
                    }
                const ESI_Parse::Device *device = nullptr;
                for (auto &parser : c->m_vESI_Parse)
                    if (parser->getVendorId() == d.at("vendorId").get<unsigned>() &&
                        (device = parser->findDeviceConfig(d.at("productCode"), d.at("revision"))))
                        break;
                require(device != nullptr, "Matching ESI was not loaded");
                auto slave = std::make_unique<LLC_SlaveBase>();
                slave->SetDeviceConfig(device);
                slave->axisOverrides = overrides;
                ST_EtherCATInterface callbacks{};
                callbacks.pSDORead = [master, local](uint16_t, uint16_t i, uint8_t sub, uint8_t ca, int *size,
                                                     void *data, int timeout) {
                    return master->ReadSDO(local, i, sub, data, *size, ca != 0, timeout);
                };
                callbacks.pSDOWrite = [master, local](uint16_t, uint16_t i, uint8_t sub, uint8_t ca, int size,
                                                      const void *data, int timeout) {
                    return master->WriteSDO(local, i, sub, data, size, ca != 0, timeout);
                };
                slave->SetEtherCATFuncCB(callbacks);
                auto initialize = [&](int count, void *data) {
                    return slave->InitSlave(local, sim, config.at("cycleUs").get<double>() / 1000000.0,
                                            c->m_vSlaveConfigParam.back(),
                                            sim ? nullptr : &static_cast<ec_slavet *>(data)[local],
                                            false) == REC_Successed;
                };
                require(sim ? initialize(0, nullptr) : master->VisitSlaves(initialize),
                        "Slave PDO initialization failed");
                slave->InsertAxis(c->m_vSlaveConfigParam.back().vAxisPDOs);
                auto axes = slave->GetCIA402Axis();
                require(axes.size() == overrides.size(),
                        "Required axis PDO objects are missing or unaligned");
                for (auto *axis : axes)
                    c->m_vAxis.push_back(axis);
                c->m_vSlaves.push_back(slave.release());
            }
        }
        c->m_stRbtEtherCATParams.dEtherCatTimeCycle = c->m_cycle_time / 1000.0;
        c->m_nConfiguredSlaveNum = static_cast<int>(c->m_vSlaves.size());
        c->m_nConfiguredAxisNum = static_cast<unsigned>(c->m_vAxis.size());
        check(c->ControllerConfig(), "Axis parameter configuration");
        check(c->InitBasicFB(), "Motion initialization");
        check(c->InitSoftMotion(), "SoftMotion initialization");
        ST_EtherCATMasterControl control{};
        control.pDisconnectSlaves = [this] {
            for (const auto &m : LLCRbt->m_masters.List())
                StopMaster(m.id);
            return true;
        };
        control.pResetSlaves = [this] {
            for (const auto &m : LLCRbt->m_masters.List())
                if (!Config(m.id).value("simulation", false))
                    Master(m.id)->Poll();
            return true;
        };
        control.pConnectSlaves = [] { return true; };
        c->SetMaster(control);
        prepared_ = true;
    } catch (...) {
        c->CleanSlaveConfig();
        c->ResetControllerConfig();
        axisIndex_.clear();
        axisMaster_.clear();
        for (const auto &m : c->m_masters.List())
            Master(m.id)->Close();
        throw;
    }
}
void LL_CpsService::Neutralize(int i) {
    auto *c = LLCRbt;
    auto *a = c->m_vAxis.at(i);
    a->CleanSoftMotionData();
    a->Axis_PDO_SetControlword(0);
    a->PDO_SetControlword(0);
    a->Axis_PDO_SetTargetVelocity(0);
    a->Axis_PDO_SetTargetTorque(0);
    a->Axis_PDO_SetTargetPosition(a->Axis_GetActualPositionPDO());
    a->Axis_SetAxisState(motionState_power_off);
    c->bAxisInitResetCmd[i] = false;
    c->bAxisEnableCmd[i] = false;
    c->bAxisDisableCmd[i] = false;
    c->bAxisResetCmd[i] = false;
    c->bAxisStopCmd[i] = false;
    c->bAxisAbsMoveCmd[i] = false;
    c->bAxisRelMoveCmd[i] = false;
    c->bAxisHomeCmd[i] = false;
    c->bAxisBackAndForthMoveCmd[i] = false;
    c->bAxisContinuousStepMoveCmd[i] = false;
    c->bAxisVelocityMoveCmd[i] = false;
    c->m_enAxisInitResetFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
    c->m_enAxisEnableFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
    c->m_enAxisDisableFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
    c->m_enAxisResetFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
    c->m_enAxisStopFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
    c->m_enAxisAbsMoveFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
    c->m_enAxisRelMoveFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
    c->m_enAxisHomeFSM[i] = EN_AxisControlFSM::enAxisControlFSM_Null;
    c->m_enAxisBackAndForthMoveFSM[i] = 0;
    c->m_enAxisContinuousStepMoveFSM[i] = 0;
    c->m_enAxisVelocityMoveFSM[i] = 0;
}
void LL_CpsService::FaultAxis(int id,const std::string& reason) {
    if(!axisIndex_.count(id))return;
    axisFaults_.emplace(id,reason);
    const int i=axisIndex_.at(id);
    Neutralize(i); // Discard this axis's trajectory and every pending command.
    auto* axis=LLCRbt->m_vAxis.at(i);
    axis->Axis_PDO_SetControlword(2); // CiA402 quick stop; do not disable other axes.
    axis->PDO_SetControlword(2);
    axis->Axis_SetAxisState(motionState_errorstop);
}
void LL_CpsService::StopMaster(const std::string &id) {
    for (const auto &axis : axisMaster_)
        if (axis.second == id) {
            const int i = axisIndex_.at(axis.first);
            Neutralize(i);
            readyAxes_.erase(axis.first);
            axisFaults_.erase(axis.first);resettingAxes_.erase(axis.first);
        }
    if (simulation_.count(id))
        simulation_[id] = false;
    else {
        Master(id)->Exchange();
        Master(id)->Stop();
    }
    bool any = false;
    for (const auto &m : LLCRbt->m_masters.List())
        any = any || Running(m.id);
    LLCRbt->m_bMasterState = any;
    LLCRbt->m_stRobotControlStatus.bStartMaster = any;
    if (!any) {
        LLCRbt->CleanSlaveConfig();
        LLCRbt->ResetControllerConfig();
        prepared_ = false;
        axisIndex_.clear();
        axisMaster_.clear();
    }
}
Json LL_CpsService::Status() {
    auto *c = LLCRbt;
    std::string state;
    const auto stateCode = c->CurLLCFSM(state);
    Json masters = Json::array(), tasks = Json::array(), axes = Json::array();
    for (const auto &config : c->m_masters.List()) {
        auto status = Master(config.id)->Status();
        Json slaves = Json::array();
        for (const auto &d : status.slaves)
            slaves.push_back({{"id", d.id},
                              {"state", d.state},
                              {"connected", d.connected},
                              {"productCode", d.productCode},
                              {"revision", d.revision},
                              {"name", d.name}});
        masters.push_back(
            {{"id", config.id},
             {"running", Running(config.id)},
             {"connected", simulation_.count(config.id) ? Running(config.id) : status.connected},
             {"actualWkc", status.actualWkc},
             {"expectedWkc", status.expectedWkc},
             {"cycles", status.cycles},
             {"sentFrames",status.sentFrames},
             {"framesPerSecond",status.framesPerSecond},
             {"txErrors",status.sendErrors},
             {"rxErrors",status.receiveErrors},
             {"simulation",simulation_.count(config.id)!=0},
             {"wkcErrors", status.wkcErrors},
             {"missedDeadlines", status.missedDeadlines},
             {"slaves", slaves}});
    }
    const char* stateNames[]={"未使能","错误停止","停止中","静止就绪","定位运动","连续运动","同步运动","回零中"};
    for(const auto& entry:axisIndex_){auto* axis=c->m_vAxis.at(entry.second);const int state=static_cast<int>(axis->Axis_GetAxisMotionState());axes.push_back({{"id",entry.first},{"master",axisMaster_.at(entry.first)},{"state",state},{"stateName",state>=0&&state<8?stateNames[state]:"未知"},{"available",readyAxes_.count(entry.first)!=0},{"enabled",(axis->Axis_GetStatusWord()&0x6f)==0x27},{"moving",axis->Axis_GetMovingState()},{"error",axis->Axis_GetErrorState()||axisFaults_.count(entry.first)!=0},{"faultReason",axisFaults_.count(entry.first)?axisFaults_.at(entry.first):""},{"errorCode",axis->Axis_GetAxisErrorID()},{"statusWord",axis->Axis_GetStatusWord()},{"controlWord",axis->Axis_GetControlWord()},{"actualPosition",axis->Axis_GetActualPosition()},{"actualVelocity",axis->Axis_GetActualVelocity()},{"actualTorque",axis->Axis_GetActualTorque()},{"mode",axis->Axis_GetActualOperationMode()}});}
    if (!buildJob_.valid())
        for (auto &t : c->HSTR_GetTaskStatistics())
            tasks.push_back({{"name", t.name},
                             {"executions", t.executions},
                             {"missedPeriods", t.missedPeriods},
                             {"lastDurationUs", t.lastDurationUs},
                             {"maxDurationUs", t.maxDurationUs}});
    auto location = !buildJob_.valid() && c->m_pHSTRruntime ? c->m_pHSTRruntime->GetDebugLocation()
                                                            : HSTR::DebugLocation{};
    return {{"version", 1},
            {"state", state},
            {"stateCode", stateCode},
            {"emergency", emergency_ || c->CheckEmergencyStop()},
            {"controllerError",c->m_stRobotControlStatus.bStatus_ControllerError},
            {"commandError",commandError_},
            {"plcStatus", c->HSTR_GetApplicationStatus()},
            {"application", buildJob_.valid() ? buildingApp_ : c->HSTR_GetSelectedApplication()},
            {"masters", masters},
            {"axes",axes},
            {"tasks", tasks},
            {"location",
             {{"file", location.file}, {"line", location.line}, {"canStepInto", location.canStepInto},
              {"pauseCount", location.pauseCount}}},
            {"lastError", buildJob_.valid() ? "" : c->HSTR_GetLastError()}};
}
Json LL_CpsService::Variables() {
    Json result = Json::array();
    if (LLCRbt->m_pHSTRruntime)
        for (const auto &v : LLCRbt->m_pHSTRruntime->GetVariableList())
            result.push_back({{"name", v.name},
                              {"type", v.type},
                              {"size", v.size},
                              {"readOnly", v.readOnly},
                              {"source", "HSTR"}});
    for (const auto &a : axisIndex_) {
        std::string axisName="Axis_"+std::to_string(a.first);
        for(const auto& master:project_.at("masters"))for(const auto& slave:master.at("slaves"))for(const auto& configured:slave.at("axes"))
            if(configured.at("id")==a.first)axisName=configured.value("name",axisName);
        const std::map<std::string,std::string> labels={{"ActualPosition","实际位置"},{"ActualVelocity","实际速度"},{"ActualTorque","实际力矩"},{"TargetPosition","目标位置"},{"StatusWord","状态字"},{"ErrorCode","错误码"}};
        for (const auto *field : {"ActualPosition", "ActualVelocity", "ActualTorque", "TargetPosition",
                                  "StatusWord", "ErrorCode"})
            result.push_back({{"name", "Axis." + std::to_string(a.first) + "." + field},
                              {"displayName",axisName+" / "+labels.at(field)},
                              {"master",axisMaster_.at(a.first)},
                              {"type", "LREAL"},
                              {"size", 8},
                              {"readOnly", true},
                              {"source", "Axis"}});
    }
    return result;
}
Json LL_CpsService::Metadata(const std::string &name) {
    auto upper = [](std::string text) {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return text;
    };
    const auto query = upper(name);
    auto vars = Variables();
    Json match;
    for (const auto &v : vars) {
        const auto candidate = upper(v.at("name"));
        if (candidate == query)
            return v;
        if (candidate.size() > query.size() &&
            candidate.compare(candidate.size() - query.size(), query.size(), query) == 0 &&
            candidate[candidate.size() - query.size() - 1] == '.') {
            require(match.is_null(), "Ambiguous variable suffix: " + name);
            match = v;
        }
    }
    if (!match.is_null())
        return match;
    throw std::runtime_error("Variable not found: " + name);
}
Json LL_CpsService::Read(const std::string &name) {
    return ReadResolved(Metadata(name));
}
Json LL_CpsService::ReadResolved(const Json& meta) {
    const auto canonical=meta.at("name").get<std::string>();
    if (meta.at("source") == "Axis") {
        const auto dot = canonical.find('.', 5);
        const int id = std::stoi(canonical.substr(5, dot - 5));
        require(axisIndex_.count(id)!=0,"轴未注册或配置已改变，请重新启动主站: "+canonical);
        auto *a = LLCRbt->m_vAxis.at(axisIndex_.at(id));
        const auto field = canonical.substr(dot + 1);
        double value = 0;
        if (field == "ActualPosition")
            value = a->Axis_GetActualPosition();
        else if (field == "ActualVelocity")
            value = a->Axis_GetActualVelocity();
        else if (field == "ActualTorque")
            value = a->Axis_GetActualTorque();
        else if (field == "TargetPosition")
            value = a->Axis_GetSetPosition();
        else if (field == "StatusWord")
            value = a->Axis_GetStatusWord();
        else
            value = a->Axis_GetAxisErrorID();
        return {{"value", std::to_string(value)}, {"number", value}, {"type", "LREAL"}, {"forced", false}};
    }
    const std::size_t size = meta.at("size");
    require(size <= 1024 * 1024, "Variable exceeds read limit");
    std::vector<std::uint8_t> bytes(size);
    LLCRbt->m_pHSTRruntime->ReadVariable(canonical, bytes.data(), size);
    const auto hex = LL::BytesHex(bytes.data(), size);
    Json result = {{"value", LL::HexToScalar(meta.at("type"), hex)},
                   {"hex", hex},
                   {"type", meta.at("type")},
                   {"forced", forces_.count(meta.at("name").get<std::string>()) != 0}};
    try {
        const auto number = LL::HexToNumber(meta.at("type"), hex);
        if (std::isfinite(number))
            result["number"] = number;
    } catch (...) {
    }
    return result;
}
void LL_CpsService::Forces() {
    if (!LLCRbt->m_pHSTRruntime)
        return;
    for (auto it = forces_.begin(); it != forces_.end();) {
        try {
            LLCRbt->m_pHSTRruntime->ModifyVariable(it->first, it->second.data(), it->second.size());
            ++it;
        } catch (...) {
            it = forces_.erase(it);
        }
    }
}
void LL_CpsService::RefreshReadyAxes() {
    readyAxes_.clear();
    if (!Attached()) return;
    for (const auto& config : project_.at("masters")) {
        const auto id = config.at("id").get<std::string>();
        if (!Running(id)) continue;
        const bool simulation = config.value("simulation", false);
        const auto physical = Master(id)->Status().slaves;
        for (const auto& slave : config.at("slaves")) {
            bool operational = simulation;
            for (const auto& state : physical)
                if (state.id == slave.at("id").get<unsigned>())
                    operational = state.connected && (state.state & 0x0f) == 8;
            if (operational) for (const auto& axis : slave.at("axes")) {
                const int axisId = axis.at("id");
                if (axisIndex_.count(axisId)) readyAxes_.insert(axisId);
            }
        }
    }
}
void LL_CpsService::BeforeCycle() {
    if (buildJob_.valid() && buildJob_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        buildResult_ = buildJob_.get();
        buildResult_["done"] = true;
    }
    if (!Attached())
        return;
    const auto now = Clock::now();
    for (const auto &m : LLCRbt->m_masters.List())
        if (Running(m.id) && !Config(m.id).value("simulation", false)) {
            Master(m.id)->Exchange();
            if (now >= nextPoll_)
                Master(m.id)->Poll();
        }
    if (now >= nextPoll_) {
        nextPoll_ = now + std::chrono::milliseconds(100);
        if (!Connected()) {
            LLCRbt->m_stRobotControlStatus.bStatus_SlaveDropError = true;
            LLCRbt->m_stRobotControlStatus.bStatus_ControllerError = true;
            std::vector<std::string> lost;
            for (const auto& master : LLCRbt->m_masters.List())
                if (Running(master.id) && !Config(master.id).value("simulation", false) &&
                    !Master(master.id)->Status().connected)
                    lost.push_back(master.id);
            for (const auto& id : lost)
                StopMaster(id);
        }
        RefreshReadyAxes();
    }
    if (fileJob_.valid() && fileJob_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        fileResult_ = fileJob_.get();
        fileDone_ = true;
        LLCRbt->m_bFoEWorking = false;
    }
}
void LL_CpsService::AfterCycle() {
    {std::lock_guard<std::mutex> lock(dataMutex_);for(auto it=dataStreams_.begin();it!=dataStreams_.end();)if(it->second->Expired())it=dataStreams_.erase(it);else++it;}
    if (Attached())
        for (const auto &a : axisMaster_)
            if (!readyAxes_.count(a.first))
                Neutralize(axisIndex_.at(a.first));
    Forces();
    for(const auto& entry:axisIndex_){
        const int id=entry.first,i=entry.second;auto* axis=LLCRbt->m_vAxis.at(i);
        if(!readyAxes_.count(id))continue;
        if(resettingAxes_.count(id)){
            if(!LLCRbt->bAxisResetCmd[i]&&LLCRbt->m_enAxisResetFSM[i]==EN_AxisControlFSM::enAxisControlFSM_Null&&axis->Axis_GetAxisMotionState()!=motionState_errorstop&&!axis->Axis_GetErrorState()&&!(axis->Axis_GetStatusWord()&8)){
                axisFaults_.erase(id);resettingAxes_.erase(id);continue;
            }
            if(LLCRbt->bAxisResetCmd[i]||LLCRbt->m_enAxisResetFSM[i]!=EN_AxisControlFSM::enAxisControlFSM_Null)continue;
            resettingAxes_.erase(id);
        }
        if(axisFaults_.count(id)||axis->Axis_GetErrorState()||(axis->Axis_GetStatusWord()&8)||axis->Axis_GetAxisMotionState()==motionState_errorstop)
            FaultAxis(id,axisFaults_.count(id)?axisFaults_.at(id):"轴运行故障");
    }
    // AxisControlFSM/HSTR update command mirrors after SoftMotion's earlier PDO sync.
    for(const int id:readyAxes_)LLCRbt->m_vAxis.at(axisIndex_.at(id))->PDOsynchronization();
    if(Attached())for(const auto& master:LLCRbt->m_masters.List())
        if(Running(master.id)&&!Config(master.id).value("simulation",false))Master(master.id)->Exchange();
    const auto now = Clock::now();
    for (auto it = subscriptions_.begin(); it != subscriptions_.end();) {
        auto &sub = it->second;
        if (sub.stream?sub.stream->Expired():now > sub.expires) {
            if(sub.stream){std::lock_guard<std::mutex> lock(dataMutex_);for(auto item=dataStreams_.begin();item!=dataStreams_.end();)if(item->second==sub.stream)item=dataStreams_.erase(item);else++item;}
            it = subscriptions_.erase(it);
            continue;
        }
        if ((sub.stream && sub.intervalMs == 1) || now >= sub.next) {
            const auto period=std::chrono::milliseconds(sub.intervalMs);
            const auto missed=now>sub.next?(now-sub.next)/period:0;
            if(sub.stream&&sub.intervalMs==1)sub.next=now+period;else sub.next+=(missed+1)*period;
            if(sub.stream) {
                Json values=Json::object();
                for(std::size_t channel=0;channel<sub.channels.size();++channel){const auto& name=sub.channels[channel];try {
                    if(buildJob_.valid())values[name]={{"error","编译期间暂停采样"}};
                    else values[name]=ReadResolved(sub.metadata.at(channel));
                }catch(const std::exception& error){values[name]={{"error",error.what()}};}}
                // Sequence numbers identify captured samples, not nominal timer slots.
                sub.missedSamples+=static_cast<std::uint64_t>(missed);
                sub.stream->Push({{"sequence",++sub.sequence},{"missedSamples",sub.missedSamples},{"timestamp_us",std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count()},{"values",std::move(values)}});
                ++it;continue;
            }
            Json values = Json::object();
            for (const auto &name : sub.channels)
                try {
                    const auto v = Read(name);
                    if (v.contains("number"))
                        values[name] = v.at("number");
                } catch (...) {
                }
            Json packet = {
                {"type", "LLECP.scope.v1"},
                {"sequence", ++sub.sequence},
                {"timestamp_us",
                 std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count()},
                {"values", values}};
            const auto bytes = packet.dump();
            sendto(udp_, bytes.data(), bytes.size(), MSG_DONTWAIT, reinterpret_cast<sockaddr *>(&sub.address),
                   sizeof(sub.address));
        }
        ++it;
    }
}
void LL_CpsService::Shutdown() {
    if (buildJob_.valid())
        buildJob_.get();
    if (fileJob_.valid())
        fileJob_.wait();
    if (udp_ >= 0) {
        close(udp_);
        udp_ = -1;
    }
    subscriptions_.clear();
    {std::lock_guard<std::mutex> lock(dataMutex_);dataStreams_.clear();}
    forces_.clear();
    for (auto &t : transfers_) {
        std::error_code ec;
        fs::remove(t.second.path, ec);
    }
    transfers_.clear();
    project_ = Json();
    commandError_.clear();
    prepared_ = false;
    simulation_.clear();
}
