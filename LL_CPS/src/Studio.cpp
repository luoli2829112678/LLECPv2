#include "Studio.h"
#include <sstream>
#include <iomanip>
#include <cctype>
namespace app {
Studio s;
void Update() {
    requestUpdate();
}
void Log(const std::string &text) {
    s.log.push_back(text);
    if (s.log.size() > 300)
        s.log.pop_front();
    Update();
}
void OperationError(const std::string &text) {
    s.busy = false;
    s.operation.clear();
    s.operationError = text.empty() ? "操作失败，控制器未提供具体原因" : text;
    s.debugPanel = true;
    s.bottomTab = 1;
    Log("错误  " + s.operationError);
}
void Try(std::function<void()> fn) {
    try {
        fn();
    } catch (const std::exception &e) {
        Log(std::string("错误  ") + e.what());
    }
}
void Dirty() {
    s.dirty = true;
    Update();
}
void Page(std::string page) {
    if (page == "hstr" && s.page != "hstr") {
        s.savedDebugPanel = s.debugPanel;
        s.debugPanel = false;
    } else if (page != "hstr" && s.page == "hstr") {
        s.debugPanel = s.savedDebugPanel;
    }
    s.page = std::move(page);
    if (s.page == "hstr")
        s.visibleSourceTab = -1;
    s.pageScroll = 0;
    s.menu.clear();
    s.drafts.clear();
    s.pdo = Json::object();
    s.pdoContext.clear();
    s.pdoPage = 0;
    Update();
}
bool HasProject() {
    return s.project.is_object() && s.project.contains("id");
}
Json *Master() {
    if (!HasProject() || s.master < 0 || s.master >= static_cast<int>(s.project["masters"].size()))
        return nullptr;
    return &s.project["masters"][s.master];
}
Json *Slave() {
    auto *m = Master();
    if (!m || s.slave < 0 || s.slave >= static_cast<int>((*m)["slaves"].size()))
        return nullptr;
    return &(*m)["slaves"][s.slave];
}
Json *Axis() {
    auto *d = Slave();
    if (!d || s.axis < 0 || s.axis >= static_cast<int>((*d)["axes"].size()))
        return nullptr;
    return &(*d)["axes"][s.axis];
}
Json MasterStatus() {
    auto *m = Master();
    if (m && s.status.contains("masters"))
        for (const auto &st : s.status["masters"])
            if (st.value("id", "") == m->value("id", ""))
                return st;
    return Json::object();
}
bool Running() {
    return s.connected && MasterStatus().value("running", false);
}
bool AnyRunning() {
    if (s.status.contains("masters"))
        for (auto &m : s.status["masters"])
            if (m.value("running", false))
                return true;
    return false;
}
std::string Hex(unsigned value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << value;
    return out.str();
}
void Text(eui::Ui &ui, const std::string &id, const std::string &text, float x, float y, float w, float h,
          float size, eui::Color color) {
    ui.text(id).position(x, y).size(w, h).text(text).fontSize(size).color(color).build();
}
void Rect(eui::Ui &ui, const std::string &id, float x, float y, float w, float h, eui::Color color,
          float radius) {
    ui.rect(id).position(x, y).size(w, h).color(color).radius(radius).build();
}
SourceStyle FileStyle(const Json &file) {
    const auto kind = file.value("kind", std::string{});
    if (kind == "gvl")
        return {"GVL", {.72f, .39f, .08f, 1}};
    if (kind == "type")
        return {"TYPE", {.70f, .24f, .42f, 1}};
    if (kind == "task")
        return {"TASK", {.32f, .41f, .59f, 1}};
    if (kind == "function") {
        auto content = file.value("content", std::string{});
        std::transform(content.begin(), content.end(), content.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (content.find("FUNCTION_BLOCK") != std::string::npos)
            return {"FB", {.49f, .29f, .72f, 1}};
        return {"FUN", {.05f, .49f, .45f, 1}};
    }
    return {"ST", {.12f, .40f, .74f, 1}};
}
void FileItem(eui::Ui &ui, const std::string &id, const Json &file, float x, float y, float w, float h,
              bool selected, std::function<void()> action) {
    const auto style = FileStyle(file);
    const auto c = style.color;
    ui.rect(id)
        .position(x, y)
        .size(w, h)
        .color(selected ? eui::Color{.90f, .94f, .99f, 1} : panel)
        .radius(3)
        .onClick([action] {
            Try(action);
            Update();
        })
        .build();
    if (selected)
        Rect(ui, id + ".active", x, y, 3, h, c, 1);
    const float iy = y + (h - 20) / 2;
    Rect(ui, id + ".paper", x + 12, iy, 15, 20, c, 2);
    Rect(ui, id + ".fold", x + 21, iy, 6, 6, {.95f, .97f, .99f, 1}, 1);
    Rect(ui, id + ".line1", x + 15, iy + 10, 8, 1, {1, 1, 1, .85f});
    Rect(ui, id + ".line2", x + 15, iy + 14, 6, 1, {1, 1, 1, .85f});
    Text(ui, id + ".name", LL::SourceFilename(file), x + 36, y + (h - 23) / 2, w - 91, 23, 12,
         selected ? c : fg);
    Rect(ui, id + ".badge", x + w - 48, y + (h - 20) / 2, 41, 20, {c.r, c.g, c.b, .10f}, 3);
    Text(ui, id + ".type", style.label, x + w - 44, y + (h - 20) / 2 + 1, 36, 19, 10, c);
}
void Button(eui::Ui &ui, const std::string &id, const std::string &text, float x, float y, float w,
            std::function<void()> action, bool enabled, bool primary, float h) {
    components::button(ui, id)
        .theme(components::theme::light(), primary)
        .position(x, y)
        .size(w, h)
        .text(text)
        .fontSize(13)
        .colors(id == "emergency" ? eui::Color{.76f, .13f, .18f, 1}
                : primary         ? eui::Color{.85f, .93f, .99f, 1}
                                  : raised,
                id == "emergency" ? eui::Color{.86f, .18f, .23f, 1} : eui::Color{.86f, .92f, .98f, 1},
                id == "emergency" ? eui::Color{.63f, .08f, .13f, 1} : eui::Color{.77f, .87f, .96f, 1})
        .textColor(id == "emergency" ? eui::Color{1, 1, 1, 1}
                   : primary         ? cyan
                                     : fg)
        .border(1, id == "emergency" ? red
                   : primary         ? eui::Color{.52f, .70f, .86f, 1}
                                     : line)
        .radius(5)
        .disabled(!enabled)
        .opacity(enabled ? 1.0f : .42f)
        .onClick([action] {
            Try(action);
            Update();
        })
        .build();
}
void Input(eui::Ui &ui, const std::string &id, const std::string &value, float x, float y, float w,
           std::function<void(const std::string &)> change, float h, bool multiline, bool enabled) {
    if (!enabled) {
        Rect(ui, id + ".locked", x, y, w, h, raised, 4);
        Text(ui, id + ".locked.value", value, x + 10, y + 5, w - 20, h - 8, 14, muted);
        return;
    }
    components::InputStyle style(components::theme::light());
    style.background = panel;
    style.focused = panel;
    style.border = line;
    style.focusBorder = cyan;
    style.text = fg;
    style.placeholder = muted;
    style.cursor = cyan;
    style.radius = 4;
    style.shadow = {};
    components::input(ui, id)
        .position(x, y)
        .size(w, h)
        .value(value)
        .placeholder(id == "watch.name" ? "变量路径，例如 Main.Counter" : "输入…")
        .multiline(multiline)
        .scrollbar(multiline)
        .fontSize(14)
        .fontFamily(multiline && fs::exists("C:/Windows/Fonts/consola.ttf") ? "C:/Windows/Fonts/consola.ttf"
                                                                            : "")
        .style(style)
        .onChange([change](const std::string &v) {
            Try([&] { change(v); });
            Update();
        })
        .build();
}
void Field(eui::Ui &ui, const std::string &id, const std::string &label, Json &data, const std::string &key,
           float x, float y, float w) {
    Text(ui, id + ".label", label, x, y, w, 22, 12, muted);
    const auto value = data.at(key);
    std::string text = value.is_string() ? value.get<std::string>() : value.dump();
    if (s.drafts.count(id))
        text = s.drafts[id];
    Input(
        ui, id, text, x, y + 25, w,
        [ptr = &data, key, id, value](const std::string &v) {
            if (AnyRunning() || s.busy)
                return;
            s.drafts[id] = v;
            try {
                if (value.is_string())
                    (*ptr)[key] = v;
                else {
                    auto n = Json::parse(v);
                    if (!n.is_number())
                        return;
                    (*ptr)[key] = n;
                }
                Dirty();
            } catch (...) {
            }
        },
        32, false, !AnyRunning() && !s.busy);
}
void Toggle(eui::Ui &ui, const std::string &id, const std::string &label, bool value, float x, float y,
            std::function<void(bool)> change, bool enabled, float w) {
    Button(ui, id, (value ? "[x]  " : "[  ]  ") + label, x, y, w, [=] { change(!value); }, enabled, value);
}
void Heading(eui::Ui &ui, const std::string &id, const std::string &title, const std::string &detail,
             float w) {
    Text(ui, id + ".title", title, 24, 22, w - 48, 36, 25);
    Text(ui, id + ".detail", detail, 24, 65, w - 48, 25, 12, muted);
    Rect(ui, id + ".line", 24, 103, w - 48, 1, line);
}
void Ask(const std::string &title, const std::string &detail, const std::string &value,
         std::function<void(const std::string &)> accept) {
    s.modal = "text";
    s.title = title;
    s.detail = detail;
    s.input = value;
    s.accept = std::move(accept);
    s.menu.clear();
    Update();
}
void Confirm(const std::string &title, const std::string &detail, std::function<void()> accept) {
    Ask(title, detail, "", [accept](const std::string &) { accept(); });
    s.modal = "confirm";
}
void Request(Json request, std::function<void(const Json &)> callback, bool quiet) {
    if (!s.net)
        return;
    const auto op = request.value("op", "");
    s.net->Request(std::move(request), [callback, quiet, op](const Json &r) {
        if (!r.value("ok", false)) {
            if (r.value("transportError", false)) {
                s.connected = false;
                s.scopeRunning = false;
            }
            if (!quiet)
                OperationError(op + "  " + r.value("error", "操作失败"));
        } else if (!quiet)
            Log(op + (r.value("accepted", false) ? "  已受理，等待轴状态反馈" : "  完成"));
        if (callback)
            callback(r);
        Update();
    });
}
void Save() {
    if (HasProject()) {
        s.repo->Save(s.project);
        s.dirty = false;
        Log("项目已保存  " + s.project.value("name", ""));
    }
}
void Enter(Json project) {
    if (s.net) {
        s.net->CloseDataStream(s.scopeToken);
        s.net->CloseDataStream(s.watchToken);
        if (s.connected) {
            Request({{"op", LL::CpsOp::Unsubscribe}, {"token", s.scopeToken}}, {}, true);
            Request({{"op", LL::CpsOp::Unsubscribe}, {"token", s.watchToken}}, {}, true);
        }
    }
    s.scopeRunning = s.scopeDraining = false;
    s.watches.clear();
    s.watchSelection.clear();
    s.values = Json::object();
    s.project = std::move(project);
    s.master = s.project["masters"].empty() ? -1 : 0;
    s.slave = s.axis = -1;
    s.source = 0;
    if (!s.connected)
        s.ip = s.project.value("controllerIp", "127.0.0.1");
    s.dirty = false;
    s.channels = s.project["scope"].value("channels", std::vector<std::string>{});
    s.interval = std::to_string(s.project["scope"].value("intervalMs", 1));
    Page("overview");
}
void GuardUnsaved(std::function<void()> next) {
    if (s.dirty)
        Confirm("保存当前项目？", "继续将先保存当前项目。取消后可返回编辑。", [next] {
            Save();
            next();
        });
    else
        next();
}
std::string Choose(const std::string &title, const std::vector<std::string> &extensions) {
    core::platform::FileDialogOptions options;
    options.prompt = title;
    options.allowedExtensions = extensions;
    auto result = core::platform::openFileDialog(options);
    if (result.status == core::platform::FileDialogStatus::Failed)
        throw std::runtime_error(result.error);
    return result.selected() ? result.paths[0] : "";
}
void Variables() {
    Request({{"op", LL::CpsOp::Variables}}, [](const Json &r) {
        if (r.value("ok", false))
            s.variables = r.at("variables");
    });
}
void Connect() {
    if (!HasProject())
        return;
    Ask("连接控制器", "输入控制器 IPv4 地址，TCP 端口 8888", s.ip, [](const std::string &ip) {
        s.net->Endpoint(ip);
        s.ip = ip;
        s.status = Json::object();
        s.connected = false;
        Request({{"op", LL::CpsOp::Hello}, {"version", 1}}, [ip](const Json &r) {
            if (r.value("ok", false)) {
                s.connected = true;
                s.status = r;
                s.lastStatus = Clock::now();
                s.poll = Clock::now();
                if (HasProject()) {
                    s.project["controllerIp"] = ip;
                    Dirty();
                }
                Variables();
            }
        });
    });
}
void ImportEsi() {
    const auto path = Choose("导入 EtherCAT ESI", {"xml"});
    if (path.empty())
        return;
    const auto file = fs::u8path(path);
    const auto xml = LL::ReadText(file);
    LL::DeviceCatalog(xml, file.filename().u8string());
    LL::WriteAtomic(s.repo->Root() / "ESI" / file.filename(), xml);
    s.catalog = LL::LoadDeviceCatalog(s.repo->Root() / "ESI");
    Log("ESI 已导入本机设备库  " + file.filename().u8string());
    if (s.connected)
        s.net->Upload("esi", file.filename().u8string(), xml, [](const Json &r) {
            Log(r.value("ok", false) ? "控制器 ESI 已更新并重载" : "控制器导入失败  " + r.value("error", ""));
        });
}
static void SyncEsis(std::function<void()> done) {
    auto files = std::make_shared<std::vector<fs::path>>();
    for (const auto &f : fs::directory_iterator(s.repo->Root() / "ESI"))
        if (f.is_regular_file() && (f.path().extension() == ".xml" || f.path().extension() == ".XML"))
            files->push_back(f.path());
    auto i = std::make_shared<std::size_t>(0);
    auto next = std::make_shared<std::function<void(const Json &)>>();
    *next = [files, i, next, done](const Json &r) {
        if (!r.value("ok", false)) {
            s.busy = false;
            OperationError("设备库同步失败  " + r.value("error", ""));
            *next = {};
            return;
        }
        if (*i < files->size()) {
            auto f = (*files)[(*i)++];
            s.net->Upload("esi", f.filename().u8string(), LL::ReadText(f), *next);
        } else {
            *next = {};
            done();
        }
    };
    auto start = *next;
    start({{"ok", true}});
}
void Apply(std::function<void()> done) {
    if (!HasProject() || !s.connected)
        return;
    Save();
    s.busy = true;
    SyncEsis([done] {
        s.net->Upload("project", "project.json", s.project.dump(), [done](const Json &upload) {
            if (!upload.value("ok", false)) {
                s.busy = false;
                OperationError(upload.value("error", "配置上传失败"));
                return;
            }
            Request({{"op", LL::CpsOp::Apply}, {"token", upload.at("token")}}, [done](const Json &r) {
                s.busy = false;
                if (r.value("ok", false))
                    done();
            });
        });
    });
}
void MasterPower() {
    if (!Master() || !s.connected || s.busy)
        return;
    const auto id = (*Master()).at("id");
    const bool stop = Running();
    s.operationError.clear();
    s.operation = stop ? "正在停止主站…" : "正在同步配置…";
    auto power = [id, stop] {
        s.busy = true;
        s.operation = stop ? "正在停止主站…" : "正在配置PDO并请求OP…";
        Request({{"op", stop ? LL::CpsOp::MasterStop : LL::CpsOp::MasterStart}, {"master", id}},
                [](const Json &reply) {
                    s.busy = false;
                    s.operation.clear();
                    if (reply.value("ok", false)) {
                        if (reply.contains("masters")) {
                            s.status = reply;
                            s.lastStatus = Clock::now();
                        }
                        Variables();
                        s.poll = {};
                    }
                });
    };
    if (stop)
        power();
    else
        Apply(power);
}
void Build(bool rebuild) {
    if (!HasProject() || !s.connected || s.busy)
        return;
    Save();
    s.busy = true;
    s.net->Upload("project", "project.json", s.project.dump(), [rebuild](const Json &r) {
        if (!r.value("ok", false)) {
            s.busy = false;
            Log(r.value("error", "上传失败"));
            return;
        }
        Request({{"op", LL::CpsOp::Download}, {"token", r.at("token")}}, [rebuild](const Json &r) {
            if (!r.value("ok", false)) {
                s.busy = false;
                return;
            }
            Request({{"op", LL::CpsOp::Build}, {"rebuild", rebuild}}, [](const Json &r) {
                s.building = r.value("building", false);
                s.busy = s.building;
                s.bottomTab = 1;
                s.debugPanel = true;
                if (r.contains("diagnostics"))
                    Log(r.at("diagnostics").get<std::string>());
                if (r.value("ok", false) && !s.building)
                    Variables();
            });
        });
    });
}
void AddMaster() {
    Ask("添加 EtherCAT 主站", "标识以英文字母开头，只包含字母、数字和下划线。",
        "EtherCAT_" + std::to_string(s.project["masters"].size() + 1), [](const std::string &id) {
            for (auto &m : s.project["masters"])
                if (m.value("id", "") == id)
                    throw std::runtime_error("主站标识已存在");
            s.project["masters"].push_back(LL::NewMaster(id));
            s.master = static_cast<int>(s.project["masters"].size() - 1);
            s.slave = s.axis = -1;
            Dirty();
            Page("master");
        });
}
void Scan() {
    auto *m = Master();
    if (!m)
        return;
    s.busy = true;
    Request({{"op", LL::CpsOp::Scan}, {"config", *m}}, [](const Json &r) {
        s.busy = false;
        if (r.value("ok", false)) {
            s.scan = r.at("slaves");
            Page("scan");
        }
    });
}
void ImportSlave(const Json &device) {
    auto *m = Master();
    if (!m)
        return;
    (*m)["slaves"].push_back(LL::NewSlave(device, static_cast<int>((*m)["slaves"].size() + 1)));
    s.slave = static_cast<int>((*m)["slaves"].size() - 1);
    s.axis = -1;
    Dirty();
    Page("slave");
}
void AddAxis() {
    auto *d = Slave();
    if (!d)
        return;
    int id = 0;
    for (auto &m : s.project["masters"])
        for (auto &sl : m["slaves"])
            for (auto &a : sl["axes"])
                id = std::max(id, a["id"].get<int>() + 1);
    (*d)["axes"].push_back(LL::NewAxis(*d, id, static_cast<int>((*d)["axes"].size())));
    s.axis = static_cast<int>((*d)["axes"].size() - 1);
    Dirty();
    Page("axis");
}
void GenerateAxes() {
    auto *slave = Slave();
    if (!slave)
        return;
    auto slots = LL::DetectAxisSlots(*slave);
    if (slots.empty())
        throw std::runtime_error(
            "已选PDO中没有唯一且对齐的CiA402控制字/状态字；请先勾选通道，或手动添加轴并映射。");
    int id = 0;
    for (const auto &master : s.project["masters"])
        for (const auto &d : master["slaves"])
            for (const auto &a : d["axes"])
                id = std::max(id, a.at("id").get<int>() + 1);
    int count = 0;
    for (int slot : slots) {
        bool exists = false;
        for (const auto &a : (*slave)["axes"])
            if (a.value("slot", 0) == slot)
                exists = true;
        if (!exists) {
            (*slave)["axes"].push_back(LL::NewAxis(*slave, id++, slot));
            ++count;
        }
    }
    Dirty();
    Log("从已选PDO生成 " + std::to_string(count) + " 个轴；点击轴进入映射检查。");
}
Json AxisStatus(int id) {
    if (s.connected && s.status.contains("axes"))
        for (const auto &a : s.status["axes"])
            if (a.value("id", -1) == id && Master() && a.value("master", "") == (*Master()).value("id", ""))
                return a;
    return Json::object();
}
std::string AxisSummary(int id) {
    auto live = AxisStatus(id);
    if (live.empty())
        return "未注册 / 等待主站启动";
    return live.value("stateName", "未知") +
           std::string(live.value("available", false) ? "  | OP" : "  | 从站非OP") + "  | 状态字 " +
           Hex(live.value("statusWord", 0u)) + "  | 错误 " + std::to_string(live.value("errorCode", 0));
}
void ScopeSubscribe() {
    if (s.channels.empty() && s.scopeRunning) {
        s.scopeRunning = false;
        s.net->CloseDataStream(s.scopeToken);
        if (s.connected)
            Request({{"op", LL::CpsOp::Unsubscribe}, {"token", s.scopeToken}}, {}, true);
    }
    if (!s.connected || s.channels.empty())
        return;
    const int interval = std::stoi(s.interval);
    if (interval < 1 || interval > 1000)
        throw std::runtime_error("采样周期范围 1..1000 ms");
    Request(
        {{"op", LL::CpsOp::Subscribe},
         {"token", s.scopeToken},
         {"port", s.net->UdpPort()},
         {"intervalMs", interval},
         {"transport", "tcp-batch"},
         {"numericOnly", true},
         {"channels", s.channels}},
        [](const Json &r) {
            s.scopeRunning = r.value("ok", false);
            if (s.scopeRunning) {
                if (!r.contains("session")) {
                    s.scopeRunning = false;
                    OperationError("控制器不支持批量数据通道，请更新控制器");
                    return;
                }
                s.net->OpenDataStream(s.scopeToken, r.at("session"), r.at("dataPort"), true);
            }
            if (!s.scopeRunning)
                OperationError("示波器订阅失败：" + r.value("error", std::string("控制器未接受采样请求")));
            s.renew = Clock::now();
        },
        true);
    if (HasProject()) {
        s.project["scope"] = {{"intervalMs", interval}, {"channels", s.channels}};
        Dirty();
    }
}
void AxisCommand(const std::string &command) {
    if (!Axis() || !Master())
        return;
    Request({{"op", LL::CpsOp::Axis},
             {"master", (*Master())["id"]},
             {"axis", (*Axis())["id"]},
             {"command", command},
             {"position", std::stod(s.target)},
             {"velocity", std::stod(s.velocity)},
             {"acceleration", std::stod(s.acceleration)},
             {"jerk", std::stod(s.jerk)}});
}
static void DownloadFile(const std::string &token, const fs::path &path) {
    auto data = std::make_shared<std::string>();
    auto next = std::make_shared<std::function<void(const Json &)>>();
    *next = [token, path, data, next](const Json &r) {
        if (!r.value("ok", false)) {
            Log(r.value("error", "读取失败"));
            *next = {};
            return;
        }
        *data += LL::Decode64(r.value("data", ""));
        if (r.value("eof", false)) {
            LL::WriteAtomic(path, *data);
            Log("文件已保存  " + path.u8string());
            *next = {};
        } else
            Request({{"op", LL::CpsOp::FileRead}, {"token", token}, {"offset", data->size()}}, *next, true);
    };
    Request({{"op", LL::CpsOp::FileRead}, {"token", token}, {"offset", 0}}, *next);
}
void FileTransfer(const std::string &mode) {
    if (mode == "progress") {
        Request({{"op", LL::CpsOp::FoEProgress}}, [](const Json &r) {
            Log(r.dump());
            if (r.value("done", false) && r.value("succeeded", false) && r.contains("downloadToken")) {
                auto token = r["downloadToken"].get<std::string>();
                Ask("保存读取的固件", "输入本机完整保存路径", (s.repo->Root() / "firmware.bin").u8string(),
                    [token](const std::string &path) { DownloadFile(token, fs::u8path(path)); });
            }
        });
        return;
    }
    if (!Master() || !Slave())
        return;
    const auto master = (*Master())["id"], slave = (*Slave())["id"];
    const auto remote = s.remote;
    const auto password = static_cast<unsigned>(std::stoul(s.password, nullptr, 0));
    if (mode == "read") {
        Request({{"op", LL::CpsOp::FoE},
                 {"master", master},
                 {"slave", slave},
                 {"mode", mode},
                 {"remote", remote},
                 {"password", password}});
        return;
    }
    const auto path = Choose(mode == "xml" ? "选择 EtherCAT SII EEPROM 镜像" : "选择固件文件",
                             mode == "xml" ? std::vector<std::string>{"bin", "eep"}
                                           : std::vector<std::string>{"bin", "fw", "hex"});
    if (path.empty())
        return;
    Confirm("确认写入从站？",
            mode == "xml" ? "ESI XML 不能直接烧入 EEPROM。请选择厂家生成的 SII 二进制镜像。此操作会停止主站。"
                          : "此操作将停止主站并进入 BOOT，完成后需重新启动。",
            [=] {
                s.busy = true;
                s.net->Upload("firmware", fs::u8path(path).filename().u8string(),
                              LL::ReadText(fs::u8path(path), 256 * 1024 * 1024), [=](const Json &r) {
                                  s.busy = false;
                                  if (r.value("ok", false))
                                      Request({{"op", LL::CpsOp::FoE},
                                               {"master", master},
                                               {"slave", slave},
                                               {"mode", mode},
                                               {"token", r.at("token")},
                                               {"remote", remote},
                                               {"password", password}});
                                  else
                                      Log(r.value("error", "上传失败"));
                              });
            });
}
void AddSource() {
    Ask("新建 HSTR 文件", "名称必须为 IEC 标识符。请选择文件类型。", "NewPOU", [](const std::string &name) {
        LL::ValidateIdentifier(name);
        for (auto &f : s.project["hstr"]["files"])
            if (f.value("name", "") == name)
                throw std::runtime_error("对象名称已存在");
        const auto kind = s.kind;
        std::string content;
        if (kind == "program")
            content = "PROGRAM " + name + "\nVAR\nEND_VAR\n\nEND_PROGRAM\n";
        else if (kind == "function")
            content = "FUNCTION " + name + " : DINT\nVAR_INPUT\n    Value : DINT;\nEND_VAR\n" + name +
                      " := Value;\nEND_FUNCTION\n";
        else if (kind == "fb")
            content = "FUNCTION_BLOCK " + name +
                      "\nVAR_INPUT\nEND_VAR\nVAR_OUTPUT\nEND_VAR\n\nEND_FUNCTION_BLOCK\n";
        else if (kind == "gvl")
            content = "VAR_GLOBAL\nEND_VAR\n";
        else if (kind == "type")
            content = "TYPE\n    " + name + " : STRUCT\n        Value : DINT;\n    END_STRUCT;\nEND_TYPE\n";
        else
            content = Json{{"name", name},    {"type", "Cyclic"},         {"intervalUs", 1000},
                           {"priority", 0},   {"watchdogUs", 0},          {"cpuAffinity", -1},
                           {"enabled", true}, {"programs", Json::array()}}
                          .dump(2);
        s.project["hstr"]["files"].push_back(
            {{"name", name}, {"kind", kind == "fb" ? "function" : kind}, {"content", content}});
        s.source = static_cast<int>(s.project["hstr"]["files"].size() - 1);
        Dirty();
        Page("hstr");
    });
    s.modal = "source";
}
#ifdef LLECP_UI_RENDER_CHECK
void UiRenderCheck();
#endif
void Tick() {
#ifdef LLECP_UI_RENDER_CHECK
    Try(UiRenderCheck);
#endif
    if (!s.net)
        return;
    s.net->Drain();
    auto incoming = s.net->DrainSamples();
    if (s.scopeRunning || s.scopeDraining) {
        s.samples.insert(s.samples.end(), incoming.begin(), incoming.end());
        if (s.samples.size() > 20000)
            s.samples.erase(s.samples.begin(), s.samples.end() - 20000);
    }
    const auto now = Clock::now();
    if (s.scopeDraining && (s.net->DataEnded(s.scopeToken) || now > s.drainDeadline)) {
        if (!s.net->DataEnded(s.scopeToken))
            Log("示波器尾包确认超时，连接已关闭");
        s.net->CloseDataStream(s.scopeToken);
        auto tail = s.net->DrainSamples();
        s.samples.insert(s.samples.end(), tail.begin(), tail.end());
        if (s.samples.size() > 20000)
            s.samples.erase(s.samples.begin(), s.samples.end() - 20000);
        s.scopeDraining = false;
    }
    if (s.page == "slave" && Slave() && Master() && Running() && !s.busy) {
        const auto id = (*Master()).at("id").get<std::string>();
        const int slave = (*Slave()).at("id");
        const int page = s.pdoPage;
        const auto context = id + "/" + std::to_string(slave) + "/" + std::to_string(page);
        if (context != s.pdoContext) {
            s.pdoContext = context;
            s.pdo = Json::object();
            s.pdoPoll = {};
        }
        if (!s.pendingPdo && now - s.pdoPoll > std::chrono::milliseconds(250)) {
            s.pendingPdo = true;
            s.pdoPoll = now;
            Request(
                {{"op", LL::CpsOp::PdoRead},
                 {"master", id},
                 {"slave", slave},
                 {"offset", page * 128},
                 {"limit", 128}},
                [context](const Json &reply) {
                    s.pendingPdo = false;
                    if (context == s.pdoContext)
                        s.pdo = reply;
                },
                true);
        }
    } else {
        s.pdo = Json::object();
        s.pdoContext.clear();
    }
    if (s.building && !s.pendingPoll && now - s.poll > std::chrono::milliseconds(400)) {
        s.pendingPoll = true;
        s.poll = now;
        Request(
            {{"op", LL::CpsOp::BuildStatus}},
            [](const Json &r) {
                s.pendingPoll = false;
                if (!r.value("ok", false) || r.value("done", false)) {
                    s.building = false;
                    s.busy = false;
                    Log(r.value("diagnostics", r.value("error", "Build complete")));
                    if (r.value("succeeded", false))
                        Variables();
                }
            },
            true);
    }
    if (s.connected && !s.pendingPoll && !s.busy && now - s.poll > std::chrono::milliseconds(600)) {
        s.pendingPoll = true;
        s.poll = now;
        Request(
            {{"op", LL::CpsOp::Status}},
            [](const Json &r) {
                s.pendingPoll = false;
                if (r.value("ok", false)) {
                    s.status = r;
                    s.lastStatus = Clock::now();
                }
            },
            true);
    }
    const auto watched = s.net->DrainData(s.watchToken);
    if (watched.is_object())
        s.values = watched;
    const auto selection = Json(s.watches).dump();
    if (s.watches.empty() && s.watchSelection.empty())
        s.watchSelection = selection;
    if (s.connected && !s.pendingWatch && !s.busy &&
        (selection != s.watchSelection ||
         (!s.watches.empty() && now - s.watchPoll > std::chrono::seconds(5)))) {
        s.pendingWatch = true;
        s.watchPoll = now;
        s.watchSelection = selection;
        if (s.watches.empty()) {
            s.net->CloseDataStream(s.watchToken);
            Request(
                {{"op", LL::CpsOp::Unsubscribe}, {"token", s.watchToken}},
                [](const Json &) { s.pendingWatch = false; }, true);
        } else {
            Request(
                {{"op", LL::CpsOp::Subscribe},
                 {"channels", s.watches},
                 {"token", s.watchToken},
                 {"transport", "tcp-batch"},
                 {"numericOnly", false},
                 {"intervalMs", 1},
                 {"port", 0}},
                [](const Json &r) {
                    s.pendingWatch = false;
                    if (r.value("ok", false) && r.contains("session"))
                        s.net->OpenDataStream(s.watchToken, r.at("session"), r.at("dataPort"), false);
                    else
                        OperationError("变量监视订阅失败：" + r.value("error", std::string("请更新控制器")));
                },
                true);
        }
    }
    if (s.scopeRunning && !s.busy && !s.building && now - s.renew > std::chrono::seconds(2)) {
        s.renew = now;
        ScopeSubscribe();
    }
    Update();
}
void Init() {
    Try([] {
        s.repo = std::make_unique<LL::ProjectRepository>();
        s.catalog = LL::LoadDeviceCatalog(s.repo->Root() / "ESI");
        s.net = std::make_unique<LL::ControllerClient>();
        Log("工程目录  " + (s.repo->Root() / "PRJ").u8string());
    });
}
void Stop() {
    if (s.repo && s.dirty)
        Try([] { Save(); });
    s.net.reset();
}
} // namespace app
