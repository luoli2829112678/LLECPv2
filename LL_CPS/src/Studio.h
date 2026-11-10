#pragma once
#include "eui_neo.h"
#include "ControllerClient.h"
#include "LL_Pdo.h"
#include "core/platform/platform.h"
#include <algorithm>
#include <chrono>
#include <deque>
#include <set>
namespace app {
using LL::Json;
namespace fs = LL::fs;
using Clock = std::chrono::steady_clock;
inline const eui::Color bg{.97f, .98f, .99f, 1}, panel{1, 1, 1, 1}, raised{.93f, .95f, .97f, 1},
    line{.79f, .83f, .88f, 1}, fg{.12f, .17f, .23f, 1}, muted{.36f, .42f, .50f, 1}, cyan{0, .35f, .65f, 1},
    red{.73f, .14f, .18f, 1}, green{.08f, .43f, .25f, 1}, amber{.61f, .37f, .05f, 1};
struct Studio {
    std::unique_ptr<LL::ProjectRepository> repo;
    std::unique_ptr<LL::ControllerClient> net;
    Json project, catalog = Json::array(), scan = Json::array(), status = Json::object(),
                  variables = Json::array(), values = Json::object();
    std::string page = "projects", menu, modal, title, detail, input, ip = "127.0.0.1", search, watchName,
                watchValue = "0", remote = "firmware.bin", password = "0", target = "10", velocity = "10",
                acceleration = "100", jerk = "1000", breakLine = "1", interval = "1", kind = "program",
                mapChoice;
    std::function<void(const std::string &)> accept;
    bool connected = false, busy = false, building = false, dirty = false, debugPanel = true,
         scopeRunning = false, pendingPoll = false, pendingWatch = false;
    int master = -1, slave = -1, axis = -1, source = 0, bottomTab = 0;
    float treeScroll = 0, pageScroll = 0, listScroll = 0;
    float sourceTabScroll = 0;
    int visibleSourceTab = -1;
    bool savedDebugPanel = true;
    float fileScroll = 0;
    std::map<std::string, std::string> drafts;
    std::vector<std::string> watches, channels;
    std::set<std::string> hidden;
    std::vector<LL::ScopeSample> samples;
    std::deque<std::string> log;
    Json pdo = Json::object();
    std::string pdoContext;
    std::string operation, operationError;
    Clock::time_point lastStatus{};
    bool pendingPdo = false;
    int pdoPage = 0;
    Clock::time_point pdoPoll{};
    std::string scopeToken = LL::NewId("scope");
    std::string watchToken = LL::NewId("watch"), watchSelection;
    bool scopeDraining = false;
    Clock::time_point drainDeadline{};
    Clock::time_point scopePlaybackClock{};
    std::uint64_t scopePlaybackUs = 0;
    Clock::time_point poll{}, watchPoll{}, renew{};
};
extern Studio s;
void Update();
void Log(const std::string &);
void Try(std::function<void()>);
void Dirty();
void Page(std::string);
bool HasProject();
Json *Master();
Json *Slave();
Json *Axis();
Json MasterStatus();
bool Running();
bool AnyRunning();
std::string Hex(unsigned);
void Text(eui::Ui &, const std::string &, const std::string &, float, float, float, float = 24, float = 14,
          eui::Color = fg);
void Rect(eui::Ui &, const std::string &, float, float, float, float, eui::Color = panel, float = 0);
struct SourceStyle {
    std::string label;
    eui::Color color;
};
SourceStyle FileStyle(const Json &);
void FileItem(eui::Ui &, const std::string &, const Json &, float, float, float, float, bool,
              std::function<void()>);
void Button(eui::Ui &, const std::string &, const std::string &, float, float, float, std::function<void()>,
            bool = true, bool = false, float = 32);
void Input(eui::Ui &, const std::string &, const std::string &, float, float, float,
           std::function<void(const std::string &)>, float = 32, bool = false, bool = true);
void Field(eui::Ui &, const std::string &, const std::string &, Json &, const std::string &, float, float,
           float = 250);
void Toggle(eui::Ui &, const std::string &, const std::string &, bool, float, float,
            std::function<void(bool)>, bool = true, float = 230);
void Heading(eui::Ui &, const std::string &, const std::string &, const std::string &, float);
void Ask(const std::string &, const std::string &, const std::string &,
         std::function<void(const std::string &)>);
void Confirm(const std::string &, const std::string &, std::function<void()>);
void Request(Json, std::function<void(const Json &)> = {}, bool = false);
void Save();
void Enter(Json);
void GuardUnsaved(std::function<void()>);
std::string Choose(const std::string &, const std::vector<std::string> &);
void Variables();
void Connect();
void ImportEsi();
void Apply(std::function<void()>);
void MasterPower();
void OperationError(const std::string &);
void Build(bool = false);
void AddMaster();
void Scan();
void ImportSlave(const Json &);
void AddAxis();
void GenerateAxes();
Json AxisStatus(int id);
std::string AxisSummary(int id);
void ScopeSubscribe();
void AxisCommand(const std::string &);
void FileTransfer(const std::string &);
void AddSource();
void Tick();
void Init();
void Stop();
void ProjectPage(eui::Ui &, float, float);
void Overview(eui::Ui &, float, float);
void MasterPage(eui::Ui &, float, float);
void DevicesPage(eui::Ui &, float, float);
void ScanPage(eui::Ui &, float, float);
void SlavePage(eui::Ui &, float, float);
void AxisPage(eui::Ui &, float, float);
void HstrPage(eui::Ui &, float, float);
void ScopePage(eui::Ui &, float, float);
void Bottom(eui::Ui &, float, float);
void Navigation(eui::Ui &, float, float);
void MenuOverlay(eui::Ui &);
void Modal(eui::Ui &, float, float);
} // namespace app
