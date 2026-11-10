#include "LL_CpsData.h"
#include "LL_Pdo.h"
#include "ESI_Parse.h"
#include <fstream>
#include <chrono>
#include <cctype>
#include <sstream>
#include <set>
#include <regex>
#include <cmath>
#include <iomanip>
#include <cstdlib>
#include <algorithm>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
namespace LL {
fs::path CpsDataRoot() {
    if (const auto *root = std::getenv("LLECP_DATA_ROOT"))
        if (*root)
            return fs::u8path(root);
#ifdef _WIN32
    if (const auto *local = std::getenv("LOCALAPPDATA"))
        return fs::u8path(local) / "LLECP";
    return fs::temp_directory_path() / "LLECP";
#else
    return "/usr/local/ect/LLECP";
#endif
}
void WriteAtomic(const fs::path &path, const std::string &data) {
    if (!path.parent_path().empty())
        fs::create_directories(path.parent_path());
    const auto temp = path.parent_path() / (path.filename().u8string() + "." + NewId("tmp"));
    {
        std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
        if (!stream || !stream.write(data.data(), static_cast<std::streamsize>(data.size()))) {
            std::error_code ec;
            fs::remove(temp, ec);
            throw std::runtime_error("Cannot write " + path.u8string());
        }
        stream.flush();
        if (!stream)
            throw std::runtime_error("Cannot flush " + path.u8string());
    }
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code ec;
        fs::remove(temp, ec);
        throw std::runtime_error("Cannot replace " + path.u8string());
    }
#else
    fs::rename(temp, path);
#endif
}
std::string ReadText(const fs::path &path, std::size_t maximum) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
        throw std::runtime_error("Cannot open " + path.u8string());
    const auto size = stream.tellg();
    if (size < 0 || static_cast<std::uint64_t>(size) > maximum)
        throw std::runtime_error("File exceeds size limit");
    std::string data(static_cast<std::size_t>(size), '\0');
    stream.seekg(0);
    if (!data.empty() && !stream.read(data.data(), static_cast<std::streamsize>(data.size())))
        throw std::runtime_error("Cannot read file");
    return data;
}
Json ReadJson(const fs::path &path) {
    return Json::parse(ReadText(path));
}
void ValidateIdentifier(const std::string &id) {
    if (!std::regex_match(id, std::regex("[A-Za-z_][A-Za-z0-9_]{0,63}")))
        throw std::runtime_error(
            "Name must begin with a letter or underscore and contain only letters, digits or underscores");
}
std::string SourceFilename(const Json &file) {
    const auto name = file.at("name").get<std::string>();
    ValidateIdentifier(name);
    const auto kind = file.at("kind").get<std::string>();
    if (kind == "program")
        return name + ".st";
    if (kind == "function")
        return name + ".st.fun";
    if (kind == "type")
        return name + ".typ";
    if (kind == "gvl")
        return name + ".GVL";
    if (kind == "task")
        return name + ".task";
    throw std::runtime_error("Unknown HSTR file kind");
}
static std::uint64_t now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
Json NewProject(const std::string &name) {
    if (name.empty() || name.size() > 192)
        throw std::runtime_error("Project name must contain 1..192 UTF-8 bytes");
    const auto id = NewId("project");
    Json task = {{"name", "CyclicTask"}, {"type", "Cyclic"},
                 {"intervalUs", 1000},   {"priority", 0},
                 {"watchdogUs", 0},      {"cpuAffinity", -1},
                 {"enabled", true},      {"programs", Json::array({"Main"})}};
    Json files = Json::array({{{"name", "Main"},
                               {"kind", "program"},
                               {"content", "PROGRAM Main\nVAR\n    Counter : DINT := 0;\nEND_VAR\n\nCounter "
                                           ":= Counter + 1;\n\nEND_PROGRAM\n"}},
                              {{"name", "Globals"},
                               {"kind", "gvl"},
                               {"content", "VAR_GLOBAL\n    Ready : BOOL := FALSE;\nEND_VAR\n"}},
                              {{"name", "CyclicTask"}, {"kind", "task"}, {"content", task.dump(2) + "\n"}}});
    return {{"schemaVersion", 1},
            {"id", id},
            {"name", name},
            {"modifiedAt", now()},
            {"controllerIp", "127.0.0.1"},
            {"masters", Json::array()},
            {"hstr", {{"application", id}, {"files", files}}},
            {"scope", {{"intervalMs", 1}, {"channels", Json::array()}}}};
}
Json NewMaster(const std::string &id) {
    ValidateIdentifier(id);
    return {{"id", id},        {"name", id},          {"nic", "enp2s0"},
            {"cycleUs", 1000}, {"simulation", false}, {"slaves", Json::array()}};
}
static Json pdoJson(const ESI_Parse::Pdo &p) {
    Json entries = Json::array();
    for (const auto &e : p.entries)
        entries.push_back({{"index", e.index},
                           {"subIndex", e.subIndex},
                           {"bits", e.bitLen},
                           {"name", e.name},
                           {"type", e.dataType},
                           {"padding", e.padding}});
    return {{"index", p.index},
            {"name", p.name},
            {"bits", p.bitSize},
            {"mandatory", p.mandatory},
            {"fixed", p.fixed},
            {"defaultSelected", p.defaultSelected},
            {"selected", p.selected || p.mandatory},
            {"excludes", p.excludes},
            {"entries", entries}};
}
Json DeviceCatalog(const std::string &xml, const std::string &filename) {
    if (xml.size() > 32 * 1024 * 1024)
        throw std::runtime_error("ESI XML exceeds 32 MiB");
    ESI_Parse parser;
    if (!parser.loadFromString(xml) || parser.getDevices().empty())
        throw std::runtime_error("XML does not contain a supported EtherCAT device description");
    Json result = Json::array();
    for (const auto &d : parser.getDevices()) {
        Json tx = Json::array(), rx = Json::array(), dc = Json::array();
        for (const auto &p : d.txPdos)
            tx.push_back(pdoJson(p));
        for (const auto &p : d.rxPdos)
            rx.push_back(pdoJson(p));
        for (const auto &p : d.dcOpModes)
            dc.push_back({{"name", p.name},
                          {"description", p.desc},
                          {"assignActivate", p.assignActivate},
                          {"cycleTimeSync0", p.cycleTimeSync0},
                          {"cycleTimeSync1", p.cycleTimeSync1}});
        result.push_back({{"vendorId", parser.getVendorId()},
                          {"vendor", parser.getVendorName()},
                          {"productCode", d.productCode},
                          {"revision", d.revisionNo},
                          {"name", d.name.empty() ? d.type : d.name},
                          {"file", filename},
                          {"txPdos", tx},
                          {"rxPdos", rx},
                          {"dcModes", dc}});
    }
    return result;
}
Json LoadDeviceCatalog(const fs::path &directory) {
    fs::create_directories(directory);
    Json result = Json::array();
    for (const auto &entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file())
            continue;
        auto extension = entry.path().extension().u8string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension != ".xml")
            continue;
        try {
            for (auto device : DeviceCatalog(ReadText(entry.path()), entry.path().filename().u8string()))
                result.push_back(std::move(device));
        } catch (const std::exception &e) {
            result.push_back(
                {{"file", entry.path().filename().u8string()}, {"error", e.what()}, {"name", "Invalid XML"}});
        }
    }
    return result;
}
Json NewSlave(const Json &device, int id) {
    if (device.contains("error") || id < 1 || id > 128)
        throw std::runtime_error("Invalid device or slave id");
    Json slave = device;
    slave["id"] = id;
    slave["axes"] = Json::array();
    slave["dc"] = {{"enabled", false}, {"mode", "FreeRun"}, {"shiftUs", 0}};
    for (const auto *key : {"rxPdos", "txPdos"}) {
        bool selected = false;
        for (auto &p : slave[key])
            selected = selected || p.value("selected", false);
        if (!selected && !slave[key].empty())
            slave[key][0]["selected"] = true;
    }
    return slave;
}
const std::vector<AxisField> &AxisFields() {
    static const std::vector<AxisField> fields = {{"Controlword", "控制字", true, 0x6040, 16},
                                                  {"StatusWord", "状态字", false, 0x6041, 16},
                                                  {"TargetPosition", "目标位置", true, 0x607a, 32},
                                                  {"ActualPosition", "实际位置", false, 0x6064, 32},
                                                  {"TargetVelocity", "目标速度", true, 0x60ff, 32},
                                                  {"ActualVelocity", "实际速度", false, 0x606c, 32},
                                                  {"TargetTorque", "目标力矩", true, 0x6071, 16},
                                                  {"ActualTorque", "实际力矩", false, 0x6077, 16},
                                                  {"TargetModesOfOperation", "目标模式", true, 0x6060, 8},
                                                  {"ActualModesOfOperation", "实际模式", false, 0x6061, 8},
                                                  {"ErrorCode", "错误码", false, 0x603f, 16},
                                                  {"DigitalOutputs", "数字输出", true, 0x60fe, 32},
                                                  {"DigitalInputs", "数字输入", false, 0x60fd, 32}};
    return fields;
}
Json NewAxis(const Json &slave, int id, int slot) {
    Json axis = {{"id", id},
                 {"name", "Axis_" + std::to_string(id)},
                 {"slot", slot},
                 {"enabled", false},
                 {"txPdo", 0},
                 {"rxPdo", 0},
                 {"mapping", Json::object()},
                 {"parameters",
                  {{"encoder", 131072},
                   {"gearRatio", 1.0},
                   {"velocityScale", 1.0},
                   {"torqueScale", 1.0},
                   {"encoderDirection", 1},
                   {"maxVelocity", 100.0},
                   {"maxAcceleration", 1000.0},
                   {"maxJerk", 10000.0},
                   {"homeMethod", 35},
                   {"homeOffset", 0.0},
                   {"stopAcceleration", 1000.0},
                   {"stopJerk", 10000.0},
                   {"softMin", -1000000.0},
                   {"softMax", 1000000.0}}}};
    AutoMapAxis(axis, slave);
    return axis;
}
void SelectPdo(Json &slave, bool rx, unsigned index, bool selected) {
    auto &list = slave[rx ? "rxPdos" : "txPdos"];
    auto found = std::find_if(list.begin(), list.end(),
                              [index](const Json &p) { return p.value("index", 0u) == index; });
    if (found == list.end())
        throw std::runtime_error("PDO was not found");
    if (!selected && found->value("mandatory", false))
        throw std::runtime_error("Mandatory PDO cannot be disabled");
    if (selected) {
        for (auto &other : list) {
            if (other.value("index", 0u) == index)
                continue;
            const auto excludes = found->value("excludes", std::vector<int>{});
            const auto reverse = other.value("excludes", std::vector<int>{});
            if (std::find(excludes.begin(), excludes.end(), other.value("index", 0)) != excludes.end() ||
                std::find(reverse.begin(), reverse.end(), static_cast<int>(index)) != reverse.end()) {
                if (other.value("mandatory", false))
                    throw std::runtime_error("PDO conflicts with a mandatory channel");
                other["selected"] = false;
            }
        }
    }
    (*found)["selected"] = selected;
}
ProjectRepository::ProjectRepository(fs::path root) : root_(std::move(root)) {
    fs::create_directories(root_ / "PRJ");
    fs::create_directories(root_ / "ESI");
    if (!fs::exists(root_ / "manager.json"))
        WriteAtomic(root_ / "manager.json",
                    Json{{"schemaVersion", 1}, {"defaultProject", ""}, {"projects", Json::array()}}.dump(2));
}
Json ProjectRepository::Manager() const {
    return ReadJson(root_ / "manager.json");
}
std::vector<Json> ProjectRepository::List() const {
    std::vector<Json> out;
    const auto manager = Manager();
    for (const auto &p : manager.at("projects"))
        out.push_back(p);
    return out;
}
std::string ProjectRepository::DefaultProject() const {
    return Manager().value("defaultProject", "");
}
void ProjectRepository::UpdateManager(const Json &project, bool select) {
    auto manager = Manager();
    auto &entries = manager["projects"];
    bool found = false;
    for (auto &p : entries)
        if (p["id"] == project["id"]) {
            p = {{"id", project["id"]},
                 {"name", project["name"]},
                 {"modifiedAt", project.value("modifiedAt", now())}};
            found = true;
            break;
        }
    if (!found)
        entries.push_back({{"id", project["id"]},
                           {"name", project["name"]},
                           {"modifiedAt", project.value("modifiedAt", now())}});
    if (select)
        manager["defaultProject"] = project["id"];
    WriteAtomic(root_ / "manager.json", manager.dump(2));
}
void ProjectRepository::Save(const Json &original) {
    Json project = original;
    if (project.at("name").get<std::string>().empty() || project.at("name").get<std::string>().size() > 192)
        throw std::runtime_error("Invalid project name");
    const auto id = project.at("id").get<std::string>();
    ValidateIdentifier(id);
    project["modifiedAt"] = now();
    const auto folder = root_ / "PRJ" / id;
    fs::create_directories(folder / "HSTR");
    std::set<std::string> files;
    for (const auto &file : project.at("hstr").at("files")) {
        const auto name = SourceFilename(file);
        if (!files.insert(name).second)
            throw std::runtime_error("Duplicate source file name");
        WriteAtomic(folder / "HSTR" / fs::u8path(name), file.at("content").get<std::string>());
    }
    WriteAtomic(folder / "project.json", project.dump(2));
    UpdateManager(project, true);
}
Json ProjectRepository::Create(const std::string &name) {
    auto project = NewProject(name);
    Save(project);
    return project;
}
Json ProjectRepository::Open(const std::string &id) {
    ValidateIdentifier(id);
    auto project = ReadJson(root_ / "PRJ" / id / "project.json");
    if (project.value("id", "") != id || project.value("schemaVersion", 0) != 1)
        throw std::runtime_error("Project identity or schema mismatch");
    UpdateManager(project, true);
    return project;
}
Json ProjectRepository::OpenFile(const fs::path &path) {
    auto project = ReadJson(path);
    if (project.value("schemaVersion", 0) != 1)
        throw std::runtime_error("Unsupported project schema");
    const auto local = root_ / "PRJ" / project.at("id").get<std::string>() / "project.json";
    if (fs::exists(local) && fs::equivalent(path, local))
        return Open(project["id"].get<std::string>());
    return SaveAs(project, project.value("name", "Imported project"));
}
Json ProjectRepository::SaveAs(const Json &project, const std::string &name) {
    auto copy = project;
    copy["id"] = NewId("project");
    copy["name"] = name;
    copy["hstr"]["application"] = copy["id"];
    Save(copy);
    return copy;
}
void ProjectRepository::Remove(const std::string &id) {
    ValidateIdentifier(id);
    const auto folder = root_ / "PRJ" / id;
    const auto parent = fs::weakly_canonical(root_ / "PRJ");
    if (fs::weakly_canonical(folder).parent_path() != parent || fs::is_symlink(folder))
        throw std::runtime_error("Unsafe project path");
    auto manager = Manager();
    auto &list = manager["projects"];
    list.erase(
        std::remove_if(list.begin(), list.end(), [&](const Json &p) { return p.value("id", "") == id; }),
        list.end());
    if (manager.value("defaultProject", "") == id)
        manager["defaultProject"] = list.empty() ? "" : list[0].at("id").get<std::string>();
    fs::remove_all(folder);
    WriteAtomic(root_ / "manager.json", manager.dump(2));
}
static std::string csvEscape(const std::string &s) {
    if (s.find_first_of(",\"\r\n") == std::string::npos)
        return s;
    std::string out = "\"";
    for (char c : s) {
        out += c;
        if (c == '\"')
            out += '\"';
    }
    return out + '\"';
}
static std::vector<std::string> csvSplit(const std::string &line) {
    std::vector<std::string> out;
    std::string field;
    bool quote = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '\"') {
            if (quote && i + 1 < line.size() && line[i + 1] == '\"') {
                field += '\"';
                ++i;
            } else
                quote = !quote;
        } else if (c == ',' && !quote) {
            out.push_back(field);
            field.clear();
        } else if (c != '\r')
            field += c;
    }
    if (quote)
        throw std::runtime_error("Unterminated CSV quote");
    out.push_back(field);
    return out;
}
void ExportCsv(const fs::path &path, const std::vector<ScopeSample> &samples,
               const std::vector<std::string> &channels) {
    std::ostringstream out;
    out << "timestamp_us";
    for (const auto &c : channels)
        out << ',' << csvEscape(c);
    out << '\n' << std::setprecision(17);
    for (const auto &sample : samples) {
        out << sample.timestampUs;
        for (const auto &channel : channels) {
            out << ',';
            const auto it = sample.values.find(channel);
            if (it != sample.values.end())
                out << it->second;
        }
        out << '\n';
    }
    WriteAtomic(path, out.str());
}
std::vector<ScopeSample> ImportCsv(const fs::path &path, std::vector<std::string> &channels) {
    std::istringstream stream(ReadText(path, 128 * 1024 * 1024));
    std::string line;
    if (!std::getline(stream, line))
        throw std::runtime_error("Empty CSV");
    if (line.rfind("\xEF\xBB\xBF", 0) == 0)
        line.erase(0, 3);
    auto header = csvSplit(line);
    if (header.size() < 2 || header[0] != "timestamp_us" || header.size() > 129)
        throw std::runtime_error("CSV requires timestamp_us and 1..128 variable columns");
    channels.assign(header.begin() + 1, header.end());
    std::set<std::string> unique(channels.begin(), channels.end());
    if (unique.size() != channels.size())
        throw std::runtime_error("Duplicate CSV channels");
    std::vector<ScopeSample> out;
    while (std::getline(stream, line)) {
        if (line.empty())
            continue;
        auto fields = csvSplit(line);
        if (fields.size() != header.size())
            throw std::runtime_error("CSV column count mismatch");
        ScopeSample sample;
        std::size_t end = 0;
        if (fields[0].empty() || fields[0][0] == '-')
            throw std::runtime_error("Invalid CSV timestamp");
        sample.timestampUs = std::stoull(fields[0], &end);
        if (end != fields[0].size() || (!out.empty() && sample.timestampUs < out.back().timestampUs))
            throw std::runtime_error("CSV timestamps must be ordered");
        sample.sequence = out.size();
        for (std::size_t i = 1; i < fields.size(); ++i)
            if (!fields[i].empty()) {
                const auto value = std::stod(fields[i], &end);
                if (end != fields[i].size() || !std::isfinite(value))
                    throw std::runtime_error("CSV contains invalid numeric data");
                sample.values[channels[i - 1]] = value;
            }
        out.push_back(std::move(sample));
        if (out.size() > 1000000)
            throw std::runtime_error("CSV exceeds one million samples");
    }
    return out;
}
} // namespace LL
