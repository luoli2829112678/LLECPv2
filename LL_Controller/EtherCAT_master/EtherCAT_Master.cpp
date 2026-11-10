#include "EtherCAT_Master.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <thread>
extern "C" {
#include "soem/soem.h"
}

namespace {
// SOEM ec_config.c 的 ecx_mapt 工作区是全局的，仅配置/重配置阶段串行。
std::mutex mappingMutex;
constexpr std::size_t maxFoEBytes = 256u * 1024u * 1024u;
// 每站输入/输出位数均为 uint16；另留每站邮箱状态和字节对齐空间。
constexpr std::size_t ioCapacity = static_cast<std::size_t>(EC_MAXSLAVE) * (2u * 8192u + 16u);
}

struct EtherCAT_Master::Impl {
    mutable std::mutex ioMutex;
    std::mutex lifecycleMutex, waitMutex;
    std::condition_variable wake;
    std::unique_ptr<ecx_contextt> context = std::make_unique<ecx_contextt>();
    std::vector<std::uint8_t> ioMap;
    std::string interfaceName, error;
    bool opened = false, scanned = false, mapped = false, requiresRescan = false;
    std::atomic<bool> running{false}, manualActive{false};
    std::vector<uint16> desiredStates;
    std::thread worker;
    CycleOptions options;
    MonitorStatus stats;
    std::atomic<bool> foeActive{false}, foeWriting{false}, foeSucceeded{false};
    std::atomic<std::size_t> foeTransferred{0}, foeTotal{0};
    std::atomic<int> foeResult{0};
    static thread_local Impl* foeOwner;

