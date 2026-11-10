#include "LL_CpsService.h"
#include "LLC_Rbt.h"
#include "LL_Pdo.h"
#include <algorithm>
LL::Json LL_CpsService::Pdo(const Json &request, bool write) {
    const auto id = request.at("master").get<std::string>();
    const auto slaveId = request.at("slave").get<unsigned>();
    if (!Attached() || !prepared_ || !Running(id))
        throw std::runtime_error("Start the configured master before accessing PDOs");
    const auto &config = Config(id);
    if (slaveId == 0 || slaveId > config.at("slaves").size())
        throw std::runtime_error("Invalid slave id");
    const auto &slave = config.at("slaves").at(slaveId - 1);
    const bool simulation = config.value("simulation", false);
    auto master = Master(id);
    auto bus=master->Status();
    bool validInput=simulation;
    if (!simulation) {
        bool connected = false;
        for (const auto &state : master->Status().slaves)
            if (state.id == slaveId)
                connected = state.connected;
        if (!connected)
            throw std::runtime_error("Slave is disconnected");
        if(!master->RefreshInputs())throw std::runtime_error("Cannot refresh slave input process image");
        bus=master->Status();
        validInput=bus.expectedWkc>0&&bus.actualWkc>=bus.expectedWkc;
    }
    const unsigned offset = request.value("offset", 0u), limit = request.value("limit", 128u);
    if (limit == 0 || limit > 256)
        throw std::runtime_error("PDO page size must be 1..256");
    Json entries = Json::array();
    unsigned total = 0;
    bool written = false;
    auto access = [&](bool rx, void *data, std::size_t bytes, unsigned start) {
        std::size_t bit = start;
        const auto &list = slave.at(rx ? "rxPdos" : "txPdos");
        for (const auto &p : list) {
            if (!p.value("selected", false))
                continue;
            for (std::size_t n = 0; n < p.at("entries").size(); ++n) {
                const auto &e = p["entries"][n];
                const unsigned bits = e.at("bits");
                if (e.value("padding", false) || e.value("index", 0) == 0 || bits == 0) {
                    bit += bits;
                    continue;
                }
                const auto key = LL::PdoKey(rx, p.at("index"), static_cast<unsigned>(n));
                bool owned = false;
                const void* virtualValue=nullptr;
                for (const auto &axis : slave.at("axes"))
                    if (axis.value("enabled", false))
                        for (const auto &f : LL::AxisFields()) {
                            if (f.rx != rx)
                                continue;
                            const auto &m = axis.at("mapping").at(f.name);
                            if (!m.is_null() && m.at("pdo") == p.at("index") &&
                                m.at("index") == e.at("index") && m.at("subIndex") == e.at("subIndex")) {
                                owned = true;
                                if(simulation&&axisIndex_.count(axis.at("id").get<int>())) {
                                    auto* a=LLCRbt->m_vAxis.at(axisIndex_.at(axis.at("id").get<int>()));
                                    const std::string field=f.name;
                                    if(field=="Controlword")virtualValue=a->m_st_map.pControlword;
                                    else if(field=="StatusWord")virtualValue=a->m_st_map.pStatusWord;
                                    else if(field=="ErrorCode")virtualValue=a->m_st_map.pErrorCode;
                                    else if(field=="TargetPosition")virtualValue=a->m_st_map.pTargetPosition;
                                    else if(field=="ActualPosition")virtualValue=a->m_st_map.pActualPosition;
                                    else if(field=="TargetVelocity")virtualValue=a->m_st_map.pTargetVelocity;
                                    else if(field=="ActualVelocity")virtualValue=a->m_st_map.pActualVelocity;
                                    else if(field=="TargetTorque")virtualValue=a->m_st_map.pTargetTorque;
                                    else if(field=="ActualTorque")virtualValue=a->m_st_map.pActualTorque;
                                    else if(field=="TargetModesOfOperation")virtualValue=a->m_st_map.pTargetModesOfOperation;
                                    else if(field=="ActualModesOfOperation")virtualValue=a->m_st_map.pActualModesOfOperation;
                                    else if(field=="DigitalInputs")virtualValue=a->m_st_map.pDigitalInputs;
                                    else if(field=="DigitalOutputs")virtualValue=a->m_st_map.pDigitalOutputs;
                                }
                            }
                        }
                if(virtualValue)LL::WritePdoBits(data,bytes,bit,bits,LL::ReadPdoBits(virtualValue,(bits+7)/8,0,bits));
                // Also protect legacy optional bindings that are not present in the CPS mapping table.
                if(!simulation&&rx&&data&&bit%8==0)for(const auto& configured:slave.at("axes")){
                    const auto found=axisIndex_.find(configured.at("id").get<int>());if(found==axisIndex_.end())continue;
                    const auto& map=LLCRbt->m_vAxis.at(found->second)->m_st_map;
                    const void* pointers[]={map.pControlword,map.pTargetPosition,map.pTargetVelocity,map.pTargetTorque,map.pTargetModesOfOperation,map.pDigitalOutputs,map.pTouchProbeFunction,map.pAddVelocityValue,map.pAddTorqueValue};
                    const auto* address=static_cast<const std::uint8_t*>(data)+bit/8;
                    for(const auto* pointer:pointers)if(pointer&&pointer==address)owned=true;
                }
                const bool writable = rx && !owned && !emergency_;
                const std::string reason = !rx          ? "从站反馈，只读"
                                           : owned      ? "已绑定注册轴，请使用轴调试或取消轴注册"
                                           : emergency_ ? "急停中禁止写入"
                                                        : "";
                if (write && request.at("key") == key) {
                    if (!writable)
                        throw std::runtime_error(reason);
                    const auto value = LL::PdoHex(e, request.at("value"));
                    LL::WritePdoBits(data, bytes, bit, bits, value);
                    written = true;
                }
                if (total >= offset && total - offset < limit) {
                    Json row = {{"key", key},
                                {"rx", rx},
                                {"pdo", p.at("index")},
                                {"index", e.at("index")},
                                {"subIndex", e.at("subIndex")},
                                {"bits", bits},
                                {"type", e.value("type", "")},
                                {"name", e.value("name", "")},
                                {"writable", writable},
                                {"valid",rx||validInput},
                                {"reason", reason}};
                    try {
                        const auto hex = LL::ReadPdoBits(data, bytes, bit, bits);
                        row["hex"] = hex;
                        row["value"] = LL::PdoValue(e, hex);
                    } catch (const std::exception &error) {
                        row["error"] = error.what();
                        row["writable"] = false;
                    }
                    entries.push_back(std::move(row));
                }
                ++total;
                bit += bits;
            }
        }
    };
    if (simulation) {
        auto &image = virtualPdos_[id + "/" + std::to_string(slaveId)];
        auto count = [&](const char *key) {
            std::size_t bits = 0;
            for (const auto &p : slave.at(key))
                if (p.value("selected", false))
                    for (const auto &e : p.at("entries"))
                        bits += e.at("bits").get<unsigned>();
            return (bits + 7) / 8;
        };
        image.rx.resize(count("rxPdos"));
        image.tx.resize(count("txPdos"));
        access(true, image.rx.data(), image.rx.size(), 0);
        access(false, image.tx.data(), image.tx.size(), 0);
    } else {
        bool visited = master->VisitSlaves([&](int count, void *data) {
            if (slaveId > static_cast<unsigned>(count))
                return false;
            auto &native = static_cast<ec_slavet *>(data)[slaveId];
            access(true, native.outputs, (native.Obits + native.Ostartbit + 7u) / 8u, native.Ostartbit);
            access(false, native.inputs, (native.Ibits + native.Istartbit + 7u) / 8u, native.Istartbit);
            return true;
        });
        if (!visited)
            throw std::runtime_error("Process image is not configured");
    }
    std::string inputHex,outputHex;
    if(!simulation)master->VisitSlaves([&](int count,void* data){
        if(slaveId>static_cast<unsigned>(count))return false;
        const auto& native=static_cast<const ec_slavet*>(data)[slaveId];
        if(native.Ibits)inputHex=LL::ReadPdoBits(native.inputs,(native.Ibits+native.Istartbit+7u)/8u,native.Istartbit,std::min<unsigned>(native.Ibits,512));
        if(native.Obits)outputHex=LL::ReadPdoBits(native.outputs,(native.Obits+native.Ostartbit+7u)/8u,native.Ostartbit,std::min<unsigned>(native.Obits,512));
        return true;
    });
    if (write) {
        if (!written)
            throw std::runtime_error("PDO entry was not found in selected channels");
        if (!simulation)
            master->Exchange();
    }
    return {{"master", id},       {"slave", slaveId}, {"simulation", simulation},
            {"entries", entries}, {"total", total},   {"offset", offset},
            {"inputValid",validInput},{"actualWkc",bus.actualWkc},{"expectedWkc",bus.expectedWkc},
            {"cycles",bus.cycles},{"inputHex",inputHex},{"outputHex",outputHex},
            {"warning",simulation?"":!validInput?"输入过程数据无效：请检查WKC、RX错误与从站状态":
                !inputHex.empty()&&inputHex.find_first_not_of('0')==std::string::npos?"已收到过程数据，但从站TxPDO全部为0，请核对驱动器状态和固件PDO布局":""},
            {"limit", limit}};
}
