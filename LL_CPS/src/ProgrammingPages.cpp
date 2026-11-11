#include "Studio.h"
#include "LL_ScopePlot.h"
#include "LL_ScopeFFT.h"
#include "LL_STEditor.h"
#include "core/window/window_backend.h"
#include <cmath>
#include <sstream>
#include <iomanip>
namespace app {
static void StoreTask(int index, const Json &config) {
    s.project["hstr"]["files"][index]["content"] = config.dump(2) + "\n";
    Dirty();
}
static void TaskPage(eui::Ui &ui, float w, float h, const Json &file) {
    const int index = s.source;
    auto config = Json::parse(file.value("content", ""), nullptr, false);
    if (!config.is_object()) {
        Text(ui, "task.invalid", "任务配置无法读取，请恢复默认配置后重新设置。", 24, 150, w - 48, 36, 16,
             red);
        Button(ui, "task.repair", "恢复默认任务", 24, 200, 160, [index] {
            Confirm("恢复任务配置？", "将替换当前损坏的任务配置。", [index] {
                StoreTask(index, {{"name", s.project["hstr"]["files"][index]["name"]},
                                  {"type", "Cyclic"},
                                  {"intervalUs", 1000},
                                  {"priority", 0},
                                  {"watchdogUs", 0},
                                  {"cpuAffinity", -1},
                                  {"enabled", true},
                                  {"programs", Json::array()}});
            });
        });
        return;
    }
    auto put = [index, config](const std::string &key, Json value) mutable {
        config[key] = std::move(value);
        StoreTask(index, config);
    };
    auto number = [&](const std::string &key, const std::string &label, float x, float y, float width,
                      long long fallback, long long low, long long high, bool enabled = true) {
        Text(ui, "task.label." + key, label, x, y, width, 22, 12, muted);
        const auto value = config.value(key, fallback);
        Input(
            ui, "task.field." + key, std::to_string(value), x, y + 25, width,
            [put, key, low, high](const std::string &text) mutable {
                std::size_t used = 0;
                const auto n = std::stoll(text, &used);
                if (used != text.size() || n < low || n > high)
                    return;
                put(key, n);
            },
            32, false, enabled);
    };
    Text(ui, "task.title", "任务配置  /  " + file.value("name", "Task"), 24, 145, w - 48, 30, 20, cyan);
    Toggle(
        ui, "task.enabled", "启用任务", config.value("enabled", true), 24, 187,
        [put](bool v) mutable { put("enabled", v); }, true, 150);
    number("priority", "优先级（数值越小越优先）", 200, 177, 260, 0, 0, 31);
    number("cpuAffinity", "CPU 绑定（-1：自动）", 480, 177, 250, -1, -1, 1024);
    Text(ui, "task.type.label", "任务类型", 24, 247, 120, 22, 12, muted);
    const std::vector<std::pair<std::string, std::string>> types = {
        {"Cyclic", "循环"}, {"Event", "事件"}, {"FreeRunning", "自由运行"}};
    for (std::size_t n = 0; n < types.size(); ++n)
        Button(
            ui, "task.type." + types[n].first, types[n].second, 24 + n * 150, 275, 138,
            [put, type = types[n].first]() mutable { put("type", type); }, true,
            config.value("type", "Cyclic") == types[n].first);
    number("intervalUs", "循环间隔 / µs", 24, 325, 240, 1000, 1, 3600000000LL,
           config.value("type", "Cyclic") == "Cyclic");
    number("watchdogUs", "看门狗超时 / µs（0：关闭）", 284, 325, 280, 0, 0, 3600000000LL);
    Text(ui, "task.event.help",
         config.value("type", "Cyclic") == "Event" ? "事件任务由调试面板触发；调用按下方顺序执行。"
                                                   : "程序调用按列表从上到下执行。",
         24, 393, w - 48, 24, 12, muted);
    const float half = (w - 72) / 2;
    Text(ui, "task.calls.title", "程序调用 / 执行顺序", 24, 432, half, 28, 17);
    Text(ui, "task.available.title", "可添加的程序（POU）", 48 + half, 432, half, 28, 17);
    auto calls = config.value("programs", Json::array());
    const float listH = std::max(120.0f, h - 482);
    ui.stack("task.call.list")
        .position(24, 474)
        .size(half, listH)
        .clip()
        .onScroll([](const core::ScrollEvent &e) {
            s.listScroll = std::max(0.0f, s.listScroll - static_cast<float>(e.y) * 34);
            Update();
        })
        .content([&] {
            for (std::size_t n = 0; n < calls.size(); ++n) {
                const float y = n * 38 - s.listScroll;
                if (y + 38 < 0 || y > listH)
                    continue;
                const auto name = calls[n].get<std::string>();
                Button(ui, "task.call.open" + std::to_string(n), std::to_string(n + 1) + "  " + name, 0, y,
                       half - 155, [name] {
                           const auto &files = s.project["hstr"]["files"];
                           for (std::size_t i = 0; i < files.size(); ++i)
                               if (files[i].value("kind", "") == "program" &&
                                   files[i].value("name", "") == name) {
                                   s.source = static_cast<int>(i);
                                   s.visibleSourceTab = -1;
                                   Update();
                                   return;
                               }
                       });
                Button(
                    ui, "task.call.up" + std::to_string(n), "↑", half - 148, y, 40,
                    [put, calls, n]() mutable {
                        std::swap(calls[n], calls[n - 1]);
                        put("programs", calls);
                    },
                    n > 0);
                Button(
                    ui, "task.call.down" + std::to_string(n), "↓", half - 102, y, 40,
                    [put, calls, n]() mutable {
                        std::swap(calls[n], calls[n + 1]);
                        put("programs", calls);
                    },
                    n + 1 < calls.size());
                Button(ui, "task.call.remove" + std::to_string(n), "移除", half - 56, y, 56,
                       [put, calls, n]() mutable {
                           calls.erase(n);
                           put("programs", calls);
                       });
            }
        })
        .build();
    ui.stack("task.program.list")
        .position(48 + half, 474)
        .size(half, listH)
        .clip()
        .onScroll([](const core::ScrollEvent &e) {
            s.taskProgramScroll = std::max(0.0f, s.taskProgramScroll - static_cast<float>(e.y) * 34);
            Update();
        })
        .content([&] {
            float y = -s.taskProgramScroll;
            for (const auto &program : s.project["hstr"]["files"]) {
                if (program.value("kind", "") != "program")
                    continue;
                const auto name = program.value("name", "");
                Button(
                    ui, "task.program.add." + name, "+  " + name, 0, y, half,
                    [put, calls, name]() mutable {
                        calls.push_back(name);
                        put("programs", calls);
                    },
                    std::find(calls.begin(), calls.end(), Json(name)) == calls.end());
                y += 38;
            }
        })
        .build();
    if (calls.empty())
        Text(ui, "task.no.calls", "请从右侧添加至少一个程序。", 24, 473, half, 28, 13, amber);
}
struct BreakpointSync {
    std::vector<Json> requests;
    std::size_t index = 0;
};
static void NextBreakpoint(const std::shared_ptr<BreakpointSync> &job) {
    if (job->index == job->requests.size()) {
        s.busy = false;
        Request({{"op", LL::CpsOp::Run}, {"debug", true}});
        return;
    }
    const auto request = job->requests[job->index++];
    Request(request, [job](const Json &reply) {
        if (reply.value("ok", false))
            NextBreakpoint(job);
        else
            s.busy = false;
    });
}
void StartHstr(bool debug) {
    if (!debug) {
        Request({{"op", LL::CpsOp::Run}, {"debug", false}});
        return;
    }
    auto job = std::make_shared<BreakpointSync>();
    for (const auto &file : s.project["hstr"]["files"]) {
        const auto name = LL::SourceFilename(file), key = s.project.value("id", "") + "/" + name;
        for (unsigned line : s.removedBreakpoints[key])
            job->requests.push_back(
                {{"op", LL::CpsOp::Breakpoint}, {"file", name}, {"line", line}, {"remove", true}});
        for (unsigned line : s.editorBreakpoints[key])
            job->requests.push_back(
                {{"op", LL::CpsOp::Breakpoint}, {"file", name}, {"line", line}, {"remove", false}});
    }
    s.busy = true;
    NextBreakpoint(job);
}
static void SourceInput(eui::Ui &ui, const std::string &id, const std::string &text, float x, float y,
                        float w, float h, unsigned first, std::function<void(const std::string &)> change) {
    const auto filename = LL::SourceFilename(s.project["hstr"]["files"][s.source]);
    const auto key = s.project.value("id", "") + "/" + filename;
    unsigned active = 0;
    if (s.status.value("plcStatus", 0) == 3 && s.status.contains("location")) {
        const auto &loc = s.status["location"];
        if (fs::u8path(loc.value("file", "")).filename().u8string() == filename)
            active = loc.value("line", 0u);
    }
    components::InputStyle style(components::theme::light());
    style.background = panel;
    style.focused = panel;
    style.border = line;
    style.focusBorder = cyan;
    style.text = fg;
    style.radius = 4;
    style.shadow = {};
    components::input(ui, id)
        .position(x, y)
        .size(w, h)
        .value(text)
        .multiline()
        .scrollbar()
        .inset(10)
        .fontSize(s.editorFontSize)
        .fontFamily(fs::exists("C:/Windows/Fonts/consola.ttf") ? "C:/Windows/Fonts/consola.ttf" : "monospace")
        .style(style)
        .codeLines(first, s.editorBreakpoints[key], active)
        .onLineClick([filename, key](unsigned number) {
            const bool remove = s.editorBreakpoints[key].count(number) != 0;
            auto apply = [key, number, remove] {
                if (remove) {
                    s.editorBreakpoints[key].erase(number);
                    s.removedBreakpoints[key].insert(number);
                } else {
                    s.editorBreakpoints[key].insert(number);
                    s.removedBreakpoints[key].erase(number);
                }
                Update();
            };
            const int status = s.status.value("plcStatus", 0);
            if (s.connected && (status == 1 || status == 3 || status == 4))
                Request(
                    {{"op", LL::CpsOp::Breakpoint}, {"file", filename}, {"line", number}, {"remove", remove}},
                    [apply](const Json &r) {
                        if (r.value("ok", false))
                            apply();
                    });
            else
                apply();
        })
        .onZoomWheel([](double delta) {
            if (delta != 0)
                s.editorFontSize = std::clamp(s.editorFontSize + (delta > 0 ? 1.0f : -1.0f), 10.0f, 30.0f);
            Update();
        })
        .onChange([change](const std::string &value) {
            change(value);
            Update();
        })
        .build();
}
void HstrPage(eui::Ui &ui, float w, float h) {
    Button(ui, "hstr.add", "+ 新建文件", 24, 12, 120, [] { AddSource(); });
    Button(ui, "hstr.build", "生成", 155, 12, 85, [] { Build(); }, s.connected && !s.busy, true);
    Button(
        ui, "hstr.run", s.status.value("plcStatus", 0) == 1 ? "运行中" : "运行", 251, 12, 85,
        [] { StartHstr(false); }, s.connected && !s.busy && s.status.value("plcStatus", 0) == 4);
    Button(
        ui, "hstr.debug", "调试运行", 347, 12, 110, [] { StartHstr(true); },
        s.connected && !s.busy && s.status.value("plcStatus", 0) == 4);
    Button(ui, "hstr.stop", "停止", 468, 12, 80, [] { Request({{"op", LL::CpsOp::Stop}}); }, s.connected);
    Button(
        ui, "hstr.reset", "复位", 559, 12, 80, [] { Request({{"op", LL::CpsOp::PlcReset}}); }, s.connected);
    Button(ui, "hstr.monitor", "监视 / 输出", 650, 12, 120, [] { s.debugPanel = !s.debugPanel; });
    auto &files = s.project["hstr"]["files"];
    if (files.empty()) {
        Text(ui, "hstr.empty", "新建程序、变量或任务文件以开始编辑。", 24, 90, w - 48);
        return;
    }
    s.source = std::clamp(s.source, 0, static_cast<int>(files.size() - 1));
    const float viewport = w - 48, tabWidth = 206,
                maxScroll = std::max(0.0f, files.size() * tabWidth - viewport);
    if (s.visibleSourceTab != s.source) {
        const float start = s.source * tabWidth;
        if (start < s.sourceTabScroll)
            s.sourceTabScroll = start;
        if (start + tabWidth > s.sourceTabScroll + viewport)
            s.sourceTabScroll = start + tabWidth - viewport;
        s.visibleSourceTab = s.source;
    }
    s.sourceTabScroll = std::clamp(s.sourceTabScroll, 0.0f, maxScroll);
    ui.stack("hstr.tabs")
        .position(24, 61)
        .size(viewport, 40)
        .clip()
        .onScroll([maxScroll](const core::ScrollEvent &e) {
            s.sourceTabScroll = std::clamp(s.sourceTabScroll - static_cast<float>(e.y) * 80, 0.0f, maxScroll);
            Update();
        })
        .content([&] {
            Rect(ui, "hstr.tabs.bg", 0, 0, viewport, 40, raised);
            for (std::size_t i = 0; i < files.size(); ++i) {
                const float x = i * tabWidth - s.sourceTabScroll;
                if (x + tabWidth < 0 || x > viewport)
                    continue;
                FileItem(ui, "hstr.tab" + std::to_string(i), files[i], x, 0, 200, 38, s.source == i,
                         [i] { s.source = static_cast<int>(i); });
                if (s.source == i)
                    Rect(ui, "hstr.tab.underline", x, 37, 200, 3, FileStyle(files[i]).color);
            }
        })
        .build();
    auto &file = files[s.source];
    const auto fileStyle = FileStyle(file);
    Text(ui, "hstr.filename", "HSTR / " + LL::SourceFilename(file) + "   ·   " + fileStyle.label, 24, 110,
         w - 220, 25, 13, fileStyle.color);
    Button(ui, "hstr.delete", "删除当前文件", w - 180, 105, 155, [] {
        Confirm("删除文件？", "请同时检查任务与程序中的引用。", [] {
            auto &files = s.project["hstr"]["files"];
            files.erase(s.source);
            s.source = 0;
            Dirty();
        });
    });
    if (file.value("kind", "") == "task") {
        TaskPage(ui, w, h, file);
        return;
    }
    float editorY = 145;
    const float editorH = std::max(90.0f, h - editorY - 105);
    const auto key = file.value("name", "");
    const auto sectionKey = s.project.value("id", "") + "/" + key;
    const auto raw = file.value("content", "");
    auto cached = s.sourceSections.find(sectionKey);
    if (cached == s.sourceSections.end() || cached->second.Join() != raw)
        s.sourceSections[sectionKey] = LL::SplitST(raw);
    const auto parts = s.sourceSections.at(sectionKey);
    if (parts.split) {
        const bool declarationOpen = !s.collapsedDeclarations.count(key),
                   bodyOpen = !s.collapsedImplementations.count(key);
        const float available = std::max(60.0f, editorH - 68);
        const float declH = declarationOpen ? (bodyOpen ? available * .38f : available) : 0;
        const float bodyH = bodyOpen ? (declarationOpen ? available - declH : available) : 0;
        Button(
            ui, "hstr.declaration.toggle", (declarationOpen ? "▾  " : ">  ") + std::string("变量声明"), 24,
            editorY, w - 48,
            [key] {
                if (s.collapsedDeclarations.count(key))
                    s.collapsedDeclarations.erase(key);
                else
                    s.collapsedDeclarations.insert(key);
            },
            true, false, 28);
        auto save = [index = s.source, sectionKey](bool declaration, const std::string &value) {
            auto &model = s.sourceSections.at(sectionKey);
            auto text = value;
            if (!text.empty() && text.back() != '\n')
                text += '\n';
            if (declaration)
                model.declarations = text;
            else
                model.body = text;
            s.project["hstr"]["files"][index]["content"] = model.Join();
            Dirty();
        };
        if (declarationOpen)
            SourceInput(ui, "hstr.declarations." + key, parts.declarations, 24, editorY + 32, w - 48, declH,
                        1, [save](const std::string &v) { save(true, v); });
        const float bodyY = editorY + 34 + declH;
        Button(
            ui, "hstr.body.toggle", (bodyOpen ? "▾  " : ">  ") + std::string("程序实现"), 24, bodyY, w - 48,
            [key] {
                if (s.collapsedImplementations.count(key))
                    s.collapsedImplementations.erase(key);
                else
                    s.collapsedImplementations.insert(key);
            },
            true, false, 28);
        if (bodyOpen)
            SourceInput(ui, "hstr.implementation." + key, parts.body, 24, bodyY + 32, w - 48, bodyH,
                        1 + static_cast<unsigned>(
                                std::count(parts.declarations.begin(), parts.declarations.end(), '\n')),
                        [save](const std::string &v) { save(false, v); });
    } else {
        SourceInput(ui, "hstr.editor." + key, raw, 24, editorY, w - 48, editorH, 1,
                    [index = s.source](const std::string &value) {
                        s.project["hstr"]["files"][index]["content"] = value;
                        Dirty();
                    });
    }
    float y = editorY + editorH + 13;
    Text(ui, "hstr.editor.help",
         "单击左侧设置断点 · Ctrl+滚轮调整字号（" + std::to_string(static_cast<int>(s.editorFontSize)) + "）",
         340, y, w - 364, 30, 12, muted);
    const std::vector<std::pair<std::string, std::string>> commands = {
        {"continue", "继续"}, {"over", "步过"}, {"into", "步入"}};
    for (std::size_t i = 0; i < commands.size(); ++i)
        Button(
            ui, "debug." + commands[i].first, commands[i].second, 24 + i * 100, y, 90,
            [cmd = commands[i].first] { Request({{"op", LL::CpsOp::Debug}, {"command", cmd}}); },
            s.connected && s.status.value("plcStatus", 0) == 3);
    if (s.status.value("plcStatus",0)==3 && s.status.contains("location")) {
        auto l = s.status["location"];
        Text(ui, "debug.location", "暂停：" + l.value("file", "") + ":" + std::to_string(l.value("line", 0)) +
             " · 暂停次数 " + std::to_string(l.value("pauseCount",std::uint64_t{0})) + " · 继续至下一断点；逐行请用步过",
             24, y + 42, w - 48, 25, 12, amber);
    } else if(s.debugPending || s.status.value("plcStatus",0)==1) {
        Text(ui,"debug.location",s.debugPending?"正在继续执行…":"运行中，等待下一个断点；逐行执行请使用步过",24,y+42,w-48,25,12,muted);
    }
}
static void Line(eui::Ui &ui, const std::string &id, float x1, float y1, float x2, float y2, eui::Color color,
                 float thickness = 1.5f) {
    const auto dx = x2 - x1, dy = y2 - y1;
    const auto length = std::sqrt(dx * dx + dy * dy);
    if (length < .01f)
        return;
    const auto nx = -dy / length * thickness / 2, ny = dx / length * thickness / 2;
    const float bx = std::min(x1, x2) - thickness, by = std::min(y1, y2) - thickness;
    ui.polygon(id)
        .position(bx, by)
        .size(std::abs(dx) + thickness * 2, std::abs(dy) + thickness * 2)
        .points({{x1 + nx - bx, y1 + ny - by},
                 {x2 + nx - bx, y2 + ny - by},
                 {x2 - nx - bx, y2 - ny - by},
                 {x1 - nx - bx, y1 - ny - by}})
        .color(color)
        .build();
}
void ScopePage(eui::Ui &ui, float w, float h) {
    Heading(ui, "scope", "示波器",
            "独立数据连接 · 控制器采样 · 每批100帧 · 确认/重传/去重。CSV使用采样时间戳。", w);
    Button(ui, "scope.pick", "+ 添加通道", 24, 121, 125, [] {
        s.modal = "variables.scope";
        s.search.clear();
        s.listScroll = 0;
        Variables();
    });
    Button(
        ui, "scope.start",
        s.scopeDraining  ? "收取尾包…"
        : s.scopeRunning ? "停止采样"
                         : "开始采样",
        161, 121, 115,
        [] {
            if (s.scopeRunning) {
                s.scopeDraining = true;
                s.drainDeadline = Clock::now() + std::chrono::seconds(5);
                Request(
                    {{"op", LL::CpsOp::Unsubscribe}, {"token", s.scopeToken}, {"drain", true}},
                    [](const Json &reply) {
                        if (!reply.value("ok", false)) {
                            s.net->CloseDataStream(s.scopeToken);
                            s.scopeDraining = false;
                        }
                    },
                    true);
                s.scopeRunning = false;
            } else {
                s.scopeZoom = s.scopeHasA = s.scopeHasB = s.scopeSpectrum = false;
                s.samples.clear();
                s.scopePlaybackUs = 0;
                ScopeSubscribe();
            }
        },
        s.connected && !s.busy && !s.scopeDraining && !s.channels.empty(), true);
    Input(ui, "scope.window", s.windowMs, 290, 121, 65, [](const std::string &v) {
        if (v.empty() || v.find_first_not_of("0123456789") != std::string::npos)
            return;
        const auto ms = std::stoul(v);
        if (ms < 1 || ms > 3600000)
            return;
        s.windowMs = v;
        Dirty();
    });
    Text(ui, "scope.ms", "窗口 / ms", 290, 99, 100, 20, 12, muted);
    Button(ui, "scope.import", "导入 CSV", 399, 121, 115, [] {
        auto path = Choose("导入示波器 CSV", {"csv"});
        if (!path.empty()) {
            s.scopeRunning = false;
            s.scopeDraining = false;
            s.net->CloseDataStream(s.scopeToken);
            Request({{"op", LL::CpsOp::Unsubscribe}, {"token", s.scopeToken}}, {}, true);
            s.scopeZoom = s.scopeHasA = s.scopeHasB = s.scopeSpectrum = false;
            s.samples = LL::ImportCsv(fs::u8path(path), s.channels);
            s.hidden.clear();
            Log("CSV 波形已导入");
        }
    });
    Button(
        ui, "scope.export", "导出 CSV", 526, 121, 115,
        [] {
            Ask("导出波形", "输入 CSV 完整保存路径", (s.repo->Root() / "wave.csv").u8string(),
                [](const std::string &path) {
                    LL::ExportCsv(fs::u8path(path), s.samples, s.channels);
                    Log("CSV 已导出");
                });
        },
        !s.samples.empty());
    const float px = 80, py = 190, pw = w - 110, ph = std::max(160.0f, h - py - 115);
    Rect(ui, "scope.plot", px, py, pw, ph, panel, 4);
    std::size_t firstVisible = 0;
    std::size_t endVisible = s.samples.size();
    if (s.scopeRunning && !s.samples.empty()) {
        if (!s.scopePlaybackUs) {
            s.scopePlaybackUs = s.samples.front().timestampUs;
            s.scopePlaybackClock = Clock::now();
        }
        // Play genuine timestamped samples from a one-batch buffer at UI frame rate.
        // This avoids 100 ms jumps when a full batch arrives; no synthetic points.
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - s.scopePlaybackClock)
                .count();
        const auto edge = std::min<std::uint64_t>(s.samples.back().timestampUs, s.scopePlaybackUs + elapsed);
        while (endVisible > 1 && s.samples[endVisible - 1].timestampUs > edge)
            --endVisible;
        const auto cutoff = edge > std::stoull(s.windowMs) * 1000 ? edge - std::stoull(s.windowMs) * 1000 : 0;
        while (firstVisible + 1 < s.samples.size() && s.samples[firstVisible].timestampUs < cutoff)
            ++firstVisible;
    }
    if (s.scopeZoom && !s.samples.empty()) {
        firstVisible = 0;
        endVisible = s.samples.size();
        while (firstVisible < endVisible && s.samples[firstVisible].timestampUs < s.scopeX0)
            ++firstVisible;
        while (endVisible > firstVisible && s.samples[endVisible - 1].timestampUs > s.scopeX1)
            --endVisible;
    }
    double min = 0, max = 1;
    bool first = true;
    for (std::size_t n = firstVisible; n < endVisible; ++n)
        for (const auto &value : s.samples[n].values)
            if (!s.hidden.count(value.first)) {
                if (first) {
                    min = max = value.second;
                    first = false;
                } else {
                    min = std::min(min, value.second);
                    max = std::max(max, value.second);
                }
            }
    if (max - min < 1e-9) {
        min -= 1;
        max += 1;
    }
    const auto pad = (max - min) * .07;
    min -= pad;
    max += pad;
    if (s.scopeZoom) {
        min = s.scopeY0;
        max = s.scopeY1;
    }
    const double viewBegin = s.scopeZoom ? s.scopeX0
                             : firstVisible < endVisible
                                 ? static_cast<double>(s.samples[firstVisible].timestampUs)
                                 : 0;
    const double viewEnd = s.scopeZoom ? s.scopeX1
                           : firstVisible < endVisible
                               ? static_cast<double>(s.samples[endVisible - 1].timestampUs)
                               : viewBegin + 1;
    const double viewSpan = std::max(1.0, viewEnd - viewBegin);
    for (int i = 0; i <= 5; ++i) {
        const float y = py + i * ph / 5;
        Line(ui, "scope.grid.y" + std::to_string(i), px, y, px + pw, y, line, 1);
        std::ostringstream label;
        label << std::setprecision(5) << max - i * (max - min) / 5;
        Text(ui, "scope.y" + std::to_string(i), label.str(), 12, y - 9, 62, 20, 10, muted);
    }
    for (int i = 0; i <= 8; ++i) {
        float x = px + i * pw / 8;
        Line(ui, "scope.grid.x" + std::to_string(i), x, py, x, py + ph, line, 1);
        if (firstVisible < endVisible) {
            double time = (viewBegin - s.samples.front().timestampUs + viewSpan * i / 8) / 1000000.0;
            std::ostringstream value;
            value << std::fixed << std::setprecision(2) << time;
            Text(ui, "scope.x" + std::to_string(i), value.str() + " s", x - 12, py + ph + 8, 62, 20, 10,
                 muted);
        }
    }
    if (endVisible - firstVisible < 2)
        Text(ui, "scope.empty",
             s.scopeRunning ? "正在接收实时采样，首包约100ms"
             : s.connected  ? "添加变量并开始采样，或导入 CSV 波形"
                            : "连接控制器以开始采样，或导入 CSV 波形",
             px + 30, py + ph / 2 - 15, pw - 60, 35, 16, muted);
    else {
        const std::vector<eui::Color> colors = {
            cyan, amber, green, {.73f, .47f, .98f, 1}, red, {.35f, .60f, .99f, 1}};
        const double begin = viewBegin, span = viewSpan;
        for (std::size_t c = 0; c < s.channels.size(); ++c) {
            const auto &channel = s.channels[c];
            if (s.hidden.count(channel))
                continue;
            const auto points = LL::ScopePlotIndices(
                s.samples, channel, firstVisible, static_cast<std::size_t>(std::max(100.0f, pw)), endVisible);
            bool previous = false;
            float x0 = 0, y0 = 0;
            for (const auto i : points) {
                if (i == LL::ScopeGap) {
                    previous = false;
                    continue;
                }
                const auto it = s.samples[i].values.find(channel);
                if (it == s.samples[i].values.end()) {
                    previous = false;
                    continue;
                }
                float x = px + static_cast<float>((s.samples[i].timestampUs - begin) / span) * pw;
                float y =
                    py + std::clamp(static_cast<float>((max - it->second) / (max - min)), 0.0f, 1.0f) * ph;
                if (previous)
                    Line(ui, "wave." + std::to_string(c) + "." + std::to_string(i), x0, y0, x, y,
                         colors[c % colors.size()]);
                x0 = x;
                y0 = y;
                previous = true;
            }
        }
    }
    auto nearest = [firstVisible, endVisible, viewBegin, viewSpan, min, max, pw, ph](double nx, double ny) {
        double distance = 1e100;
        s.scopeNearestChannel.clear();
        for (std::size_t i = firstVisible; i < endVisible; ++i)
            for (const auto &v : s.samples[i].values) {
                if (s.hidden.count(v.first))
                    continue;
                const double dx = ((s.samples[i].timestampUs - viewBegin) / viewSpan - nx) * pw;
                const double dy = ((max - v.second) / (max - min) - ny) * ph;
                const double d = dx * dx + dy * dy;
                if (d < distance) {
                    distance = d;
                    s.scopeNearest = s.samples[i];
                    s.scopeNearestChannel = v.first;
                }
            }
    };
    auto cursor = [&](const LL::ScopeSample &sample, bool enabled, const char *id, eui::Color color) {
        if (!enabled || sample.timestampUs < viewBegin || sample.timestampUs > viewEnd)
            return;
        const float x = px + static_cast<float>((sample.timestampUs - viewBegin) / viewSpan) * pw;
        Line(ui, id, x, py, x, py + ph, color, 2);
        Text(ui, std::string(id) + ".label", id, x + 4, py + 4, 40, 20, 12, color);
    };
    cursor(s.scopeCursorA, s.scopeHasA, "A", amber);
    cursor(s.scopeCursorB, s.scopeHasB, "B", red);
    if (s.scopeDragging && s.scopeCursorDrag < 0) {
        const float x = px + std::min(s.scopeDownX, s.scopeMouseX) * pw,
                    y = py + std::min(s.scopeDownY, s.scopeMouseY) * ph;
        Rect(ui, "scope.selection", x, y, std::abs(s.scopeMouseX - s.scopeDownX) * pw,
             std::abs(s.scopeMouseY - s.scopeDownY) * ph, {0, .35f, .65f, .15f});
    }
    ui.rect("scope.interaction")
        .position(px, py)
        .size(pw, ph)
        .color({0, 0, 0, 0})
        .acceptedButtons(core::PointerButton::Left | core::PointerButton::Right)
        .onHover([](bool on) {
            s.scopeHover = on;
            Update();
        })
        .onMove([nearest](const core::PointerEvent &e, const core::Rect &r) {
            s.scopeMouseX = std::clamp((e.x - r.x) / r.width, 0.0, 1.0);
            s.scopeMouseY = std::clamp((e.y - r.y) / r.height, 0.0, 1.0);
            nearest(s.scopeMouseX, s.scopeMouseY);
            Update();
            return true;
        })
        .onPress([nearest, viewBegin, viewSpan, pw](const core::PointerEvent &e, const core::Rect &r) {
            s.scopeMouseX = std::clamp((e.x - r.x) / r.width, 0.0, 1.0);
            s.scopeMouseY = std::clamp((e.y - r.y) / r.height, 0.0, 1.0);
            nearest(s.scopeMouseX, s.scopeMouseY);
            if (e.button == core::PointerButton::Right) {
                s.scopeContext = true;
                Update();
                return;
            }
            if (e.button != core::PointerButton::Left)
                return;
            s.scopeContext = false;
            if (Clock::now() - s.scopeLastClick < std::chrono::milliseconds(350)) {
                s.scopeZoom = false;
                s.scopeDragging = false;
                s.scopeLastClick = {};
                Update();
                return;
            }
            s.scopeLastClick = Clock::now();
            s.scopeDownX = s.scopeMouseX;
            s.scopeDownY = s.scopeMouseY;
            s.scopeDragging = true;
            s.scopeCursorDrag = -1;
            if (s.scopeHasA &&
                std::abs((s.scopeCursorA.timestampUs - viewBegin) / viewSpan - s.scopeMouseX) * pw < 9)
                s.scopeCursorDrag = 0;
            else if (s.scopeHasB &&
                     std::abs((s.scopeCursorB.timestampUs - viewBegin) / viewSpan - s.scopeMouseX) * pw < 9)
                s.scopeCursorDrag = 1;
            Update();
        })
        .onDrag([nearest, px, py, pw, ph](const core::dsl::DragEvent &e) {
            if (e.button != core::PointerButton::Left)
                return;
            s.scopeMouseX = std::clamp(s.scopeDownX + e.totalX / pw, 0.0, 1.0);
            s.scopeMouseY = std::clamp(s.scopeDownY + e.totalY / ph, 0.0, 1.0);
            nearest(s.scopeMouseX, s.scopeMouseY);
            if (s.scopeCursorDrag == 0)
                s.scopeCursorA = s.scopeNearest;
            if (s.scopeCursorDrag == 1)
                s.scopeCursorB = s.scopeNearest;
            Update();
        })
        .onRelease([viewBegin, viewSpan, min, max, pw, ph](const core::PointerEvent &e, const core::Rect &) {
            if (e.button != core::PointerButton::Left || !s.scopeDragging)
                return;
            if (s.scopeCursorDrag < 0 && std::abs(s.scopeMouseX - s.scopeDownX) * pw > 6 &&
                std::abs(s.scopeMouseY - s.scopeDownY) * ph > 6) {
                s.scopeX0 = viewBegin + std::min(s.scopeMouseX, s.scopeDownX) * viewSpan;
                s.scopeX1 = viewBegin + std::max(s.scopeMouseX, s.scopeDownX) * viewSpan;
                s.scopeY0 = max - std::max(s.scopeMouseY, s.scopeDownY) * (max - min);
                s.scopeY1 = max - std::min(s.scopeMouseY, s.scopeDownY) * (max - min);
                s.scopeZoom = true;
                s.scopeLastClick = {};
            }
            s.scopeDragging = false;
            s.scopeCursorDrag = -1;
            Update();
        })
        .build();
    auto fmt = [](double v) {
        std::ostringstream o;
        o << std::setprecision(8) << v;
        return o.str();
    };
    if (s.scopeHover && !s.scopeNearestChannel.empty()) {
        const auto &p = s.scopeNearest;
        Text(ui, "scope.hover.value",
             s.scopeNearestChannel + "   X=" +
                 fmt((static_cast<double>(p.timestampUs) - s.samples.front().timestampUs) / 1000000.0) +
                 " s   Y=" + fmt(p.values.at(s.scopeNearestChannel)),
             px + 10, py + ph - 28, pw - 20, 24, 13, cyan);
    }
    std::string measurement = "左键拖框放大 · 双击还原 · 右键：FFT / 光标";
    if (s.scopeHasA && s.scopeHasB) {
        const auto &c = s.scopeCursorChannel;
        if (s.scopeCursorA.values.count(c) && s.scopeCursorB.values.count(c))
            measurement = c + "   A=" + fmt(s.scopeCursorA.values.at(c)) +
                          "   B=" + fmt(s.scopeCursorB.values.at(c)) + "   ΔX / Δt=" +
                          fmt((static_cast<double>(s.scopeCursorB.timestampUs) - s.scopeCursorA.timestampUs) /
                              1000000.0) +
                          " s   ΔY=" + fmt(s.scopeCursorB.values.at(c) - s.scopeCursorA.values.at(c));
    }
    Text(ui, "scope.measurement", measurement, 24, py + ph + 32, w - 48, 24, 13, cyan);
    if (s.scopeContext) {
        ui.stack("scope.context")
            .position(px + std::min(static_cast<float>(s.scopeMouseX) * pw, pw - 225),
                      py + std::min(static_cast<float>(s.scopeMouseY) * ph, ph - 175))
            .size(225, 175)
            .zIndex(70)
            .content([&] {
                Rect(ui, "scope.context.bg", 0, 0, 225, 175, panel, 5);
                for (int n = 0; n < 2; ++n)
                    Button(
                        ui, "scope.addcursor" + std::to_string(n), n ? "设置光标 B" : "设置光标 A", 5,
                        5 + n * 33, 215,
                        [n] {
                            if (n) {
                                s.scopeCursorB = s.scopeNearest;
                                s.scopeHasB = true;
                            } else {
                                s.scopeCursorA = s.scopeNearest;
                                s.scopeHasA = true;
                            }
                            s.scopeCursorChannel = s.scopeNearestChannel;
                            s.scopeContext = false;
                        },
                        !s.scopeNearestChannel.empty());
                Button(ui, "scope.clearcursor", "清除光标", 5, 71, 215,
                       [] { s.scopeHasA = s.scopeHasB = s.scopeContext = false; });
                Button(ui, "scope.restore", "还原缩放", 5, 104, 215,
                       [] { s.scopeZoom = s.scopeContext = false; });
                Button(
                    ui, "scope.fft", "FFT 频谱（当前通道）", 5, 137, 215,
                    [firstVisible, endVisible] {
                        s.scopeContext = false;
                        const auto channel = s.scopeNearestChannel;
                        std::vector<std::pair<double, double>> points;
                        for (std::size_t i = firstVisible; i < endVisible; ++i) {
                            if (!s.samples[i].values.count(channel) ||
                                (i > firstVisible && s.samples[i].sequence != s.samples[i - 1].sequence + 1))
                                throw std::runtime_error("选区存在缺样，请选择连续有效数据进行FFT");
                            points.push_back({static_cast<double>(s.samples[i].timestampUs),
                                              s.samples[i].values.at(channel)});
                        }
                        if (points.size() < 16)
                            throw std::runtime_error("FFT需要至少16点");
                        std::size_t n = 1;
                        while (n * 2 <= points.size() && n < 8192)
                            n *= 2;
                        const double dt = (points[n - 1].first - points[0].first) / (n - 1);
                        if (dt <= 0)
                            throw std::runtime_error("采样时间戳无效");
                        std::vector<double> uniform;
                        std::size_t j = 1;
                        for (std::size_t k = 0; k < n; ++k) {
                            const double t = points[0].first + k * dt;
                            while (j + 1 < n && points[j].first < t)
                                ++j;
                            const double width = points[j].first - points[j - 1].first;
                            if (width <= 0)
                                throw std::runtime_error("采样时间戳未递增");
                            const double f = std::clamp((t - points[j - 1].first) / width, 0.0, 1.0);
                            uniform.push_back(points[j - 1].second * (1 - f) + points[j].second * f);
                        }
                        s.scopeFft = LL::ScopeFFT(uniform, 1000000.0 / dt);
                        s.scopeSpectrumChannel = channel;
                        s.scopeSpectrum = true;
                    },
                    !s.scopeNearestChannel.empty());
            })
            .build();
    }
    if (s.scopeSpectrum) {
        ui.stack("scope.spectrum")
            .position(px + 20, py + 20)
            .size(pw - 40, ph - 40)
            .zIndex(80)
            .content([&] {
                const float sw = pw - 40, sh = ph - 40;
                Rect(ui, "fft.bg", 0, 0, sw, sh, panel, 5);
                Text(ui, "fft.title", "FFT / " + s.scopeSpectrumChannel + " · Hann窗 · 去均值 · 单边峰值幅度",
                     16, 10, sw - 110, 28, 14, cyan);
                Button(ui, "fft.close", "关闭", sw - 85, 8, 70, [] { s.scopeSpectrum = false; });
                double peak = 1e-12;
                std::size_t peakIndex = 0;
                for (std::size_t i = 0; i < s.scopeFft.size(); ++i)
                    if (s.scopeFft[i].second > peak) {
                        peak = s.scopeFft[i].second;
                        peakIndex = i;
                    }
                if (!s.scopeFft.empty()) {
                    const double last = s.scopeFft.back().first;
                    for (std::size_t i = 1; i < s.scopeFft.size(); ++i)
                        Line(ui, "fft.bin" + std::to_string(i),
                             45 + (sw - 70) * s.scopeFft[i - 1].first / last,
                             sh - 55 - (sh - 120) * s.scopeFft[i - 1].second / peak,
                             45 + (sw - 70) * s.scopeFft[i].first / last,
                             sh - 55 - (sh - 120) * s.scopeFft[i].second / peak, cyan);
                    Text(ui, "fft.info",
                         "0 — " + fmt(last) + " Hz   峰值：" + fmt(s.scopeFft[peakIndex].first) + " Hz / " +
                             fmt(peak),
                         16, sh - 40, sw - 32, 25, 13);
                }
            })
            .build();
    }
    const float y = py + ph + 62;
    for (std::size_t c = 0; c < s.channels.size(); ++c) {
        const auto channel = s.channels[c];
        float x = 24 + (c % 3) * ((w - 64) / 3 + 8);
        Button(
            ui, "scope.channel." + channel, (s.hidden.count(channel) ? "[  ] " : "[x] ") + channel, x,
            y + (c / 3) * 39, (w - 80) / 3 - 30,
            [channel] {
                if (s.hidden.count(channel))
                    s.hidden.erase(channel);
                else
                    s.hidden.insert(channel);
            },
            true, !s.hidden.count(channel));
        Button(
            ui, "scope.remove." + channel, "×", x + (w - 80) / 3 - 25, y + (c / 3) * 39, 28,
            [channel] {
                s.channels.erase(std::remove(s.channels.begin(), s.channels.end(), channel),
                                 s.channels.end());
                s.hidden.erase(channel);
                if (s.scopeRunning)
                    ScopeSubscribe();
            },
            true, false);
    }
    const auto stats = s.net ? s.net->DataStats(s.scopeToken) : Json::object();
    const auto count = stats.value("received", 0ull), beginUs = stats.value("firstTimestampUs", 0ull),
               endUs = stats.value("lastTimestampUs", 0ull);
    const auto rate = endUs > beginUs ? (count - 1) * 1000000.0 / (endUs - beginUs) : 0.0;
    std::ostringstream summary;
    summary << "已收 " << count << " 点 · " << std::fixed << std::setprecision(0) << rate << " Hz · 调度迟到 "
            << stats.value("missed", 0ull) << " · 传输溢出 "
            << stats.value("discarded", 0ull) + stats.value("clientDropped", 0ull) << " · 重传 "
            << stats.value("replayed", 0ull) << " · 校验失败 " << stats.value("crcErrors", 0ull);
    if (!s.samples.empty())
        summary << " · 显示 " << s.samples.size() << " 点";
    Text(ui, "scope.stats", summary.str(), 24, 159, w - 48, 23, 11, muted);
    const auto error = s.net ? s.net->DataError(s.scopeToken) : "";
    if (!error.empty())
        Text(ui, "scope.error", error, 24, 183, w - 48, 23, 11, red);
}
void LogsPage(eui::Ui &ui, float w, float h) {
    Heading(ui, "logs", "日志", "查看本次会话日志；支持文本选择、Ctrl+C及按关键词筛选。", w);
    Input(ui, "logs.search", s.logSearch, 24, 122, std::max(180.0f, w - 450),
          [](const std::string &v) { s.logSearch = v; });
    std::string all, filtered;
    auto lower = [](std::string text) {
        for (auto &c : text)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text;
    };
    const auto query = lower(s.logSearch);
    std::size_t hits = 0;
    for (const auto &entry : s.log) {
        all += entry + "\n";
        std::istringstream input(entry);
        std::string row;
        while (std::getline(input, row))
            if (query.empty() || lower(row).find(query) != std::string::npos) {
                filtered += row + "\n";
                ++hits;
            }
    }
    Button(
        ui, "logs.copy.filtered", "复制筛选结果", w - 410, 122, 145,
        [filtered] { core::window::setClipboardText(filtered); }, !filtered.empty());
    Button(
        ui, "logs.copy.all", "复制全部", w - 253, 122, 105, [all] { core::window::setClipboardText(all); },
        !all.empty());
    Button(ui, "logs.clear.search", "清除搜索", w - 136, 122, 112, [] { s.logSearch.clear(); });
    Text(ui, "logs.count",
         "保留 " + std::to_string(s.log.size()) + " 条记录 · 匹配 " + std::to_string(hits) + " 行", 24, 165,
         w - 48, 24, 12, muted);
    Input(
        ui, "logs.content", filtered, 24, 200, w - 48, [](const std::string &) {}, std::max(100.0f, h - 220),
        true);
}
void Bottom(eui::Ui &ui, float w, float h) {
    Rect(ui, "bottom.bg", 0, 0, w, h, panel);
    Button(ui, "bottom.watch", "变量监视", 16, 7, 105, [] { s.bottomTab = 0; }, true, s.bottomTab == 0, 28);
    Button(
        ui, "bottom.output", "生成与输出", 129, 7, 115, [] { s.bottomTab = 1; }, true, s.bottomTab == 1, 28);
    Button(ui, "bottom.tasks", "任务统计", 252, 7, 105, [] { s.bottomTab = 2; }, true, s.bottomTab == 2, 28);
    Button(ui, "bottom.close", "收起", w - 75, 7, 60, [] { s.debugPanel = false; }, true, false, 28);
    Rect(ui, "bottom.line", 0, 43, w, 1, line);
    if (s.bottomTab == 1) {
        Button(
            ui, "output.full.logs", "查看完整日志", w - 242, 7, 155, [] { Page("logs"); }, true, false, 28);
        std::string out;
        const auto count = std::min<std::size_t>(s.log.size(), 10);
        for (auto i = s.log.size() - count; i < s.log.size(); ++i)
            out += s.log[i] + "\n";
        Text(ui, "output", out, 18, 54, w - 36, h - 60, 12, muted);
        return;
    }
    if (s.bottomTab == 2) {
        float y = 56;
        if (s.status.contains("tasks"))
            for (auto &t : s.status["tasks"]) {
                Text(ui, "task." + t.value("name", ""),
                     t.value("name", "") + "    执行 " + std::to_string(t.value("executions", 0ull)) +
                         "    错过 " + std::to_string(t.value("missedPeriods", 0ull)) + "    最近 / 最大 " +
                         std::to_string(t.value("lastDurationUs", 0ull)) + " / " +
                         std::to_string(t.value("maxDurationUs", 0ull)) + " µs",
                     18, y, w - 180, 24, 12, muted);
                bool event = false;
                if (HasProject())
                    for (const auto &file : s.project["hstr"]["files"]) {
                        if (file.value("kind", "") != "task" || file.value("name", "") != t.value("name", ""))
                            continue;
                        const auto config = Json::parse(file.value("content", ""), nullptr, false);
                        event = config.is_object() && config.value("type", "") == "Event";
                    }
                if (event)
                    Button(
                        ui, "task.trigger." + t.value("name", ""), "触发事件", w - 150, y, 125,
                        [name = t.value("name", "")] {
                            Request({{"op", LL::CpsOp::Debug}, {"command", "event"}, {"task", name}});
                        },
                        s.connected && s.status.value("plcStatus", 0) == 1, false, 25);
                y += 29;
            }
        return;
    }
    Input(ui, "watch.name", s.watchName, 16, 54, 240, [](const std::string &v) { s.watchName = v; });
    Button(
        ui, "watch.picker", "搜索变量", 266, 54, 100,
        [] {
            s.modal = "variables.watch";
            s.search.clear();
            s.listScroll = 0;
            Variables();
        },
        s.connected);
    Button(
        ui, "watch.add", "添加", 376, 54, 65,
        [] {
            if (s.watchName.empty())
                return;
            if (std::find(s.watches.begin(), s.watches.end(), s.watchName) == s.watches.end())
                s.watches.push_back(s.watchName);
        },
        !s.watchName.empty());
    Input(ui, "watch.value", s.watchValue, 453, 54, 145, [](const std::string &v) { s.watchValue = v; });
    Button(
        ui, "watch.write", "写入", 610, 54, 65,
        [] { Request({{"op", LL::CpsOp::Write}, {"name", s.watchName}, {"value", s.watchValue}}); },
        s.connected && !s.watchName.empty());
    Button(
        ui, "watch.force", "强制", 686, 54, 65,
        [] { Request({{"op", LL::CpsOp::Force}, {"name", s.watchName}, {"value", s.watchValue}}); },
        s.connected && !s.watchName.empty());
    Button(
        ui, "watch.release", "取消强制", 762, 54, 100,
        [] { Request({{"op", LL::CpsOp::Release}, {"name", s.watchName}}); },
        s.connected && !s.watchName.empty());
    float y = 100;
    for (const auto &name : s.watches) {
        if (y + 28 > h)
            break;
        Json value = s.values.value(name, Json::object());
        Button(
            ui, "watch.row." + name, name, 16, y, 320, [name] { s.watchName = name; }, true,
            s.watchName == name, 26);
        Text(ui, "watch.val." + name,
             value.value("value", value.value("error", s.connected ? "等待数据" : "离线")), 350, y, w - 475,
             26, 12, value.value("forced", false) ? amber : fg);
        Button(
            ui, "watch.del." + name, "移除", w - 100, y, 80,
            [name] {
                s.watches.erase(std::remove(s.watches.begin(), s.watches.end(), name), s.watches.end());
            },
            true, false, 26);
        y += 31;
    }
}
} // namespace app