    int fail(const std::string& message) { error = message; return -1; }
    bool validSlave(std::uint16_t slave) const {
        return opened && slave > 0 && slave <= context->slavecount;
    }
    bool changeState(std::uint16_t slave, State state, int timeout) {
        context->slavelist[slave].state = static_cast<uint16>(state);
        if (ecx_writestate(context.get(), slave) <= 0) return false;
        return ecx_statecheck(context.get(), slave, static_cast<uint16>(state), timeout) == static_cast<uint16>(state);
    }
    void resetMapping() {
        mapped = false;
        stats.mapped = false;
        stats.expectedWkc = 0;
        stats.actualWkc = 0;
        stats.allOperational = false;
        // 保留缓冲区：SOEM 中仍可能暂存该地址，直到下次扫描。
    }
    int exchange(int timeout) {
        const int sent = ecx_send_processdata(context.get());
        if (sent <= 0) ++stats.sendErrors;
        const int wkc = ecx_receive_processdata(context.get(), timeout);
        ++stats.cycles;
        stats.actualWkc = wkc;
        stats.dcTimeNs = context->DCtime;
        if (sent <= 0 || wkc < stats.expectedWkc) {
            stats.allOperational = false;
            ++stats.wkcErrors;
            ++stats.consecutiveWkcErrors;
        } else stats.consecutiveWkcErrors = 0;
        return wkc;
    }
    bool monitor(bool recover) {
        const int stateResult = ecx_readstate(context.get());
        if (context->slavecount <= 0) {
            stats.allOperational = false;
            error = "Failed to read slave states";
            return false;
        }
        stats.masterState = context->slavelist[0].state;
        bool allOp = context->slavecount > 0;
        for (int i = 1; i <= context->slavecount; ++i) {
            auto& slave = context->slavelist[i];
            if (slave.state != EC_STATE_OPERATIONAL) allOp = false;
            slave.islost = slave.state == EC_STATE_NONE;
            if (!recover || slave.state == EC_STATE_OPERATIONAL ||
                (static_cast<std::size_t>(i) < desiredStates.size() && desiredStates[i] != EC_STATE_OPERATIONAL)) continue;
            ++stats.recoveryAttempts;
            if (slave.state == (EC_STATE_SAFE_OP + EC_STATE_ERROR)) {
                slave.state = EC_STATE_SAFE_OP + EC_STATE_ACK;
                ecx_writestate(context.get(), static_cast<uint16>(i));
            } else if (slave.state == EC_STATE_SAFE_OP) {
                slave.state = EC_STATE_OPERATIONAL;
                ecx_writestate(context.get(), static_cast<uint16>(i));
            } else if (slave.state == EC_STATE_NONE) {
                std::lock_guard<std::mutex> configLock(mappingMutex);
                if (ecx_recover_slave(context.get(), static_cast<uint16>(i), EC_TIMEOUTRET)) slave.islost = FALSE;
            } else {
                std::lock_guard<std::mutex> configLock(mappingMutex);
                ecx_reconfig_slave(context.get(), static_cast<uint16>(i), EC_TIMEOUTRET);
            }
        }
        stats.allOperational = allOp; // 恢复后的状态由下一次检测确认
        return stateResult > 0;
    }
    void loop() noexcept {
        try {
            using Clock = std::chrono::steady_clock;
            auto next = Clock::now();
            auto last = next;
            auto nextMonitor = next + options.monitorPeriod;
            while (running.load()) {
                const auto now = Clock::now();
                {
                    std::lock_guard<std::mutex> lock(ioMutex);
                    stats.lastCycleTime = std::chrono::duration_cast<std::chrono::microseconds>(now - last);
                    exchange(options.receiveTimeoutUs);
                    if (now >= nextMonitor || stats.consecutiveWkcErrors == options.wkcErrorThreshold) {
                        monitor(options.autoRecover);
                        nextMonitor = Clock::now() + options.monitorPeriod;
                    }
                }
                last = now;
                next += options.period;
                const auto end = Clock::now();
                if (end > next) {
                    const auto missed = (end - next) / options.period + 1;
                    {
                        std::lock_guard<std::mutex> lock(ioMutex);
                        stats.missedDeadlines += static_cast<std::uint64_t>(missed);
                    }
                    next += options.period * missed;
                }
                std::unique_lock<std::mutex> waitLock(waitMutex);
                wake.wait_until(waitLock, next, [this] { return !running.load(); });
            }
        } catch (const std::exception& ex) {
            std::lock_guard<std::mutex> lock(ioMutex);
            error = std::string("Cycle thread failed: ") + ex.what();
            running = false;
        } catch (...) {
            std::lock_guard<std::mutex> lock(ioMutex);
            error = "Cycle thread failed";
            running = false;
        }
    }
    // lifecycleMutex 必须已持有；join 时不能持有 ioMutex。
    void stop() {
        const bool wasManual = manualActive.exchange(false);
        const bool wasRunning = running.exchange(false);
        wake.notify_all();
        const bool hadWorker = worker.joinable();
        if (hadWorker) worker.join();
        std::lock_guard<std::mutex> lock(ioMutex);
        if ((wasRunning || wasManual || hadWorker) && opened && mapped) {
            if (!changeState(0, State::SafeOp, EC_TIMEOUTSTATE)) error = "Stopped cycle, but SAFE_OP request failed";
            stats.allOperational = false;
        }
    }
    void close() {
        if (opened) ecx_close(context.get());
        opened = scanned = mapped = requiresRescan = false;
        context->slavecount = 0;
        ioMap.clear();
        stats = {};
    }
    static int foeHook(uint16, int, int size) {
        if (foeOwner && size >= 0) {
            const auto count = static_cast<std::size_t>(size);
            // 当前 SOEM write hook 给出剩余字节，read hook 给出累计读取字节。
            foeOwner->foeTransferred = foeOwner->foeWriting.load()
                ? foeOwner->foeTotal.load() - std::min(foeOwner->foeTotal.load(), count) : count;
        }
        return 0;
    }
    bool enterBoot(std::uint16_t id) {
        if (!changeState(id, State::Init, EC_TIMEOUTSTATE)) return false;
        const uint32 rx = ecx_readeeprom(context.get(), id, ECT_SII_BOOTRXMBX, EC_TIMEOUTEEP);
        const uint32 tx = ecx_readeeprom(context.get(), id, ECT_SII_BOOTTXMBX, EC_TIMEOUTEEP);
        const auto rxOffset = static_cast<uint16>(LO_WORD(rx)), rxLength = static_cast<uint16>(HI_WORD(rx));
        const auto txOffset = static_cast<uint16>(LO_WORD(tx)), txLength = static_cast<uint16>(HI_WORD(tx));
        if (!rxOffset || !txOffset || rxLength <= 12 || txLength <= 12 || rxLength > EC_MAXMBX || txLength > EC_MAXMBX) return false;
        auto& slave = context->slavelist[id];
        slave.SM[0].StartAddr = htoes(rxOffset);
        slave.SM[0].SMlength = htoes(rxLength);
        slave.SM[1].StartAddr = htoes(txOffset);
        slave.SM[1].SMlength = htoes(txLength);
        slave.mbx_wo = rxOffset; slave.mbx_l = rxLength;
        slave.mbx_ro = txOffset; slave.mbx_rl = txLength;
        if (ecx_FPWR(&context->port, slave.configadr, ECT_REG_SM0, sizeof(ec_smt), &slave.SM[0], EC_TIMEOUTRET) <= 0 ||
            ecx_FPWR(&context->port, slave.configadr, ECT_REG_SM1, sizeof(ec_smt), &slave.SM[1], EC_TIMEOUTRET) <= 0) return false;
        return changeState(id, State::Boot, EC_TIMEOUTSTATE);
    }
    int transfer(bool writing, std::uint16_t id, const std::string& name,
                 std::vector<std::uint8_t>& data, const FoEOptions& opts) {
        std::lock_guard<std::mutex> life(lifecycleMutex);
        std::lock_guard<std::mutex> lock(ioMutex);
        error.clear();
        foeSucceeded = false; foeTransferred = 0; foeTotal = writing ? data.size() : 0; foeResult = -1;
        if (running || manualActive || !validSlave(id)) return fail("FoE requires a stopped master and a valid slave");
        if (name.empty() || name.find('\0') != std::string::npos || opts.timeoutUs <= 0 || data.size() > maxFoEBytes)
            return fail("Invalid FoE name, timeout or buffer size");
        ecx_readstate(context.get());
        auto& slave = context->slavelist[id];
        if (!opts.bootstrap && slave.state != EC_STATE_PRE_OP && slave.state != EC_STATE_BOOT)
            return fail("FoE requires PRE_OP/BOOT, or bootstrap=true");
        const ec_smt savedSm0 = slave.SM[0], savedSm1 = slave.SM[1];
        const auto savedWo = slave.mbx_wo, savedWl = slave.mbx_l, savedRo = slave.mbx_ro, savedRl = slave.mbx_rl;
        std::vector<char> filename(name.begin(), name.end()); filename.push_back('\0');
        const auto savedHook = context->FOEhook;
        foeWriting = writing; foeActive = true;
        int result = -1;
        bool prepared = !opts.bootstrap || enterBoot(id);
        if (prepared && (slave.mbx_l <= 12 || slave.mbx_rl <= 12 || slave.mbx_l > EC_MAXMBX || slave.mbx_rl > EC_MAXMBX ||
                         name.size() > static_cast<std::size_t>(std::min(slave.mbx_l, slave.mbx_rl) - 12))) {
            prepared = false;
            error = "FoE mailbox is invalid or remote filename does not fit";
        }
        if (prepared) {
            // 预先构造可写文件名，C ABI 不接受 const char*。
            context->FOEhook = &Impl::foeHook;
            foeOwner = this;
            int size = static_cast<int>(data.size());
            std::uint8_t emptyByte = 0;
            void* buffer = data.empty() ? &emptyByte : data.data();
            result = writing ? ecx_FOEwrite(context.get(), id, filename.data(), opts.password, size, buffer, opts.timeoutUs)
                             : ecx_FOEread(context.get(), id, filename.data(), opts.password, &size, buffer, opts.timeoutUs);
            foeOwner = nullptr;
            context->FOEhook = savedHook;
            if (result > 0 && !writing) {
                if (size < 0 || static_cast<std::size_t>(size) > data.size()) result = -1;
                else data.resize(static_cast<std::size_t>(size));
            }
            if (result <= 0) error = "FoE transfer failed (SOEM result " + std::to_string(result) + ")";
        } else if (error.empty()) error = "Failed to configure BOOT mailboxes/state";
        if (opts.bootstrap) {
            const bool restored = changeState(id, State::Init, EC_TIMEOUTSTATE);
            slave.SM[0] = savedSm0; slave.SM[1] = savedSm1;
            slave.mbx_wo = savedWo; slave.mbx_l = savedWl; slave.mbx_ro = savedRo; slave.mbx_rl = savedRl;
            resetMapping();
            requiresRescan = true;
            if (!restored) { result = -1; error = "FoE finished, but returning to INIT failed; rescan required"; }
        }
        if (result > 0) {
            foeTransferred = data.size(); foeTotal = data.size(); error.clear();
        }
        foeResult = result; foeSucceeded = result > 0; foeActive = false;
        return result;
    }
};
thread_local EtherCAT_Master::Impl* EtherCAT_Master::Impl::foeOwner = nullptr;

