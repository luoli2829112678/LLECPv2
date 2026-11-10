#include "Studio.h"
#include <sstream>
#include <iomanip>
namespace app {
static std::string Number(double value) {
    std::ostringstream out;
    out << std::setprecision(8) << value;
    return out.str();
}
static void PdoRow(eui::Ui &ui, const Json &entry, bool rx, unsigned pdo, unsigned ordinal, float y, float w,
                   bool selected) {
    const auto key = LL::PdoKey(rx, pdo, ordinal);
    Json live = Json::object();
    if (Running() && s.pdo.value("ok", false) && s.pdo.contains("entries"))
        for (const auto &row : s.pdo["entries"])
            if (row.value("key", "") == key) {
                live = row;
                break;
            }
    const auto name = Hex(entry.value("index", 0u)) + ":" + std::to_string(entry.value("subIndex", 0)) +
                      "  " + entry.value("name", "") + "  " + entry.value("type", "") + " / " +
                      std::to_string(entry.value("bits", 0)) + " bit";
    Text(ui, "entry." + key, name, 50, y, w * .52f - 50, 27, 12, selected ? fg : muted);
    std::string value = !Running()                                                      ? "离线"
                        : !selected                                                     ? "未选通道"
                        : entry.value("padding", false) || entry.value("index", 0) == 0 ? "填充"
                        : live.empty() ? "等待数据"
                                       : live.value("value", live.value("error", "—"));
    if (!rx && !live.value("valid", true))
        value = "无有效输入帧（缓存：" + value + "）";
    Text(ui, "entry.value." + key, value, w * .53f, y, w * .30f, 27, 12, live.contains("error") ? red : cyan);
    if (!entry.value("padding", false) && entry.value("index", 0) != 0) {
        const bool writable = Running() && live.value("writable", false);
        Button(
            ui, "entry.write." + key,
            rx ? (live.value("reason", "").find("注册轴") != std::string::npos ? "轴占用" : "写入") : "只读",
            w - 112, y, 84,
            [key, live, rx] {
                const auto id = (*Master())["id"];
                const auto slave = (*Slave())["id"];
                Ask("写入 RxPDO",
                    live.value("name", "") + "  " + Hex(live.value("index", 0u)) + ":" +
                        std::to_string(live.value("subIndex", 0)),
                    live.value("value", "0"), [key, id, slave](const std::string &value) {
                        Request({{"op", LL::CpsOp::PdoWrite},
                                 {"master", id},
                                 {"slave", slave},
                                 {"key", key},
                                 {"value", value},
                                 {"offset", s.pdoPage * 128},
                                 {"limit", 128}},
                                [](const Json &r) {
                                    if (r.value("ok", false))
                                        s.pdo = r;
                                });
                    });
            },
            writable, false, 25);
    }
}
void ProjectPage(eui::Ui &ui, float w, float) {
    Heading(ui, "projects", "项目中心", "创建、打开和管理控制项目。每个项目独立保存总线配置与 HSTR 应用。",
            w);
    Button(
        ui, "prj.new", "+ 新建项目", 24, 124, 125,
        [] {
            GuardUnsaved([] {
                Ask("新建项目", "输入项目名称", "Untitled",
                    [](const std::string &n) { Enter(s.repo->Create(n)); });
            });
        },
        s.repo != nullptr, true);
    Button(ui, "prj.open", "打开项目…", 164, 124, 125, [] {
        auto path = Choose("打开 LLECP 项目", {"json"});
        if (!path.empty())
            GuardUnsaved([path] { Enter(s.repo->OpenFile(fs::u8path(path))); });
    });
    Text(ui, "prj.path", s.repo ? (s.repo->Root() / "PRJ").u8string() : "数据目录不可用", 310, 130, w - 345,
         22, 12, muted);
    if (!s.repo)
        return;
    auto list = s.repo->List();
    float y = 183;
    for (auto &project : list) {
        const auto id = project.at("id").get<std::string>();
        Rect(ui, "prj.card." + id, 24, y, w - 48, 82, raised, 7);
        Text(ui, "prj.name." + id, project.value("name", ""), 42, y + 12, w - 350, 27, 18);
        Text(ui, "prj.id." + id, (s.repo->DefaultProject() == id ? "默认项目  ·  " : "") + id, 42, y + 46,
             w - 350, 20, 11, muted);
        Button(
            ui, "prj.enter." + id, "进入项目", w - 270, y + 23, 105,
            [id] { GuardUnsaved([id] { Enter(s.repo->Open(id)); }); }, true, true);
        Button(
            ui, "prj.delete." + id, "删除", w - 150, y + 23, 95,
            [id] {
                Confirm("删除项目？", "删除项目及该项目的源文件，无法撤销。", [id] {
                    if (HasProject() && s.project.value("id", "") == id) {
                        s.project = Json();
                        s.dirty = false;
                    }
                    s.repo->Remove(id);
                    Log("项目已删除");
                });
            },
            !AnyRunning());
        y += 96;
    }
    if (list.empty())
        Text(ui, "prj.empty", "尚无项目。新建一个项目，开始配置 EtherCAT 与 HSTR。", 24, 218, w - 48, 45, 17,
             muted);
}
void Overview(eui::Ui &ui, float w, float) {
    Heading(ui, "overview", s.project.value("name", "项目"), "PROJECT / 工程概览", w);
    const float cw = (w - 80) / 3;
    for (int i = 0; i < 3; ++i) {
        Rect(ui, "metric" + std::to_string(i), 24 + i * (cw + 16), 126, cw, 105, raised, 7);
        int n = 0;
        if (i == 0)
            n = static_cast<int>(s.project["masters"].size());
        if (i == 1)
            for (auto &m : s.project["masters"])
                n += static_cast<int>(m["slaves"].size());
        if (i == 2)
            n = static_cast<int>(s.project["hstr"]["files"].size());
        Text(ui, "metric.value" + std::to_string(i), std::to_string(n), 42 + i * (cw + 16), 140, cw - 30, 42,
             32, cyan);
        Text(ui, "metric.label" + std::to_string(i),
             i == 0   ? "EtherCAT 主站"
             : i == 1 ? "已配置从站"
                      : "HSTR 源文件",
             42 + i * (cw + 16), 191, cw - 30, 24, 13, muted);
    }
    Button(
        ui, "overview.master", "+ 添加 EtherCAT 主站", 24, 258, 210, [] { AddMaster(); }, !AnyRunning(),
        true);
    Button(ui, "overview.plc", "打开 HSTR 编辑器", 250, 258, 200, [] { Page("hstr"); });
    Text(ui, "overview.steps", "工作流程", 24, 328, w - 48, 30, 19);
    const char *rows[] = {
        "01    配置主站网口、周期与模拟模式", "02    导入 ESI，添加从站，配置 PDO / DC / 轴",
        "03    编辑 HSTR 程序与任务，生成解决方案并下载", "04    启动主站与 PLC，使用变量监视和示波器调试"};
    for (int i = 0; i < 4; ++i)
        Text(ui, "steps" + std::to_string(i), rows[i], 24, 380 + i * 40, w - 48, 30, 14, muted);
}
void MasterPage(eui::Ui &ui, float w, float) {
    auto *m = Master();
    if (!m)
        return;
    Heading(ui, "master", m->value("name", "EtherCAT"), "EtherCAT / 主站配置", w);
    Field(ui, "master.name", "显示名称", *m, "name", 24, 126);
    Field(ui, "master.nic", "控制器网口名称（Linux 控制器）", *m, "nic", 310, 126, 320);
    Field(ui, "master.cycle", "总线周期 / µs", *m, "cycleUs", 24, 208);
    Toggle(
        ui, "master.sim", "模拟模式", m->value("simulation", false), 310, 233,
        [](bool v) {
            (*Master())["simulation"] = v;
            Dirty();
        },
        !AnyRunning() && !s.busy);
    Button(
        ui, "master.scan", "扫描从站", 24, 307, 130, [] { Scan(); }, s.connected && !AnyRunning() && !s.busy,
        true);
    Button(
        ui, "master.addslave", "手动添加从站", 170, 307, 150, [] { Page("devices"); },
        !AnyRunning() && !s.busy);
    Button(ui, "master.apply", "写入控制器配置", 337, 307, 160, [] { Apply([] {}); }, s.connected && !s.busy);
    Button(
        ui, "master.remove", "删除主站", 514, 307, 120,
        [] {
            Confirm("删除主站？", "同时删除该主站的从站和轴配置。", [] {
                s.project["masters"].erase(s.master);
                s.master = -1;
                Dirty();
                Page("overview");
            });
        },
        !AnyRunning());
    auto status = MasterStatus();
    Text(ui, "master.live",
         "主站状态  " + std::string(Running() ? "RUNNING" : "STOPPED") + "    WKC " +
             std::to_string(status.value("actualWkc", 0)) + " / " +
             std::to_string(status.value("expectedWkc", 0)) + "    丢帧 " +
             std::to_string(status.value("wkcErrors", 0ull)),
         24, 369, w - 48, 30, 14, Running() ? green : muted);
    const float card = (w - 80) / 3;
    std::vector<std::pair<std::string, std::string>> metrics = {
        {"发送帧数量", std::to_string(status.value("sentFrames", 0ull))},
        {"每秒发送帧 / fps", Number(status.value("framesPerSecond", 0.0))},
        {"TX 发送错误", std::to_string(status.value("txErrors", 0ull))},
        {"RX 接收超时 / 无帧", std::to_string(status.value("rxErrors", 0ull))},
        {"WKC 异常", std::to_string(status.value("wkcErrors", 0ull))},
        {"总线扫描次数", std::to_string(status.value("cycles", 0ull))}};
    const char *metricKeys[] = {"sentFrames", "framesPerSecond", "txErrors",
                                "rxErrors",   "wkcErrors",       "cycles"};
    for (std::size_t i = 0; i < metrics.size(); ++i)
        if (!status.contains(metricKeys[i]))
            metrics[i].second = "—";
    for (std::size_t i = 0; i < metrics.size(); ++i) {
        const float x = 24 + (i % 3) * (card + 16), y = 415 + (i / 3) * 81;
        Rect(ui, "master.metric." + std::to_string(i), x, y, card, 70, panel, 5);
        Text(ui, "master.metric.title." + std::to_string(i), metrics[i].first, x + 12, y + 7, card - 24, 22,
             12, muted);
        Text(ui, "master.metric.value." + std::to_string(i), s.connected ? metrics[i].second : "—", x + 12,
             y + 32, card - 24, 30, 23, (i == 2 || i == 3) && metrics[i].second != "0" ? red : cyan);
    }
    Text(ui, "master.metrics.note",
         status.value("simulation", false) ? "模拟模式：无物理EtherCAT发送帧。"
                                           : "TX=发送失败的过程数据帧；RX=接收无帧/超时，WKC异常单独统计。",
         24, 581, w - 48, 25, 12, muted);
    float y = 625;
    Text(ui, "slaves.header", "从站拓扑", 24, y, w - 48, 27, 18);
    y += 42;
    for (std::size_t i = 0; i < (*m)["slaves"].size(); ++i) {
        auto &d = (*m)["slaves"][i];
        Button(
            ui, "master.slave" + std::to_string(i),
            std::to_string(i + 1) + "   " + d.value("name", "") + "   " + Hex(d.value("productCode", 0u)), 24,
            y, w - 48,
            [i] {
                s.slave = static_cast<int>(i);
                s.axis = -1;
                Page("slave");
            },
            true, false, 38);
        y += 47;
    }
    y += 20;
    Text(ui, "master.axes", "轴状态", 24, y, w - 48, 30, 18);
    y += 40;
    for (std::size_t si = 0; si < (*m)["slaves"].size(); ++si)
        for (std::size_t ai = 0; ai < (*m)["slaves"][si]["axes"].size(); ++ai) {
            const auto &a = (*m)["slaves"][si]["axes"][ai];
            Button(
                ui, "master.axis." + std::to_string(a.value("id", 0)),
                a.value("name", "Axis") + "   " + AxisSummary(a.at("id")), 24, y, w - 48,
                [si, ai] {
                    s.slave = static_cast<int>(si);
                    s.axis = static_cast<int>(ai);
                    Page("axis");
                },
                true, false, 36);
            y += 44;
        }
}
void DevicesPage(eui::Ui &ui, float w, float) {
    Heading(ui, "devices", "ESI 设备库", "启动和导入时自动重新加载；按产品代码与版本精确匹配。", w);
    Button(ui, "esi.import", "+ 导入 XML", 24, 124, 130, [] { ImportEsi(); }, s.repo != nullptr, true);
    Button(ui, "esi.reload", "重新加载", 170, 124, 120,
           [] { s.catalog = LL::LoadDeviceCatalog(s.repo->Root() / "ESI"); });
    Text(ui, "esi.path", s.repo ? (s.repo->Root() / "ESI").u8string() : "", 308, 130, w - 330, 25, 11, muted);
    float y = 181;
    for (std::size_t i = 0; i < s.catalog.size(); ++i) {
        const auto &d = s.catalog[i];
        Rect(ui, "esi.row" + std::to_string(i), 24, y, w - 48, 70, raised, 4);
        Text(ui, "esi.name" + std::to_string(i), d.value("name", ""), 38, y + 8, w - 250, 24, 15,
             d.contains("error") ? red : fg);
        Text(ui, "esi.ids" + std::to_string(i),
             d.contains("error") ? d.value("error", "")
                                 : d.value("vendor", "") + "  ·  " + Hex(d.value("productCode", 0u)) + " / " +
                                       Hex(d.value("revision", 0u)) + "  ·  " + d.value("file", ""),
             38, y + 37, w - 250, 24, 11, muted);
        Button(
            ui, "esi.add" + std::to_string(i), "添加到主站", w - 180, y + 20, 135,
            [i] { ImportSlave(s.catalog[i]); }, Master() && !AnyRunning() && !d.contains("error"));
        y += 80;
    }
    if (s.catalog.empty())
        Text(ui, "esi.empty", "设备库为空，请导入厂商提供的 EtherCAT XML 文件。", 24, 200, w - 48, 35, 15,
             muted);
}
void ScanPage(eui::Ui &ui, float w, float) {
    Heading(ui, "scan", "扫描结果", "产品代码、版本与 ESI 精确匹配后可导入。不支持的设备需先导入 XML。", w);
    Button(
        ui, "scan.again", "重新扫描", 24, 124, 130, [] { Scan(); }, s.connected && !AnyRunning() && !s.busy);
    float y = 183;
    for (std::size_t i = 0; i < s.scan.size(); ++i) {
        const auto row = s.scan[i];
        Json device;
        for (auto &d : s.catalog)
            if (!d.contains("error") && d.value("vendorId", 0u) == row.value("vendorId", 0u) &&
                d.value("productCode", 0u) == row.value("productCode", 0u) &&
                d.value("revision", 0u) == row.value("revision", 0u)) {
                device = d;
                break;
            }
        Text(ui, "scan.name" + std::to_string(i), std::to_string(i + 1) + "  " + row.value("name", ""), 24, y,
             w - 230, 26, 16);
        Text(ui, "scan.ids" + std::to_string(i),
             Hex(row.value("productCode", 0u)) + "  /  " + Hex(row.value("revision", 0u)), 24, y + 31,
             w - 230, 22, 12, muted);
        Button(
            ui, "scan.add" + std::to_string(i), device.is_null() ? "不支持该设备" : "导入从站", w - 190,
            y + 6, 145,
            [device, i] {
                auto *m = Master();
                if (!m)
                    return;
                if (i != (*m)["slaves"].size())
                    throw std::runtime_error("请按扫描拓扑顺序导入从站");
                ImportSlave(device);
            },
            !device.is_null() && !AnyRunning() && !s.busy);
        y += 80;
    }
}
void SlavePage(eui::Ui &ui, float w, float) {
    auto *d = Slave();
    if (!d)
        return;
    Heading(ui, "slave", d->value("name", "从站"),
            "EtherCAT / 从站 " + std::to_string(d->value("id", 0)) + "    " +
                Hex(d->value("productCode", 0u)) + " / " + Hex(d->value("revision", 0u)),
            w);
    Button(ui, "slave.axis", "+ 添加轴", 24, 122, 120, [] { AddAxis(); }, !AnyRunning() && !s.busy, true);
    Button(
        ui, "slave.generate", "从 PDO 生成轴", 156, 122, 145, [] { GenerateAxes(); },
        !AnyRunning() && !s.busy, true);
    Button(
        ui, "slave.del", "删除从站", 313, 122, 110,
        [] {
            Confirm("删除从站？", "后续从站按拓扑顺序重新编号。", [] {
                auto &list = (*Master())["slaves"];
                list.erase(s.slave);
                for (std::size_t i = 0; i < list.size(); ++i)
                    list[i]["id"] = i + 1;
                s.slave = s.axis = -1;
                Dirty();
                Page("master");
            });
        },
        !AnyRunning() && !s.busy);
    Toggle(
        ui, "dc.enable", "使能 DC", (*d)["dc"].value("enabled", false), 435, 122,
        [](bool v) {
            (*Slave())["dc"]["enabled"] = v;
            Dirty();
        },
        !AnyRunning() && !s.busy, 135);
    Field(ui, "dc.shift", "DC 偏移 / µs", (*d)["dc"], "shiftUs", 590, 119, 190);
    float y = 202;
    Text(ui, "dc.mode.label", "DC 模式", 24, y, w - 48, 22, 12, muted);
    float mx = 24;
    for (const auto &mode : d->at("dcModes")) {
        const auto name = mode.value("name", "");
        Button(
            ui, "dc.mode." + name, name, mx, y + 28, 160,
            [name] {
                (*Slave())["dc"]["mode"] = name;
                Dirty();
            },
            !AnyRunning() && !s.busy, (*d)["dc"].value("mode", "") == name);
        mx += 174;
        if (mx + 160 > w)
            break;
    }
    y += 86;
    Text(ui, "slave.axes.title", "轴 / 映射与运行状态（修改配置需停止主站后重新写入）", 24, y, w - 48, 25,
         15);
    y += 36;
    for (std::size_t i = 0; i < (*d)["axes"].size(); ++i) {
        const auto &a = (*d)["axes"][i];
        Button(
            ui, "slave.axis.open." + std::to_string(i),
            a.value("name", "Axis") + "   " + AxisSummary(a.at("id")) + "   → PDO 映射 / 调试", 24, y, w - 48,
            [i] {
                s.axis = static_cast<int>(i);
                Page("axis");
            },
            true, false, 34);
        y += 42;
    }
    if ((*d)["axes"].empty()) {
        Text(ui, "slave.axes.empty", "点击“从 PDO 生成轴”识别 CiA402 通道，或手动添加轴。", 24, y, w - 48, 25,
             12, muted);
        y += 34;
    }
    Text(ui, "pdo.title", "PDO 数据通道", 24, y, w - 48, 30, 19);
    y += 42;
    Text(ui, "pdo.live.note",
         Running() ? (Master()->value("simulation", false) ? "模拟过程映像每250ms刷新；TxPDO只读，RxPDO可写。"
                                                           : "在线值每250ms刷新；TxPDO只读，RxPDO可写。")
                   : "主站启动后显示过程数据值。",
         24, y, w - 48, 24, 12, muted);
    y += 31;
    if (Running()) {
        if (s.pdo.value("ok", false) && !s.pdo.value("simulation", false) && s.pdo.contains("inputValid")) {
            const bool valid = s.pdo.value("inputValid", false);
            Text(ui, "pdo.quality",
                 std::string(valid ? "输入有效" : "输入无效") + "  WKC " +
                     std::to_string(s.pdo.value("actualWkc", 0)) + " / " +
                     std::to_string(s.pdo.value("expectedWkc", 0)) + "  周期 " +
                     std::to_string(s.pdo.value("cycles", 0ull)),
                 24, y, w - 48, 24, 12, valid ? green : red);
            y += 28;
            for (const auto &direction : std::vector<std::pair<std::string, std::string>>{
                     {"inputHex", "Tx原始输入"}, {"outputHex", "Rx原始输出"}}) {
                Text(ui, "pdo.raw." + direction.first,
                     direction.second + "（前64字节）  " + s.pdo.value(direction.first, std::string{}), 24, y,
                     w - 48, 24, 12, muted);
                y += 28;
            }
            const auto warning = s.pdo.value("warning", std::string{});
            if (!warning.empty()) {
                Text(ui, "pdo.warning", warning, 24, y, w - 48, 25, 12, amber);
                y += 30;
            }
        }
        Button(ui, "pdo.prev", "上一页", 24, y, 95, [] { --s.pdoPage; }, s.pdoPage > 0);
        Button(
            ui, "pdo.next", "下一页", 131, y, 95, [] { ++s.pdoPage; },
            s.pdo.value("total", 0) > (s.pdoPage + 1) * 128);
        Text(ui, "pdo.page",
             "第 " + std::to_string(s.pdoPage + 1) + " 页 / 每页128项，共 " +
                 std::to_string(s.pdo.value("total", 0)) + " 项",
             245, y + 4, w - 280, 24, 12, muted);
        y += 42;
        if (s.pdo.contains("error")) {
            Text(ui, "pdo.error", s.pdo.value("error", ""), 24, y, w - 48, 25, 12, red);
            y += 32;
        }
    }
    unsigned liveOrdinal = 0;
    for (const auto *key : {"rxPdos", "txPdos"}) {
        Text(ui, std::string("pdo.direction.") + key,
             key == std::string("rxPdos") ? "RxPDO  控制器 → 从站" : "TxPDO  从站 → 控制器", 24, y, w - 48,
             25, 14, cyan);
        y += 34;
        for (const auto &p : d->at(key)) {
            const unsigned index = p.at("index");
            const bool selected = p.value("selected", false);
            Toggle(
                ui, "pdo." + std::to_string(index),
                Hex(index) + "  " + p.value("name", "") + (p.value("mandatory", false) ? "  [必选]" : ""),
                selected, 24, y,
                [index, rx = key == std::string("rxPdos")](bool v) {
                    LL::SelectPdo(*Slave(), rx, index, v);
                    Dirty();
                },
                !AnyRunning() && !s.busy, w - 48);
            y += 37;
            for (std::size_t n = 0; n < p.at("entries").size(); ++n) {
                const auto &e = p["entries"][n];
                const bool data = selected && !e.value("padding", false) && e.value("index", 0) != 0 &&
                                  e.value("bits", 0) > 0;
                bool visible = true;
                if (Running() && data) {
                    visible = liveOrdinal >= static_cast<unsigned>(s.pdoPage * 128) &&
                              liveOrdinal < static_cast<unsigned>((s.pdoPage + 1) * 128);
                    ++liveOrdinal;
                }
                if (!visible)
                    continue;
                PdoRow(ui, e, key == std::string("rxPdos"), index, static_cast<unsigned>(n), y, w, selected);
                y += 32;
            }
            y += 12;
        }
    }
    Text(ui, "foe.title", "文件传输与从站状态", 24, y, w - 48, 28, 19);
    y += 42;
    Text(ui, "foe.note", "FoE / EEPROM 操作会停止主站；XML 描述应先转换成 SII 二进制镜像。", 24, y, w - 48,
         26, 12, muted);
    y += 36;
    Input(ui, "foe.remote", s.remote, 24, y, 290, [](const std::string &v) { s.remote = v; });
    Input(ui, "foe.password", s.password, 332, y, 170, [](const std::string &v) { s.password = v; });
    y += 46;
    Button(ui, "foe.write", "烧录固件", 24, y, 125, [] { FileTransfer("write"); }, Running() && !s.busy);
    Button(ui, "foe.read", "读取固件", 164, y, 125, [] { FileTransfer("read"); }, Running() && !s.busy);
    Button(ui, "foe.xml", "烧录 XML / SII", 304, y, 155, [] { FileTransfer("xml"); }, Running() && !s.busy);
    Button(ui, "foe.progress", "传输进度 / 保存", 475, y, 165, [] { FileTransfer("progress"); }, s.connected);
    y += 48;
    const std::vector<std::pair<std::string, int>> states = {
        {"INIT", 1}, {"PRE-OP", 2}, {"SAFE-OP", 4}, {"OP", 8}};
    for (std::size_t i = 0; i < states.size(); ++i)
        Button(
            ui, "slave.state" + std::to_string(i), states[i].first, 24 + i * 140, y, 125,
            [state = states[i].second] {
                Request({{"op", LL::CpsOp::SlaveState},
                         {"master", (*Master())["id"]},
                         {"slave", (*Slave())["id"]},
                         {"state", state}});
            },
            Running() && !s.busy);
}
void AxisPage(eui::Ui &ui, float w, float) {
    auto *a = Axis();
    if (!a || !Slave())
        return;
    Heading(ui, "axis", a->value("name", "Axis"), "CiA 402 / 映射与调试", w);
    Field(ui, "axis.name", "轴名称", *a, "name", 24, 122, 220);
    Field(ui, "axis.slot", "轴通道（0起）", *a, "slot", 260, 122, 140);
    Toggle(
        ui, "axis.enabled", "注册轴", a->value("enabled", false), 420, 147,
        [](bool v) {
            (*Axis())["enabled"] = v;
            Dirty();
        },
        !AnyRunning() && !s.busy, 120);
    Button(
        ui, "axis.autobind", "自动映射 PDO", 552, 147, 140,
        [] {
            LL::AutoMapAxis(*Axis(), *Slave());
            Log("PDO自动映射完成；缺失或重复的对象保留为空，请检查映射表。");
            Dirty();
        },
        !AnyRunning() && !s.busy);
    Button(
        ui, "axis.remove", "删除轴", 704, 147, 100,
        [] {
            (*Slave())["axes"].erase(s.axis);
            s.axis = -1;
            Dirty();
            Page("slave");
        },
        !AnyRunning() && !s.busy);
    auto live = AxisStatus(a->at("id"));
    Text(ui, "axis.live.title", "实时轴状态", 24, 195, w - 48, 29, 18);
    auto number = [&](const char *key) {
        return live.empty() ? std::string("—") : Number(live.value(key, 0.0));
    };
    auto flag = [&](const char *key) {
        return live.empty()             ? std::string("—")
               : live.value(key, false) ? std::string("是")
                                        : std::string("否");
    };
    const std::vector<std::pair<std::string, std::string>> rows = {
        {"轴运行状态", live.empty() ? "未注册 / 等待主站启动" : live.value("stateName", std::string("未知"))},
        {"从站状态", live.empty()                     ? "—"
                     : live.value("available", false) ? "OP"
                                                      : "非OP / 不可用"},
        {"已使能", flag("enabled")},
        {"运动中", flag("moving")},
        {"轴故障", flag("error")},
        {"错误码", live.empty() ? "—" : std::to_string(live.value("errorCode", 0))},
        {"状态字", live.empty() ? "—" : Hex(live.value("statusWord", 0u))},
        {"控制字", live.empty() ? "—" : Hex(live.value("controlWord", 0u))},
        {"实际位置", number("actualPosition")},
        {"实际速度", number("actualVelocity")},
        {"实际力矩", number("actualTorque")},
        {"运行模式", live.empty() ? "—" : std::to_string(live.value("mode", 0))}};
    const float pair = (w - 48) / 2;
    Rect(ui, "axis.live.header", 24, 231, w - 48, 30, raised);
    for (int col = 0; col < 2; ++col) {
        Text(ui, "axis.live.h.label" + std::to_string(col), "项目", 36 + col * pair, 235, 140, 24, 13, muted);
        Text(ui, "axis.live.h.value" + std::to_string(col), "当前值", 186 + col * pair, 235, pair - 165, 24,
             13, muted);
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const float x = 24 + (i % 2) * pair, ry = 261 + (i / 2) * 36;
        const auto id = "axis.live.row" + std::to_string(i);
        Rect(ui, id, x, ry, pair, 36, (i / 2) % 2 ? raised : panel);
        Text(ui, id + ".label", rows[i].first, x + 12, ry + 7, 140, 26, 14, muted);
        Text(ui, id + ".value", rows[i].second, x + 162, ry + 5, pair - 174, 28, 16,
             (i == 0 || i == 4 || i == 5) && live.value("error", false) ? red : fg);
        Rect(ui, id + ".line", x, ry + 35, pair, 1, line);
    }
    float y = 501;
    Text(ui, "mapping.head", "PDO 映射 / 未匹配的字段保持为空；点击字段手动选择", 24, y, w - 48, 30, 16);
    y += 42;
    int n = 0;
    for (const auto &field : LL::AxisFields()) {
        const auto mapping = (*a)["mapping"][field.name];
        const std::string label = std::string(field.label) + "  " +
                                  (mapping.is_null() ? "未映射"
                                                     : Hex(mapping.value("index", 0u)) + ":" +
                                                           std::to_string(mapping.value("subIndex", 0)) +
                                                           "  [" + Hex(mapping.value("pdo", 0u)) + "]");
        const float col = (w - 64) / 2;
        Button(
            ui, "map." + std::string(field.name), label, 24 + (n % 2) * (col + 16), y + (n / 2) * 40, col,
            [key = std::string(field.name)] {
                s.mapChoice = key;
                s.modal = "mapping";
                s.title = "映射 " + key;
                s.listScroll = 0;
                s.menu.clear();
            },
            !AnyRunning() && !s.busy, !mapping.is_null());
        ++n;
    }
    y += ((n + 1) / 2) * 40 + 26;
    Text(ui, "axis.params", "轴参数", 24, y, w - 48, 28, 19);
    y += 43;
    n = 0;
    for (const auto &item :
         std::vector<std::pair<std::string, std::string>>{{"encoder", "编码器分辨率"},
                                                          {"gearRatio", "减速比"},
                                                          {"velocityScale", "速度比例"},
                                                          {"torqueScale", "力矩比例"},
                                                          {"encoderDirection", "编码器方向 ±1"},
                                                          {"maxVelocity", "最大速度"},
                                                          {"maxAcceleration", "最大加速度"},
                                                          {"maxJerk", "最大加加速度"},
                                                          {"homeMethod", "回零方法"},
                                                          {"homeOffset", "回零偏移 / counts"},
                                                          {"stopAcceleration", "停止减速度"},
                                                          {"stopJerk", "停止加加速度"},
                                                          {"softMin", "软限位下限"},
                                                          {"softMax", "软限位上限"}}) {
        Field(ui, "param." + item.first, item.second, (*a)["parameters"], item.first,
              24 + (n % 3) * ((w - 64) / 3 + 8), y + (n / 3) * 78, (w - 80) / 3);
        ++n;
    }
    y += ((n + 2) / 3) * 78 + 25;
    Text(ui, "axis.debug", "轴调试", 24, y, w - 48, 30, 19);
    y += 40;
    const auto prefix = "Axis." + std::to_string(a->value("id", 0)) + ".";
    Button(
        ui, "axis.monitor", "监视当前轴", 24, y, 170,
        [prefix] {
            for (const auto *f :
                 {"ActualPosition", "ActualVelocity", "ActualTorque", "StatusWord", "ErrorCode"}) {
                auto name = prefix + f;
                if (std::find(s.watches.begin(), s.watches.end(), name) == s.watches.end())
                    s.watches.push_back(name);
            }
            s.debugPanel = true;
            s.bottomTab = 0;
        },
        s.connected);
    Button(
        ui, "axis.scope", "添加到示波器", w - 210, 192, 185,
        [prefix] {
            for (const auto *field : {"ActualPosition", "ActualVelocity", "ActualTorque"}) {
                const auto name = prefix + field;
                if (std::find(s.channels.begin(), s.channels.end(), name) == s.channels.end()) {
                    if (s.channels.size() >= 32)
                        throw std::runtime_error("示波器最多32个通道");
                    s.channels.push_back(name);
                }
            }
            if (s.scopeRunning)
                ScopeSubscribe();
            Page("scope");
            Variables();
        },
        s.connected && live.value("available", false));
    y += 45;
    Input(ui, "motion.pos", s.target, 24, y, 150, [](const std::string &v) { s.target = v; });
    Input(ui, "motion.vel", s.velocity, 190, y, 150, [](const std::string &v) { s.velocity = v; });
    Input(ui, "motion.acc", s.acceleration, 356, y, 150, [](const std::string &v) { s.acceleration = v; });
    Input(ui, "motion.jerk", s.jerk, 522, y, 150, [](const std::string &v) { s.jerk = v; });
    y += 38;
    Text(ui, "motion.labels",
         "目标位置                     速度                          加速度                       加加速度",
         24, y, w - 48, 24, 12, muted);
    y += 36;
    const bool enabled = Running() && live.value("available", false) && a->value("enabled", false) &&
                         !s.status.value("emergency", false);
    n = 0;
    for (const auto &command : std::vector<std::pair<std::string, std::string>>{{"enable", "使能"},
                                                                                {"disable", "去使能"},
                                                                                {"reset", "复位"},
                                                                                {"absolute", "绝对运动"},
                                                                                {"relative", "相对运动"},
                                                                                {"home", "回零"},
                                                                                {"origin", "设置原点"},
                                                                                {"stop", "停止"}}) {
        Button(
            ui, "axis.command." + command.first, command.second, 24 + (n % 4) * 150, y + (n / 4) * 44, 135,
            [cmd = command.first] { AxisCommand(cmd); }, enabled, command.first == "stop");
        ++n;
    }
}
} // namespace app
