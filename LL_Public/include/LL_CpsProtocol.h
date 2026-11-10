#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cstdint>
namespace LL {
using Json = nlohmann::json;
inline constexpr const char *CpsCommand = "CPS_Request";
namespace CpsOp {
inline constexpr const char* PdoRead="pdo.read";
inline constexpr const char* PdoWrite="pdo.write";
inline constexpr const char *Hello = "hello";
inline constexpr const char *Status = "status";
inline constexpr const char *Devices = "devices.list";
inline constexpr const char *Scan = "master.scan";
inline constexpr const char *Apply = "project.apply";
inline constexpr const char *MasterStart = "master.start";
inline constexpr const char *MasterStop = "master.stop";
inline constexpr const char *Reset = "controller.reset";
inline constexpr const char *Emergency = "controller.emergency";
inline constexpr const char *EmergencyRelease = "controller.emergencyRelease";
inline constexpr const char *SlaveState = "slave.state";
inline constexpr const char *FileBegin = "file.begin";
inline constexpr const char *FileChunk = "file.chunk";
inline constexpr const char *FileCommit = "file.commit";
inline constexpr const char *FileRead = "file.read";
inline constexpr const char *FoE = "slave.foe";
inline constexpr const char *FoEProgress = "slave.foeProgress";
inline constexpr const char *Download = "plc.download";
inline constexpr const char *Build = "plc.build";
inline constexpr const char *BuildStatus = "plc.buildStatus";
inline constexpr const char *Clean = "plc.clean";
inline constexpr const char *Run = "plc.run";
inline constexpr const char *Stop = "plc.stop";
inline constexpr const char *PlcReset = "plc.reset";
inline constexpr const char *Breakpoint = "plc.breakpoint";
inline constexpr const char *Debug = "plc.debug";
inline constexpr const char *Variables = "variables.list";
inline constexpr const char *Read = "variables.read";
inline constexpr const char *Write = "variables.write";
inline constexpr const char *Force = "variables.force";
inline constexpr const char *Release = "variables.release";
inline constexpr const char *Subscribe = "scope.subscribe";
inline constexpr const char *Unsubscribe = "scope.unsubscribe";
inline constexpr const char *Axis = "axis.command";
} // namespace CpsOp
std::string Encode64(const std::string &value);
std::string Decode64(const std::string &value);
std::string CpsRequestFrame(const Json &request);
Json CpsResponse(const std::string &frame);
std::string CpsResponseFrame(const Json &response);
std::string NewId(const std::string &prefix = "p");
std::vector<std::uint8_t> HexBytes(const std::string &hex);
std::string BytesHex(const void *data, std::size_t size);
std::string ScalarToHex(const std::string &type, std::size_t size, const std::string &value);
std::string HexToScalar(const std::string &type, const std::string &hex);
double HexToNumber(const std::string &type, const std::string &hex);
} // namespace LL
