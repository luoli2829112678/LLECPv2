#include "LL_CpsData.h"
#include "LL_CommandCodec.h"
#include "LL_Pdo.h"
#include "ESI_Parse.h"
#include "LL_DataStream.h"
#include "LL_ScopePlot.h"
#include "LL_ScopeFFT.h"
#include "LL_STEditor.h"
#include <iostream>
#include <cmath>
#include <functional>
using namespace LL;
static int checks = 0;
static void check(bool c) {
    ++checks;
    if (!c)
        throw std::runtime_error("Check failed: " + std::to_string(checks));
}
static void rejects(std::function<void()> f) {
    bool threw = false;
    try {
        f();
    } catch (...) {
        threw = true;
    }
    check(threw);
}
int main() {
    auto root = fs::temp_directory_path() / NewId("llcps_test");
    try {
        for (const auto &source : std::vector<std::string>{
                 "PROGRAM Main\nVAR\n A : DINT;\nEND_VAR\nA := 1;\nEND_PROGRAM\n",
                 "FUNCTION F : DINT\nVAR_INPUT\n V : DINT;\nEND_VAR\nF := V;\nEND_FUNCTION\n",
                 "FUNCTION_BLOCK F\nVAR_INPUT\n V : BOOL;\nEND_VAR\nVAR_OUTPUT\n R : BOOL;\nEND_VAR\n(* "
                 "END_VAR (* PROGRAM *) *)\nR := V;\nEND_FUNCTION_BLOCK\n",
                 "PROGRAM P\nVAR\n S : STRING := 'END_VAR';\nEND_VAR\nS := 'END_PROGRAM';\nEND_PROGRAM\n",
                 "FUNCTION F : DINT\r\nF := 1;\r\nEND_FUNCTION\r\n"}) {
            const auto parts = SplitST(source);
            check(parts.split);
            check(parts.Join() == source);
            check(!parts.body.empty());
        }
        check(!SplitST("VAR_GLOBAL\nX : DINT;\nEND_VAR\n").split);
        std::vector<double> sine(1024);
        for (std::size_t i = 0; i < sine.size(); ++i)
            sine[i] = 5 + 2 * std::sin(6.283185307179586 * 50 * i / 1024);
        const auto spectrum = ScopeFFT(sine, 1024);
        const auto peak = std::max_element(spectrum.begin(), spectrum.end(),
                                           [](const auto &a, const auto &b) { return a.second < b.second; });
        check(peak->first == 50 && std::abs(peak->second - 2) < .01);
        rejects([&] { ScopeFFT(sine, 0); });
        DataStream stream("queue_test");
        for (int i = 0; i < 100; ++i)
            stream.Push({{"sequence", i + 1},
                         {"timestamp_us", i * 1000},
                         {"values", {{"Axis.0.ActualPosition", {{"number", i}}}}}});
        auto batch = stream.Peek();
        check(batch.at("frames").size() == 100 && batch.at("batch") == 1);
        stream.Ack(2);
        check(stream.Peek().at("batch") == 1);
        stream.Ack(1);
        check(stream.Peek().is_null());
        for (int i = 0; i < 6600; ++i)
            stream.Push({{"sequence", i + 101}});
        batch = stream.Peek();
        check(batch.at("batch") == 2 && batch.at("droppedSamples") == 200);
        check(stream.Peek().at("frames") == batch.at("frames"));
        DataStream tail("tail_test");
        tail.Push({{"sequence", 1}});
        tail.Finish();
        check(tail.Peek().at("frames").size() == 1);
        tail.Ack(1);
        check(tail.Peek().value("finished", false));
        tail.Ack(2);
        check(tail.Expired());
        check(DataCrc32("123456789") == 0xcbf43926u);
        auto sealed = SealDataBatch(batch);
        check(OpenDataBatch(sealed) == batch);
        sealed["payload"] = sealed.at("payload").get<std::string>() + " ";
        rejects([&] { OpenDataBatch(sealed); });
        std::vector<ScopeSample> plot(1000);
        for (std::size_t i = 0; i < plot.size(); ++i) {
            plot[i].sequence = i + 1;
            plot[i].timestampUs = i * 1000;
            plot[i].values["x"] = 0;
        }
        plot[157].values["x"] = 99;
        plot[158].values["x"] = -50;
        plot[500].values.clear();
        for (std::size_t i = 700; i < plot.size(); ++i)
            plot[i].sequence += 5;
        auto points = ScopePlotIndices(plot, "x", 0, 20);
        check(std::find(points.begin(), points.end(), 157) != points.end());
        check(std::find(points.begin(), points.end(), 158) != points.end());
        check(std::count(points.begin(), points.end(), ScopeGap) == 2);
        for (const auto &value :
             std::vector<std::string>{"", ",;\n\r", std::string("a\0b", 3), "中文源代码 := 1;"})
            check(Decode64(Encode64(value)) == value);
        for (const auto *invalid : {"A", "!!!!", "=AAA", "AA=A", "A==="})
            rejects([=] { Decode64(invalid); });
        Json request = {{"op", "hello"}, {"code", "a,b;\n中文"}};
        check(CpsResponse(EncodeResponse(CpsCommand, {"0", Encode64(request.dump())})) == request);
        check(CpsResponse(CpsResponseFrame(request)) == request);
        rejects([] { CpsResponse("hello,0,;"); });
        rejects([] { CpsRequestFrame({{"text", std::string(600000, 'x')}}); });
        for (const auto &item :
             std::vector<std::tuple<std::string, int, std::string>>{{"INT", 2, "-32768"},
                                                                    {"UINT", 2, "65535"},
                                                                    {"DINT", 4, "-12345"},
                                                                    {"ULINT", 8, "18446744073709551615"},
                                                                    {"BOOL", 1, "TRUE"},
                                                                    {"LREAL", 8, "1.25"},
                                                                    {"REAL", 4, "-2.5"},
                                                                    {"STRING", 128, "Hello, 世界"}}) {
            auto type = std::get<0>(item), value = std::get<2>(item);
            check(HexToScalar(type, ScalarToHex(type, std::get<1>(item), value)) == value);
        }
        rejects([] { ScalarToHex("UINT", 2, "65536"); });
        rejects([] { ScalarToHex("INT", 2, "32768"); });
        rejects([] { ScalarToHex("ULINT", 8, "-1"); });
        rejects([] { ScalarToHex("LREAL", 8, "nan"); });
        rejects([] { ScalarToHex("BOOL", 1, "yes"); });
        rejects([] { ScalarToHex("ARRAY", 12, "123"); });
        rejects([] { ScalarToHex("DINT", 4, "HEX:00"); });
        check(ScalarToHex("ARRAY", 2, "HEX:0AFF") == "0AFF");
        ProjectRepository repo(root);
        auto project = repo.Create("测试项目");
        check(repo.List().size() == 1);
        check(repo.DefaultProject() == project.at("id"));
        auto master = NewMaster("EtherCAT_1");
        master["simulation"] = true;
        project["masters"].push_back(master);
        project["hstr"]["files"][0]["content"] = "PROGRAM Main\nEND_PROGRAM\n";
        repo.Save(project);
        check(repo.Open(project.at("id"))["masters"][0]["simulation"] == true);
        check(ReadText(root / "PRJ" / project.at("id").get<std::string>() / "HSTR" / "Main.st") ==
              "PROGRAM Main\nEND_PROGRAM\n");
        auto copy = repo.SaveAs(project, "复制");
        check(copy["id"] != project["id"]);
        check(repo.List().size() == 2);
        repo.Remove(copy.at("id"));
        check(repo.List().size() == 1);
        check(repo.DefaultProject() == project.at("id"));
        rejects([&] { repo.Remove("../outside"); });
        rejects([] { SourceFilename({{"name", "../file"}, {"kind", "program"}}); });
        rejects([] { NewMaster("123bad"); });
        Json device = Json::parse(
            R"({"name":"Drive","productCode":1,"revision":1,"rxPdos":[{"index":5632,"name":"Rx","selected":true,"mandatory":true,"entries":[{"index":24640,"subIndex":0,"bits":16}]}],"txPdos":[{"index":6656,"name":"Tx","selected":true,"mandatory":false,"entries":[{"index":24641,"subIndex":0,"bits":16}]}]})");
        auto slave = NewSlave(device, 1);
        auto axis = NewAxis(slave, 0);
        check(axis["enabled"] == true);
        check(axis["mapping"]["TargetPosition"].is_null());
        check(axis["mapping"]["Controlword"]["index"] == 0x6040);
        rejects([&] { SelectPdo(slave, true, 0x1600, false); });
        SelectPdo(slave, false, 0x1a00, false);
        check(NewAxis(slave, 0)["enabled"] == false);
        std::vector<ScopeSample> samples = {{0, 0, {{"Main.Value", 1.25}, {"Axis,quoted", -1}}},
                                            {1000, 1, {{"Main.Value", 2.5}}}};
        std::vector<std::string> channels = {"Main.Value", "Axis,quoted"}, loaded;
        ExportCsv(root / "wave.csv", samples, channels);
        auto imported = ImportCsv(root / "wave.csv", loaded);
        check(loaded == channels);
        check(imported.size() == 2);
        check(imported[0].values.at("Axis,quoted") == -1);
        check(imported[1].values.count("Axis,quoted") == 0);
        WriteAtomic(root / "bad.csv", "timestamp_us,x\n100,1\n50,2\n");
        rejects([&] { ImportCsv(root / "bad.csv", loaded); });
        WriteAtomic(root / "bad.csv", "timestamp_us,x\n-1,1\n");
        rejects([&] { ImportCsv(root / "bad.csv", loaded); });
        WriteAtomic(root / "bad.csv", "timestamp_us,x\n0,nan\n");
        rejects([&] { ImportCsv(root / "bad.csv", loaded); });
        rejects([] { DeviceCatalog("<invalid/>", "bad.xml"); });
        ESI_Parse fixedParser;
        const std::string fixedXml =
            R"(<EtherCATInfo><Vendor><Id>1</Id></Vendor><Descriptions><Devices><Device><Type ProductCode="27" RevisionNo="65536">fixed</Type><Mailbox><CoE PdoUpload="0"/></Mailbox><RxPdo Fixed="1" Sm="2"><Index>#x1600</Index><Entry><Index>#x6040</Index><SubIndex>0</SubIndex><BitLen>16</BitLen></Entry></RxPdo><TxPdo Fixed="1" Sm="3"><Index>#x1A00</Index><Entry><Index>#x6041</Index><SubIndex>0</SubIndex><BitLen>16</BitLen></Entry></TxPdo></Device></Devices></Descriptions></EtherCATInfo>)";
        check(fixedParser.loadFromString(fixedXml));
        const auto &fixedDevice = fixedParser.getDevices().at(0);
        check(!fixedDevice.supportsPdoUpload);
        check(fixedDevice.rxPdos.at(0).fixed && fixedDevice.txPdos.at(0).fixed);
        for (unsigned offset = 0; offset < 8; ++offset)
            for (unsigned bits = 1; bits <= 64; ++bits) {
                std::uint8_t image[12];
                std::fill(std::begin(image), std::end(image), 0xa5);
                std::vector<std::uint8_t> value((bits + 7) / 8, 0x5a);
                if (bits % 8)
                    value.back() &= static_cast<std::uint8_t>((1u << (bits % 8)) - 1);
                auto before = std::vector<std::uint8_t>(std::begin(image), std::end(image));
                const auto hex = BytesHex(value.data(), value.size());
                WritePdoBits(image, sizeof(image), offset, bits, hex);
                check(ReadPdoBits(image, sizeof(image), offset, bits) == hex);
                for (unsigned bit = 0; bit < 96; ++bit)
                    if (bit < offset || bit >= offset + bits)
                        check(((image[bit / 8] >> (bit % 8)) & 1) == ((before[bit / 8] >> (bit % 8)) & 1));
            }
        std::uint8_t small[1] = {0xa5};
        rejects([&] { WritePdoBits(small, 1, 7, 2, "03"); });
        check(small[0] == 0xa5);
        rejects([&] { WritePdoBits(small, 1, 0, 3, "08"); });
        check(PdoHex({{"type", "UNSIGNED16"}, {"bits", 16}}, "65535") == "FFFF");
        check(PdoValue({{"type", "DINT"}, {"bits", 32}}, "FFFFFFFF") == "-1");
        check(PdoHex({{"type", "INT"}, {"bits", 3}}, "-1") == "07");
        check(PdoValue({{"type", "INT"}, {"bits", 3}}, "07") == "-1");
        rejects([] { PdoHex({{"type", "UINT"}, {"bits", 3}}, "8"); });
        slave = NewSlave(device, 1);
        check(DetectAxisSlots(slave) == std::vector<int>{0});
        auto dual = slave;
        auto rx2 = dual["rxPdos"][0], tx2 = dual["txPdos"][0];
        rx2["index"] = 0x1610;
        rx2["entries"][0]["index"] = 0x6840;
        tx2["index"] = 0x1a10;
        tx2["entries"][0]["index"] = 0x6841;
        dual["rxPdos"].push_back(rx2);
        dual["txPdos"].push_back(tx2);
        check(DetectAxisSlots(dual) == std::vector<int>({0, 1}));
        auto second = NewAxis(dual, 7, 1);
        check(second["mapping"]["Controlword"]["index"] == 0x6840);
        check(second["mapping"]["StatusWord"]["pdo"] == 0x1a10);
        second["mapping"]["StatusWord"] = nullptr;
        SyncAxisMapping(second);
        check(second["enabled"] == false);
        AutoMapAxis(second, dual);
        check(second["enabled"] == true);
        auto duplicate = dual;
        duplicate["rxPdos"].push_back(rx2);
        check(NewAxis(duplicate, 8, 1)["mapping"]["Controlword"].is_null());
        auto unaligned = slave;
        unaligned["rxPdos"][0]["entries"].insert(
            unaligned["rxPdos"][0]["entries"].begin(),
            Json{{"index", 0}, {"subIndex", 0}, {"bits", 1}, {"padding", true}});
        check(NewAxis(unaligned, 0)["mapping"]["Controlword"].is_null());
        std::cout << checks << " project/protocol/PDO/CSV checks passed\n";
        fs::remove_all(root);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nArtifacts: " << root << '\n';
        return 1;
    }
}
