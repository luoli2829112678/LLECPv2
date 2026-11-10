#include "LL_CommandCodec.h"
#include <sstream>
#include <limits>
namespace LL {
bool DecodeCommand(const std::string& frame, Command& command) {
    command = {};
    if (frame.empty() || frame.size() > MaxCommandBytes || frame.find('\0') != std::string::npos) return false;
    std::string text = frame;
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) text.pop_back();
    if (!text.empty() && text.back() == ';') text.pop_back();
    if (!text.empty() && text.back() == ',') text.pop_back();
    const auto first = text.find(',');
    command.name = text.substr(0, first);
    if (command.name.empty() || command.name.find_first_of(";\r\n") != std::string::npos) return false;
    if (first == std::string::npos) return true;
    std::size_t pos = first + 1;
    while (pos <= text.size()) {
        const auto next = text.find(',', pos);
        auto value = text.substr(pos, next == std::string::npos ? next : next - pos);
        if (value.empty()) return false;
        command.parameters.push_back(std::move(value));
        if (next == std::string::npos) break;
        pos = next + 1;
    }
    return true;
}
std::string EncodeCommand(const Command& command) {
    std::string frame = command.name;
    for (const auto& value : command.parameters) frame += ',' + value;
    return frame + ",;";
}
std::string EncodeResponse(const std::string& name, const std::vector<std::string>& result) {
    if (result.empty()) return name + ",Fail,-1,;";
    const bool success = result[0] == "0";
    std::string frame = name + (success ? ",OK" : ",Fail");
    for (std::size_t i = success ? 1 : 0; i < result.size(); ++i) frame += ',' + result[i];
    return frame + ",;";
}
bool FrameBuffer::Append(const std::string& bytes, std::vector<std::string>& frames) {
    frames.clear();
    if (bytes.size() > MaxCommandBytes || pending.size() > MaxCommandBytes - bytes.size()) return false;
    pending += bytes;
    for (;;) {
        const auto end = pending.find(';');
        if (end == std::string::npos) break;
        frames.push_back(pending.substr(0, end + 1));
        pending.erase(0, end + 1);
        while (!pending.empty() && (pending[0] == '\r' || pending[0] == '\n')) pending.erase(0, 1);
    }
    return true;
}
}
