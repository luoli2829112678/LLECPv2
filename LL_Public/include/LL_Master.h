#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace LL {
enum class BusType { EtherCAT, Profinet, Modbus };
struct MasterConfig {
    std::string id;
    BusType type = BusType::EtherCAT;
    std::string endpoint;
    std::uint32_t cycleUs = 1000;
};
struct SlaveState {
    std::uint16_t id = 0, state = 0, alStatusCode = 0;
    std::uint32_t productCode = 0, revision = 0;
    std::string name;
    bool connected = false; // 是否实际响应；PRE_OP/SAFE_OP 本身不表示断联。
    std::uint32_t vendorId = 0;
};
struct MasterStatus {
    bool opened = false, running = false, connected = false;
    int expectedWkc = 0, actualWkc = 0;
    std::uint64_t cycles = 0, wkcErrors = 0, missedDeadlines = 0;
    std::uint64_t sentFrames = 0, sendErrors = 0, receiveErrors = 0;
    double framesPerSecond = 0;
    std::vector<SlaveState> slaves;
};
struct FileTransferStatus {
    bool active = false, succeeded = false;
    std::size_t transferredBytes = 0, totalBytes = 0;
    int result = 0;
};
// 控制器只依赖此契约。未安装的协议由注册工厂明确拒绝，不静默退化。
class IMaster {
public:
    virtual ~IMaster() = default;
    virtual const MasterConfig& Config() const = 0;
    virtual bool Scan() = 0;
    virtual bool Configure() = 0;
    virtual bool Start(bool background = true) = 0;
    virtual void Stop() = 0;
    virtual void Close() = 0;
    virtual int Exchange() = 0;
    // Refresh input mirrors without sending or changing output data.
    virtual bool RefreshInputs() { return true; }
    virtual bool Poll() = 0;
    virtual MasterStatus Status() const = 0;
    virtual std::string LastError() const = 0;
    virtual bool ConfigureSync0(std::uint16_t, bool enabled, std::uint32_t, std::int32_t) { return !enabled; }
    virtual bool ConfigureSync01(std::uint16_t id, std::uint32_t cycle0, std::uint32_t cycle1, std::int32_t shift) {
        return cycle1 == 0 && ConfigureSync0(id, true, cycle0, shift);
    }
    virtual bool ChangeSlaveState(std::uint16_t slave, std::uint16_t state) = 0;
    virtual int ReadSDO(std::uint16_t slave, std::uint16_t index, std::uint8_t sub,
                        void* data, int& size, bool ca, int timeout) = 0;
    virtual int WriteSDO(std::uint16_t slave, std::uint16_t index, std::uint8_t sub,
                         const void* data, int size, bool ca, int timeout) = 0;
    virtual int WriteFile(std::uint16_t slave, const std::string& local,
                          const std::string& remote, std::uint32_t password) = 0;
    virtual int ReadFile(std::uint16_t slave, const std::string& local,
                         const std::string& remote, std::uint32_t password) = 0;
    virtual FileTransferStatus FileStatus() const = 0;
    virtual int WriteSiiFile(std::uint16_t, const std::string&) { return -1; }
    // EtherCAT 原生描述符只在控制器已停止且 callback 持锁期间访问。
    virtual bool VisitSlaves(const std::function<bool(int, void*)>&) { return false; }
};
using MasterFactory = std::function<std::shared_ptr<IMaster>(const MasterConfig&)>;
}
