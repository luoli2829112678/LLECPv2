#include "EtherCAT_Master.h"
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {
volatile std::sig_atomic_t stopRequested = 0;
void stopHandler(int) { stopRequested = 1; }
}

// LLECP enp2s0 enp3s0            扫描多张独立网卡
// LLECP --run enp2s0 enp3s0      多主站同时周期运行，Ctrl+C 停止
int main(int argc, char* argv[])
{
    bool run = false;
    std::vector<std::string> interfaces;
    std::set<std::string> uniqueInterfaces;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--run") run = true;
        else if (arg == "--help") {
            std::cout << "Usage: LLECP [--run] interface [interface ...]\n";
            return 0;
        } else if (arg.empty() || arg[0] == '-' || !uniqueInterfaces.insert(arg).second) {
            std::cerr << "Invalid or duplicate interface: " << arg << '\n';
            return 1;
        } else interfaces.push_back(arg);
    }
    if (interfaces.empty()) interfaces.push_back("enp2s0");
    std::signal(SIGINT, stopHandler);
    std::signal(SIGTERM, stopHandler);
    std::vector<std::unique_ptr<EtherCAT_Master>> masters;
    for (const auto& name : interfaces) {
        auto master = std::make_unique<EtherCAT_Master>();
        if (!master->Open(name.c_str()) || master->ScanSlaves() <= 0) {
            std::cerr << name << ": " << master->GetLastError() << '\n';
            return 2;
        }
        std::cout << name << ": " << master->GetSlaveCount() << " slaves\n";
        for (const auto& slave : master->GetSlaves())
            std::cout << "  " << slave.id << " " << slave.name << " state=0x" << std::hex
                      << slave.state << " AL=0x" << slave.alStatusCode << std::dec << '\n';
        if (run && (master->ConfigureProcessData() <= 0 || !master->Start())) {
            std::cerr << name << ": " << master->GetLastError() << '\n';
            return 3;
        }
        masters.push_back(std::move(master));
    }
    while (run && !stopRequested) {
        for (std::size_t i = 0; i < masters.size(); ++i) {
            const auto status = masters[i]->GetMonitorStatus();
            std::cout << interfaces[i] << " WKC=" << status.actualWkc << '/' << status.expectedWkc
                      << " cycles=" << status.cycles << " errors=" << status.wkcErrors
                      << " missed=" << status.missedDeadlines << " allOP=" << status.allOperational << '\n';
            if (!status.running) {
                std::cerr << interfaces[i] << ": " << masters[i]->GetLastError() << '\n';
                return 4;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    // unique_ptr 析构先停止各自线程，再关闭对应网卡。
    return 0;
}