#include "ControllerClient.h"
#include "LL_DataStream.h"
#include "LL_CommandCodec.h"
#include <chrono>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using DataSocket = SOCKET;
static void closeData(DataSocket fd) {
    closesocket(fd);
}
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>
using DataSocket = int;
static void closeData(DataSocket fd) {
    close(fd);
}
#endif
namespace LL {
struct ControllerClient::StreamWorker {
    std::string session;
    std::atomic<bool> stop{false};
    std::atomic<bool> ended{false};
    std::atomic<std::intptr_t> socket{-1};
    std::mutex socketMutex;
    std::thread thread;
};
void ControllerClient::CloseDataStream(const std::string &token) {
    std::shared_ptr<StreamWorker> worker;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = streams_.find(token);
        if (it == streams_.end())
            return;
        worker = it->second;
        streams_.erase(it);
    }
    worker->stop = true;
    {
        std::lock_guard<std::mutex> lock(worker->socketMutex);
        const auto socket = worker->socket.load();
        if (socket != -1)
            shutdown(static_cast<DataSocket>(socket), 2);
    }
    if (worker->thread.joinable())
        worker->thread.join();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        dataLatest_.erase(token);
        dataErrors_.erase(token);
    }
}
void ControllerClient::StopDataStreams() {
    for (;;) {
        std::string token;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (streams_.empty())
                return;
            token = streams_.begin()->first;
        }
        CloseDataStream(token);
    }
}
Json ControllerClient::DrainData(const std::string &token) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = dataLatest_.find(token);
    if (it == dataLatest_.end())
        return Json();
    auto result = std::move(it->second);
    dataLatest_.erase(it);
    return result;
}
std::string ControllerClient::DataError(const std::string &token) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = dataErrors_.find(token);
    return it == dataErrors_.end() ? "" : it->second;
}
bool ControllerClient::DataEnded(const std::string &token) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = streams_.find(token);
    return it != streams_.end() && it->second->ended;
}
Json ControllerClient::DataStats(const std::string &token) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = dataStats_.find(token);
    return it == dataStats_.end() ? Json::object() : it->second;
}
void ControllerClient::OpenDataStream(const std::string &token, const std::string &session, unsigned port,
                                      bool numeric) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = streams_.find(token);
        if (it != streams_.end() && it->second->session == session)
            return;
    }
    CloseDataStream(token);
    if (port == 0 || port > 65535)
        throw std::runtime_error("Invalid data channel port");
    auto worker = std::make_shared<StreamWorker>();
    worker->session = session;
    const auto ip = Endpoint();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        streams_[token] = worker;
        dataLatest_.erase(token);
        dataErrors_.erase(token);
        dataStats_[token] = {{"received", 0ull},
                             {"missed", 0ull},
                             {"discarded", 0ull},
                             {"replayed", 0ull},
                             {"crcErrors", 0ull}};
        if (numeric)
            samples_.clear();
    }
    worker->thread = std::thread([this, worker, ip, port, token, numeric] {
        std::uint64_t accepted = 0, dropped = 0, lastSample = 0;
        while (!worker->stop) {
            DataSocket fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd == static_cast<DataSocket>(-1))
                break;
            {
                std::lock_guard<std::mutex> lock(worker->socketMutex);
                worker->socket = static_cast<std::intptr_t>(fd);
            }
            try {
                sockaddr_in address{};
                address.sin_family = AF_INET;
                address.sin_port = htons(static_cast<unsigned short>(port));
                if (inet_pton(AF_INET, ip.c_str(), &address.sin_addr) != 1)
                    throw std::runtime_error("Invalid controller IP");
#ifdef _WIN32
                u_long mode = 1;
                ioctlsocket(fd, FIONBIO, &mode);
#else
                const int flags = fcntl(fd, F_GETFL, 0);
                fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
                const int rc = connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address));
                if (rc != 0) {
                    bool ready = false;
                    for (int i = 0; i < 30 && !worker->stop; ++i) {
                        fd_set writes;
                        FD_ZERO(&writes);
                        FD_SET(fd, &writes);
                        timeval timeout{0, 100000};
                        if (select(static_cast<int>(fd + 1), nullptr, &writes, nullptr, &timeout) > 0) {
                            ready = true;
                            break;
                        }
                    }
                    if (!ready)
                        throw std::runtime_error("Data channel connection timeout");
                    int error = 0;
#ifdef _WIN32
                    int length = sizeof(error);
#else
                    socklen_t length = sizeof(error);
#endif
                    getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &length);
                    if (error)
                        throw std::runtime_error("Cannot connect realtime data port");
                }
#ifdef _WIN32
                mode = 0;
                ioctlsocket(fd, FIONBIO, &mode);
                DWORD timeout = 1000;
                setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout),
                           sizeof(timeout));
