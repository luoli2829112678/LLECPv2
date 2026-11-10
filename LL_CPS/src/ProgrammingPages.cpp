#include "Studio.h"
#include "LL_ScopePlot.h"
#include <cmath>
#include <sstream>
#include <iomanip>
namespace app {
void HstrPage(eui::Ui &ui, float w, float h) {
    Button(ui, "hstr.add", "+ 新建文件", 24, 12, 120, [] { AddSource(); });
    Button(ui, "hstr.build", "生成", 155, 12, 85, [] { Build(); }, s.connected && !s.busy, true);
    Button(
        ui, "hstr.run", "运行", 251, 12, 85, [] { Request({{"op", LL::CpsOp::Run}, {"debug", false}}); },
        s.connected && !s.busy);
    Button(
        ui, "hstr.debug", "调试运行", 347, 12, 110,
        [] { Request({{"op", LL::CpsOp::Run}, {"debug", true}}); }, s.connected && !s.busy);
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
    float editorY = 145;
    const float editorH = std::max(90.0f, h - editorY - 105);
    Input(
        ui, "hstr.editor." + file.value("name", ""), file.value("content", ""), 24, editorY, w - 48,
        [index = s.source](const std::string &value) {
            s.project["hstr"]["files"][index]["content"] = value;
            Dirty();
        },
        editorH, true);
    float y = editorY + editorH + 13;
    Input(ui, "break.line", s.breakLine, 24, y, 65, [](const std::string &value) { s.breakLine = value; });
    Button(
        ui, "break.add", "添加断点", 100, y, 110,
        [] {
            Request({{"op", LL::CpsOp::Breakpoint},
                     {"file", LL::SourceFilename(s.project["hstr"]["files"][s.source])},
                     {"line", std::stoul(s.breakLine)},
                     {"remove", false}});
        },
        s.connected);
    Button(
        ui, "break.remove", "移除断点", 220, y, 110,
        [] {
            Request({{"op", LL::CpsOp::Breakpoint},
                     {"file", LL::SourceFilename(s.project["hstr"]["files"][s.source])},
                     {"line", std::stoul(s.breakLine)},
                     {"remove", true}});
        },
        s.connected);
    const std::vector<std::pair<std::string, std::string>> commands = {
        {"continue", "继续"}, {"over", "步过"}, {"into", "步入"}};
    for (std::size_t i = 0; i < commands.size(); ++i)
        Button(
            ui, "debug." + commands[i].first, commands[i].second, 345 + i * 90, y, 80,
            [cmd = commands[i].first] { Request({{"op", LL::CpsOp::Debug}, {"command", cmd}}); },
            s.connected && s.status.value("plcStatus", 0) == 3);
    if (s.status.contains("location")) {
        auto l = s.status["location"];
        Text(ui, "debug.location", "暂停：" + l.value("file", "") + ":" + std::to_string(l.value("line", 0)),
             24, y + 42, w - 48, 25, 12, amber);
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
                s.samples.clear();
                s.scopePlaybackUs = 0;
                ScopeSubscribe();
            }
        },
        s.connected && !s.busy && !s.scopeDraining && !s.channels.empty(), true);
    Input(
        ui, "scope.interval", s.interval, 290, 121, 65, [](const std::string &v) { s.interval = v; }, 32,
        false, !s.scopeRunning && !s.scopeDraining);
    Text(ui, "scope.ms", "ms", 361, 125, 30, 24, 12, muted);
    Button(
        ui, "scope.realtime", "实时 1 ms", 655, 121, 110, [] { s.interval = "1"; },
        !s.scopeRunning && !s.scopeDraining);
    Button(ui, "scope.import", "导入 CSV", 399, 121, 115, [] {
        auto path = Choose("导入示波器 CSV", {"csv"});
        if (!path.empty()) {
            s.scopeRunning = false;
            s.scopeDraining = false;
            s.net->CloseDataStream(s.scopeToken);
            Request({{"op", LL::CpsOp::Unsubscribe}, {"token", s.scopeToken}}, {}, true);
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
    const float px = 80, py = 215, pw = w - 110, ph = std::max(160.0f, h - py - 155);
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
        const auto cutoff = edge > 5000000 ? edge - 5000000 : 0;
        while (firstVisible + 1 < s.samples.size() && s.samples[firstVisible].timestampUs < cutoff)
            ++firstVisible;
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
        if (!s.samples.empty()) {
            double time = (s.samples[endVisible - 1].timestampUs - s.samples[firstVisible].timestampUs) /
                          1000000.0 * i / 8;
            std::ostringstream value;
            value << std::fixed << std::setprecision(2) << time;
            Text(ui, "scope.x" + std::to_string(i), value.str() + " s", x - 12, py + ph + 8, 62, 20, 10,
                 muted);
        }
    }
    if (endVisible - firstVisible < 2)
        Text(ui, "scope.empty",
             s.scopeRunning ? "正在积累100点首包 · 周期 " + s.interval + " ms；快速运动请选择实时1ms"
             : s.connected  ? "添加变量并开始采样，或导入 CSV 波形"
                            : "连接控制器以开始采样，或导入 CSV 波形",
             px + 30, py + ph / 2 - 15, pw - 60, 35, 16, muted);
    else {
        const std::vector<eui::Color> colors = {
            cyan, amber, green, {.73f, .47f, .98f, 1}, red, {.35f, .60f, .99f, 1}};
        const auto begin = s.samples[firstVisible].timestampUs;
        const double span = std::max<double>(1, s.samples[endVisible - 1].timestampUs - begin);
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
                float y = py + static_cast<float>((max - it->second) / (max - min)) * ph;
                if (previous)
                    Line(ui, "wave." + std::to_string(c) + "." + std::to_string(i), x0, y0, x, y,
                         colors[c % colors.size()]);
                x0 = x;
                y0 = y;
                previous = true;
            }
        }
    }
    const float y = py + ph + 42;
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
    summary << "已收 " << count << " 点 · " << std::fixed << std::setprecision(0) << rate << " Hz · 漏采 "
            << stats.value("missed", 0ull) << " · 传输溢出 "
            << stats.value("discarded", 0ull) + stats.value("clientDropped", 0ull) << " · 重传 "
            << stats.value("replayed", 0ull) << " · 校验失败 " << stats.value("crcErrors", 0ull);
    if (!s.samples.empty())
        summary << " · 显示 " << s.samples.size() << " 点";
    if (s.interval == "1000")
        summary << " · 当前每秒仅采1点，请选择实时1ms";
    Text(ui, "scope.stats", summary.str(), 24, 159, w - 48, 23, 11, muted);
    const auto error = s.net ? s.net->DataError(s.scopeToken) : "";
    if (!error.empty())
        Text(ui, "scope.error", error, 24, 183, w - 48, 23, 11, red);
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
