#include "ControllerClient.h"
#include <chrono>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
static void Close(Socket s) {
    closesocket(s);
}
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
using Socket = int;
static void Close(Socket s) {
    close(s);
}
#endif
namespace LL {
namespace {
void InitSockets() {
#ifdef _WIN32
    static const int initialized = [] {
        WSADATA w{};
        return WSAStartup(MAKEWORD(2, 2), &w);
    }();
    if (initialized)
        throw std::runtime_error("Winsock initialization failed");
#endif
}
void Timeout(Socket s, int ms) {
#ifdef _WIN32
    DWORD value = ms;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&value), sizeof(value));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&value), sizeof(value));
#else
    timeval value{ms / 1000, (ms % 1000) * 1000};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &value, sizeof(value));
#endif
}
} // namespace
Json ControllerClient::Call(const std::string &ip, const Json &request) {
    InitSockets();
    Socket s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == static_cast<Socket>(-1))
        throw std::runtime_error("Cannot create TCP socket");
    struct Guard {
        Socket s;
        ~Guard() {
            Close(s);
        }
    } guard{s};
    Timeout(s, 30000);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1)
        throw std::runtime_error("请输入有效的 IPv4 地址");
#ifdef _WIN32
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
    int rc = connect(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    if (rc != 0) {
        fd_set writes;
        FD_ZERO(&writes);
        FD_SET(s, &writes);
        timeval wait{3, 0};
        if (select(static_cast<int>(s + 1), nullptr, &writes, nullptr, &wait) <= 0)
            throw std::runtime_error("连接控制器超时");
        int error = 0;
#ifdef _WIN32
        int len = sizeof(error);
#else
        socklen_t len = sizeof(error);
#endif
        getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &len);
        if (error)
            throw std::runtime_error("无法连接控制器 8888 端口");
    }
#ifdef _WIN32
    nb = 0;
    ioctlsocket(s, FIONBIO, &nb);
#else
    fcntl(s, F_SETFL, flags);
