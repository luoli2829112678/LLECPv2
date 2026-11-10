#pragma once
#include "LL_CpsData.h"
namespace LL {
std::string PdoKey(bool rx, unsigned pdo, unsigned entry);
std::string ReadPdoBits(const void *memory, std::size_t bytes, std::size_t offset, unsigned bits);
void WritePdoBits(void *memory, std::size_t bytes, std::size_t offset, unsigned bits, const std::string &hex);
std::string PdoValue(const Json &entry, const std::string &hex);
std::string PdoHex(const Json &entry, const std::string &value);
void AutoMapAxis(Json &axis, const Json &slave);
void SyncAxisMapping(Json &axis, bool enableWhenComplete = false);
std::vector<int> DetectAxisSlots(const Json &slave);
} // namespace LL
