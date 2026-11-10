#pragma once
#include <HSTR/HSTR.hpp>
#include <HSTR/module.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <map>

namespace HSTR::detail
{
namespace fs = std::filesystem;
using json = nlohmann::json;

struct BuildResult
{
    fs::path module;
    std::string diagnostics;
    json sourceMap;
};

/// 检查宿主 main 是否遗漏编译子模式分流；遗漏时抛错，防止自启动递归。
void rejectUnhandledCompilerInvocation();
/// 按工程清单建立源码快照，调用内嵌 matiec 和系统 cc，返回模块及诊断。
BuildResult build(const Options&,
                  const fs::path&,
                  const json&,
                  const std::map<std::string, std::string>& inputFiles);
/// 以 O_EXCL 创建零字节文件；文件已存在时抛错，绝不截断第三方内容。
void createEmptyFile(const fs::path& path);
/// 将旧 v1 清单转换成 v2 文件登记结构；只处理内存中的 .prj 元数据，不写 .task。
json normalizeProjectManifest(json project);
/// 以二进制方式读取整个文件；读取失败抛出 IOError。
std::string readFile(const fs::path&);
/// 写入同目录临时文件并 fsync/rename，避免读到半份工程内容。
void atomicWrite(const fs::path&, const std::string&);
/// 转为大写，用于 IEC 标识符的不区分大小写匹配。
std::string upper(std::string);
/// 校验 IEC 名称格式及 HSTR 保留前缀；非法名称抛出 InvalidArgument。
void validateName(const std::string&);
/// 严格校验第三方任务 JSON 的字段类型和取值，返回内存配置；不序列化、不写文件。
STTaskConfig parseTask(const json&);
} // namespace HSTR::detail