#else
                fcntl(fd, F_SETFL, flags);
                timeval timeout{1, 0};
                setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
                auto sendFrame = [&](const Command &command) {
                    const auto text = EncodeCommand(command);
                    std::size_t offset = 0;
                    while (offset < text.size()) {
#ifdef _WIN32
                        const int n =
                            send(fd, text.data() + offset, static_cast<int>(text.size() - offset), 0);
#else
                        const int n = send(fd, text.data() + offset, text.size() - offset, MSG_NOSIGNAL);
#endif
                        if (n <= 0)
                            throw std::runtime_error("Data channel ACK/send failed");
                        offset += n;
                    }
                };
                int noDelay = 1;
                setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&noDelay),
                           sizeof(noDelay));
                sendFrame({DataCommand::Attach, {worker->session}});
                FrameBuffer buffer;
                while (!worker->stop) {
                    fd_set reads;
                    FD_ZERO(&reads);
                    FD_SET(fd, &reads);
                    timeval timeout{0, 200000};
                    const int ready = select(static_cast<int>(fd + 1), &reads, nullptr, nullptr, &timeout);
                    if (ready < 0)
                        throw std::runtime_error("Data channel select failed");
                    if (!ready)
                        continue;
                    char bytes[16384];
                    const int n = recv(fd, bytes, sizeof(bytes), 0);
                    if (n <= 0)
                        throw std::runtime_error("Data channel disconnected");
                    std::vector<std::string> frames;
                    if (!buffer.Append(std::string(bytes, n), frames))
                        throw std::runtime_error("Data batch exceeds limit");
                    for (const auto &frame : frames) {
                        Command command;
                        if (!DecodeCommand(frame, command) || command.name != DataCommand::Batch ||
                            command.parameters.size() != 1)
                            throw std::runtime_error("Invalid data batch");
                        Json batch;
                        try {
                            batch = OpenDataBatch(Json::parse(Decode64(command.parameters[0])));
                        } catch (...) {
                            std::lock_guard<std::mutex> lock(mutex_);
                            auto &stat = dataStats_[token];
                            stat["crcErrors"] = stat.value("crcErrors", 0ull) + 1;
                            dataErrors_[token] = "数据包校验失败，等待重传";
                            continue; // Never ACK corrupt data; sender retries the retained batch.
                        }
                        if (batch.value("type", std::string{}) != DataCommand::Type)
                            throw std::runtime_error("Unsupported data protocol");
                        if (batch.at("session") != worker->session)
                            throw std::runtime_error("Data session mismatch");
                        if (!batch.at("frames").is_array() || batch.at("frames").size() > 100)
                            throw std::runtime_error("Invalid batch frame count");
                        const auto sequence = batch.at("batch").get<std::uint64_t>();
                        if (sequence > accepted) {
                            std::lock_guard<std::mutex> lock(mutex_);
                            std::string acquisitionError;
                            if (numeric && samples_.size() + batch.at("frames").size() > 10000) {
                                const auto count = samples_.size() + batch.at("frames").size() - 10000;
                                samples_.erase(samples_.begin(), samples_.begin() + count);
                                lost_ += count;
                                auto &stat = dataStats_[token];
                                stat["clientDropped"] = stat.value("clientDropped", 0ull) + count;
                            }
                            const auto total = batch.value("droppedSamples", 0ull);
                            if (total > dropped)
                                lost_ += total - dropped;
                            dropped = total;
                            auto &stat = dataStats_[token];
                            stat["discarded"] = total;
                            for (const auto &row : batch.at("frames")) {
                                const auto sampleSequence = row.at("sequence").get<std::uint64_t>();
                                // Controller deadline misses are separate from transport buffer loss.
                                stat["missed"] = row.value("missedSamples", stat.value("missed", 0ull));
                                if (lastSample && sampleSequence <= lastSample)
                                    throw std::runtime_error("Data sample sequence is not increasing");
                                lastSample = sampleSequence;
                                stat["received"] = stat.value("received", 0ull) + 1;
                                stat["lastTimestampUs"] = row.at("timestamp_us");
                                if (!stat.contains("firstTimestampUs"))
                                    stat["firstTimestampUs"] = row.at("timestamp_us");
                                dataLatest_[token] = row.at("values");
                                for (auto value = row.at("values").begin(); value != row.at("values").end();
                                     ++value)
                                    if (value.value().contains("error"))
                                        acquisitionError =
                                            value.key() + ": " + value.value().at("error").get<std::string>();
                                if (numeric) {
                                    ScopeSample sample;
                                    sample.timestampUs = row.at("timestamp_us");
                                    sample.sequence = row.at("sequence");
                                    for (auto value = row.at("values").begin();
                                         value != row.at("values").end(); ++value)
                                        if (value.value().contains("number") &&
                                            value.value()["number"].is_number())
                                            sample.values[value.key()] =
                                                value.value()["number"].get<double>();
                                    samples_.push_back(std::move(sample));
                                }
                            }
                            if (acquisitionError.empty())
                                dataErrors_.erase(token);
                            else
                                dataErrors_[token] = acquisitionError;
                            accepted = sequence;
                        } else {
                            std::lock_guard<std::mutex> lock(mutex_);
                            auto &stat = dataStats_[token];
                            stat["replayed"] = stat.value("replayed", 0ull) + 1;
                        }
                        sendFrame({DataCommand::Ack, {std::to_string(sequence)}});
                        if (batch.value("finished", false)) {
                            worker->ended = true;
                            worker->stop = true;
                        }
                    }
                }
            } catch (const std::exception &error) {
                if (!worker->stop) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    dataErrors_[token] = error.what();
                }
            }
            {
                std::lock_guard<std::mutex> lock(worker->socketMutex);
                worker->socket = -1;
                closeData(fd);
            }
            for (int i = 0; i < 10 && !worker->stop; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });
}
} // namespace LL
