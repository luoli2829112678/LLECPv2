#include "Studio.h"
#include <cctype>
namespace app {
void Navigation(eui::Ui &ui, float width, float height) {
    Rect(ui, "nav.bg", 0, 0, width, height, panel);
    if (s.page == "hstr" && HasProject()) {
        Text(ui, "files.title", "HSTR 编程工作区", 18, 14, width - 36, 28, 18, cyan);
        Button(ui, "files.back", "‹ 返回工程", 14, 51, width - 28, [] { Page("overview"); });
        Text(ui, "files.project", s.project.value("name", "") + (s.dirty ? " *" : ""), 18, 91, width - 36, 25,
             13);
        Button(ui, "files.new", "+ 新建文件", 14, 122, 125, [] { AddSource(); });
        Button(ui, "files.save", "保存项目", 149, 122, width - 163, [] { Save(); });
        auto &files = s.project["hstr"]["files"];
        Text(ui, "files.count", "源文件  ·  " + std::to_string(files.size()), 18, 166, width - 36, 23, 12,
             muted);
        const float listH = std::max(100.0f, height - 202);
        const float limit = std::max(0.0f, files.size() * 32.0f + 4*28.0f - listH);
        s.fileScroll = std::clamp(s.fileScroll, 0.0f, limit);
        ui.stack("files.list")
            .position(8, 197)
            .size(width - 16, listH)
            .clip()
            .onScroll([limit](const core::ScrollEvent &e) {
                s.fileScroll = std::clamp(s.fileScroll - static_cast<float>(e.y) * 32, 0.0f, limit);
                Update();
            })
            .content([&] {
                float y=-s.fileScroll;
                const std::vector<std::pair<std::string,std::string>> groups={{"pou","POUs / 程序与功能块"},{"gvl","GVLs / 全局变量"},{"type","数据类型"},{"task","PLCTasks / 任务配置"}};
                for(const auto& group:groups){
                    const auto key=group.first;
                    Button(ui,"files.folder."+key,(s.closedFolders.count(key)?">  ":"▾  ")+group.second,0,y,width-16,[key]{if(s.closedFolders.count(key))s.closedFolders.erase(key);else s.closedFolders.insert(key);},true,false,26);y+=28;
                    if(s.closedFolders.count(key))continue;
                    for(std::size_t i=0;i<files.size();++i){
                        const auto kind=files[i].value("kind","");
                        if((key=="pou"&&(kind=="gvl"||kind=="type"||kind=="task"))||(key!="pou"&&kind!=key))continue;
                        if(y+32>=0&&y<=listH)FileItem(ui,"files.row"+std::to_string(i),files[i],12,y,width-28,31,s.source==i,[i]{s.source=static_cast<int>(i);s.listScroll=s.taskProgramScroll=0;});
                        y+=32;
                    }
                }

            })
            .build();
        return;
    }
    Text(ui, "nav.label", "工程资源", 20, 18, width - 40, 24, 12, muted);
    Button(
        ui, "nav.projects", "项目中心", 14, 53, width - 28, [] { GuardUnsaved([] { Page("projects"); }); },
        true, s.page == "projects", 34);
    if (!HasProject())
        return;
    Button(
        ui, "nav.root", s.project.value("name", "Project") + (s.dirty ? " *" : ""), 14, 100, width - 28,
        [] { Page("overview"); }, true, s.page == "overview", 36);
    float y = 157;
    Text(ui, "nav.bus", "总线与设备", 20, y, width - 40, 24, 11, muted);
    Button(ui, "nav.addmaster", "+", width - 47, y - 4, 30, [] { AddMaster(); }, !AnyRunning(), false, 26);
    y += 35;
    for (std::size_t mi = 0; mi < s.project["masters"].size(); ++mi) {
        const auto &m = s.project["masters"][mi];
        Button(
            ui, "tree.m" + std::to_string(mi), ">  " + m.value("name", "EtherCAT"), 14, y, width - 28,
            [mi] {
                s.master = static_cast<int>(mi);
                s.slave = s.axis = -1;
                Page("master");
            },
            true, s.master == mi && (s.page == "master" || s.page == "scan"), 32);
        y += 38;
        for (std::size_t si = 0; si < m["slaves"].size(); ++si) {
            const auto &d = m["slaves"][si];
            Button(
                ui, "tree.s" + std::to_string(mi) + "." + std::to_string(si),
                "  " + std::to_string(si + 1) + "  " + d.value("name", "Slave"), 27, y, width - 41,
                [mi, si] {
                    s.master = static_cast<int>(mi);
                    s.slave = static_cast<int>(si);
                    s.axis = -1;
                    Page("slave");
                },
                true, s.master == mi && s.slave == si && s.page == "slave", 30);
            y += 35;
            for (std::size_t ai = 0; ai < d["axes"].size(); ++ai) {
                Button(
                    ui, "tree.a" + std::to_string(mi) + "." + std::to_string(si) + "." + std::to_string(ai),
                    "     " + d["axes"][ai].value("name", "Axis"), 38, y, width - 52,
                    [mi, si, ai] {
                        s.master = static_cast<int>(mi);
                        s.slave = static_cast<int>(si);
                        s.axis = static_cast<int>(ai);
                        Page("axis");
                    },
                    true, s.master == mi && s.slave == si && s.axis == ai && s.page == "axis", 28);
                y += 33;
            }
        }
    }
    y += 18;
    Text(ui, "nav.plc", "编程与调试", 20, y, width - 40, 24, 11, muted);
    y += 32;
    Button(
        ui, "nav.hstr", "▾  HSTR 源文件", 14, y, width - 28, [] { Page("hstr"); }, true, s.page == "hstr",
        32);
    y += 40;
    for (std::size_t i = 0; i < s.project["hstr"]["files"].size(); ++i) {
        const auto file = s.project["hstr"]["files"][i];
        FileItem(ui, "tree.source" + std::to_string(i), file, 27, y, width - 41, 32,
                 s.page == "hstr" && s.source == i, [i] {
                     s.source = static_cast<int>(i);
                     Page("hstr");
                 });
        y += 34;
    }
    y += 15;
    Button(ui, "nav.scope", "示波器", 14, y, width - 28, [] { Page("scope"); }, true, s.page == "scope", 32);
    y += 40;
    Button(
        ui, "nav.esi", "ESI 设备库", 14, y, width - 28, [] { Page("devices"); }, true, s.page == "devices",
        32);
}
void MenuOverlay(eui::Ui &ui) {
    if (s.menu.empty())
        return;
    const auto menu = s.menu;
    const float x=MenuStartX+MenuStep*(menu=="文件"?0:menu=="生成"?1:menu=="PLC"?2:menu=="调试"?3:4);
    std::vector<std::pair<std::string, std::function<void()>>> actions;
    if (menu == "文件")
        actions = {{"新建项目…",
                    [] {
                        GuardUnsaved([] {
                            Ask("新建项目", "输入项目名称", "Untitled",
                                [](const std::string &v) { Enter(s.repo->Create(v)); });
                        });
                    }},
                   {"打开项目…",
                    [] {
                        auto p = Choose("打开项目", {"json"});
                        if (!p.empty())
                            GuardUnsaved([p] { Enter(s.repo->OpenFile(fs::u8path(p))); });
                    }},
                   {"保存项目", [] { Save(); }},
                   {"另存为项目…",
                    [] {
                        if (HasProject())
                            Ask("另存为项目", "输入新项目名称", s.project.value("name", "") + " Copy",
                                [](const std::string &n) { Enter(s.repo->SaveAs(s.project, n)); });
                    }},
                   {"项目管理", [] { GuardUnsaved([] { Page("projects"); }); }}};
    else if (menu == "生成")
        actions = {{"生成解决方案", [] { Build(); }},
                   {"重新生成解决方案", [] { Build(true); }},
                   {"清理解决方案", [] { Request({{"op", LL::CpsOp::Clean}}); }}};
    else if (menu == "PLC")
        actions = {{"下载项目与配置", [] { Apply([] { Build(); }); }},
                   {"运行应用", [] { Request({{"op", LL::CpsOp::Run}, {"debug", false}}); }},
                   {"停止应用", [] { Request({{"op", LL::CpsOp::Stop}}); }},
                   {"复位应用", [] { Request({{"op", LL::CpsOp::PlcReset}}); }}};
    else if (menu == "调试")
        actions = {{"打开变量调试框",
                    [] {
                        s.debugPanel = true;
                        s.bottomTab = 0;
                    }},
                   {"调试运行", [] { StartHstr(true); }},
                   {"继续", [] { Request({{"op", LL::CpsOp::Debug}, {"command", "continue"}}); }},
                   {"单步步过", [] { Request({{"op", LL::CpsOp::Debug}, {"command", "over"}}); }},
                   {"单步进入", [] { Request({{"op", LL::CpsOp::Debug}, {"command", "into"}}); }}};
    else
        actions = {{"示波器", [] { Page("scope"); }},
                   {"ESI 设备库", [] { Page("devices"); }},
                   {"数据目录", [] {
                        Ask("数据目录",
                            "Linux 默认 /usr/local/ect/LLECP；Windows 使用 LOCALAPPDATA。可用 "
                            "LLECP_DATA_ROOT 覆盖。",
                            s.repo->Root().u8string(), [](const std::string &) {});
                    }}};
    ui.stack("menu.overlay")
        .position(x, MenuTop+MenuHeight+4)
        .size(245, static_cast<float>(actions.size() * 40 + 14))
        .zIndex(100)
        .content([&] {
            Rect(ui, "menu.bg", 0, 0, 245, static_cast<float>(actions.size() * 40 + 14), raised, 7);
            for (std::size_t i = 0; i < actions.size(); ++i)
                Button(
                    ui, "menu.item" + std::to_string(i), actions[i].first, 7, 7 + i * 40, 231,
                    [fn = actions[i].second] {
                        s.menu.clear();
                        fn();
                    },
                    true, false, 34);
        })
        .build();
}
void Modal(eui::Ui &ui, float w, float h) {
    if (s.modal.empty())
        return;
    ui.stack("modal.root")
        .size(w, h)
        .zIndex(200)
        .content([&] {
            ui.rect("modal.scrim")
                .size(w, h)
                .color({.12f, .17f, .23f, .25f})
                .onClick([] { s.modal.clear(); })
                .build();
            const float mw = std::min(820.0f, w - 100),
                        mh = s.modal.rfind("variables", 0) == 0 || s.modal == "mapping"
                                 ? std::min(650.0f, h - 100)
                             : s.modal == "source" ? 330.0f
                                                   : 260.0f;
            ui.stack("modal.box")
                .position((w - mw) / 2, (h - mh) / 2)
                .size(mw, mh)
                .clip()
                .content([&] {
                    Rect(ui, "modal.panel", 0, 0, mw, mh, panel, 10);
                    Text(ui, "modal.title",
                         s.modal == "variables.scope"         ? "添加示波器通道"
                         : s.modal.rfind("variables", 0) == 0 ? "搜索并选择变量"
                                                              : s.title,
                         26, 22, mw - 100, 35, 21);
                    Button(ui, "modal.x", "×", mw - 60, 20, 35, [] { s.modal.clear(); });
                    if (s.modal == "error") {
                        Input(
                            ui, "operation.error.detail", s.detail, 26, 72, mw - 52,
                            [](const std::string &) {}, 115, true);
                        Button(ui, "operation.error.close", "关闭", mw - 120, mh - 52, 94,
                               [] { s.modal.clear(); });
                        return;
                    }
                    if (s.modal.rfind("variables", 0) == 0) {
                        const bool scopePicker = s.modal == "variables.scope";
                        auto isAxis = [](const Json &variable) {
                            return variable.value("source", std::string{}) == "Axis" ||
                                   variable.value("name", std::string{}).rfind("Axis.", 0) == 0;
                        };
                        if (scopePicker) {
                            const auto axes = std::count_if(s.variables.begin(), s.variables.end(), isAxis);
                            Button(
                                ui, "scope.pick.axis", "轴数据 (" + std::to_string(axes) + ")", 26, 72, 156,
                                [] {
                                    s.scopePickAxis = true;
                                    s.search.clear();
                                    s.listScroll = 0;
                                },
                                true, s.scopePickAxis);
                            Button(
                                ui, "scope.pick.hstr",
                                "HSTR 变量 (" + std::to_string(s.variables.size() - axes) + ")", 194, 72, 176,
                                [] {
                                    s.scopePickAxis = false;
                                    s.search.clear();
                                    s.listScroll = 0;
                                },
                                true, !s.scopePickAxis);
                        }
                        Input(ui, "variable.search", s.search, 26, scopePicker ? 119 : 75, mw - 52,
                              [](const std::string &v) {
                                  s.search = v;
                                  s.listScroll = 0;
                              });
                        const float listTop = scopePicker ? 168 : 124, listHeight = mh - listTop - 43;
                        std::vector<Json> options;
                        for (const auto &variable : s.variables) {
                            if (scopePicker && isAxis(variable) != s.scopePickAxis)
                                continue;
                            const auto name = variable.value("name", std::string{});
                            auto match = name + " " + variable.value("displayName", name) + " " +
                                         variable.value("source", std::string{}) + " " +
                                         variable.value("master", std::string{});
                            auto query = s.search;
                            auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
                            std::transform(match.begin(), match.end(), match.begin(), lower);
                            std::transform(query.begin(), query.end(), query.begin(), lower);
                            if (match.find(query) != std::string::npos)
                                options.push_back(variable);
                        }
                        s.listScroll =
                            std::clamp(s.listScroll, 0.0f,
                                       std::max(0.0f, static_cast<float>(options.size()) * 39 - listHeight));
                        if (options.empty())
                            Text(ui, "variable.empty",
                                 !s.search.empty()                ? "没有匹配的数据"
                                 : scopePicker && s.scopePickAxis ? "暂无轴数据，请确认主站已启动且轴已注册"
                                                                  : "暂无 HSTR 变量，请先生成并下载程序",
                                 26, listTop + 16, mw - 52, 40, 14, muted);
                        Text(ui, "variable.count",
                             "共 " + std::to_string(options.size()) + " 项" +
                                 (scopePicker ? " · 轴数据和 HSTR 变量可混合添加到同一张波形图" : ""),
                             26, mh - 36, mw - 52, 24, 12, muted);
                        ui.stack("variable.rows")
                            .position(20, listTop)
                            .size(mw - 40, listHeight)
                            .clip()
                            .onScroll([](const core::ScrollEvent &e) {
                                s.listScroll = std::max(0.0f, s.listScroll - static_cast<float>(e.y) * 34);
                                Update();
                            })
                            .content([&] {
                                float y = -s.listScroll;
                                for (const auto &variable : options) {
                                    const auto name = variable.value("name", "");
                                    const auto display = variable.value("displayName", name);
                                    Button(
                                        ui, "var." + name,
                                        (display == name ? name : display + "   ·   " + name) + "   " +
                                            variable.value("type", ""),
                                        6, y, mw - 64,
                                        [name] {
                                            if (s.modal == "variables.scope") {
                                                if (std::find(s.channels.begin(), s.channels.end(), name) ==
                                                    s.channels.end()) {
                                                    if (s.channels.size() >= 32)
                                                        throw std::runtime_error("最多订阅 32 个通道");
                                                    s.channels.push_back(name);
                                                    if (s.scopeRunning)
                                                        ScopeSubscribe();
                                                }
                                            } else {
                                                s.watchName = name;
                                                if (std::find(s.watches.begin(), s.watches.end(), name) ==
                                                    s.watches.end())
                                                    s.watches.push_back(name);
                                            }
                                            s.modal.clear();
                                        },
                                        true, false, 32);
                                    y += 39;
                                }
                            })
                            .build();
                        return;
                    }
                    if (s.modal == "mapping") {
                        if (AnyRunning() || s.busy) {
                            Text(ui, "mapping.locked", "主站运行中，PDO 映射已锁定；停止主站后再修改。", 26,
                                 90, mw - 52, 60, 15, amber);
                            return;
                        }
                        Text(ui, "mapping.info", "显示已选 PDO 中方向与位宽匹配的通道。", 26, 67, mw - 52, 26,
                             12, muted);
                        Button(ui, "map.clear", "清空映射", 26, 105, mw - 52, [] {
                            if (AnyRunning() || s.busy)
                                return;
                            (*Axis())["mapping"][s.mapChoice] = nullptr;
                            LL::SyncAxisMapping(*Axis());
                            Dirty();
                            s.modal.clear();
                        });
                        auto field =
                            std::find_if(LL::AxisFields().begin(), LL::AxisFields().end(),
                                         [](const LL::AxisField &f) { return s.mapChoice == f.name; });
                        if (field == LL::AxisFields().end())
                            return;
                        ui.stack("mapping.rows")
                            .position(20, 150)
                            .size(mw - 40, mh - 170)
                            .clip()
                            .onScroll([](const core::ScrollEvent &e) {
                                s.listScroll = std::max(0.0f, s.listScroll - static_cast<float>(e.y) * 34);
                                Update();
                            })
                            .content([&] {
                                float y = -s.listScroll;
                                std::size_t bitOffset = 0;
                                for (const auto &p : (*Slave())[field->rx ? "rxPdos" : "txPdos"]) {
                                    if (!p.value("selected", false))
                                        continue;
                                    for (const auto &entry : p["entries"]) {
                                        const bool aligned = bitOffset % 8 == 0;
                                        bitOffset += entry.value("bits", 0u);
                                        if (entry.value("padding", false) ||
                                            entry.value("bits", 0) != field->bits || !aligned)
                                            continue;
                                        Button(
                                            ui, "map.option." + std::to_string(y),
                                            Hex(entry.value("index", 0u)) + ":" +
                                                std::to_string(entry.value("subIndex", 0)) + "  " +
                                                entry.value("name", "") + "  [" + Hex(p.value("index", 0u)) +
                                                "]",
                                            6, y, mw - 64,
                                            [entry, p] {
                                                if (AnyRunning() || s.busy)
                                                    return;
                                                (*Axis())["mapping"][s.mapChoice] = {
                                                    {"index", entry["index"]},
                                                    {"subIndex", entry["subIndex"]},
                                                    {"pdo", p["index"]}};
                                                if (s.mapChoice == "Controlword")
                                                    (*Axis())["rxPdo"] = p["index"];
                                                if (s.mapChoice == "StatusWord")
                                                    (*Axis())["txPdo"] = p["index"];
                                                LL::SyncAxisMapping(*Axis(), s.mapChoice == "Controlword" ||
                                                                                 s.mapChoice == "StatusWord");
                                                Dirty();
                                                s.modal.clear();
                                            },
                                            true, false, 32);
                                        y += 39;
                                    }
                                }
                            })
                            .build();
                        return;
                    }
                    Text(ui, "modal.detail", s.detail, 26, 72, mw - 52, 55, 13, muted);
                    if (s.modal != "confirm")
                        Input(ui, "modal.value", s.input, 26, 135, mw - 52,
                              [](const std::string &v) { s.input = v; });
                    if (s.modal == "source") {
                        const std::vector<std::pair<std::string, std::string>> kinds = {
                            {"program", "程序"}, {"function", "函数"}, {"fb", "功能块"},
                            {"type", "类型"},    {"gvl", "GVL"},       {"task", "任务"}};
                        for (std::size_t i = 0; i < kinds.size(); ++i)
                            Button(
                                ui, "kind." + kinds[i].first, kinds[i].second, 26 + i * (mw - 52) / 6, 186,
                                (mw - 64) / 6, [kind = kinds[i].first] { s.kind = kind; }, true,
                                s.kind == kinds[i].first);
                    }
                    Button(ui, "modal.cancel", "取消", mw - 240, mh - 61, 95, [] { s.modal.clear(); });
                    Button(
                        ui, "modal.ok", "确定", mw - 130, mh - 61, 104,
                        [] {
                            auto fn = s.accept;
                            auto value = s.input;
                            s.modal.clear();
                            if (fn)
                                fn(value);
                        },
                        true, true);
                })
                .build();
        })
        .build();
}
} // namespace app
