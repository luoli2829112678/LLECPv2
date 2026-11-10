#pragma once
#include "LL_CpsProtocol.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>
namespace LL {
namespace fs = std::filesystem;
fs::path CpsDataRoot();
void WriteAtomic(const fs::path &path, const std::string &data);
std::string ReadText(const fs::path &path, std::size_t maximum = 32 * 1024 * 1024);
Json ReadJson(const fs::path &path);
void ValidateIdentifier(const std::string &id);
std::string SourceFilename(const Json &file);
Json NewProject(const std::string &name);
Json NewMaster(const std::string &id);
Json DeviceCatalog(const std::string &xml, const std::string &filename);
Json LoadDeviceCatalog(const fs::path &directory);
Json NewSlave(const Json &device, int id);
struct AxisField {
    const char *name;
    const char *label;
    bool rx;
    unsigned index;
    unsigned bits;
};
const std::vector<AxisField> &AxisFields();
Json NewAxis(const Json &slave, int id, int slot = 0);
void SelectPdo(Json &slave, bool rx, unsigned index, bool selected);
class ProjectRepository {
  public:
    explicit ProjectRepository(fs::path root = CpsDataRoot());
    std::vector<Json> List() const;
    Json Create(const std::string &name);
    Json Open(const std::string &id);
    Json OpenFile(const fs::path &path);
    void Save(const Json &project);
    Json SaveAs(const Json &project, const std::string &name);
    void Remove(const std::string &id);
    std::string DefaultProject() const;
    const fs::path &Root() const {
        return root_;
    }

  private:
    fs::path root_;
    Json Manager() const;
    void UpdateManager(const Json &project, bool select);
};
struct ScopeSample {
    std::uint64_t timestampUs = 0;
    std::uint64_t sequence = 0;
    std::map<std::string, double> values;
};
void ExportCsv(const fs::path &path, const std::vector<ScopeSample> &samples,
               const std::vector<std::string> &channels);
std::vector<ScopeSample> ImportCsv(const fs::path &path, std::vector<std::string> &channels);
} // namespace LL
