#include "Studio.h"
#include "LL_ScopeFFT.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <cstdlib>
#include <cmath>
#include <fstream>
namespace app {
// Application-owned render check. It never sends motion commands or injects OS input.
void UiRenderCheck() {
    const auto *folder = std::getenv("LLECP_UI_CHECK");
    if (!folder || !*folder)
        return;
    static unsigned frame = 0;
    ++frame;
    const auto output = fs::u8path(folder);
    fs::create_directories(output);
    auto screenshot = [&](const char *name) {
        auto *window = glfwGetCurrentContext();
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        std::vector<unsigned char> pixels(static_cast<std::size_t>(w) * h * 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_FRONT);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        stbi_flip_vertically_on_write(1);
        if (!stbi_write_png((output / (std::string(name) + ".png")).string().c_str(), w, h, 3, pixels.data(),
                            w * 3))
            throw std::runtime_error("Screenshot failed");
    };
    if (frame == 20)
        screenshot("01-project-center");
    if (frame == 25) {
        auto project = s.repo->Create("Motion Cell / 界面验证");
        Enter(project);
        AddMaster();
        s.accept("EtherCAT_1");
        s.modal.clear();
        (*Master())["simulation"] = true;
        (*Master())["nic"] = "Simulation";
        const auto xml =
            R"(<EtherCATInfo><Vendor><Id>#x00000001</Id><Name>LLECP QA</Name></Vendor><Descriptions><Devices><Device Physics="YY"><Type ProductCode="#x1000" RevisionNo="#x0001">QA_CiA402</Type><Name>CiA 402 测试驱动</Name><RxPdo Sm="2" Mandatory="1"><Index>#x1600</Index><Name>Position command</Name><Entry><Index>#x6040</Index><SubIndex>0</SubIndex><BitLen>16</BitLen><Name>Controlword</Name><DataType>UINT</DataType></Entry><Entry><Index>#x607A</Index><SubIndex>0</SubIndex><BitLen>32</BitLen><Name>Target position</Name><DataType>DINT</DataType></Entry></RxPdo><TxPdo Sm="3" Mandatory="1"><Index>#x1A00</Index><Name>Position feedback</Name><Entry><Index>#x6041</Index><SubIndex>0</SubIndex><BitLen>16</BitLen><Name>Statusword</Name><DataType>UINT</DataType></Entry><Entry><Index>#x6064</Index><SubIndex>0</SubIndex><BitLen>32</BitLen><Name>Actual position</Name><DataType>DINT</DataType></Entry></TxPdo><Dc><OpMode><Name>DC Sync0</Name><AssignActivate>#x0300</AssignActivate></OpMode></Dc></Device></Devices></Descriptions></EtherCATInfo>)";
        LL::WriteAtomic(s.repo->Root() / "ESI" / "QA.xml", xml);
        s.catalog = LL::LoadDeviceCatalog(s.repo->Root() / "ESI");
        ImportSlave(s.catalog.at(0));
        GenerateAxes();
        if ((*Slave())["axes"].size() != 1)
            throw std::runtime_error("PDO axis generation failed");
        s.axis = 0;
        Save();
        Page("overview");
    }
    if (frame == 45)
        screenshot("02-overview");
    if (frame == 50)
        Page("master");
    if (frame == 70)
        screenshot("03-master");
    if (frame == 75)
        Page("slave");
    if (frame == 95)
        screenshot("04-slave");
    if (frame == 100) {
        s.connected = true;
        s.pendingPoll = true;
        s.lastStatus = Clock::now();
        s.status = {{"axes", Json::array({Json{{"id", 0},
                                               {"master", "EtherCAT_1"},
                                               {"stateName", "错误停止"},
                                               {"available", true},
                                               {"enabled", false},
                                               {"moving", false},
                                               {"error", true},
                                               {"errorCode", 0},
                                               {"statusWord", 0x23b},
                                               {"controlWord", 0},
                                               {"actualPosition", 1.1186676},
                                               {"actualVelocity", -.0015258789},
                                               {"actualTorque", 0.0},
                                               {"mode", 8}}})}};
        Log("轴状态表离线渲染测试数据");
        Page("axis");
    }
    if (frame == 120)
        screenshot("05-axis");
    if(frame==121){s.axisTab=2;s.pageScroll=0;s.debugPanel=false;}
    if(frame==124)screenshot("20-axis-motion-layout");
    if(frame==125){s.axisTab=1;}
    if(frame==128)screenshot("21-axis-parameters-tab");
    if(frame==129){s.axisTab=0;}
    if(frame==132)screenshot("22-axis-mapping-tab");
    if (frame == 135) {
        s.connected = false;
        s.pendingPoll = false;
        s.status = Json::object();
        s.project["hstr"]["files"].push_back(
            {{"name", "ScaleValue"},
             {"kind", "function"},
             {"content", "FUNCTION ScaleValue : REAL\nVAR_INPUT\n    Value : REAL;\nEND_VAR\nScaleValue := "
                         "Value;\nEND_FUNCTION\n"}});
        s.project["hstr"]["files"].push_back(
            {{"name", "AxisControl"},
             {"kind", "function"},
             {"content", "FUNCTION_BLOCK AxisControl\nVAR_INPUT\n    Enable : BOOL;\nEND_VAR\nVAR_OUTPUT\n   "
                         " Ready : BOOL;\nEND_VAR\n\nReady := Enable;\nEND_FUNCTION_BLOCK\n"}});
        s.project["hstr"]["files"].push_back(
            {{"name", "MotionTypes"},
             {"kind", "type"},
             {"content", "TYPE\n    AxisMode : (Idle, Moving, Homing);\nEND_TYPE\n"}});
        for (int i = 1; i <= 12; ++i)
            s.project["hstr"]["files"].push_back(
                {{"name", "Program_" + std::to_string(i)},
                 {"kind", "program"},
                 {"content", "PROGRAM Program_" + std::to_string(i) + "\nEND_PROGRAM\n"}});
        Page("hstr");
        s.source = 4;
    }
    if(frame==138){
        const auto& file=s.project["hstr"]["files"][s.source];const auto name=LL::SourceFilename(file);
        s.editorBreakpoints[s.project.value("id","")+"/"+name].insert(9);
        s.status={{"plcStatus",3},{"location",{{"file",name},{"line",9},{"pauseCount",1}}}};
        s.editorFontSize=18;
    }
    if(frame==139)
        screenshot("17-debug-paused");
    if(frame==140){
        s.debugPending=true;
        s.status["plcStatus"]=1;
        Update();
    }
    if(frame==142)
        screenshot("18-debug-resuming");
    if(frame==143){
        s.debugPending=false;
        s.status["plcStatus"]=3;
        s.status["location"]["pauseCount"]=2;
        Update();
    }
    if (frame == 145)
        screenshot("06-hstr");
    if (frame == 146) {
        s.source = 2;
    }
    if (frame == 149)
        screenshot("16-task-editor");
    if (frame == 150) {
        Page("scope");
        s.channels = {"QA.Position", "QA.Velocity"};
        for (int i = 0; i < 600; ++i) {
            LL::ScopeSample sample;
            sample.timestampUs = i * 10000;
            sample.sequence = i;
            sample.values = {{"QA.Position", 20 * std::sin(i * .02)},
                             {"QA.Velocity", 12 * std::cos(i * .02)}};
            s.samples.push_back(sample);
        }
        LL::ExportCsv(output / "render-fixture.csv", s.samples, s.channels);
        s.samples = LL::ImportCsv(output / "render-fixture.csv", s.channels);
        Log("离线 CSV 渲染测试数据");
    }
    if (frame == 170)
        screenshot("07-scope");
    if (frame == 171) {
        s.scopeCursorA = s.samples[100];
        s.scopeCursorB = s.samples[300];
        s.scopeHasA = s.scopeHasB = true;
        s.scopeCursorChannel = "QA.Position";
    }
    if (frame == 175)
        screenshot("17-scope-cursors");
    if (frame == 177) {
        std::vector<double> values;
        for (const auto &p : s.samples)
            values.push_back(p.values.at("QA.Position"));
        s.scopeFft = LL::ScopeFFT(values, 100);
        s.scopeSpectrumChannel = "QA.Position";
        s.scopeSpectrum = true;
    }
    if (frame == 181)
        screenshot("18-scope-fft");
    if (frame == 185) {
        s.scopeSpectrum = false;
        Page("devices");
    }
    if (frame == 195)
        screenshot("08-devices");
    if (frame == 200) {
        Connect();
    }
    if (frame == 220)
        screenshot("09-connect");
    if (frame == 225) {
        s.modal.clear();
        Page("hstr");
        AddSource();
    }
    if (frame == 245)
        screenshot("10-add-source");
    if (frame == 250) {
        s.modal.clear();
        Page("master");
        // Explicit UI fixtures; suppress polling so these never contact a controller.
        s.connected = true;
        s.pendingPoll = true;
        s.lastStatus = Clock::now();
        s.status = {{"state", "StandBy"},
                    {"masters", Json::array({Json{{"id", "EtherCAT_1"}, {"running", false}}})}};
        Log("LED离线渲染夹具：以下状态来自测试数据，不代表已连接设备");
    }
    if (frame == 270)
        screenshot("11-led-standby-fixture");
    if (frame == 275) {
        s.status["masters"][0]["running"] = true;
        s.lastStatus = Clock::now();
    }
    if (frame == 295)
        screenshot("12-led-running-fixture");
    if (frame == 300) {
        s.status["controllerError"] = true;
        OperationError(
            "界面测试夹具：Master EtherCAT_1, slave 1, PDO assignment 0x1c12: cannot clear assignment");
        s.modal = "error";
        s.title = "控制器操作失败";
        s.detail = s.operationError;
    }
    if (frame == 320)
        screenshot("13-led-error-fixture");
    if (frame == 325) {
        s.modal.clear();
        s.operationError.clear();
        s.status["controllerError"] = false;
        Page("slave");
        s.pendingPdo = true;
        s.debugPanel = false;
        s.pageScroll = 170;
        (*Master())["simulation"] = false;
        s.pdoContext = (*Master()).at("id").get<std::string>() + "/" +
                       std::to_string((*Slave()).at("id").get<int>()) + "/0";
        s.pdo = {{"ok", true},
                 {"simulation", false},
                 {"inputValid", true},
                 {"actualWkc", 3},
                 {"expectedWkc", 3},
                 {"cycles", 1234},
                 {"inputHex", "400000000000"},
                 {"outputHex", "000000000000"},
                 {"warning", ""},
                 {"entries", Json::array()},
                 {"total", 4}};
        for (const bool rx : {true, false})
            for (const auto &p : (*Slave()).at(rx ? "rxPdos" : "txPdos"))
                for (std::size_t n = 0; n < p.at("entries").size(); ++n)
                    s.pdo["entries"].push_back(
                        {{"key", LL::PdoKey(rx, p.at("index"), static_cast<unsigned>(n))},
                         {"value", !rx && n == 0 ? "64" : "0"},
                         {"valid", true},
                         {"writable", false},
                         {"reason", "渲染测试数据"}});
        Log("PDO输入质量离线渲染夹具，数值来自测试数据");
    }
    if (frame == 345)
        screenshot("14-pdo-valid-fixture");
    if (frame == 350) {
        s.pdo["inputValid"] = false;
        s.pdo["actualWkc"] = 0;
        s.pdo["warning"] = "输入过程数据无效：请检查WKC、RX错误与从站状态";
        for (auto &row : s.pdo["entries"])
            row["valid"] = false;
    }
    if (frame == 370)
        screenshot("15-pdo-invalid-fixture");
    if(frame==371)s.menu="文件";
    if(frame==374)screenshot("19-top-menu");
    if(frame==375){s.menu.clear();Log("Native compilation failed:\nPOUS.c: error: example compiler diagnostic\n完整日志离线渲染夹具");Page("logs");}
    if(frame==380)screenshot("23-log-viewer");
    if(frame==381)s.logSearch="error";
    if(frame==385)screenshot("24-log-search");
    if(frame==386){
        Page("scope");
        s.modal="variables.scope";
        s.scopePickAxis=true;
        s.search.clear();s.listScroll=0;
        s.variables=Json::array({
            Json{{"name","Axis.0.ActualPosition"},{"displayName","Axis_0 / 实际位置"},{"source","Axis"},{"type","LREAL"},{"master","EtherCAT_1"}},
            Json{{"name","Axis.0.ActualVelocity"},{"displayName","Axis_0 / 实际速度"},{"source","Axis"},{"type","LREAL"},{"master","EtherCAT_1"}},
            Json{{"name","Axis.0.StatusWord"},{"displayName","Axis_0 / 状态字"},{"source","Axis"},{"type","LREAL"},{"master","EtherCAT_1"}},
            Json{{"name","Main.Counter"},{"source","HSTR"},{"type","DINT"}},
            Json{{"name","Main.ActualPosition"},{"source","HSTR"},{"type","LREAL"}}
        });
        Update();
    }
    if(frame==395)screenshot("25-scope-pick-axis");
    if(frame==397){s.scopePickAxis=false;Update();}
    if(frame==407)screenshot("26-scope-pick-hstr");
    if (frame == 420) {
        LL::WriteAtomic(
            output / "result.json",
            Json{{"ok", true},
                 {"screenshots", 15},
                 {"project", s.project.at("id")},
                 {"note", "Offline rendering and project persistence; no physical controller was connected"}}
                .dump(2));
        glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE);
    }
}
} // namespace app
