#pragma once
#include <string>
#include <cstdint>
int LL_GetSocketInfo(int socket, std::string& interfaceName, std::uint16_t& localPort,
                     std::uint16_t& remotePort, std::string& remoteIp, std::string& remoteMac);