EtherCAT_Master::EtherCAT_Master() : m_impl(std::make_unique<Impl>()) {}
EtherCAT_Master::~EtherCAT_Master() { Close(); }
bool EtherCAT_Master::Open(const char* name) {
    auto& p = *m_impl;
    std::lock_guard<std::mutex> life(p.lifecycleMutex);
    p.stop();
    std::lock_guard<std::mutex> lock(p.ioMutex);
    p.close();
    if (!name || !*name) { p.fail("Interface name is empty"); return false; }
    p.context = std::make_unique<ecx_contextt>();
    p.interfaceName = name;
    p.opened = ecx_init(p.context.get(), name) > 0;
    if (!p.opened) { ecx_close(p.context.get()); p.fail("Failed to open interface: " + p.interfaceName); }
    else p.error.clear();
    return p.opened;
}
void EtherCAT_Master::Close() {
    auto& p = *m_impl;
    std::lock_guard<std::mutex> life(p.lifecycleMutex);
    p.stop();
    std::lock_guard<std::mutex> lock(p.ioMutex);
    p.close();
}
int EtherCAT_Master::ScanSlaves() {
    auto& p = *m_impl;
    std::lock_guard<std::mutex> life(p.lifecycleMutex);
    std::lock_guard<std::mutex> lock(p.ioMutex);
    if (!p.opened || p.running || p.manualActive) return p.fail("Scan requires an open, stopped master");
    if (p.scanned) {
        p.close();
        p.context = std::make_unique<ecx_contextt>();
            p.opened = ecx_init(p.context.get(), p.interfaceName.c_str()) > 0;
        if (!p.opened) { ecx_close(p.context.get()); return p.fail("Failed to reopen interface for rescan"); }
    }
    p.resetMapping();
    std::lock_guard<std::mutex> configLock(mappingMutex);
    const int count = ecx_config_init(p.context.get());
    p.scanned = true;
    p.requiresRescan = false;
    if (count <= 0) p.fail("No slaves found or configuration failed"); else p.error.clear();
    return count;
}
int EtherCAT_Master::ConfigureProcessData(bool dc) {
    auto& p = *m_impl;
    std::lock_guard<std::mutex> life(p.lifecycleMutex);
    std::lock_guard<std::mutex> lock(p.ioMutex);
    if (!p.opened || !p.scanned || p.requiresRescan || p.running || p.manualActive || p.mapped || p.context->slavecount <= 0)
        return p.fail("Mapping requires scanned slaves, stopped cycles and no existing mapping");
    p.context->packedMode = FALSE;
    p.context->overlappedMode = FALSE;
    p.ioMap.assign(ioCapacity, 0);
    std::lock_guard<std::mutex> configLock(mappingMutex);
    const int bytes = ecx_config_map_group(p.context.get(), p.ioMap.data(), 0);
    if (bytes <= 0 || static_cast<std::size_t>(bytes) > p.ioMap.size()) return p.fail("PDO mapping failed");
    if (dc) ecx_configdc(p.context.get());
    p.mapped = true;
    p.stats.expectedWkc = p.context->grouplist[0].outputsWKC * 2 + p.context->grouplist[0].inputsWKC;
    p.stats.mapped = true;
    p.desiredStates.assign(static_cast<std::size_t>(p.context->slavecount) + 1, EC_STATE_OPERATIONAL);
    p.error.clear();
    return bytes;
}
bool EtherCAT_Master::ConfigureSync0(std::uint16_t id, bool enabled, std::uint32_t cycleNs, std::int32_t shiftNs) {
    auto& p = *m_impl;
    std::lock_guard<std::mutex> life(p.lifecycleMutex);
    std::lock_guard<std::mutex> lock(p.ioMutex);
    if (!p.validSlave(id) || p.running || p.manualActive || !p.mapped || !p.context->slavelist[id].hasdc || (enabled && !cycleNs)) {
        p.fail("SYNC0 requires stopped, mapped DC-capable slave and nonzero cycle"); return false;
    }
    ecx_dcsync0(p.context.get(), id, enabled ? TRUE : FALSE, cycleNs, shiftNs);
    p.error.clear();
    return true; // SOEM 的配置函数无返回值；实际同步状态需在从站上确认
}
bool EtherCAT_Master::RequestState(std::uint16_t slave, State state, int timeout) {
    auto& p = *m_impl;
    std::lock_guard<std::mutex> life(p.lifecycleMutex);
    std::lock_guard<std::mutex> lock(p.ioMutex);
    if (!p.opened || p.running || p.manualActive || slave > p.context->slavecount || timeout <= 0 || state == State::Boot || state == State::Operational) {
        p.fail("State change requires stopped master; use Start for OP and FoE bootstrap for BOOT"); return false;
    }
    if (state != State::Init && state != State::PreOp && state != State::SafeOp) { p.fail("Invalid requested state"); return false; }
    if (state == State::SafeOp && !p.mapped) { p.fail("SAFE_OP requires PDO mapping"); return false; }
    if (state == State::Init) { p.resetMapping(); p.requiresRescan = true; }
    if (!p.changeState(slave, state, timeout)) { p.fail("Requested state was not reached"); return false; }
    p.error.clear(); return true;
}
bool EtherCAT_Master::Start() { return Start(CycleOptions{}); }
bool EtherCAT_Master::Start(const CycleOptions& opts) {
    auto& p = *m_impl;
    std::lock_guard<std::mutex> life(p.lifecycleMutex);
    if (p.running || p.manualActive) return true;
    if (p.worker.joinable()) p.stop();
    std::lock_guard<std::mutex> lock(p.ioMutex);
    if (!p.opened || !p.mapped || opts.period.count() < 100 || opts.period > std::chrono::seconds(1) ||
        opts.monitorPeriod.count() <= 0 || opts.receiveTimeoutUs <= 0 || opts.wkcErrorThreshold == 0) {
        p.fail("Start requires mapped PDO and valid cycle options"); return false;
    }
    if (!p.changeState(0, State::SafeOp, EC_TIMEOUTSTATE)) { p.fail("SAFE_OP was not reached"); return false; }
    p.exchange(opts.receiveTimeoutUs);
    p.context->slavelist[0].state = EC_STATE_OPERATIONAL;
    bool operational = ecx_writestate(p.context.get(), 0) > 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    if (operational) {
        operational = false;
        do {
            p.exchange(opts.receiveTimeoutUs);
            if (ecx_statecheck(p.context.get(), 0, EC_STATE_OPERATIONAL, 1000) == EC_STATE_OPERATIONAL) { operational = true; break; }
        } while (std::chrono::steady_clock::now() < deadline);
    }
    if (!operational) {
        p.changeState(0, State::SafeOp, EC_TIMEOUTSTATE);
        p.fail("OP was not reached; inspect slave AL status and PDO configuration"); return false;
    }
    p.options = opts;
    if (!opts.background) { p.manualActive = true; p.error.clear(); return true; }
    p.running = true;
    try { p.worker = std::thread([&p] { p.loop(); }); }
    catch (...) {
        p.running = false;
        p.changeState(0, State::SafeOp, EC_TIMEOUTSTATE);
        p.fail("Failed to create cycle thread"); return false;
    }
    p.error.clear(); return true;
}
void EtherCAT_Master::Stop() {
    std::lock_guard<std::mutex> life(m_impl->lifecycleMutex);
    m_impl->stop();
}
int EtherCAT_Master::ExchangeProcessData(int timeout) {
    auto& p = *m_impl;
    std::lock_guard<std::mutex> lock(p.ioMutex);
    if (!p.opened || !p.mapped || p.running || timeout <= 0) return p.fail("Manual exchange requires mapped PDO and stopped background cycle");
    return p.exchange(timeout);
}
bool EtherCAT_Master::PollMonitor() {
    std::lock_guard<std::mutex> lock(m_impl->ioMutex);
    if (!m_impl->opened) { m_impl->fail("Master is closed"); return false; }
    return m_impl->monitor(false);
}
int EtherCAT_Master::GetSlaveCount() const { std::lock_guard<std::mutex> lock(m_impl->ioMutex); return m_impl->context->slavecount; }
bool EtherCAT_Master::IsOpen() const { std::lock_guard<std::mutex> lock(m_impl->ioMutex); return m_impl->opened; }
bool EtherCAT_Master::IsRunning() const { return m_impl->running.load() || m_impl->manualActive.load(); }
std::string EtherCAT_Master::GetLastError() const { std::lock_guard<std::mutex> lock(m_impl->ioMutex); return m_impl->error; }
EtherCAT_Master::MonitorStatus EtherCAT_Master::GetMonitorStatus() const {
    std::lock_guard<std::mutex> lock(m_impl->ioMutex);
    auto result = m_impl->stats;
    result.opened = m_impl->opened; result.running = m_impl->running || m_impl->manualActive;
    result.mapped = m_impl->mapped; result.slaveCount = m_impl->context->slavecount;
    return result;
}
std::vector<EtherCAT_Master::SlaveInfo> EtherCAT_Master::GetSlaves() const {
    std::lock_guard<std::mutex> lock(m_impl->ioMutex);
    std::vector<SlaveInfo> result;
    for (int i = 1; i <= m_impl->context->slavecount; ++i) {
        const auto& s = m_impl->context->slavelist[i];
        SlaveInfo info;
        info.id = static_cast<std::uint16_t>(i); info.state = s.state; info.alStatusCode = s.ALstatuscode;
        info.vendorId = s.eep_man; info.productCode = s.eep_id; info.revision = s.eep_rev; info.serial = s.eep_ser;
        info.name = s.name; info.inputBytes = (s.Ibits + 7u) / 8u; info.outputBytes = (s.Obits + 7u) / 8u;
        info.inputStartBit = s.Istartbit; info.outputStartBit = s.Ostartbit;
        info.lost = s.islost != 0; info.hasDistributedClock = s.hasdc != 0;
        result.push_back(info);
    }
    return result;
}
bool EtherCAT_Master::WriteOutputs(std::uint16_t id, const void* data, std::size_t size, std::size_t offset) {
    auto& p = *m_impl;
    std::lock_guard<std::mutex> lock(p.ioMutex);
    if (!p.mapped || !p.validSlave(id) || (!data && size)) { p.fail("Invalid PDO output request"); return false; }
    const auto& s = p.context->slavelist[id];
    const std::size_t bytes = (s.Obits + 7u) / 8u;
    if (offset > bytes || size > bytes - offset || (size && !s.outputs)) { p.fail("PDO output range exceeds slave mapping"); return false; }
    if (size) std::memcpy(s.outputs + offset, data, size);
    return true;
}
std::vector<std::uint8_t> EtherCAT_Master::ReadInputs(std::uint16_t id) const {
    auto& p = *m_impl;
    std::lock_guard<std::mutex> lock(p.ioMutex);
    if (!p.mapped || !p.validSlave(id)) return {};
    const auto& s = p.context->slavelist[id];
    const std::size_t bytes = (s.Ibits + 7u) / 8u;
    if (!bytes || !s.inputs) return {};
    return {s.inputs, s.inputs + bytes};
}
int EtherCAT_Master::ReadSDO(std::uint16_t id, std::uint16_t index, std::uint8_t sub,
                            void* data, int& size, bool ca, int timeout) {
    auto& p = *m_impl;
    std::lock_guard<std::mutex> lock(p.ioMutex);
    if (!p.validSlave(id) || !data || size <= 0 || timeout <= 0) return p.fail("Invalid SDO read request");
    const int result = ecx_SDOread(p.context.get(), id, index, sub, ca ? TRUE : FALSE, &size, data, timeout);
    if (result <= 0) p.fail("SDO read failed");
    return result;
}
int EtherCAT_Master::WriteSDO(std::uint16_t id, std::uint16_t index, std::uint8_t sub,
                             const void* data, int size, bool ca, int timeout) {
    auto& p = *m_impl;
    std::lock_guard<std::mutex> lock(p.ioMutex);
    if (!p.validSlave(id) || !data || size <= 0 || timeout <= 0) return p.fail("Invalid SDO write request");
    const int result = ecx_SDOwrite(p.context.get(), id, index, sub, ca ? TRUE : FALSE, size, data, timeout);
    if (result <= 0) p.fail("SDO write failed");
    return result;
}
int EtherCAT_Master::FoEWrite(std::uint16_t id, const std::string& name,
                             const std::vector<std::uint8_t>& data, const FoEOptions& opts) {
    if (data.size() > maxFoEBytes) { std::lock_guard<std::mutex> lock(m_impl->ioMutex); return m_impl->fail("FoE file exceeds 256 MiB"); }
    auto buffer = data; // SOEM C ABI 使用可写指针
    return m_impl->transfer(true, id, name, buffer, opts);
}
int EtherCAT_Master::FoERead(std::uint16_t id, const std::string& name,
                            std::vector<std::uint8_t>& data, std::size_t maxBytes, const FoEOptions& opts) {
    if (!maxBytes || maxBytes > maxFoEBytes) { std::lock_guard<std::mutex> lock(m_impl->ioMutex); return m_impl->fail("Invalid FoE read capacity (1..256 MiB)"); }
    std::vector<std::uint8_t> buffer(maxBytes);
    const int result = m_impl->transfer(false, id, name, buffer, opts);
    if (result > 0) data = std::move(buffer); // 失败时保留调用方原始数据
    return result;
}
int EtherCAT_Master::FoEWriteFile(std::uint16_t id, const std::string& path,
                                 const std::string& name, const FoEOptions& opts) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file ? file.tellg() : std::streampos(-1);
    if (size < 0 || size > static_cast<std::streamoff>(maxFoEBytes)) {
        std::lock_guard<std::mutex> lock(m_impl->ioMutex); return m_impl->fail("Cannot open FoE input file, or file exceeds 256 MiB");
    }
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!data.empty() && !file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
        std::lock_guard<std::mutex> lock(m_impl->ioMutex); return m_impl->fail("Failed to read FoE input file");
    }
    return m_impl->transfer(true, id, name, data, opts);
}
int EtherCAT_Master::FoEReadFile(std::uint16_t id, const std::string& path,
                                const std::string& name, std::size_t maxBytes, const FoEOptions& opts) {
    std::vector<std::uint8_t> data;
    const int result = FoERead(id, name, data, maxBytes, opts);
    if (result <= 0) return result;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (file && !data.empty()) file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    file.close();
    if (!file) {
        std::lock_guard<std::mutex> lock(m_impl->ioMutex); return m_impl->fail("FoE succeeded but saving local file failed");
    }
    return result;
}
EtherCAT_Master::FoEStatus EtherCAT_Master::GetFoEStatus() const {
    FoEStatus status;
    status.active = m_impl->foeActive; status.writing = m_impl->foeWriting; status.succeeded = m_impl->foeSucceeded;
    status.transferredBytes = m_impl->foeTransferred; status.totalBytes = m_impl->foeTotal; status.result = m_impl->foeResult;
    return status;
}
// 独立 AL 状态控制：主动切换到 PRE_OP/SAFE_OP 不等价于掉站，也不由恢复线程覆盖。
bool EtherCAT_Master::ChangeSlaveState(std::uint16_t id, State state, int timeout) {
    auto& p = *m_impl;
    std::lock_guard<std::mutex> life(p.lifecycleMutex);
    std::lock_guard<std::mutex> lock(p.ioMutex);
    if (!p.validSlave(id) || timeout <= 0 || state == State::Boot ||
        (state != State::Init && state != State::PreOp && state != State::SafeOp && state != State::Operational)) {
        p.fail("Invalid slave state request; BOOT is reserved for FoE"); return false;
    }
    if ((state == State::SafeOp || state == State::Operational) && !p.mapped) {
        p.fail("PDO mapping is required for SAFE_OP/OP"); return false;
    }
    if (state == State::Operational && p.context->slavelist[id].state != EC_STATE_SAFE_OP &&
        p.context->slavelist[id].state != EC_STATE_OPERATIONAL) {
        std::lock_guard<std::mutex> configLock(mappingMutex);
        if (ecx_reconfig_slave(p.context.get(), id, timeout) <= 0) {
            p.fail("Slave reconfiguration failed before OP"); return false;
        }
    }
    if (state == State::Operational) p.exchange(EC_TIMEOUTRET);
    if (!p.changeState(id, state, timeout)) { p.fail("Slave did not reach requested AL state"); return false; }
    if (id < p.desiredStates.size()) p.desiredStates[id] = static_cast<uint16>(state);
    p.stats.allOperational = false;
    p.error.clear();
    return true;
}
bool EtherCAT_Master::VisitNativeSlaves(const std::function<bool(int, void*)>& visitor) {
    auto& p = *m_impl;
    std::lock_guard<std::mutex> life(p.lifecycleMutex);
    std::lock_guard<std::mutex> lock(p.ioMutex);
    if (!visitor || !p.opened || p.running || p.manualActive) return false;
    return visitor(p.context->slavecount, p.context->slavelist);
}

