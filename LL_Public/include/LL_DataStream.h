#pragma once
#include "LL_CpsProtocol.h"
#include <chrono>
#include <deque>
#include <mutex>
namespace LL {
// Existing controller realtime-data port; this does not add a listener.
inline constexpr unsigned CpsDataPort = 8893;
namespace DataCommand {
inline constexpr const char *Attach = "DataAttach";
inline constexpr const char *Batch = "DataBatch";
inline constexpr const char *Ack = "DataAck";
inline constexpr const char *Type = "LLECP.data.v1";
} // namespace DataCommand
inline std::uint32_t DataCrc32(const std::string &bytes) {
    std::uint32_t crc = 0xffffffffu;
    for (unsigned char byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
// Hash the exact transmitted bytes, including session, sequence and sample data.
inline Json SealDataBatch(const Json &batch) {
    const auto payload = batch.dump();
    return {{"payload", payload}, {"crc32", DataCrc32(payload)}};
}
inline Json OpenDataBatch(const Json &envelope) {
    const auto &payload = envelope.at("payload").get_ref<const std::string &>();
    if (DataCrc32(payload) != envelope.at("crc32").get<std::uint32_t>())
        throw std::runtime_error("Data batch CRC32 mismatch");
    return Json::parse(payload);
}
class DataStream {
    mutable std::mutex mutex_;
    Json filling_ = Json::array();
    std::deque<Json> pending_;
    std::uint64_t next_ = 1, dropped_ = 0;
    std::size_t bytes_ = 0;
    bool finished_ = false;
    std::chrono::steady_clock::time_point touched_ = std::chrono::steady_clock::now();
    std::chrono::milliseconds timeout_;
    void flush() {
        if (filling_.empty())
            return;
        if (pending_.size() >= 64) {
            dropped_ += pending_.front().at("frames").size();
            pending_.pop_front();
        }
        pending_.push_back({{"type", DataCommand::Type},
                            {"session", session},
                            {"batch", next_++},
                            {"frames", std::move(filling_)}});
        filling_ = Json::array();
        bytes_ = 0;
    }

  public:
    const std::string session;
    explicit DataStream(std::string id, std::chrono::milliseconds timeout = std::chrono::seconds(30))
        : timeout_(timeout), session(std::move(id)) {}
    void Push(Json frame) {
        const auto size = frame.dump().size();
        std::lock_guard<std::mutex> lock(mutex_);
        if (finished_)
            return;
        if (bytes_ + size > 300000)
            flush();
        bytes_ += size;
        filling_.push_back(std::move(frame));
        if (filling_.size() == 100)
            flush();
    }
    Json Peek(std::uint64_t last = 0, bool resend = true) {
        std::lock_guard<std::mutex> lock(mutex_);
        // A running stream sends full 100-sample batches. Finish flushes the final tail.
        if (pending_.empty())
            return Json();
        if (!resend && pending_.front().at("batch") == last)
            return Json();
        auto result = pending_.front();
        result["droppedSamples"] = dropped_;
        return result;
    }
    void Ack(std::uint64_t sequence) {
        std::lock_guard<std::mutex> lock(mutex_);
        touched_ = std::chrono::steady_clock::now();
        if (!pending_.empty() && pending_.front().at("batch") == sequence)
            pending_.pop_front();
    }
    void Finish() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (finished_)
            return;
        flush();
        finished_ = true;
        pending_.push_back({{"type", DataCommand::Type},
                            {"session", session},
                            {"batch", next_++},
                            {"frames", Json::array()},
                            {"finished", true}});
    }
    bool Expired() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return (finished_ && pending_.empty()) || std::chrono::steady_clock::now() - touched_ > timeout_;
    }
};
} // namespace LL
