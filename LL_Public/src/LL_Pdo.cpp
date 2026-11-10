#include "LL_Pdo.h"
#include <algorithm>
#include <cctype>
#include <limits>
#include <set>
namespace LL {
std::string PdoKey(bool rx, unsigned pdo, unsigned entry) {
    return std::string(rx ? "rx:" : "tx:") + std::to_string(pdo) + ":" + std::to_string(entry);
}
static void bounds(const void *p, std::size_t bytes, std::size_t offset, unsigned bits) {
    if (!p || bits == 0 || bits > 8192 || offset > bytes * 8 || bits > bytes * 8 - offset)
        throw std::runtime_error("PDO bit range exceeds mapped process image");
}
std::string ReadPdoBits(const void *p, std::size_t bytes, std::size_t offset, unsigned bits) {
    bounds(p, bytes, offset, bits);
    auto *src = static_cast<const std::uint8_t *>(p);
    std::vector<std::uint8_t> out((bits + 7) / 8, 0);
    for (unsigned i = 0; i < bits; ++i)
        out[i / 8] |=
            static_cast<std::uint8_t>(((src[(offset + i) / 8] >> ((offset + i) % 8)) & 1u) << (i % 8));
    return BytesHex(out.data(), out.size());
}
void WritePdoBits(void *p, std::size_t bytes, std::size_t offset, unsigned bits, const std::string &hex) {
    bounds(p, bytes, offset, bits);
    auto raw = HexBytes(hex);
    if (raw.size() != (bits + 7) / 8 || (bits % 8 && raw.back() >> (bits % 8)))
        throw std::runtime_error("Value exceeds PDO bit width");
    auto *dst = static_cast<std::uint8_t *>(p);
    for (unsigned i = 0; i < bits; ++i) {
        auto &b = dst[(offset + i) / 8];
        const auto mask = static_cast<std::uint8_t>(1u << ((offset + i) % 8));
        b = static_cast<std::uint8_t>((b & ~mask) | (((raw[i / 8] >> (i % 8)) & 1u) ? mask : 0));
    }
}
static std::string iec(const Json &e) {
    auto t = e.value("type", std::string{});
    for (auto &c : t)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    static const std::map<std::string, std::string> aliases = {
        {"BIT", "BOOL"},         {"INTEGER8", "SINT"},   {"INTEGER16", "INT"},   {"INTEGER32", "DINT"},
        {"INTEGER64", "LINT"},   {"UNSIGNED8", "USINT"}, {"UNSIGNED16", "UINT"}, {"UNSIGNED32", "UDINT"},
        {"UNSIGNED64", "ULINT"}, {"INT8", "SINT"},       {"INT16", "INT"},       {"INT32", "DINT"},
        {"INT64", "LINT"},       {"UINT8", "USINT"},     {"UINT16", "UINT"},     {"UINT32", "UDINT"},
        {"UINT64", "ULINT"},     {"REAL32", "REAL"},     {"REAL64", "LREAL"}};
    auto a = aliases.find(t);
    if (a != aliases.end())
        t = a->second;
    if (t.empty())
        t = e.value("bits", 0) == 1 ? "BOOL" : "RAW";
    return t;
}
std::string PdoValue(const Json &e, const std::string &hex) {
    auto raw = HexBytes(hex);
    const auto bits = e.at("bits").get<unsigned>();
    const auto t = iec(e);
    if ((t == "SINT" || t == "INT" || t == "DINT" || t == "LINT") && bits % 8 && bits &&
        raw.size() == (bits + 7) / 8 && (raw.back() & (1u << ((bits - 1) % 8))))
        raw.back() |= static_cast<std::uint8_t>(0xffu << (bits % 8));
    return HexToScalar(t, BytesHex(raw.data(), raw.size()));
}
std::string PdoHex(const Json &e, const std::string &value) {
    const auto bits = e.at("bits").get<unsigned>();
    if (!bits || bits > 8192)
        throw std::runtime_error("Unsupported PDO width");
    auto t = iec(e);
    auto raw = HexBytes(ScalarToHex(t, (bits + 7) / 8, value));
    if (bits % 8) {
        const unsigned mask = (1u << (bits % 8)) - 1;
        const bool sign = t == "SINT" || t == "INT" || t == "DINT" || t == "LINT";
        if (value.rfind("HEX:", 0) != 0 && sign) {
            std::size_t end = 0;
            const auto n = std::stoll(value, &end);
            if (end != value.size() || n < -(std::int64_t(1) << (bits - 1)) ||
                n > ((std::int64_t(1) << (bits - 1)) - 1))
                throw std::runtime_error("Signed value exceeds PDO width");
            raw.back() &= static_cast<std::uint8_t>(mask);
        } else if (raw.back() & ~mask)
            throw std::runtime_error("Value exceeds PDO width");
    }
    return BytesHex(raw.data(), raw.size());
}
static std::vector<Json> candidates(const Json &slave, bool rx, unsigned index, unsigned sub, unsigned bits) {
    std::vector<Json> out;
    std::size_t offset = 0;
    for (const auto &p : slave.at(rx ? "rxPdos" : "txPdos")) {
        if (!p.value("selected", false))
            continue;
        for (const auto &e : p.at("entries")) {
            if (!e.value("padding", false) && e.value("index", 0u) == index &&
                e.value("subIndex", 0u) == sub && e.value("bits", 0u) == bits && offset % 8 == 0)
                out.push_back({{"index", index}, {"subIndex", sub}, {"pdo", p.at("index")}});
            offset += e.at("bits").get<unsigned>();
        }
    }
    return out;
}
void SyncAxisMapping(Json &axis, bool enable) {
    const auto &m = axis.at("mapping");
    const bool complete = !m.at("Controlword").is_null() && !m.at("StatusWord").is_null();
    axis["rxPdo"] = m.at("Controlword").is_null() ? Json(0) : m.at("Controlword").at("pdo");
    axis["txPdo"] = m.at("StatusWord").is_null() ? Json(0) : m.at("StatusWord").at("pdo");
    if (!complete)
        axis["enabled"] = false;
    else if (enable)
        axis["enabled"] = true;
}
void AutoMapAxis(Json &axis, const Json &slave) {
    auto &mapping = axis["mapping"];
    const int slot = axis.value("slot", 0);
    if (slot < 0 || slot > 15)
        throw std::runtime_error("Axis channel must be 0..15");
    for (const auto &f : AxisFields()) {
        auto found = candidates(slave, f.rx, f.index + slot * 0x800,
                                std::string(f.name) == "DigitalOutputs" ? 1 : 0, f.bits);
        mapping[f.name] = found.size() == 1 ? found.front() : Json();
    }
    SyncAxisMapping(axis, true);
}
std::vector<int> DetectAxisSlots(const Json &slave) {
    std::vector<int> slots;
    for (int slot = 0; slot < 16; ++slot)
        if (candidates(slave, true, 0x6040 + slot * 0x800, 0, 16).size() == 1 &&
            candidates(slave, false, 0x6041 + slot * 0x800, 0, 16).size() == 1)
            slots.push_back(slot);
    return slots;
}
} // namespace LL
