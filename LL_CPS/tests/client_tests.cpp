#include "ControllerClient.h"
#include "LL_CommandCodec.h"
#include "LL_DataStream.h"
#include <iostream>
#include <chrono>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
static void closeSocket(Socket s) {
    closesocket(s);
}
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
using Socket = int;
static void closeSocket(Socket s) {
    close(s);
}
#endif
int main() {
    try {
        LL::ControllerClient client;
        client.Endpoint("127.0.0.1");
        Socket listener = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(8888);
        if (bind(listener, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 ||
            listen(listener, 2) != 0)
            throw std::runtime_error("Test requires available loopback port 8888");
        std::string serverError;
        std::thread server([&] {
            try {
                Socket peer = accept(listener, nullptr, nullptr);
                std::string data;
                char b[1024];
                while (data.find(",;") == std::string::npos) {
                    int n = recv(peer, b, sizeof(b), 0);
                    if (n <= 0)
                        throw std::runtime_error("Incomplete test request");
                    data.append(b, n);
                }
                const auto first = data.find(',');
                auto request = LL::Json::parse(LL::Decode64(data.substr(first + 1, data.size() - first - 3)));
                if (request.at("op") != "hello")
                    throw std::runtime_error("Wrong request");
                const auto response = LL::CpsResponseFrame({{"ok", true}, {"echo", request.at("text")}});
                for (std::size_t i = 0; i < response.size(); i += 3)
                    if (send(peer, response.data() + i,
                             static_cast<int>(std::min<std::size_t>(3, response.size() - i)), 0) <= 0)
                        throw std::runtime_error("Mock response send failed");
                closeSocket(peer);
            } catch (const std::exception &e) {
                serverError = e.what();
            }
        });
        bool done = false, passed = false;
        client.Request({{"op", "hello"}, {"text", "comma,semicolon;\n中文"}}, [&](const LL::Json &r) {
            done = true;
            passed = r.value("ok", false) && r.at("echo") == "comma,semicolon;\n中文";
        });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
        while (!done && std::chrono::steady_clock::now() < deadline) {
            client.Drain();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        server.join();
        closeSocket(listener);
        if (!serverError.empty() || !done || !passed)
            throw std::runtime_error("Async TCP roundtrip failed: " + serverError);
        Socket udp = socket(AF_INET, SOCK_DGRAM, 0);
        addr.sin_port = htons(static_cast<unsigned short>(client.UdpPort()));
        const auto packet = LL::Json{{"type", "LLECP.scope.v1"},
                                     {"sequence", 1},
                                     {"timestamp_us", 1000},
                                     {"values",
                                      {{"Main.Counter", 42.0},
                                       {"Axis.0.ActualPosition", 1.1186676},
                                       {"Axis.0.ActualVelocity", -.0015258789}}}}
                                .dump();
        sendto(udp, packet.data(), static_cast<int>(packet.size()), 0, reinterpret_cast<sockaddr *>(&addr),
               sizeof(addr));
        std::vector<LL::ScopeSample> samples;
        for (int i = 0; i < 100 && samples.empty(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            samples = client.DrainSamples();
        }
        closeSocket(udp);
        if (samples.size() != 1 || samples[0].values.at("Main.Counter") != 42)
            throw std::runtime_error("UDP scope receive failed");
        if (samples[0].values.at("Axis.0.ActualPosition") != 1.1186676 ||
            samples[0].values.at("Axis.0.ActualVelocity") != -.0015258789)
            throw std::runtime_error("UDP axis feedback was dropped");
        Socket dataListener = socket(AF_INET, SOCK_STREAM, 0);
        addr.sin_port = 0;
        if (bind(dataListener, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 ||
            listen(dataListener, 2) != 0)
            throw std::runtime_error("Data test bind failed");
#ifdef _WIN32
        int dataLength = sizeof(addr);
#else
        socklen_t dataLength = sizeof(addr);
#endif
        getsockname(dataListener, reinterpret_cast<sockaddr *>(&addr), &dataLength);
        const auto dataPort = ntohs(addr.sin_port);
        std::string dataError;
        std::thread dataServer([&] {
            try {
                LL::DataStream queue("test_session");
                auto readCommand = [](Socket peer) {
                    LL::FrameBuffer input;
                    std::vector<std::string> frames;
                    while (frames.empty()) {
                        char bytes[4096];
                        int n = recv(peer, bytes, sizeof(bytes), 0);
                        if (n <= 0)
                            throw std::runtime_error("Missing data ACK");
                        if (!input.Append(std::string(bytes, n), frames))
                            throw std::runtime_error("Invalid ACK frame");
                    }
                    LL::Command command;
                    if (!LL::DecodeCommand(frames.front(), command))
                        throw std::runtime_error("Malformed ACK");
                    return command;
                };
                auto fill = [&](int begin) {
                    for (int i = begin; i < begin + 100; ++i)
                        queue.Push(
                            {{"sequence", i},
                             {"timestamp_us", i * 1000},
                             {"values",
                              {{"Axis.0.ActualPosition", {{"number", i}, {"value", std::to_string(i)}}}}}});
                };
                auto sendBatch = [&](Socket peer, bool corrupt = false) {
                    auto envelope = LL::SealDataBatch(queue.Peek());
                    if (corrupt)
                        envelope["crc32"] = 0;
                    const auto text = LL::EncodeCommand({"DataBatch", {LL::Encode64(envelope.dump())}});
                    for (std::size_t i = 0; i < text.size(); i += 17)
                        if (send(peer, text.data() + i,
                                 static_cast<int>(std::min<std::size_t>(17, text.size() - i)), 0) <= 0)
                            throw std::runtime_error("Data batch send failed");
                };
                fill(1);
                Socket peer = accept(dataListener, nullptr, nullptr);
                if (readCommand(peer).name != "DataAttach")
                    throw std::runtime_error("Missing data attachment");
                sendBatch(peer, true);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                sendBatch(peer);
                if (readCommand(peer).parameters.at(0) != "1")
                    throw std::runtime_error("Wrong first ACK");
                sendBatch(peer);
                if (readCommand(peer).parameters.at(0) != "1")
                    throw std::runtime_error("Duplicate batch not acknowledged");
                closeSocket(peer);
                peer = accept(dataListener, nullptr, nullptr);
                if (readCommand(peer).name != "DataAttach")
                    throw std::runtime_error("Missing reconnect attachment");
                sendBatch(peer);
                if (readCommand(peer).parameters.at(0) != "1")
                    throw std::runtime_error("Unacknowledged batch not recovered across reconnect");
                queue.Ack(1);
                fill(101);
                sendBatch(peer);
                if (readCommand(peer).parameters.at(0) != "2")
                    throw std::runtime_error("Wrong reconnect ACK");
                queue.Ack(2);
                for (int begin = 201; begin <= 10000; begin += 100) {
                    fill(begin);
                    sendBatch(peer);
                    const auto ack = readCommand(peer);
                    if (ack.parameters.at(0) != std::to_string(begin / 100 + 1))
                        throw std::runtime_error("Stress batch ACK missing");
                    queue.Ack(begin / 100 + 1);
                }
                queue.Finish();
                sendBatch(peer);
                if (readCommand(peer).parameters.at(0) != "101")
                    throw std::runtime_error("Stream-end ACK missing");
                closeSocket(peer);
                LL::DataStream monitor("monitor_session");
                for (int i = 0; i < 100; ++i)
                    monitor.Push({{"sequence", i + 1},
                                  {"timestamp_us", i * 1000},
                                  {"values",
                                   {{"Main.Ready", {{"value", "TRUE"}, {"type", "BOOL"}}},
                                    {"Main.Message", {{"value", "READY"}, {"type", "STRING"}}}}}});
                peer = accept(dataListener, nullptr, nullptr);
                if (readCommand(peer).parameters.at(0) != "monitor_session")
                    throw std::runtime_error("Wrong monitor session");
                const auto text = LL::EncodeCommand(
                    {"DataBatch", {LL::Encode64(LL::SealDataBatch(monitor.Peek()).dump())}});
                for (std::size_t i = 0; i < text.size(); i += 23)
                    if (send(peer, text.data() + i,
                             static_cast<int>(std::min<std::size_t>(23, text.size() - i)), 0) <= 0)
                        throw std::runtime_error("Monitor send failed");
                if (readCommand(peer).parameters.at(0) != "1")
                    throw std::runtime_error("Monitor ACK missing");
                closeSocket(peer);
            } catch (const std::exception &error) {
                dataError = error.what();
            }
        });
        client.OpenDataStream("test_scope", "test_session", dataPort, true);
        std::vector<LL::ScopeSample> collected;
        const auto dataDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (collected.size() < 10000 && std::chrono::steady_clock::now() < dataDeadline) {
            auto incoming = client.DrainSamples();
            collected.insert(collected.end(), incoming.begin(), incoming.end());
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        while (!client.DataEnded("test_scope") && std::chrono::steady_clock::now() < dataDeadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        const auto stats = client.DataStats("test_scope");
        if (stats.value("crcErrors", 0ull) != 1 || stats.value("received", 0ull) != 10000 ||
            stats.value("replayed", 0ull) < 2)
            throw std::runtime_error("Data integrity/replay statistics wrong");
        client.CloseDataStream("test_scope");
        client.OpenDataStream("test_watch", "monitor_session", dataPort, false);
        LL::Json monitored;
        const auto watchDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (monitored.is_null() && std::chrono::steady_clock::now() < watchDeadline) {
            monitored = client.DrainData("test_watch");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        client.CloseDataStream("test_watch");
        dataServer.join();
        closeSocket(dataListener);
        if (!dataError.empty() || collected.size() != 10000 ||
            collected.front().values.at("Axis.0.ActualPosition") != 1 ||
            collected.back().values.at("Axis.0.ActualPosition") != 10000)
            throw std::runtime_error("Reliable data batch/reconnect failed: " + dataError);
        for (std::size_t i = 0; i < collected.size(); ++i)
            if (collected[i].sequence != i + 1 || collected[i].values.at("Axis.0.ActualPosition") != i + 1)
                throw std::runtime_error("Stress sequence/value missing");
        if (monitored.at("Main.Ready").at("value") != "TRUE" ||
            monitored.at("Main.Message").at("value") != "READY")
            throw std::runtime_error("Typed watch data channel failed");
        std::cout << "TCP fragmentation/Unicode, UDP compatibility, 100-frame batches, ACK, duplicate "
                     "suppression and reconnect passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what();
        return 1;
    }
}