// 兼容参考工程的 UpdateESI：输入必须为有效的 SII EEPROM 二进制映像，不能直接写 XML。
int EtherCAT_Master::WriteSiiFile(std::uint16_t id, const std::string& path) {
    auto& p=*m_impl;
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    const auto length=file?file.tellg():std::streampos(-1);
    if(length<128||length>128*1024||static_cast<std::streamoff>(length)%2) {
        std::lock_guard<std::mutex> lock(p.ioMutex);return p.fail("SII image must be an even-sized 128 B..128 KiB binary");
    }
    std::vector<std::uint8_t> image(static_cast<std::size_t>(length));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(image.data()),static_cast<std::streamsize>(image.size()))) {
        std::lock_guard<std::mutex> lock(p.ioMutex);return p.fail("Cannot read SII image");
    }
    std::uint8_t crc=0xff;
    for(std::size_t i=0;i<14;++i){crc^=image[i];for(int bit=0;bit<8;++bit)crc=static_cast<std::uint8_t>((crc&0x80)?(crc<<1)^0x07:crc<<1);}
    if(image[14]!=crc||image[15]!=0){std::lock_guard<std::mutex> lock(p.ioMutex);return p.fail("Invalid SII header CRC; XML is not an EEPROM image");}
    std::lock_guard<std::mutex> life(p.lifecycleMutex);
    std::lock_guard<std::mutex> lock(p.ioMutex);
    if(p.running||p.manualActive||!p.validSlave(id))return p.fail("SII write requires a stopped master and valid slave");
    if(!p.changeState(id,State::Init,EC_TIMEOUTSTATE))return p.fail("SII write requires INIT");
    p.resetMapping();p.requiresRescan=true;
    const auto sizeWord=ecx_readeeprom(p.context.get(),id,0x003e,EC_TIMEOUTEEP)&0xffffu;
    const auto capacity=(static_cast<std::size_t>(sizeWord)+1u)*128u;
    if(image.size()>capacity)return p.fail("SII image exceeds device EEPROM capacity");
    if(ecx_eeprom2master(p.context.get(),id)<=0)return p.fail("Cannot acquire EEPROM ownership");
    p.foeActive=true;p.foeWriting=true;p.foeSucceeded=false;p.foeTransferred=0;p.foeTotal=image.size();p.foeResult=-1;
    int result=1;
    for(std::size_t offset=0;offset<image.size();offset+=2){
        const auto word=static_cast<uint16>(image[offset]|(static_cast<unsigned>(image[offset+1])<<8));
        const auto address=static_cast<uint16>(offset/2);
        if(ecx_writeeeprom(p.context.get(),id,address,word,EC_TIMEOUTEEP)<=0||
            static_cast<uint16>(ecx_readeeprom(p.context.get(),id,address,EC_TIMEOUTEEP))!=word){
            result=-1;p.error="EEPROM write/verification failed at word "+std::to_string(address);break;
        }
        p.foeTransferred=offset+2;
    }
    if(ecx_eeprom2pdi(p.context.get(),id)<=0){result=-1;p.error="EEPROM ownership could not be returned to device";}
    if(result>0)p.error.clear();
    p.foeSucceeded=result>0;p.foeResult=result;p.foeActive=false;
    return result;
}
