#include "Studio.h"
namespace app {
const DslAppConfig &dslAppConfig() {
    static const auto config = DslAppConfig{}
                                   .title("LLECP | Engineering Studio")
                                   .pageId("ll_cps")
                                   .windowSize(1440, 960)
                                   .minWindowSize(1150, 780)
                                   .clearColor(bg)
                                   .textFont(
#ifdef _WIN32
                                       "C:/Windows/Fonts/msyh.ttc"
#else
                                       fs::exists("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc")
                                           ? "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"
                                           : ""
#endif
                                       )
                                   .fps(30)
                                   .showDebugStatsInTitle(false)
                                   .showDebugOverlay(false)
                                   .onStart(Init)
                                   .onShutdown(Stop);
    return config;
}
void compose(eui::Ui &ui, const eui::Screen &screen) {
    const float w = screen.width, h = screen.height, nav = s.page == "hstr" ? 290.0f : 260.0f,
                bottom = s.debugPanel ? 240.0f : 0;
    const float top = s.page == "hstr" ? 55.0f : 116.0f;
    ui.stack("root")
        .size(w, h)
        .onFrame([](float) { Tick(); })
        .content([&] {
            Rect(ui, "background", 0, 0, w, h, bg);
            Rect(ui, "menubar", 0, 0, w, 54, panel);
            Text(ui, "brand", "LL / CPS", 20, 14, 132, 28, 18, cyan);
            const std::vector<std::string> menus = {"文件", "生成", "PLC", "调试", "工具", "日志"};
            for (std::size_t i = 0; i < menus.size(); ++i) {
                const auto label=menus[i];
                const bool open=s.menu==label||(label=="日志"&&s.page=="logs");
                components::ButtonStyle style(components::theme::light(),false);
                style.normal=open?eui::Color{.86f,.93f,.99f,1}:eui::Color{.93f,.95f,.97f,1};
                style.hover={.86f,.93f,.99f,1};style.pressed={.80f,.89f,.97f,1};
                style.text=fg;style.radius=4;style.pressScale=1;
                style.border.width=1;style.border.color=open?eui::Color{.55f,.72f,.88f,1}:line;
                style.shadow.blur=5;style.shadow.spread=0;
                components::button(ui,"top."+label).position(MenuStartX+i*MenuStep,MenuTop)
                    .size(MenuWidth,MenuHeight).fontSize(12).text(label).style(style)
                    .onClick([label]{if(label=="日志")Page("logs");else s.menu=s.menu==label?"":label;Update();}).build();
            }
            Button(
                ui, "connect", s.connected ? "已连接  " + s.ip : "连接控制器", w - 255, 11, 230,
                [] { Connect(); }, HasProject() && !s.busy, s.connected);
            const bool stale =
                s.connected && !s.busy && Clock::now() - s.lastStatus > std::chrono::seconds(3);
            const bool fault = s.status.value("controllerError", false) ||
                               !s.status.value("commandError", std::string{}).empty() ||
                               !s.operationError.empty();
            const bool emergency = s.status.value("emergency", false);
            const auto led = !s.connected         ? muted
                             : emergency || fault ? red
                             : stale || s.busy    ? amber
                                                  : green;
            const std::string controllerState = !s.connected   ? "控制器离线"
                                                : emergency    ? "控制器急停"
                                                : fault        ? "控制器故障"
                                                : s.busy       ? "控制器处理中"
                                                : stale        ? "状态更新超时"
                                                : AnyRunning() ? "控制器运行"
                                                               : "控制器待机";
            Rect(ui, "controller.led.halo", w - 489, 17, 22, 22, {led.r, led.g, led.b, .12f}, 11);
            Rect(ui, "controller.led", w - 484, 22, 12, 12, led, 6);
            Text(ui, "controller.state", controllerState, w - 458, 17, 195, 26, 13, led);
            if (s.page != "hstr") {
                Rect(ui, "toolbar", 0, 55, w, 60, raised);
                Text(ui, "breadcrumb", HasProject() ? s.project.value("name", "") : "LLECP / 项目中心", 20,
                     72, 350, 28, 14);
                Button(ui, "start",
                       !s.operation.empty() ? "处理中…"
                       : Running()          ? "■ 停止主站"
                                            : "▶ 启动主站",
                       w - 490, 68, 145, MasterPower,
                       s.connected && Master() && !s.busy && !s.status.value("emergency", false), Running());
                if (!s.operationError.empty())
                    Button(ui, "operation.error", "操作失败 · 点击查看原因", 380, 68,
                           std::max(150.0f, w - 890), [] {
                               s.modal = "error";
                               s.title = "控制器操作失败";
                               s.detail = s.operationError;
                           });
                else if (!s.operation.empty())
                    Text(ui, "operation.progress", s.operation, 380, 72, std::max(150.0f, w - 890), 28, 13,
                         amber);
                Button(
                    ui, "emergency", s.status.value("emergency", false) ? "解除急停" : "急停", w - 330, 68,
                    130,
                    [] {
                        Request({{"op", s.status.value("emergency", false) ? LL::CpsOp::EmergencyRelease
                                                                           : LL::CpsOp::Emergency}});
                    },
                    s.connected, true);
                Button(
                    ui, "reset", "复位", w - 185, 68, 80, [] { Request({{"op", LL::CpsOp::Reset}}); },
                    s.connected);
                Button(ui, "showdebug", "监视", w - 94, 68, 70, [] { s.debugPanel = !s.debugPanel; });
            }
            const float mainH = h - top - 28 - bottom;
            ui.stack("nav.clip")
                .position(0, top)
                .size(nav, mainH + bottom)
                .clip()
                .onScroll([](const core::ScrollEvent &e) {
                    s.treeScroll = std::max(0.0f, s.treeScroll - static_cast<float>(e.y) * 34);
                    Update();
                })
                .content([&] {
                    ui.stack("nav.content")
                        .position(0, s.page == "hstr" ? 0 : -s.treeScroll)
                        .size(nav, 4000)
                        .content([&] { Navigation(ui, nav, s.page == "hstr" ? mainH + bottom : 4000); })
                        .build();
                })
                .build();
            ui.stack("page.clip")
                .position(nav + 1, top)
                .size(w - nav - 1, mainH)
                .clip()
                .onScroll([](const core::ScrollEvent &e) {
                    if (s.page != "hstr" && s.page != "scope" && s.page != "logs")
                        s.pageScroll = std::max(0.0f, s.pageScroll - static_cast<float>(e.y) * 38);
                    Update();
                })
                .content([&] {
                    ui.stack("page")
                        .position(0, -s.pageScroll)
                        .size(w - nav - 1, std::max(mainH, 4000.0f))
                        .content([&] {
                            const auto pw = w - nav - 1;
                            if (s.page == "logs")
                                LogsPage(ui,pw,mainH);
                            else if (s.page == "projects")
                                ProjectPage(ui, pw, mainH);
                            else if (s.page == "devices")
                                DevicesPage(ui, pw, mainH);
                            else if (!HasProject())
                                ProjectPage(ui, pw, mainH);
                            else if (s.page == "overview")
                                Overview(ui, pw, mainH);
                            else if (s.page == "master")
                                MasterPage(ui, pw, mainH);
                            else if (s.page == "scan")
                                ScanPage(ui, pw, mainH);
                            else if (s.page == "slave")
                                SlavePage(ui, pw, mainH);
                            else if (s.page == "axis")
                                AxisPage(ui, pw, mainH);
                            else if (s.page == "hstr")
                                HstrPage(ui, pw, mainH);
                            else if (s.page == "scope")
                                ScopePage(ui, pw, mainH);
                        })
                        .build();
                })
                .build();
            if (s.debugPanel)
                ui.stack("bottom")
                    .position(nav + 1, h - bottom - 28)
                    .size(w - nav - 1, bottom)
                    .clip()
                    .content([&] { Bottom(ui, w - nav - 1, bottom); })
                    .build();
            Rect(ui, "status.bg", 0, h - 28, w, 28, raised);
            Text(ui, "status.text",
                 std::string(s.connected ? "● ONLINE" : "○ OFFLINE") + "   |   " +
                     (s.status.value("emergency", false) ? "急停" : s.status.value("state", "控制器未连接")) +
                     "   |   PLC " + std::to_string(s.status.value("plcStatus", 0)) + "   |   " +
                     (s.busy    ? "处理中…"
                      : s.dirty ? "有未保存修改"
                                : "就绪"),
                 16, h - 24, w - 380, 22, 11, s.connected ? green : muted);
            Text(ui, "status.tech", "EUI-NEO / HSTR / EtherCAT", w - 305, h - 24, 290, 22, 11, muted);
            MenuOverlay(ui);
            Modal(ui, w, h);
        })
        .build();
}
} // namespace app
