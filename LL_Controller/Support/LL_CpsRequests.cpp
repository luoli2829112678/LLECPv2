#include "LL_CpsService.h"
#include "LLC_Rbt.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fstream>
#include <cmath>
using LL::Json;
namespace fs = std::filesystem;
namespace {
void require(bool condition, const std::string &text) {
    if (!condition)
        throw std::runtime_error(text);
}
void check(int result, const std::string &text) {
    require(result == REC_Successed, text + ": " + std::to_string(result));
}
} // namespace
void LL_CpsService::Download(const Json &project) {
    auto *c = LLCRbt;
    const auto id = project.at("id").get<std::string>();
    LL::ValidateIdentifier(id);
    require(project.at("hstr").at("files").size() <= 256, "Too many HSTR objects");
    c->HSTR_StopApplication();
    forces_.clear();
    c->m_pHSTRruntime.reset();
    const fs::path folder = fs::path(c->m_sApplicationPath) / id;
    fs::create_directories(folder);
    Json manifest = {{"schemaVersion", 2},        {"name", id},
                     {"system", "LLPC"},          {"compiled", false},
                     {"types", Json::array()},    {"globals", Json::array()},
                     {"programs", Json::array()}, {"functions", Json::array()},
                     {"tasks", Json::array()}};
    std::set<std::string> names;
    for (const auto &file : project["hstr"]["files"]) {
        const auto filename = LL::SourceFilename(file);
        require(names.insert(filename).second, "Duplicate HSTR object");
        const auto source = file.at("content").get<std::string>();
        require(source.size() <= 4 * 1024 * 1024, "HSTR source file exceeds 4 MiB");
        auto kind = file.at("kind").get<std::string>();
        auto key = kind == "program"    ? "programs"
                   : kind == "function" ? "functions"
                   : kind == "type"     ? "types"
                   : kind == "gvl"      ? "globals"
                                        : "tasks";
        if (kind == "task") {
            auto task = Json::parse(source);
            require(task.at("name") == file.at("name"), "Task name differs from filename");
        }
        LL::WriteAtomic(folder / fs::u8path(filename), source);
        manifest[key].push_back({{"name", file.at("name")}, {"file", filename}});
    }
    LL::WriteAtomic(folder / (id + ".prj"), manifest.dump(2));
    LL::ProjectRepository().Save(project);
    check(c->HSTR_Init(), "Initialize HSTR");
    check(c->HSTR_SelectedApplication(id), "Select application");
    c->hstrLogicalTimeUs = 0;
}
Json LL_CpsService::Dispatch(const Json &request, const std::string &peer) {
    using namespace LL::CpsOp;
    const auto op = request.at("op").get<std::string>();
    auto *c = LLCRbt;
    const auto now = Clock::now();
    for (auto it = transfers_.begin(); it != transfers_.end();) {
        if (now - it->second.touched > std::chrono::minutes(15) && it->first != fileToken_) {
            std::error_code ec;
            fs::remove(it->second.path, ec);
            it = transfers_.erase(it);
        } else
            ++it;
    }
    if (op == LL::CpsOp::Hello || op == LL::CpsOp::Status)
        return this->Status();
    if (op == LL::CpsOp::BuildStatus) {
        if (buildJob_.valid() && buildJob_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            buildResult_ = buildJob_.get();
            buildResult_["done"] = true;
        }
        if (buildJob_.valid())
            return {{"done", false}};
        return buildResult_.is_object()
                   ? buildResult_
                   : Json{{"done", true}, {"succeeded", false}, {"diagnostics", "No build has been started"}};
    }
    require(!buildJob_.valid() || op == LL::CpsOp::Emergency || op == LL::CpsOp::MasterStop ||
                op == LL::CpsOp::Unsubscribe,
            "Build in progress; wait for plc.buildStatus");

    if (op == LL::CpsOp::Devices)
        return {{"devices", LL::LoadDeviceCatalog(LL::CpsDataRoot() / "ESI")}};
    if(op==LL::CpsOp::PdoRead||op==LL::CpsOp::PdoWrite)return Pdo(request,op==LL::CpsOp::PdoWrite);
    if (op == LL::CpsOp::Apply) {
        if (request.contains("token")) {
            const auto token = request.at("token").get<std::string>();
            const auto it = transfers_.find(token);
            require(it != transfers_.end() && it->second.owner == peer && it->second.committed &&
                        it->second.purpose == "project", "Upload a project configuration first");
            this->Apply(LL::ReadJson(it->second.path));
            std::error_code ec;
            fs::remove(it->second.path, ec);
            transfers_.erase(it);
        } else {
            this->Apply(request.at("project"));
        }
        commandError_.clear();
        return {};
    }
    if (op == LL::CpsOp::Scan) {
        const auto config = request.at("config");
        const auto id = config.at("id").get<std::string>();
        LL::ValidateIdentifier(id);
        require(!Running(id), "Stop master before scanning");
        if (config.value("simulation", false)) {
            Json result = Json::array();
            for (const auto &d : config.at("slaves"))
                result.push_back({{"id", d["id"]},
                                  {"name", d["name"]},
                                  {"productCode", d["productCode"]},
                                  {"revision", d["revision"]}});
            return {{"slaves", result}, {"simulation", true}};
        }
        auto master = c->m_masters.Get(id);
        if (master && (master->Config().endpoint != config.at("nic").get<std::string>() ||
                       master->Config().cycleUs != config.at("cycleUs").get<unsigned>())) {
            require(!prepared_, "Apply changed bus configuration before scanning");
            c->m_masters.Unregister(id);
            master.reset();
        }
        if (!master) {
            require(
                c->m_masters.Register({id, LL::BusType::EtherCAT, config.at("nic"), config.at("cycleUs")}),
                c->m_masters.LastError());
            master = Master(id);
        }
        require(!prepared_, "Stop and apply project configuration before rescanning prepared buses");
        require(master->Scan(), master->LastError());
        master->Poll();
        Json result = Json::array();
        for (const auto &d : master->Status().slaves)
            result.push_back({{"id", d.id},
                              {"name", d.name},
                              {"productCode", d.productCode},
                              {"revision", d.revision},
                              {"vendorId", d.vendorId}});
        return {{"slaves", result}};
    }
    if (op == LL::CpsOp::MasterStart) {
        commandError_.clear();
        require(Attached(), "Apply project first");
        require(!emergency_ && !c->CheckEmergencyStop(), "Emergency stop is active");
        require(!c->m_bFoEWorking, "File transfer in progress");
        const auto id = request.at("master").get<std::string>();
        Config(id);
        Prepare();
        if (simulation_.count(id))
            simulation_[id] = true;
        else if(!Master(id)->Start(true)) {
            const auto master=Master(id);
            std::string message="Master "+id+" start failed: "+master->LastError();
            for(const auto& slave:master->Status().slaves)
                message+="; slave "+std::to_string(slave.id)+" state="+std::to_string(slave.state)+" AL="+std::to_string(slave.alStatusCode);
            throw std::runtime_error(message);
        }
        c->m_bMasterState = true;
        RefreshReadyAxes();
        c->m_stRobotControlStatus.bStartMaster = true;
        c->m_stRobotControlStatus.bStatus_ControllerError = false;
        c->m_stRobotControlStatus.bStatus_SlaveDropError = false;
        SetState(enCDSState_StandBy, "StandBy");
        return this->Status();
    }
    if (op == LL::CpsOp::MasterStop) {
        const auto id = request.at("master").get<std::string>();
        Config(id);
        StopMaster(id);
        if (!c->m_bMasterState)
            SetState(enCDSState_ControllerNotConnect, "ControllerNotConnect");
        return this->Status();
    }
    if (op == LL::CpsOp::Emergency) {
        emergency_ = true;
        if (!buildJob_.valid())
            c->HSTR_StopApplication();
        forces_.clear();
        for (const auto &m : c->m_masters.List())
            StopMaster(m.id);
        SetState(enCDSState_EmergencyStop, "EmergencyStop");
        return {};
    }
    if (op == LL::CpsOp::EmergencyRelease) {
        require(c->m_pLLC_CPS2LLCMemory->st_BoxInfo.nIsEStop != enum_StatusHigh,
                "Hardware emergency-stop input remains active");
        emergency_ = false;
        SetState(enCDSState_ControllerNotConnect, "ControllerNotConnect");
        return {};
    }
    if (op == LL::CpsOp::Reset) {
        require(!buildJob_.valid(),"请等待HSTR生成结束后再复位");
        require(!emergency_ && !c->CheckEmergencyStop(), "Clear emergency stop first");
        if(c->m_pHSTRruntime && c->HSTR_GetApplicationStatus()==static_cast<int>(HSTR::ApplicationStatus::Error))check(c->HSTR_ResetProgram(),"Reset HSTR error");
        for (std::size_t i = 0; i < c->m_vAxis.size(); ++i)
            check(c->AxisReset({static_cast<int>(i)}), "Reset axis");
        c->m_stRobotControlStatus.bStatus_ControllerError = false;
        c->m_stRobotControlStatus.bStatus_AxisError = false;
        c->m_stRobotControlStatus.bStatus_SoftError = false;
        c->m_stRobotControlStatus.bStatus_SlaveDropError = !Connected();
        SetState(c->m_bMasterState ? enCDSState_StandBy : enCDSState_ControllerNotConnect,
                 c->m_bMasterState ? "StandBy" : "ControllerNotConnect");
        for(const auto& fault:axisFaults_)resettingAxes_.insert(fault.first);
        commandError_.clear();
        return {{"accepted",true}};
    }
    if (op == LL::CpsOp::SlaveState) {
        auto id = request.at("master").get<std::string>();
        require(Running(id), "Start master before changing slave state");
        require(!Config(id).value("simulation", false), "Simulated slaves do not have physical states");
        const auto slave = request.at("slave").get<unsigned>(), state = request.at("state").get<unsigned>();
        require(slave > 0 && slave <= Config(id).at("slaves").size() &&
                    (state == 1 || state == 2 || state == 4 || state == 8),
                "Invalid slave state request");
        if (state != 8) for (const auto& configured : Config(id).at("slaves")) {
            if (configured.at("id").get<unsigned>() != slave) continue;
            for (const auto& axis : configured.at("axes")) {
                const auto found = axisIndex_.find(axis.at("id").get<int>());
                if (found != axisIndex_.end()) Neutralize(found->second);
            }
        }
        Master(id)->Exchange();
        require(Master(id)->ChangeSlaveState(slave, state), Master(id)->LastError());
        RefreshReadyAxes();
        return {};
    }
    if (op == LL::CpsOp::FileBegin) {
        const auto purpose = request.at("purpose").get<std::string>(),
                   name = request.at("name").get<std::string>();
        require(purpose == "esi" || purpose == "project" || purpose == "firmware",
                "Unsupported upload purpose");
        require(!name.empty() && name.size() <= 240 && fs::u8path(name).filename().u8string() == name &&
                    name != "." && name != "..",
                "Invalid upload filename");
        require(transfers_.size() < 16, "Too many file transfers");
        const auto size = request.at("size").get<std::size_t>();
        require(size <= (purpose == "firmware" ? 256u : 32u) * 1024 * 1024, "Upload exceeds file limit");
        const auto token = LL::NewId("transfer");
        const auto folder = LL::CpsDataRoot() / "transfers";
        fs::create_directories(folder);
        const auto path = folder / token;
        LL::WriteAtomic(path, "");
        transfers_[token] = {purpose, name, peer, path, size, 0, false, now};
        return {{"token", token}};
    }
    if (op == LL::CpsOp::FileChunk || op == LL::CpsOp::FileCommit || op == LL::CpsOp::FileRead) {
        const auto token = request.at("token").get<std::string>();
        auto it = transfers_.find(token);
        require(it != transfers_.end() && it->second.owner == peer,
                "Transfer does not exist for this client");
        auto &transfer = it->second;
        transfer.touched = now;
        if (op == LL::CpsOp::FileChunk) {
            require(!transfer.committed, "Transfer already committed");
            require(request.at("offset").get<std::size_t>() == transfer.offset, "Upload offset mismatch");
            const auto data = LL::Decode64(request.at("data"));
            require(data.size() <= 256 * 1024 && data.size() <= transfer.size - transfer.offset,
                    "Invalid chunk length");
            std::ofstream out(transfer.path, std::ios::binary | std::ios::app);
            require(static_cast<bool>(out.write(data.data(), data.size())), "Cannot write upload");
            transfer.offset += data.size();
            return {{"offset", transfer.offset}};
        }
        if (op == LL::CpsOp::FileCommit) {
            require(transfer.offset == transfer.size, "Upload is incomplete");
            if (transfer.purpose == "esi") {
                const auto xml = LL::ReadText(transfer.path);
                LL::DeviceCatalog(xml, transfer.name);
                auto extension = fs::u8path(transfer.name).extension().u8string();
                require(extension == ".xml" || extension == ".XML", "ESI filename must end in XML");
                LL::WriteAtomic(LL::CpsDataRoot() / "ESI" / fs::u8path(transfer.name), xml);
                c->Initialize_ESI();
                const auto size = transfer.size;
                std::error_code ec;
                fs::remove(transfer.path, ec);
                transfers_.erase(it);
                return {{"token", token}, {"size", size}};
            }
            if (transfer.purpose == "project") {
                const auto project = LL::ReadJson(transfer.path);
                LL::ValidateIdentifier(project.at("id"));
                require(project.value("schemaVersion", 0) == 1, "Unsupported project schema");
            }
            transfer.committed = true;
            return {{"token", token}, {"size", transfer.size}};
        }
        require(transfer.committed, "File is not ready");
        const auto offset = request.at("offset").get<std::size_t>();
        require(offset <= transfer.size, "Read offset exceeds file");
        std::ifstream file(transfer.path, std::ios::binary);
        file.seekg(offset);
        std::string bytes(std::min<std::size_t>(192 * 1024, transfer.size - offset), '\0');
        require(bytes.empty() || static_cast<bool>(file.read(bytes.data(), bytes.size())),
                "Cannot read transfer file");
        return {{"data", LL::Encode64(bytes)},
                {"eof", offset + bytes.size() == transfer.size},
                {"size", transfer.size}};
    }
    if (op == LL::CpsOp::FoE) {
        const auto id = request.at("master").get<std::string>();
        require(Running(id), "Start master before using FoE");
        require(!Config(id).value("simulation", false), "FoE requires physical EtherCAT hardware");
        require(!fileJob_.valid(), "File transfer already active");
        const auto local = request.at("slave").get<unsigned>();
        require(local > 0 && local <= Config(id).at("slaves").size(), "Invalid slave");
        const auto mode = request.at("mode").get<std::string>();
        require(mode == "read" || mode == "write" || mode == "xml", "Invalid FoE mode");
        const auto remote = request.at("remote").get<std::string>();
        require(!remote.empty() && remote.size() <= 255, "Invalid remote filename");
        const auto password = request.value("password", 0u);
        auto master = Master(id);
        fs::path path;
        if (mode == "read") {
            fileToken_ = LL::NewId("download");
            path = LL::CpsDataRoot() / "transfers" / fileToken_;
            fs::create_directories(path.parent_path());
            transfers_[fileToken_] = {"firmware", remote, peer, path, 0, 0, false, now};
        } else {
            fileToken_ = request.at("token").get<std::string>();
            auto it = transfers_.find(fileToken_);
            require(it != transfers_.end() && it->second.owner == peer && it->second.committed &&
                        it->second.purpose == "firmware",
                    "Upload firmware before writing");
            path = it->second.path;
            if (mode == "xml") {
                std::ifstream input(path, std::ios::binary);
                char first = 0;
                input.get(first);
                require(first != '<', "ESI XML must be converted to SII EEPROM binary by vendor tools");
            }
        }
        c->HSTR_StopApplication();
        StopMaster(id);
        c->m_bFoEWorking = true;
        fileDone_ = false;
        fileResult_ = 0;
        fileJob_ = std::async(std::launch::async, [=] {
            try {
                if (mode == "read")
                    return master->ReadFile(local, path.string(), remote, password);
                if (mode == "xml")
                    return master->WriteSiiFile(local, path.string());
                return master->WriteFile(local, path.string(), remote, password);
            } catch (...) {
                return -1;
            }
        });
        return {{"token", fileToken_}, {"started", true}};
    }
    if (op == LL::CpsOp::FoEProgress) {
        Json result = {{"active", fileJob_.valid()},
                       {"done", fileDone_},
                       {"succeeded", fileDone_ && fileResult_ > 0},
                       {"result", fileResult_}};
        if (fileDone_ && fileResult_ > 0) {
            auto it = transfers_.find(fileToken_);
            if (it != transfers_.end() && it->second.owner == peer && fs::exists(it->second.path)) {
                it->second.size = fs::file_size(it->second.path);
                it->second.committed = true;
                it->second.touched = now;
                result["downloadToken"] = fileToken_;
            }
        }
        return result;
    }
    if (op == LL::CpsOp::Download) {
        auto it = transfers_.find(request.at("token").get<std::string>());
        require(it != transfers_.end() && it->second.owner == peer && it->second.committed &&
                    it->second.purpose == "project",
                "Upload a project before downloading");
        this->Download(LL::ReadJson(it->second.path));
        std::error_code ec;
        fs::remove(it->second.path, ec);
        transfers_.erase(it);
        return {};
    }
    if (op == LL::CpsOp::Build) {
        require(c->m_pHSTRruntime != nullptr, "HSTR is not initialized");
        for (auto *a : c->m_vAxis)
            require(!a->Axis_GetMovingState(), "Stop axis motion before compiling");
        forces_.clear();
        c->HSTR_StopApplication();
        buildingApp_ = c->HSTR_GetSelectedApplication();
        auto *runtime = c->m_pHSTRruntime.get();
        buildResult_ = Json::object();
        buildJob_ = std::async(std::launch::async, [runtime] {
            try {
                runtime->CompileProgram();
                return Json{{"succeeded", runtime->GetApplicationStatus()==HSTR::ApplicationStatus::Ready}, {"diagnostics", runtime->GetCompilerDiagnostics()},{"error",runtime->GetLastError()}};
            } catch (const std::exception &e) {
                return Json{{"succeeded", false},
                            {"diagnostics", runtime->GetCompilerDiagnostics()},
                            {"error", e.what()}};
            }
        });
        return {{"building", true}};
    }
    if (op == LL::CpsOp::Clean) {
        require(c->m_pHSTRruntime != nullptr, "HSTR is not initialized");
        c->HSTR_StopApplication();
        forces_.clear();
        const auto name = c->HSTR_GetSelectedApplication();
        require(!name.empty(), "No selected application");
        c->m_pHSTRruntime.reset();
        const auto folder = fs::weakly_canonical(fs::path(c->m_sApplicationPath) / name);
        for (const auto& entry : fs::directory_iterator(folder)) {
            const auto filename = entry.path().filename().string();
            if (std::regex_match(filename, std::regex("\\.hstr-build-[A-Za-z0-9]{6}")) &&
                !entry.is_symlink() && fs::weakly_canonical(entry.path()).parent_path() == folder)
                fs::remove_all(entry.path());
        }
        check(c->HSTR_Init(), "Reinitialize HSTR");
        return {};
    }
    if (op == LL::CpsOp::Run) {
        require(!buildJob_.valid(),"HSTR正在生成，请等待生成成功后再运行");
        require(!buildResult_.is_object()||!buildResult_.value("done",false)||buildResult_.value("succeeded",true),"HSTR生成失败，请修复日志中的编译错误后重新生成，不能运行未编译的应用");
        require(!emergency_ && !c->CheckEmergencyStop(), "Emergency stop is active");
        require(c->m_pHSTRruntime!=nullptr,"HSTR尚未初始化，请先下载并生成应用");
        const auto state=c->HSTR_GetApplicationStatus();
        if(state==static_cast<int>(HSTR::ApplicationStatus::Running))
            return {{"alreadyRunning",true},{"plcStatus",state},{"application",c->HSTR_GetSelectedApplication()}};
        require(state==static_cast<int>(HSTR::ApplicationStatus::Ready),
            "HSTR应用不在就绪状态："+std::to_string(state)+"（0未编译，1运行，2错误，3断点暂停，4就绪）。"+
            (state==3?std::string("请使用调试继续或先停止应用。"):
            state==2?std::string("具体错误：")+c->HSTR_GetLastError():std::string("请先成功生成应用。")));
        const int result=c->HSTR_RunApplication(request.value("debug",false));
        require(result==REC_Successed,"启动HSTR失败，返回码="+std::to_string(result)+"，应用状态="+std::to_string(c->HSTR_GetApplicationStatus())+"，详情："+c->HSTR_GetLastError());
        return {{"alreadyRunning",false},{"plcStatus",c->HSTR_GetApplicationStatus()},{"application",c->HSTR_GetSelectedApplication()}};
    }
    if (op == LL::CpsOp::Stop) {
        forces_.clear();
        check(c->HSTR_StopApplication(), "Stop HSTR");
        return {};
    }
    if (op == LL::CpsOp::PlcReset) {
        require(!buildJob_.valid(),"请等待HSTR生成结束后再复位");
        forces_.clear();
        check(c->HSTR_ResetProgram(), "Reset HSTR");
        return {};
    }
    if (op == LL::CpsOp::Breakpoint) {
        const auto file = request.at("file").get<std::string>();
        const auto line = request.at("line").get<unsigned>();
        require(line > 0, "Breakpoint line starts at 1");
        check(request.value("remove", false) ? c->HSTR_RemoveBreakpoint(file, line)
                                             : c->HSTR_AddBreakpoint(file, line),
              "Breakpoint");
        return {};
    }
    if (op == LL::CpsOp::Debug) {
        const auto command = request.at("command").get<std::string>();
        if (command == "event") {
            c->m_pHSTRruntime->TriggerEvent(request.at("task"));
            return {};
        }
        require(command == "continue" || command == "over" || command == "into", "Invalid debug command");
        check(command == "continue" ? c->HSTR_DebugContinue()
              : command == "over"   ? c->HSTR_DebugSingleStep()
                                    : c->HSTR_DebugStepInto(),
              "HSTR debug");
        return Status();
    }
    if (op == LL::CpsOp::Variables)
        return {{"variables", this->Variables()}};
    if (op == LL::CpsOp::Read) {
        const auto names = request.at("names").get<std::vector<std::string>>();
        require(names.size() <= 128, "At most 128 watch variables");
        Json values = Json::object();
        for (const auto &name : names)
            try {
                values[name] = this->Read(name);
            } catch (const std::exception &e) {
                values[name] = {{"error", e.what()}};
            }
        return {{"values", values}};
    }
    if (op == LL::CpsOp::Release) {
        forces_.erase(Metadata(request.at("name")).at("name").get<std::string>());
        return {};
    }
    if (op == LL::CpsOp::Write || op == LL::CpsOp::Force) {
        const auto name = request.at("name").get<std::string>();
        const auto meta = Metadata(name);
        require(!meta.at("readOnly").get<bool>(), "Variable is read-only");
        const auto hex = LL::ScalarToHex(meta.at("type"), meta.at("size"), request.at("value"));
        const auto bytes = LL::HexBytes(hex);
        c->m_pHSTRruntime->ModifyVariable(name, bytes.data(), bytes.size());
        if (op == LL::CpsOp::Force) {
            require(forces_.size() < 128 || forces_.count(name), "Too many forced variables");
            forces_[meta.at("name").get<std::string>()] = bytes;
        }
        return this->Read(name);
    }
    if (op == LL::CpsOp::Unsubscribe) {
        const auto key=peer+"/"+request.at("token").get<std::string>();
        const auto existing=subscriptions_.find(key);
        if(existing!=subscriptions_.end()&&existing->second.stream&&request.value("drain",false)){
            existing->second.stream->Finish();subscriptions_.erase(existing);return {{"draining",true}};
        }
        if(existing!=subscriptions_.end()&&existing->second.stream){std::lock_guard<std::mutex> lock(dataMutex_);dataStreams_.erase(peer+"/"+existing->second.stream->session);}
        subscriptions_.erase(key);
        return {};
    }
    if (op == LL::CpsOp::Subscribe) {
        const auto token = request.at("token").get<std::string>();
        LL::ValidateIdentifier(token);
        const auto channels = request.at("channels").get<std::vector<std::string>>();
        require(!channels.empty() && channels.size() <= 32, "Subscribe to 1..32 channels");
        const int interval = request.at("intervalMs"), port = request.at("port");
        const bool batched=request.value("transport",std::string{})=="tcp-batch";
        require(interval >= 1 && interval <= 1000 && (batched||(port > 0 && port <= 65535)), "Invalid data subscription");
        const auto key = peer + "/" + token;
        auto existing=subscriptions_.find(key);
        if(batched&&existing!=subscriptions_.end()&&existing->second.stream&&existing->second.channels==channels&&existing->second.intervalMs==interval){
            for(std::size_t i=0;i<channels.size();++i)try{existing->second.metadata[i]=Metadata(channels[i]);}catch(...){}
            return {{"session",existing->second.stream->session},{"dataPort",LL::CpsDataPort},{"batchSize",100}};
        }
        std::vector<Json> resolved;
        for (const auto &name : channels) {
            const auto meta=Metadata(name);
            resolved.push_back(meta);
            require(meta.at("size").get<unsigned>()<=256,"Data channel variable exceeds 256 bytes: "+name);
            if(!batched||request.value("numericOnly",true))require(this->Read(name).contains("number"),"Scope channel must be numeric: "+name);
        }
        require(subscriptions_.size() < 16 || subscriptions_.count(key), "Too many scope subscribers");
        if (!batched&&udp_ < 0) {
            udp_ = socket(AF_INET, SOCK_DGRAM, 0);
            require(udp_ >= 0, "Cannot open UDP socket");
        }
        auto &sub = subscriptions_[key];
        sub.metadata=std::move(resolved);
        if(sub.stream){std::lock_guard<std::mutex> lock(dataMutex_);dataStreams_.erase(peer+"/"+sub.stream->session);}
        sub.stream.reset();sub.sequence=0;sub.missedSamples=0;sub.next=now;
        sub.address.sin_family = AF_INET;
        sub.address.sin_port = htons(port);
        inet_pton(AF_INET, peer.c_str(), &sub.address.sin_addr);
        sub.channels = channels;
        sub.intervalMs = interval;
        sub.expires = now + std::chrono::seconds(6);
        if(batched){
            sub.stream=std::make_shared<LL::DataStream>(LL::NewId("data"),std::chrono::milliseconds(std::max(30000,interval*100+5000)));
            std::lock_guard<std::mutex> lock(dataMutex_);dataStreams_[peer+"/"+sub.stream->session]=sub.stream;
            return {{"session",sub.stream->session},{"dataPort",LL::CpsDataPort},{"batchSize",100}};
        }
        return {};
    }
    if (op == LL::CpsOp::Axis) {
        const auto id = request.at("master").get<std::string>();
        const int axis = request.at("axis");
        require(readyAxes_.count(axis), "Axis slave must be in OP before motion commands");
        require(Running(id) && axisIndex_.count(axis) && axisMaster_.at(axis) == id,
                "Axis is not available on a running master");
        require(!emergency_ && !c->CheckEmergencyStop(), "Emergency stop is active");
        const int i = axisIndex_.at(axis);
        try {
        const auto command = request.at("command").get<std::string>();
        require(!axisFaults_.count(axis)||command=="reset"||command=="stop","该轴已因故障停止，请先复位后再使能或运动");
        int result = -1;
        const double position = request.value("position", 0.0), velocity = request.value("velocity", 0.0),
                     acc = request.value("acceleration", 0.0), jerk = request.value("jerk", 0.0);
        require(std::isfinite(position) && std::isfinite(velocity) && std::isfinite(acc) &&
                    std::isfinite(jerk),
                "Motion parameters must be finite");
        if(command=="absolute"||command=="relative"||command=="home") {
            auto* target=c->m_vAxis.at(i);
            require(!target->Axis_GetErrorState(),"轴存在故障，请先在轴页面复位");
            require(target->Axis_GetAxisMotionState()==motionState_standstill &&
                    (target->Axis_GetVirtualState()||(target->Axis_GetStatusWord()&0x6f)==0x27),
                    "轴未使能，请先使能后再运动");
        }
        if (command == "enable") {
            auto* target=c->m_vAxis.at(i);
            require(!target->Axis_GetErrorState()&&(target->Axis_GetStatusWord()&0x8)==0,"轴存在故障，请先复位并确认故障清除后再使能");
            require(target->Axis_GetAxisMotionState()==motionState_power_off||target->Axis_GetAxisMotionState()==motionState_standstill,"当前轴状态不允许使能");
            if(!Config(id).value("simulation",false))require(target->m_st_map.pTargetModesOfOperation!=nullptr,"使能前请映射操作模式PDO 0x6060");
            target->Axis_PDO_SetModesOfOperation(EN_ModesOfOperation::enModeCyclicSyncPosition);
            result = c->AxisEnable({i});
        }
        else if (command == "disable")
            result = c->AxisDisable({i});
        else if (command == "reset") {
            result = c->AxisReset({i});
            if(result==REC_Successed)resettingAxes_.insert(axis);
        }
        else if (command == "stop")
            result = c->AxisStop({i});
        else if (command == "home") {
            if (!Config(id).value("simulation", false)) {
                const Json *selectedAxis = nullptr;
                unsigned local = 0;
                for (const auto &slave : Config(id).at("slaves"))
                    for (const auto &candidate : slave.at("axes"))
                        if (candidate.at("id").get<int>() == axis) {
                            selectedAxis = &candidate;
                            local = slave.at("id");
                        }
                require(selectedAxis != nullptr, "Axis configuration not found");
                const auto &mapping = selectedAxis->at("mapping");
                require(!mapping.at("TargetModesOfOperation").is_null() &&
                            !mapping.at("ActualModesOfOperation").is_null(),
                        "Homing requires command and feedback operation-mode PDOs");
                const auto &p = selectedAxis->at("parameters");
                const auto methodValue = p.at("homeMethod").get<int>();
                require(methodValue >= -128 && methodValue <= 127, "Homing method must fit SINT");
                const std::int8_t method = static_cast<std::int8_t>(methodValue);
                const double rawOffset = p.at("homeOffset").get<double>();
                require(std::isfinite(rawOffset) && rawOffset >= -2147483648.0 && rawOffset <= 2147483647.0,
                        "Homing offset must fit DINT counts");
                const std::int32_t offset = static_cast<std::int32_t>(std::llround(rawOffset));
                require(Master(id)->WriteSDO(local, 0x6098, 0, &method, 1, false, 700000) > 0 &&
                            Master(id)->WriteSDO(local, 0x607c, 0, &offset, 4, false, 700000) > 0,
                        "Cannot set drive homing parameters");
            }
            result = c->AxisHome({i});
        } else if (command == "origin")
            result = c->AxisSetOffset(i, position);
        else {
            const auto params = c->v_AxisHardParam.at(i);
            if (!Config(id).value("simulation", false)) {
                for (const auto &slave : Config(id).at("slaves"))
                    for (const auto &candidate : slave.at("axes"))
                        if (candidate.at("id").get<int>() == axis)
                            require(!candidate.at("mapping").at("TargetPosition").is_null() &&
                                        !candidate.at("mapping").at("ActualPosition").is_null(),
                                    "Position moves require target and actual position PDO mappings");
            }
            require(velocity > 0 && velocity <= params.dMaxMotionVel && acc > 0 &&
                        acc <= params.dMaxMotionAcc && jerk > 0 && jerk <= params.dMaxMotionJerk,
                    "Motion exceeds configured velocity/acceleration/jerk limits");
            require(command == "absolute" || command == "relative", "Unknown axis command");
            const auto dest =
                command == "relative" ? c->m_vAxis[i]->Axis_GetActualPosition() + position : position;
            require(dest >= params.dSafeSpaceLimitLower && dest <= params.dSafeSpaceLimitUpper,
                    "Target exceeds soft limits");
            result = command == "absolute" ? c->AxisAbsMove(i, position, velocity, acc, acc, jerk)
                                           : c->AxisRelMove(i, position, velocity, acc, acc, jerk);
        }
        require(result==REC_Successed,"轴操作 "+command+" 失败："+std::to_string(result)+
            (result==CEC_SDL_INVALID_AXIS_STATE?"（当前轴状态不允许此操作，请检查使能及运行状态）":""));
        return {{"accepted",true},{"axis",axis},{"master",id}};
        } catch(const std::exception& error) { FaultAxis(axis,error.what());throw; }
    }
    throw std::runtime_error("Unknown CPS operation: " + op);
}
