#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cstddef>
namespace LL {
struct Command { std::string name; std::vector<std::string> parameters; };
inline constexpr std::size_t MaxCommandBytes = 1024 * 1024;
// 保持参考控制器的 command,param,...,; 和 command,OK,...,; 格式。
bool DecodeCommand(const std::string& frame, Command& command);
std::string EncodeCommand(const Command& command);
std::string EncodeResponse(const std::string& name, const std::vector<std::string>& result);
class FrameBuffer {
public:
    bool Append(const std::string& bytes, std::vector<std::string>& frames);
private:
    std::string pending;
};
}
