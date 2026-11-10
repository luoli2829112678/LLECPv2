#include "EtherCAT_Master.h"
#include "LL_MasterRegistry.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <filesystem>
#include <fstream>
#include <array>
#include <iostream>
#include <stdexcept>
#include <thread>
extern "C" {
#include "soem/soem.h"
}
using namespace std::chrono_literals;
namespace {
std::atomic<bool> loseFirst{false}, failFoe{false}, failBoot{false};
std::atomic<bool> failSend{false}, failReceive{false};
std::atomic<uint16_t> feedback{0};
std::atomic<bool> failPreOp{false}, failSdo{false}, pendingSdoError{false};
std::atomic<bool> eepromOwned{false}, failEeprom{false};
std::array<uint16,65536> eepromWords{};
std::atomic<int> mapActive{0}, maxMapActive{0}, openCount{0}, closeCount{0};
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool first(ecx_contextt* ctx) { return ctx->slavelist[1].eep_id == 1; }
}
// 用真实 SOEM 头文件检查 ABI，替换网络后端以确定性覆盖错误和并发路径。
extern "C" {
int ecx_init(ecx_contextt* ctx, const char* name) {
    ++openCount;
    ctx->userdata = reinterpret_cast<void*>(static_cast<std::uintptr_t>(name[0] == 'a' ? 1u : 2u));
    return std::strcmp(name, "bad") != 0;
}
void ecx_close(ecx_contextt*) { ++closeCount; }
int ecx_config_init(ecx_contextt* ctx) {
    ctx->slavecount = 1;
    auto& s = ctx->slavelist[1];
    s.eep_id = static_cast<uint32>(reinterpret_cast<std::uintptr_t>(ctx->userdata));
    std::strcpy(s.name, first(ctx) ? "first" : "second");
    s.state = EC_STATE_INIT; s.mbx_l = s.mbx_rl = 128;
    s.mbx_wo = 0x1000; s.mbx_ro = 0x1100;
    ctx->slavelist[0].state = EC_STATE_INIT;
    return 1;
}
int ecx_config_map_group(ecx_contextt* ctx, void* buffer, uint8) {
    const int active = ++mapActive;
    if (active > maxMapActive) maxMapActive = active;
    std::this_thread::sleep_for(5ms);
    auto& s = ctx->slavelist[1];
    s.hasdc = TRUE; s.Obits = s.Ibits = 16; s.Obytes = s.Ibytes = 2;
    s.outputs = static_cast<uint8*>(buffer); s.inputs = s.outputs + 2;
    ctx->grouplist[0].outputsWKC = ctx->grouplist[0].inputsWKC = 1;
    s.state = EC_STATE_SAFE_OP; ctx->slavelist[0].state = EC_STATE_SAFE_OP;
    --mapActive;
    return 4;
}
boolean ecx_configdc(ecx_contextt*) { return TRUE; }
void ecx_dcsync0(ecx_contextt*, uint16, boolean, uint32, int32) {}
void ecx_dcsync01(ecx_contextt*, uint16, boolean, uint32, uint32, int32) {}
int ecx_writestate(ecx_contextt* ctx, uint16 slave) {
    if (slave == 0) ctx->slavelist[1].state = ctx->slavelist[0].state;
    return 1;
}
uint16 ecx_statecheck(ecx_contextt* ctx, uint16 slave, uint16 wanted, int) {
    if(wanted==EC_STATE_PRE_OP&&!failPreOp)ctx->slavelist[slave].state=EC_STATE_PRE_OP;
    return ctx->slavelist[slave].state;
}
int ecx_readstate(ecx_contextt* ctx) {
    if (loseFirst && first(ctx)) ctx->slavelist[1].state = EC_STATE_NONE;
    ctx->slavelist[0].state = ctx->slavelist[1].state;
    return ctx->slavelist[0].state;
}
int ecx_send_processdata(ecx_contextt*) { return failSend ? -1 : 1; }
int ecx_receive_processdata(ecx_contextt* ctx, int) {
    if(failReceive)return -1;
    if (ctx->slavelist[1].inputs) {
        const uint16_t value=feedback.load();
        std::memcpy(ctx->slavelist[1].inputs,value?static_cast<const void*>(&value):ctx->slavelist[1].outputs,2);
    }
    return loseFirst && first(ctx) ? 0 : 3;
}
int ecx_recover_slave(ecx_contextt* ctx, uint16 id, int) {
    loseFirst = false; ctx->slavelist[id].state = EC_STATE_SAFE_OP; return 1;
}
int ecx_reconfig_slave(ecx_contextt* ctx, uint16 id, int) { ctx->slavelist[id].state = EC_STATE_SAFE_OP; return 1; }
int ecx_SDOread(ecx_contextt* ctx, uint16, uint16, uint8, boolean, int* size, void* data, int) {
    if(ctx->slavelist[1].state<EC_STATE_PRE_OP)return 0;
    if (*size < 4) return 0;
    std::memcpy(data, &ctx->slavelist[1].eep_id, 4); *size = 4; return 1;
}
int ecx_SDOwrite(ecx_contextt* ctx, uint16 slave, uint16, uint8, boolean, int, const void*, int) {
    if(failSdo){pendingSdoError=true;return 0;}
    return ctx->slavelist[slave].state>=EC_STATE_PRE_OP?1:0;
}
boolean ecx_poperror(ecx_contextt*,ec_errort* error) {
    if(!pendingSdoError.exchange(false))return FALSE;
    *error={};error->Slave=1;error->Index=0x1c12;error->Etype=EC_ERR_TYPE_SDO_ERROR;error->AbortCode=0x06010002;
    return TRUE;
}
uint32 ecx_readeeprom(ecx_contextt*, uint16, uint16 address, int) {
    if (eepromOwned) return eepromWords[address];
    if (address==0x003e) return 7;
    if (failBoot) return 0;
    return (128u << 16) | (address == ECT_SII_BOOTRXMBX ? 0x1200u : 0x1300u);
}
int ecx_writeeeprom(ecx_contextt*, uint16, uint16 address, uint16 value, int) { if(failEeprom)return 0;eepromWords[address]=value;return 1; }
int ecx_eeprom2master(ecx_contextt*, uint16) { eepromOwned=true;return 1; }
int ecx_eeprom2pdi(ecx_contextt*, uint16) { eepromOwned=false;return 1; }
int ecx_FPWR(ecx_portt*, uint16, uint16, uint16, void*, int) { return 1; }
int ecx_FOEwrite(ecx_contextt* ctx, uint16 id, char*, uint32, int size, void*, int) {
    if (ctx->FOEhook) ctx->FOEhook(id, 0, size);
    std::this_thread::sleep_for(40ms);
    if (ctx->FOEhook) ctx->FOEhook(id, 1, 0);
    return failFoe ? -5 : 1;
}
int ecx_FOEread(ecx_contextt* ctx, uint16 id, char*, uint32, int* size, void* data, int) {
    std::this_thread::sleep_for(20ms);
    if (failFoe || *size < 3) return -5;
    std::memcpy(data, "abc", 3); *size = 3;
    if (ctx->FOEhook) ctx->FOEhook(id, 1, 3);
    return 1;
}
}
int main() {
    try {
        {
            auto backend=LL::CreateEtherCATMaster({"mirror",LL::BusType::EtherCAT,"a",1000});
            check(backend->Scan()&&backend->Configure(),"backend configure failed");
            uint8_t* input=nullptr;uint8_t* output=nullptr;
            backend->VisitSlaves([&](int,void* native){auto& s=static_cast<ec_slavet*>(native)[1];input=s.inputs;output=s.outputs;return true;});
            check(backend->Start(false),"backend manual start failed");
            output[0]=0x56;output[1]=0x12;
            check(backend->Exchange()==3&&input[0]==0x56&&input[1]==0x12,"backend input mirror lags the received manual frame");
            backend->Stop();
            feedback=0x2345;
            check(backend->Start(true),"backend background start failed");
            const auto target=backend->Status().cycles+2;
            const auto deadline=std::chrono::steady_clock::now()+1s;
            while(backend->Status().cycles<target&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
            check(backend->RefreshInputs()&&input[0]==0x45&&input[1]==0x23,"background feedback never reached stable PDO mirror");
            check(output[0]==0x56&&output[1]==0x12,"refreshing input changed command outputs");
            backend->Stop();backend->Close();feedback=0;
        }
        {
            EtherCAT_Master notReady;check(notReady.Open("a"),"PRE_OP failure fixture open failed");
            failPreOp=true;check(notReady.ScanSlaves()<0,"scan accepted a slave that never reached PRE_OP");failPreOp=false;
            check(notReady.GetLastError().find("PRE_OP")!=std::string::npos,"PRE_OP failure has no state diagnostic");
            check(notReady.ConfigureProcessData()<0,"failed PRE_OP scan allowed PDO mapping");
        }
        {
            EtherCAT_Master invalid;
            check(!invalid.Open(""), "empty interface accepted");
            check(!invalid.Open("bad"), "failed NIC accepted");
            check(invalid.ScanSlaves() == -1, "closed scan accepted");
            check(!invalid.Start(), "unmapped start accepted");
        }
        EtherCAT_Master a, b;
        check(a.Open("a") && b.Open("b"), "open failed");
        check(a.ScanSlaves() == 1 && b.ScanSlaves() == 1, "scan failed");
        check(a.GetSlaves()[0].state==EC_STATE_PRE_OP,"scan did not refresh cached PRE_OP before SDO initialization");
        const uint32 zero=0;
        check(a.WriteSDO(1,0x1c12,0,&zero,4)>0,"PDO initialization blocked by stale slave state");
        failSdo=true;check(a.WriteSDO(1,0x1c12,0,&zero,4)<=0,"SDO failure fixture accepted");failSdo=false;
        check(a.GetLastError().find("code=0x6010002")!=std::string::npos,"SDO abort detail was discarded");
        auto mapA = std::async(std::launch::async, [&] { return a.ConfigureProcessData(); });
        auto mapB = std::async(std::launch::async, [&] { return b.ConfigureProcessData(); });
        check(mapA.get() == 4 && mapB.get() == 4 && maxMapActive == 1, "SOEM shared mapping workspace not protected");
        check(a.ConfigureSync0(1, true, 1000000), "SYNC0 configuration failed");
        const uint16 output = 42;
        check(a.WriteOutputs(1, &output, 2), "PDO write failed");
        check(!a.WriteOutputs(1, &output, 2, 1), "PDO boundary accepted");
        check(a.ExchangeProcessData() == 3 && b.ExchangeProcessData() == 3, "manual exchange failed");
        check(a.ReadInputs(1)[0] == 42 && b.ReadInputs(1)[0] == 0, "master PDO contamination");
        const auto initialStats=a.GetMonitorStatus();failSend=true;failReceive=true;a.ExchangeProcessData();failSend=false;failReceive=false;const auto failedStats=a.GetMonitorStatus();check(failedStats.sendErrors==initialStats.sendErrors+1&&failedStats.receiveErrors==initialStats.receiveErrors+1,"TX/RX failures were not counted independently");check(failedStats.sentFrames==initialStats.sentFrames,"Failed TX counted as sent frame");check(b.GetMonitorStatus().sendErrors==0,"TX errors leaked to another master");
        uint32 value = 0; int size = 4;
        check(a.ReadSDO(1, 0x1000, 0, &value, size) == 1 && value == 1, "SDO context wrong");
        size = 4;
        check(b.ReadSDO(1, 0x1000, 0, &value, size) == 1 && value == 2, "SDO context contamination");
        EtherCAT_Master::CycleOptions cycle;
        cycle.period = 1ms; cycle.monitorPeriod = 2ms; cycle.autoRecover = true;
        check(a.Start(cycle) && b.Start(cycle), "parallel start failed");
        std::this_thread::sleep_for(1050ms);
        check(a.GetMonitorStatus().framesPerSecond>0&&b.GetMonitorStatus().framesPerSecond>0,"Measured frame rate did not update");
        check(a.ExchangeProcessData() == -1 && a.ScanSlaves() == -1, "running master allowed reconfiguration");
        loseFirst = true;
        const auto recoveryDeadline = std::chrono::steady_clock::now() + 1s;
        do { std::this_thread::sleep_for(2ms); }
        while ((a.GetMonitorStatus().recoveryAttempts == 0 || !a.GetMonitorStatus().allOperational) &&
               std::chrono::steady_clock::now() < recoveryDeadline);
        check(a.GetMonitorStatus().wkcErrors > 0 && a.GetMonitorStatus().recoveryAttempts > 0, "loss/recovery not detected");
        check(a.GetMonitorStatus().allOperational, "recovery did not return to OP");
        check(b.GetMonitorStatus().wkcErrors == 0, "monitor state contaminated");
        check(a.ChangeSlaveState(1,EtherCAT_Master::State::SafeOp), "intentional SAFE_OP rejected");
        std::this_thread::sleep_for(8ms);
        a.PollMonitor();
        check(a.GetSlaves()[0].state==EC_STATE_SAFE_OP && !a.GetSlaves()[0].lost, "SAFE_OP treated as loss or auto-restored");
        check(a.ChangeSlaveState(1,EtherCAT_Master::State::Operational), "explicit OP request rejected");
        EtherCAT_Master::FoEOptions foe;
        std::vector<uint8_t> firmware{1, 2, 3, 4}, received{9};
        check(a.FoEWrite(1, "fw.bin", firmware, foe) < 0, "running FoE accepted");
        a.Stop();
        const auto before = b.GetMonitorStatus().cycles;
        foe.bootstrap = true;
        auto update = std::async(std::launch::async, [&] { return a.FoEWrite(1, "fw.bin", firmware, foe); });
        bool active = false;
        for (int i = 0; i < 100 && !active; ++i) { active = a.GetFoEStatus().active; std::this_thread::sleep_for(1ms); }
        check(active, "FoE progress not observable");
        check(update.get() > 0 && a.GetFoEStatus().succeeded && a.GetFoEStatus().transferredBytes == 4, "bootstrap write failed");
        check(b.GetMonitorStatus().cycles > before && b.IsRunning(), "one master FoE stopped another master");
        check(!a.GetMonitorStatus().mapped && !a.Start(), "bootstrap allowed stale PDO mapping");
        check(a.ConfigureProcessData() < 0, "bootstrap allowed mapping without rescan");
        check(a.ScanSlaves() == 1, "rescan failed");
        foe.bootstrap = false;
        check(a.FoERead(1, "log", received, 100, foe) > 0 && std::string(received.begin(), received.end()) == "abc", "FoE read failed");
        failFoe = true;
        check(a.FoERead(1, "log", received, 100, foe) <= 0 && std::string(received.begin(), received.end()) == "abc", "failed read overwrote caller data");
        foe.bootstrap = true;
        check(a.FoEWrite(1, "fw", firmware, foe) <= 0 && !a.GetFoEStatus().active && a.GetSlaves()[0].state == EC_STATE_INIT,
              "failed bootstrap transfer did not clean up");
        failFoe = false; failBoot = true;
        check(a.FoEWrite(1, "fw", firmware, foe) < 0 && !a.GetFoEStatus().active, "invalid BOOT mailbox accepted");
        failBoot = false;
        check(a.ScanSlaves()==1 && a.ConfigureProcessData()>0, "manual mode setup failed");
        EtherCAT_Master::CycleOptions manual;manual.background=false;
        check(a.Start(manual) && a.IsRunning() && a.ExchangeProcessData()==3, "manual controller cycle failed");
        check(a.ScanSlaves()<0, "manual active master allowed rescan");
        a.Stop();check(!a.IsRunning(), "manual master failed to stop");
        const auto siiPath=std::filesystem::temp_directory_path()/"llecp-sii-unit-test.bin";
        std::vector<std::uint8_t> image(128,0);image[14]=48;
        {std::ofstream output(siiPath,std::ios::binary);output.write(reinterpret_cast<const char*>(image.data()),static_cast<std::streamsize>(image.size()));}
        check(a.WriteSiiFile(1,siiPath.string())>0 && !eepromOwned && a.GetFoEStatus().succeeded, "SII EEPROM write/verify failed");
        failEeprom=true;
        check(a.WriteSiiFile(1,siiPath.string())<0 && !eepromOwned && !a.GetFoEStatus().active && !a.GetFoEStatus().succeeded, "EEPROM failure left ownership/progress active");
        failEeprom=false;
        image[14]^=1;
        {std::ofstream output(siiPath,std::ios::binary);output.write(reinterpret_cast<const char*>(image.data()),static_cast<std::streamsize>(image.size()));}
        check(a.WriteSiiFile(1,siiPath.string())<0, "invalid SII CRC accepted");
        std::filesystem::remove(siiPath);
        a.Close(); a.Close();
        check(b.IsRunning(), "closing A stopped B");
        b.Stop(); b.Close();
        check(openCount == closeCount, "NIC lifecycle imbalance");
        std::cout << "PASS: multi-master isolation, lifecycle, mapping lock, PDO/SDO, WKC recovery, intentional AL states, FoE and EEPROM cleanup\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: " << ex.what() << '\n'; return 1;
    }
}