#endif
    const auto frame = CpsRequestFrame(request);
    std::size_t sent = 0;
    while (sent < frame.size()) {
#ifdef _WIN32
        const int n = send(s, frame.data() + sent, static_cast<int>(frame.size() - sent), 0);
#else
        const int n = send(s, frame.data() + sent, frame.size() - sent, MSG_NOSIGNAL);
#endif
        if (n <= 0)
            throw std::runtime_error("TCP 发送失败");
        sent += n;
    }
    std::string response;
    char buffer[8192];
    while (response.find(",;") == std::string::npos) {
        int n = recv(s, buffer, sizeof(buffer), 0);
        if (n <= 0)
            throw std::runtime_error("控制器未返回完整响应");
        response.append(buffer, n);
        if (response.size() > 2 * 1024 * 1024)
            throw std::runtime_error("响应过大");
    }
    return CpsResponse(response);
}
ControllerClient::ControllerClient() {
    InitSockets();
    Socket s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s != static_cast<Socket>(-1)) {
        Timeout(s, 200);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0) {
#ifdef _WIN32
            int len = sizeof(addr);
#else
            socklen_t len = sizeof(addr);
#endif
            getsockname(s, reinterpret_cast<sockaddr *>(&addr), &len);
            udp_ = static_cast<std::intptr_t>(s);
            udpPort_ = ntohs(addr.sin_port);
        } else
            Close(s);
    }
    worker_ = std::thread([this] { Work(); });
    if (udp_ != -1)
        receiver_ = std::thread([this] { Receive(); });
}
ControllerClient::~ControllerClient() {
    StopDataStreams();
    stop_ = true;
    cv_.notify_all();
    if (worker_.joinable())
        worker_.join();
    if (receiver_.joinable())
        receiver_.join();
    if (udp_ != -1)
        Close(static_cast<Socket>(udp_));
}
void ControllerClient::Endpoint(std::string ip) {
    StopDataStreams();
    std::lock_guard<std::mutex> l(mutex_);
    ip_ = std::move(ip);
    samples_.clear();
    dataLatest_.clear();dataErrors_.clear();
}
std::string ControllerClient::Endpoint() const {
    std::lock_guard<std::mutex> l(mutex_);
    return ip_;
}
void ControllerClient::Request(Json request, Callback callback) {
    std::lock_guard<std::mutex> l(mutex_);
    if (jobs_.size() > 32)
        throw std::runtime_error("通信队列繁忙");
    jobs_.push_back({ip_, std::move(request), std::move(callback)});
    cv_.notify_one();
}
void ControllerClient::Work() {
    while (!stop_) {
        Job job;
        {
            std::unique_lock<std::mutex> l(mutex_);
            cv_.wait(l, [&] { return stop_ || !jobs_.empty(); });
            if (stop_)
                break;
            job = std::move(jobs_.front());
            jobs_.pop_front();
        }
        Json result;
        try {
            result = Call(job.ip, job.request);
        } catch (const std::exception &e) {
            result = {{"ok", false}, {"error", e.what()}, {"transportError", true}};
        }
        if (job.callback) {
            std::lock_guard<std::mutex> l(mutex_);
            done_.push_back([fn = std::move(job.callback), result] { fn(result); });
        }
    }
}
void ControllerClient::Drain() {
    std::deque<std::function<void()>> done;
    {
        std::lock_guard<std::mutex> l(mutex_);
        done.swap(done_);
    }
    for (auto &fn : done)
        fn();
}
std::vector<ScopeSample> ControllerClient::DrainSamples() {
    std::lock_guard<std::mutex> l(mutex_);
    std::vector<ScopeSample> out;
    out.swap(samples_);
    return out;
}
void ControllerClient::Receive() {
    char buffer[60000];
    std::uint64_t previous = 0;
    while (!stop_) {
        sockaddr_in source{};
#ifdef _WIN32
        int len = sizeof(source);
#else
        socklen_t len = sizeof(source);
#endif
        const int n = recvfrom(static_cast<Socket>(udp_), buffer, sizeof(buffer), 0,
                               reinterpret_cast<sockaddr *>(&source), &len);
        if (n <= 0)
            continue;
        char address[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &source.sin_addr, address, sizeof(address));
        if (Endpoint() != address)
            continue;
        try {
            auto j = Json::parse(buffer, buffer + n);
            if (j.value("type", "") != "LLECP.scope.v1")
                continue;
            ScopeSample sample;
            sample.timestampUs = j.at("timestamp_us");
            sample.sequence = j.at("sequence");
            for (auto it = j.at("values").begin(); it != j.at("values").end(); ++it)
                if (it.value().is_number())
                    sample.values[it.key()] = it.value().get<double>();
            if (previous && sample.sequence > previous + 1)
                lost_ += sample.sequence - previous - 1;
            previous = sample.sequence;
            std::lock_guard<std::mutex> l(mutex_);
            if (samples_.size() < 10000)
                samples_.push_back(std::move(sample));
            else
                ++lost_;
        } catch (...) {
        }
    }
}
void ControllerClient::Upload(const std::string &purpose, const std::string &name, const std::string &bytes,
                              Callback callback) {
    auto data = std::make_shared<std::string>(bytes);
    auto token = std::make_shared<std::string>();
    auto offset = std::make_shared<std::size_t>(0);
    auto next = std::make_shared<std::function<void(const Json &)>>();
    *next = [this, data, token, offset, callback, next](const Json &r) {
        if (!r.value("ok", false)) {
            callback(r);
            *next = {};
            return;
        }
        if (token->empty())
            *token = r.at("token").get<std::string>();
        if (*offset < data->size()) {
            const auto size = std::min<std::size_t>(192 * 1024, data->size() - *offset);
            Json chunk = {{"op", CpsOp::FileChunk},
                          {"token", *token},
                          {"offset", *offset},
                          {"data", Encode64(data->substr(*offset, size))}};
            *offset += size;
            Request(std::move(chunk), *next);
        } else {
            Request({{"op", CpsOp::FileCommit}, {"token", *token}}, callback);
            *next = {};
        }
    };
    Request({{"op", CpsOp::FileBegin}, {"purpose", purpose}, {"name", name}, {"size", bytes.size()}}, *next);
}
} // namespace LL
