#include "LL_CpsProtocol.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <random>
#include <stdexcept>
#include <algorithm>
namespace LL {
std::string Encode64(const std::string &value) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((value.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < value.size(); i += 3) {
        const auto a = static_cast<unsigned char>(value[i]);
        const auto b = i + 1 < value.size() ? static_cast<unsigned char>(value[i + 1]) : 0;
        const auto c = i + 2 < value.size() ? static_cast<unsigned char>(value[i + 2]) : 0;
        const unsigned word = (a << 16) | (b << 8) | c;
        out += alphabet[(word >> 18) & 63];
        out += alphabet[(word >> 12) & 63];
        out += i + 1 < value.size() ? alphabet[(word >> 6) & 63] : '=';
        out += i + 2 < value.size() ? alphabet[word & 63] : '=';
    }
    return out;
}
std::string Decode64(const std::string &value) {
    if (value.size() % 4 || value.size() > 2 * 1024 * 1024)
        throw std::runtime_error("Invalid base64 length");
    std::array<int, 256> map{};
    map.fill(-1);
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (std::size_t i = 0; i < alphabet.size(); ++i)
        map[static_cast<unsigned char>(alphabet[i])] = static_cast<int>(i);
    std::string out;
    for (std::size_t i = 0; i < value.size(); i += 4) {
        unsigned word = 0;
        int padding = 0;
        for (int j = 0; j < 4; ++j) {
            const auto c = static_cast<unsigned char>(value[i + j]);
            if (c == '=') {
                if (j < 2 || i + 4 != value.size())
                    throw std::runtime_error("Invalid base64 padding");
                ++padding;
                word <<= 6;
            } else {
                if (padding || map[c] < 0)
                    throw std::runtime_error("Invalid base64 character");
                word = (word << 6) | static_cast<unsigned>(map[c]);
            }
        }
        if (padding > 2)
            throw std::runtime_error("Invalid base64 padding");
        out += static_cast<char>((word >> 16) & 255);
        if (padding < 2)
            out += static_cast<char>((word >> 8) & 255);
        if (!padding)
            out += static_cast<char>(word & 255);
    }
    return out;
}
std::string CpsRequestFrame(const Json &request) {
    const auto data = request.dump();
    if (data.size() > 512 * 1024)
        throw std::runtime_error("Request exceeds 512 KiB; use chunk transfer");
    return std::string(CpsCommand) + "," + Encode64(data) + ",;";
}
std::string CpsResponseFrame(const Json &response) {
    return std::string(CpsCommand) + ",OK," + Encode64(response.dump()) + ",;";
}
Json CpsResponse(const std::string &frame) {
    const auto prefix = std::string(CpsCommand) + ",OK,";
    if (frame.compare(0, prefix.size(), prefix) != 0)
        throw std::runtime_error("Controller does not support the LLCPS protocol, or rejected the request");
    auto tail = frame.substr(prefix.size());
    if (tail.size() < 2 || tail.substr(tail.size() - 2) != ",;")
        throw std::runtime_error("Malformed controller response");
    tail.resize(tail.size() - 2);
    return Json::parse(Decode64(tail));
}
std::string NewId(const std::string &prefix) {
    static std::atomic<std::uint64_t> counter{0};
    std::random_device random;
    std::ostringstream out;
    out << prefix << '_' << std::hex << std::chrono::high_resolution_clock::now().time_since_epoch().count()
        << ++counter << random();
    return out.str();
}
std::vector<std::uint8_t> HexBytes(const std::string &hex) {
    if (hex.size() % 2 || hex.size() > 2 * 1024 * 1024)
        throw std::runtime_error("Invalid hexadecimal data length");
    auto digit = [](char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        throw std::runtime_error("Invalid hexadecimal digit");
    };
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i < hex.size(); i += 2)
        out.push_back(static_cast<std::uint8_t>((digit(hex[i]) << 4) | digit(hex[i + 1])));
    return out;
}
std::string BytesHex(const void *data, std::size_t size) {
    constexpr char chars[] = "0123456789ABCDEF";
    const auto *bytes = static_cast<const unsigned char *>(data);
    std::string result;
    result.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        result += chars[bytes[i] >> 4];
        result += chars[bytes[i] & 15];
    }
    return result;
}
static std::string upper(std::string text) {
    for (auto &c : text)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}
