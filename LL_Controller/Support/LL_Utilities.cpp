#include "LL_String.h"
#include "LL_Process.h"
#include "LL_netplan.h"
#include <chrono>
#include <cmath>
#include <charconv>
#include <system_error>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
int LL_SplitString(const std::string& text, const std::string& mark, std::vector<std::string>& out) {
    out.clear(); if (mark.empty()) return -1;
    std::size_t start = 0;
    for (;;) {
        auto pos = text.find(mark, start);
        out.push_back(text.substr(start, pos == std::string::npos ? pos : pos - start));
        if (pos == std::string::npos) break;
        start = pos + mark.size();
    }
    return static_cast<int>(out.size());
}
bool LL_isInt(const std::string& text) {
    if (text.empty()) return false;
    long long value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}
unsigned long long LL_GetCurUS() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
unsigned long long LL_GetCurMS() { return LL_GetCurUS() / 1000; }
int LL_GetSocketInfo(int socket, std::string& interfaceName, std::uint16_t& localPort,
                     std::uint16_t& remotePort, std::string& remoteIp, std::string& remoteMac) {
    sockaddr_in local{}, remote{}; socklen_t length = sizeof(local);
    interfaceName.clear(); remoteMac.clear(); remoteIp.clear(); localPort = remotePort = 0;
    if (getsockname(socket, reinterpret_cast<sockaddr*>(&local), &length) != 0) return -1;
    length = sizeof(remote);
    if (getpeername(socket, reinterpret_cast<sockaddr*>(&remote), &length) != 0) return -1;
    char address[INET_ADDRSTRLEN]{};
    if (!inet_ntop(AF_INET, &remote.sin_addr, address, sizeof(address))) return -1;
    localPort = ntohs(local.sin_port); remotePort = ntohs(remote.sin_port); remoteIp = address;
    return 0;
}

bool LL_Str2Int(const std::string& text, int& value) {
    if(text.empty())return false;
    int parsed=0;const auto result=std::from_chars(text.data(),text.data()+text.size(),parsed);
    if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size())return false;
    value=parsed;return true;
}
bool LL_Str2Num(const std::string& text, double& value) {
    try{std::size_t end=0;const double parsed=std::stod(text,&end);if(end!=text.size()||!std::isfinite(parsed))return false;value=parsed;return true;}catch(...){return false;}
}
