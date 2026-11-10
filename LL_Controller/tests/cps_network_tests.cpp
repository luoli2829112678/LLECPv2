#include "LL_ControllerRuntime.h"
#include "LL_CpsData.h"
#include "LL_Pdo.h"
#include "LL_DataStream.h"
#include "LL_CommandCodec.h"
#include "HSTR/HSTR.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <iostream>
using LL::Json;
static void check(bool condition, const std::string &text) {
    if (!condition)
        throw std::runtime_error(text);
}
static Json rpc(const Json &request) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct Guard {
        int fd;
        ~Guard() {
            close(fd);
        }
    } guard{fd};
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(8888);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    timeval timeout{30, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    check(connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0, "Connect failed");
    const auto bytes = LL::CpsRequestFrame(request);
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        auto n = send(fd, bytes.data() + offset, bytes.size() - offset, MSG_NOSIGNAL);
        check(n > 0, "Send failed");
        offset += n;
    }
    std::string reply;
    while (reply.find(",;") == std::string::npos) {
        char b[4096];
        int n = recv(fd, b, sizeof(b), 0);
        check(n > 0, "Receive failed");
        reply.append(b, n);
    }
    auto result = LL::CpsResponse(reply);
    check(result.value("ok", false), result.value("error", "RPC failed"));
    return result;
}
int main(int argc, char **argv) {
    int compilerCode = 0;
    if (HSTR::HandleCompilerInvocation(argc, argv, compilerCode))
        return compilerCode;
    auto root = std::filesystem::temp_directory_path() / LL::NewId("cps_network");
    try {
        setenv("LLECP_DATA_ROOT", root.c_str(), 1);
        check(LL_InitializeController((root / "runtime").string()), "Controller startup failed");
        check(rpc({{"op", LL::CpsOp::Hello}}).at("version") == 1, "Protocol version");
        auto project = LL::NewProject("CPS network test");
        for (const auto *id : {"EC_A", "EC_B"}) {
            auto master = LL::NewMaster(id);
            master["simulation"] = true;
            project["masters"].push_back(master);
        }
        rpc({{"op", LL::CpsOp::Apply}, {"project", project}});
        rpc({{"op", LL::CpsOp::MasterStart}, {"master", "EC_A"}});
        rpc({{"op", LL::CpsOp::MasterStart}, {"master", "EC_B"}});
        auto status = rpc({{"op", LL::CpsOp::Status}});
        check(status["masters"][0]["running"] == true && status["masters"][1]["running"] == true,
              "Simultaneous master start");
        rpc({{"op", LL::CpsOp::MasterStop}, {"master", "EC_A"}});
        status = rpc({{"op", LL::CpsOp::Status}});
        check(status["masters"][0]["running"] == false && status["masters"][1]["running"] == true,
              "Independent master stop");
        rpc({{"op", LL::CpsOp::MasterStop}, {"master", "EC_B"}});
        const auto xml=R"(<EtherCATInfo><Vendor><Id>1</Id><Name>Test</Name></Vendor><Descriptions><Devices><Device Physics="YY"><Type ProductCode="1" RevisionNo="1">Drive</Type><Name>Drive</Name><RxPdo Sm="2" Mandatory="1"><Index>#x1600</Index><Name>Rx</Name><Entry><Index>#x6040</Index><SubIndex>0</SubIndex><BitLen>16</BitLen><Name>Controlword</Name><DataType>UINT</DataType></Entry><Entry><Index>#x7000</Index><SubIndex>1</SubIndex><BitLen>1</BitLen><Name>Output</Name><DataType>BOOL</DataType></Entry></RxPdo><TxPdo Sm="3" Mandatory="1"><Index>#x1a00</Index><Name>Tx</Name><Entry><Index>#x6041</Index><SubIndex>0</SubIndex><BitLen>16</BitLen><Name>Statusword</Name><DataType>UINT</DataType></Entry></TxPdo></Device></Devices></Descriptions></EtherCATInfo>)";
        LL::WriteAtomic(root/"ESI"/"test.xml",xml);auto device=LL::DeviceCatalog(xml,"test.xml").at(0);project["masters"][0]["slaves"].push_back(LL::NewSlave(device,1));rpc({{"op",LL::CpsOp::Apply},{"project",project}});rpc({{"op",LL::CpsOp::MasterStart},{"master","EC_A"}});auto pdo=rpc({{"op",LL::CpsOp::PdoWrite},{"master","EC_A"},{"slave",1},{"key",LL::PdoKey(true,0x1600,0)},{"value","12345"}});check(pdo["entries"][0]["value"]=="12345","PDO output write/readback failed");pdo=rpc({{"op",LL::CpsOp::PdoWrite},{"master","EC_A"},{"slave",1},{"key",LL::PdoKey(true,0x1600,1)},{"value","TRUE"}});check(pdo["entries"][0]["value"]=="12345"&&pdo["entries"][1]["value"]=="TRUE","Bit PDO write corrupted adjacent output");check(pdo["entries"][2]["writable"]==false,"TxPDO was not read-only");rpc({{"op",LL::CpsOp::MasterStop},{"master","EC_A"}});project["masters"][0]["slaves"][0]["axes"].push_back(LL::NewAxis(project["masters"][0]["slaves"][0],0));rpc({{"op",LL::CpsOp::Apply},{"project",project}});rpc({{"op",LL::CpsOp::MasterStart},{"master","EC_A"}});status=rpc({{"op",LL::CpsOp::Status}});check(status.at("axes").size()==1&&status["axes"][0]["available"]==true,"Axis state unavailable on simulated master");check(status["masters"][0].contains("sentFrames")&&status["masters"][0].contains("txErrors")&&status["masters"][0].contains("rxErrors"),"Master diagnostics missing");pdo=rpc({{"op",LL::CpsOp::PdoRead},{"master","EC_A"},{"slave",1}});check(pdo["entries"][0]["writable"]==false,"Axis-owned controlword allowed manual PDO writes");rpc({{"op",LL::CpsOp::MasterStop},{"master","EC_A"}});
        const auto axisVars=rpc({{"op",LL::CpsOp::Variables}}).at("variables");
        bool namedAxis=false;for(const auto& variable:axisVars)if(variable.value("source","")=="Axis"&&variable.value("displayName",std::string{}).find("Axis_0")!=std::string::npos)namedAxis=true;
        check(namedAxis,"Axis picker has no configured axis name");
        const auto aliasValues=rpc({{"op",LL::CpsOp::Read},{"names",Json::array({"axis.0.actualposition","0.ActualPosition"})}}).at("values");
        check(aliasValues.at("axis.0.actualposition").at("number")==aliasValues.at("0.ActualPosition").at("number"),"Axis canonical alias resolution differs");
        auto text = project.dump();
        auto token = rpc({{"op", LL::CpsOp::FileBegin},
                          {"purpose", "project"},
                          {"name", "project.json"},
                          {"size", text.size()}})
                         .at("token");
        const auto split = text.size() / 2;
        rpc({{"op", LL::CpsOp::FileChunk},
             {"token", token},
             {"offset", 0},
             {"data", LL::Encode64(text.substr(0, split))}});
        rpc({{"op", LL::CpsOp::FileChunk},
             {"token", token},
             {"offset", split},
             {"data", LL::Encode64(text.substr(split))}});
        rpc({{"op", LL::CpsOp::FileCommit}, {"token", token}});
        rpc({{"op", LL::CpsOp::Download}, {"token", token}});
        rpc({{"op", LL::CpsOp::Build}});
        Json build;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            build = rpc({{"op", LL::CpsOp::BuildStatus}});
        } while (!build.value("done", false) && std::chrono::steady_clock::now() < deadline);
        check(build.value("done", false) && build.value("succeeded", false),
              "HSTR build failed: " + build.value("diagnostics", ""));
        auto variables = rpc({{"op", LL::CpsOp::Variables}}).at("variables");
        std::string counter;
        for (auto &v : variables)
            if (v.at("name").get<std::string>().find("COUNTER") != std::string::npos ||
                v.at("name").get<std::string>().find("Counter") != std::string::npos)
                counter = v.at("name");
        check(!counter.empty(), "Counter variable not exported");
        rpc({{"op", LL::CpsOp::Run}, {"debug", false}});
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        rpc({{"op", LL::CpsOp::Force}, {"name", counter}, {"value", "42"}});
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        auto values = rpc({{"op", LL::CpsOp::Read}, {"names", Json::array({counter})}}).at("values");
        check(values[counter]["value"] == "42" && values[counter]["forced"] == true,
              "Continuous force failed");
        int udp = socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        check(bind(udp, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0, "UDP bind");
        socklen_t length = sizeof(address);
        getsockname(udp, reinterpret_cast<sockaddr *>(&address), &length);
        timeval timeout{2, 0};
        setsockopt(udp, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        rpc({{"op", LL::CpsOp::Subscribe},
             {"token", "test_scope"},
             {"port", ntohs(address.sin_port)},
             {"intervalMs", 5},
             {"channels", Json::array({counter,"axis.0.actualposition"})}});
        char packet[8192];
        const int n = recv(udp, packet, sizeof(packet), 0);
        close(udp);
        check(n > 0, "UDP push timed out");
        auto scope = Json::parse(packet, packet + n);
        check(scope.at("values").at(counter) == 42, "UDP pushed wrong forced value");
        check(scope.at("values").at("axis.0.actualposition").is_number(),"UDP did not push axis feedback");
        rpc({{"op", LL::CpsOp::Unsubscribe}, {"token", "test_scope"}});
        const auto subscription=rpc({{"op",LL::CpsOp::Subscribe},{"token","reliable"},{"channels",Json::array({counter,"axis.0.actualposition"})},{"intervalMs",1},{"port",0},{"transport","tcp-batch"}});
        auto sendCommand=[](int socket,const LL::Command& command){const auto text=LL::EncodeCommand(command);std::size_t offset=0;while(offset<text.size()){auto n=send(socket,text.data()+offset,text.size()-offset,MSG_NOSIGNAL);check(n>0,"Data command send failed");offset+=n;}};
        auto attach=[&]{int socket=::socket(AF_INET,SOCK_STREAM,0);sockaddr_in peer{};peer.sin_family=AF_INET;peer.sin_port=htons(subscription.at("dataPort").get<unsigned>());peer.sin_addr.s_addr=htonl(INADDR_LOOPBACK);timeval limit{3,0};setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,&limit,sizeof(limit));check(connect(socket,reinterpret_cast<sockaddr*>(&peer),sizeof(peer))==0,"Data attach connect failed");sendCommand(socket,{"DataAttach",{subscription.at("session")}});return socket;};
        auto receiveBatch=[](int socket){LL::FrameBuffer buffer;std::vector<std::string> frames;while(frames.empty()){char bytes[16384];auto n=recv(socket,bytes,sizeof(bytes),0);check(n>0,"Data batch timeout");check(buffer.Append(std::string(bytes,n),frames),"Data framing failed");}LL::Command command;check(LL::DecodeCommand(frames[0],command)&&command.name=="DataBatch","Wrong data frame");return LL::OpenDataBatch(Json::parse(LL::Decode64(command.parameters.at(0))));};
        int data=attach();const auto firstBatch=receiveBatch(data);check(!firstBatch.at("frames").empty()&&firstBatch.at("frames").size()<=100,"Wrong batch frame count");
        check(firstBatch["frames"][0]["values"][counter]["number"]==42,"Data batch lost typed numeric value");close(data);
        data=attach();const auto retry=receiveBatch(data);check(retry.at("batch")==firstBatch.at("batch")&&retry.at("frames")==firstBatch.at("frames"),"Unacknowledged batch was not retained");
        sendCommand(data,{"DataAck",{std::to_string(retry.at("batch").get<unsigned long long>())}});const auto nextBatch=receiveBatch(data);check(nextBatch.at("batch")>retry.at("batch"),"ACK did not advance batches");close(data);
        rpc({{"op",LL::CpsOp::Unsubscribe},{"token","reliable"}});
        rpc({{"op", LL::CpsOp::Release}, {"name", counter}});
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        values = rpc({{"op", LL::CpsOp::Read}, {"names", Json::array({counter})}}).at("values");
        check(values[counter]["forced"] == false && values[counter]["value"] != "42", "Force release failed");
        rpc({{"op", LL::CpsOp::Stop}});
        rpc({{"op", LL::CpsOp::Emergency}});
        check(rpc({{"op", LL::CpsOp::Status}}).at("emergency") == true, "Emergency latch missing");
        rpc({{"op", LL::CpsOp::EmergencyRelease}});
        LL_ShutdownController();
        std::filesystem::remove_all(root);
        std::cout << "CPS controller integration passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nArtifacts: " << root << '\n';
        LL_ShutdownController();
        return 1;
    }
}