std::string ScalarToHex(const std::string &rawType, std::size_t size, const std::string &value) {
    const auto type = upper(rawType);
    if (size == 0 || size > 1048576)
        throw std::runtime_error("Invalid variable size");
    if (value.rfind("HEX:", 0) == 0) {
        const auto bytes = HexBytes(value.substr(4));
        if (bytes.size() != size)
            throw std::runtime_error("Hexadecimal value does not match variable size");
        return BytesHex(bytes.data(), bytes.size());
    }
    std::vector<unsigned char> bytes(size, 0);
    if (type == "STRING" || type.rfind("STRING[", 0) == 0) {
        if (value.size() >= size)
            throw std::runtime_error("String exceeds target capacity");
        if (value.size() > 255)
            throw std::runtime_error("IEC STRING exceeds 255 bytes");
        bytes[0] = static_cast<unsigned char>(value.size());
        std::copy(value.begin(), value.end(), bytes.begin() + 1);
        return BytesHex(bytes.data(), bytes.size());
    }
    if (type == "BOOL") {
        if (size != 1)
            throw std::runtime_error("BOOL size mismatch");
        const auto text = upper(value);
        if (text != "TRUE" && text != "FALSE" && text != "1" && text != "0")
            throw std::runtime_error("BOOL expects TRUE/FALSE or 1/0");
        bytes[0] = (text == "TRUE" || text == "1") ? 1 : 0;
    } else if (type == "REAL" || type == "LREAL") {
        std::size_t end = 0;
        const auto number = std::stod(value, &end);
        if (end != value.size() || !std::isfinite(number))
            throw std::runtime_error("Expected a finite real number");
        if (type == "REAL" && size == 4) {
            const auto f = static_cast<float>(number);
            if (!std::isfinite(f))
                throw std::runtime_error("REAL overflow");
            std::memcpy(bytes.data(), &f, 4);
        } else if (type == "LREAL" && size == 8)
            std::memcpy(bytes.data(), &number, 8);
        else
            throw std::runtime_error("Floating-point size mismatch");
    } else {
        const bool signedType = type == "SINT" || type == "INT" || type == "DINT" || type == "LINT";
        const bool unsignedType = type == "USINT" || type == "UINT" || type == "UDINT" || type == "ULINT" ||
                                  type == "BYTE" || type == "WORD" || type == "DWORD" || type == "LWORD";
        if ((!signedType && !unsignedType) || size > 8)
            throw std::runtime_error("Aggregate or unsupported type; use HEX: followed by exact raw bytes");
        std::size_t end = 0;
        std::uint64_t number = 0;
        if (signedType) {
            const auto v = std::stoll(value, &end, 10);
            if (size < 8 &&
                (v < -(std::int64_t(1) << (size * 8 - 1)) || v > ((std::int64_t(1) << (size * 8 - 1)) - 1)))
                throw std::runtime_error("Signed integer overflow");
            number = static_cast<std::uint64_t>(v);
        } else {
            if (!value.empty() && value[0] == '-')
                throw std::runtime_error("Unsigned integer cannot be negative");
            number = std::stoull(value, &end, 10);
            if (size < 8 && number >= (std::uint64_t(1) << (size * 8)))
                throw std::runtime_error("Unsigned integer overflow");
        }
        if (end != value.size())
            throw std::runtime_error("Invalid integer");
        for (std::size_t i = 0; i < size; ++i)
            bytes[i] = static_cast<unsigned char>((number >> (i * 8)) & 255);
    }
    return BytesHex(bytes.data(), bytes.size());
}
std::string HexToScalar(const std::string &rawType, const std::string &hex) {
    auto bytes = HexBytes(hex);
    const auto type = upper(rawType);
    if (bytes.empty())
        return {};
    if (type == "BOOL")
        return bytes[0] ? "TRUE" : "FALSE";
    if (type == "STRING" || type.rfind("STRING[", 0) == 0)
        return std::string(bytes.begin() + 1,
                           bytes.begin() + 1 + std::min<std::size_t>(bytes[0], bytes.size() - 1));
    std::ostringstream out;
    out << std::setprecision(12);
    if (type == "REAL" && bytes.size() == 4) {
        float v;
        std::memcpy(&v, bytes.data(), 4);
        out << v;
        return out.str();
    }
    if (type == "LREAL" && bytes.size() == 8) {
        double v;
        std::memcpy(&v, bytes.data(), 8);
        out << v;
        return out.str();
    }
    const bool signedType = type == "SINT" || type == "INT" || type == "DINT" || type == "LINT";
    const bool integer = signedType || type == "USINT" || type == "UINT" || type == "UDINT" ||
                         type == "ULINT" || type == "BYTE" || type == "WORD" || type == "DWORD" ||
                         type == "LWORD";
    if (integer && bytes.size() <= 8) {
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < bytes.size(); ++i)
            value |= std::uint64_t(bytes[i]) << (i * 8);
        if (signedType) {
            if (bytes.size() < 8 && (bytes.back() & 0x80))
                value |= (~std::uint64_t(0)) << (bytes.size() * 8);
            out << static_cast<std::int64_t>(value);
        } else
            out << value;
        return out.str();
    }
    return "HEX:" + hex;
}
double HexToNumber(const std::string &type, const std::string &hex) {
    auto value = HexToScalar(type, hex);
    if (value == "TRUE")
        return 1;
    if (value == "FALSE")
        return 0;
    if (value.rfind("HEX:", 0) == 0)
        throw std::runtime_error("Scope accepts scalar numeric variables only");
    std::size_t end = 0;
    const auto number = std::stod(value, &end);
    if (end != value.size() || !std::isfinite(number))
        throw std::runtime_error("Variable is not a finite numeric scalar");
    return number;
}
} // namespace LL
