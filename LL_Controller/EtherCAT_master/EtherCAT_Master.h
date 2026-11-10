#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <functional>
#include <string>
#include <vector>

// 一个实例对应一张网卡。所有 SOEM 对象、PDO 和线程均由该实例持有。
class EtherCAT_Master
{
public:
    enum class State : std::uint16_t { Init = 1, PreOp = 2, Boot = 3, SafeOp = 4, Operational = 8 };
    struct CycleOptions {
        std::chrono::microseconds period{1000};
        std::chrono::milliseconds monitorPeriod{100};
        int receiveTimeoutUs = 2000;
        unsigned wkcErrorThreshold = 3;
        bool background = true; // false: 由控制器统一调度手动周期
        bool autoRecover = false; // 恢复 OP 会重新发送已有输出，须由应用显式启用
    };
    struct SlaveInfo {
        std::uint16_t id = 0, state = 0, alStatusCode = 0;
        std::uint32_t vendorId = 0, productCode = 0, revision = 0, serial = 0;
        std::string name;
        std::size_t inputBytes = 0, outputBytes = 0;
        std::uint8_t inputStartBit = 0, outputStartBit = 0;
        bool lost = false, hasDistributedClock = false;
    };
    struct MonitorStatus {
        bool opened = false, mapped = false, running = false, allOperational = false;
        int slaveCount = 0, expectedWkc = 0, actualWkc = 0;
        std::uint16_t masterState = 0;
        std::uint64_t cycles = 0, sendErrors = 0, wkcErrors = 0;
        std::uint64_t sentFrames = 0, receiveErrors = 0;
        double framesPerSecond = 0;
        std::uint64_t consecutiveWkcErrors = 0, missedDeadlines = 0, recoveryAttempts = 0;
        std::int64_t dcTimeNs = 0;
        std::chrono::microseconds lastCycleTime{0};
    };
    struct FoEStatus {
        bool active = false, writing = false, succeeded = false;
        std::size_t transferredBytes = 0, totalBytes = 0; // 读取时总量在完成后确定
        int result = 0;
    };
    struct FoEOptions {
        std::uint32_t password = 0;
        int timeoutUs = 2000000; // 每次邮箱交互超时，非整个文件的总时限
        bool bootstrap = false; // true: INIT -> BOOT，配置 SII 中的 BOOT 邮箱
    };

    EtherCAT_Master();
    ~EtherCAT_Master();
    EtherCAT_Master(const EtherCAT_Master&) = delete;
    EtherCAT_Master& operator=(const EtherCAT_Master&) = delete;
    bool Open(const char* interfaceName);
    void Close(); // 停止并回收该实例的周期线程后关闭网卡
    int ScanSlaves(); // 再次扫描会重新打开该网卡；运行中拒绝扫描
    int GetSlaveCount() const;
    bool IsOpen() const;
    bool IsRunning() const;
    std::string GetLastError() const;

    int ConfigureProcessData(bool distributedClocks = true); // 返回映射字节数，失败为 -1
    bool ConfigureSync0(std::uint16_t slave, bool enabled, std::uint32_t cycleNs, std::int32_t shiftNs = 0);
    bool ConfigureSync01(std::uint16_t slave, std::uint32_t cycle0Ns, std::uint32_t cycle1Ns, std::int32_t shiftNs = 0);
    bool RequestState(std::uint16_t slave, State state, int timeoutUs = 2000000);
    bool Start(const CycleOptions& options);
    bool Start();
    bool ChangeSlaveState(std::uint16_t slave, State state, int timeoutUs = 2000000);
    bool VisitNativeSlaves(const std::function<bool(int, void*)>& visitor);
    void Stop(); // 停止周期并请求 SAFE_OP；不会关闭网卡
    int ExchangeProcessData(int timeoutUs = 2000); // 手动周期接口，后台运行时拒绝
    bool PollMonitor(); // 刷新从站状态；不触发自动恢复
    MonitorStatus GetMonitorStatus() const;
    std::vector<SlaveInfo> GetSlaves() const;

    // 按从站复制 PDO，避免向调用方暴露可失效指针；位偏移见 SlaveInfo。
    bool WriteOutputs(std::uint16_t slave, const void* data, std::size_t size, std::size_t offset = 0);
    std::vector<std::uint8_t> ReadInputs(std::uint16_t slave) const;
    int ReadSDO(std::uint16_t slave, std::uint16_t index, std::uint8_t subIndex,
                void* data, int& size, bool completeAccess = false, int timeoutUs = 700000);
    int WriteSDO(std::uint16_t slave, std::uint16_t index, std::uint8_t subIndex,
                 const void* data, int size, bool completeAccess = false, int timeoutUs = 700000);

    // 同步调用；必须先 Stop()。默认要求从站处于 PRE_OP 或 BOOT。
    // bootstrap 传输结束（含失败）后请求 INIT，必须重新扫描、映射后才能 Start。
    int FoEWrite(std::uint16_t slave, const std::string& remoteName,
                 const std::vector<std::uint8_t>& data, const FoEOptions& options);
    int FoERead(std::uint16_t slave, const std::string& remoteName,
                std::vector<std::uint8_t>& data, std::size_t maxBytes, const FoEOptions& options);
    int FoEWriteFile(std::uint16_t slave, const std::string& localPath,
                     const std::string& remoteName, const FoEOptions& options);
    int FoEReadFile(std::uint16_t slave, const std::string& localPath,
                    const std::string& remoteName, std::size_t maxBytes, const FoEOptions& options);
    int WriteSiiFile(std::uint16_t slave, const std::string& localPath);
    FoEStatus GetFoEStatus() const; // 传输期间可从其他线程读取

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
