"""在临时工程中验证：只追加接口清单，再增量构建，就能从 ST 调用新 SDK。"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


# 独立选择边界值，验证符号位、64 位精度、BOOL 和浮点参数没有被错误装箱。
CASES = [
    ("BOOL", "bool", "TRUE", "true"),
    ("SINT", "int8_t", "SINT#-127", "-127"),
    ("USINT", "uint8_t", "USINT#250", "250"),
    ("BYTE", "uint8_t", "BYTE#16#E1", "0xE1"),
    ("INT", "int16_t", "INT#-32767", "-32767"),
    ("UINT", "uint16_t", "UINT#65000", "65000"),
    ("WORD", "uint16_t", "WORD#16#FEDC", "0xFEDC"),
    ("DINT", "int32_t", "DINT#-2147483647", "-2147483647"),
    ("UDINT", "uint32_t", "UDINT#4000000000", "4000000000U"),
    ("DWORD", "uint32_t", "DWORD#16#FEDCBA98", "0xFEDCBA98U"),
    ("LINT", "int64_t", "LINT#-9007199254740993", "-9007199254740993LL"),
    ("ULINT", "uint64_t", "ULINT#18446744073709551600", "18446744073709551600ULL"),
    ("LWORD", "uint64_t", "LWORD#16#FEDCBA9876543210", "0xFEDCBA9876543210ULL"),
    ("REAL", "float", "REAL#1.25", "1.25f"),
    ("LREAL", "double", "LREAL#-42.125", "-42.125"),
]

DRIVER = r'''
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "LLPCslave.h"
#include "LLPCAxis.h"

static bool ready = false;
template<class T> T TestIdentity(T value) { return value; }
static void TestWrite(int32_t axis, double pos, float vel, bool enabled, uint64_t mask, int64_t delta)
{
    if (axis != 7 || pos != 12.5 || vel != 2.5f || !enabled ||
        mask != 18446744073709551600ULL || delta != -9007199254740993LL)
        throw std::runtime_error("mixed SDK argument mismatch");
    ready = true;
}
static bool TestReady() { return ready; }
#include <HSTR/LLPCAdapter.hpp>

int main(int argc, char** argv)
{
    try
    {
        int result = 0;
        if (HSTR::HandleCompilerInvocation(argc, argv, result)) return result;
        if (argc != 2) return 2;
        HSTR::Runtime runtime({argv[1], "cc"});
        runtime.SetHardwareCallbacks(HSTR::MakeLLPCHardwareCallbacks());
        // 宿主直接方法也无需手工增加声明/实现。
        if (runtime.EXT_READY()) throw std::runtime_error("initial readiness");
        runtime.EXT_WRITE(7, 12.5, 2.5f, true, 18446744073709551600ULL, -9007199254740993LL);
        if (!runtime.EXT_READY()) throw std::runtime_error("direct VOID/zero-argument bridge");
        ready = false;
        @DIRECT_CHECKS@
        runtime.AddApplication("Extension");
        runtime.SelectApplication("Extension");
        runtime.AddProgram("Main");
        runtime.AddTask("Scan");
        const auto app = std::filesystem::path(argv[1]) / "Extension";
        std::ofstream(app / "Main.st") << R"ST(@ST_SOURCE@)ST";
        std::ofstream(app / "Scan.task") << R"JSON({"name":"Scan","type":"Cyclic","intervalUs":1000,"priority":0,"watchdogUs":0,"cpuAffinity":-1,"enabled":true,"programs":["Main"]})JSON";
        runtime.CompileProgram();
        runtime.ExecuteProgram();
        if (runtime.Cycle(0) != HSTR::ApplicationStatus::Running)
            throw std::runtime_error(runtime.GetLastError());
        if (!ready || !runtime.ReadVariable<bool>("MAIN_INSTANCE.READYVALUE"))
            throw std::runtime_error("ST mixed/VOID/zero-argument bridge");
        @ST_CHECKS@
        runtime.StopProgram();
        std::cout << "PASS manifest-only extension: 17 new interfaces; 15 scalar types, "
                     "6 mixed inputs, VOID and zero-argument calls through Runtime and ST -> SDK\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
'''


def run(command, log):
    result = subprocess.run([str(arg) for arg in command], stdout=log, stderr=subprocess.STDOUT)
    log.flush()
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {command[0]}; see {log.name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--cxx", default="c++")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--work-dir", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    work = args.work_dir or Path(tempfile.mkdtemp(prefix="hstr-interface-extension-"))
    work = work.resolve()
    source, build = work / "source", work / "build"
    source.mkdir(parents=True, exist_ok=False)
    # 源码只读复用；唯一接口清单复制到临时目录，原工程绝不改写。
    for name in ("CMakeLists.txt", "src", "include", "tools", "third_party", "demo", "README.md", "THIRD_PARTY_NOTICES.md"):
        (source / name).symlink_to(root / name, target_is_directory=(root / name).is_dir())
    (source / "interfaces").mkdir()
    manifest = source / "interfaces/llpc.json"
    shutil.copy2(root / "interfaces/llpc.json", manifest)
    print(f"Extension verification directory: {work}", flush=True)
    with (work / "build.log").open("w", encoding="utf-8") as log:
        run([args.cmake, "-S", source, "-B", build, "-DCMAKE_BUILD_TYPE=Release"], log)
        run([args.cmake, "--build", build, "--target", "HSTR", "-j", args.jobs], log)
        signature_path = build / "generated/include/HSTR/generated/interface_signature.h"
        old_signature = signature_path.read_text()
        original_stat = (build / "HSTR.so").stat().st_mtime_ns
        # 正常无变更构建不应重复生成或重新链接 HSTR.so。
        run([args.cmake, "--build", build, "--target", "HSTR", "-j", args.jobs], log)
        if (build / "HSTR.so").stat().st_mtime_ns != original_stat:
            raise RuntimeError("no-op build unexpectedly relinked HSTR.so")

        data = json.loads(manifest.read_text(encoding="utf-8"))
        for iec, _, _, _ in CASES:
            data["functions"].append({"name": f"EXT_{iec}", "return": iec,
                "callback": f"ext{iec}", "sdk": "TestIdentity", "params": [["value", iec]]})
        data["functions"] += [
            {"name": "EXT_WRITE", "return": "VOID", "callback": "extWrite", "sdk": "TestWrite",
             "params": [["axis", "DINT"], ["pos", "LREAL"], ["vel", "REAL"], ["enabled", "BOOL"], ["mask", "ULINT"], ["delta", "LINT"]]},
            {"name": "EXT_READY", "return": "BOOL", "callback": "extReady", "sdk": "TestReady", "params": []},
        ]
        manifest.write_text(json.dumps(data, indent=2), encoding="utf-8")
        # 关键验证：只改 JSON，直接增量构建，不重新执行 cmake -S/-B。
        run([args.cmake, "--build", build, "--target", "HSTR", "-j", args.jobs], log)
        if signature_path.read_text() == old_signature:
            raise RuntimeError("interface signature was not regenerated")

        st = "PROGRAM Main\nVAR\nReadyValue : BOOL;\n"
        st += "\n".join(f"V{iec} : {iec};" for iec, _, _, _ in CASES)
        st += "\nEND_VAR\nEXT_WRITE(7, 12.5, REAL#2.5, TRUE, ULINT#18446744073709551600, LINT#-9007199254740993);\n"
        st += "ReadyValue := EXT_READY();\n"
        st += "\n".join(f"V{iec} := EXT_{iec}({value});" for iec, _, value, _ in CASES)
        st += "\nEND_PROGRAM\n"
        direct, checks = [], []
        for iec, cpp, _, value in CASES:
            direct.append(f'if (runtime.EXT_{iec}(static_cast<{cpp}>({value})) != static_cast<{cpp}>({value})) throw std::runtime_error("direct {iec}");')
            checks.append(f'if (runtime.ReadVariable<{cpp}>("MAIN_INSTANCE.V{iec}") != static_cast<{cpp}>({value})) throw std::runtime_error("ST {iec}");')
        driver = work / "extension_host.cpp"
        driver.write_text(DRIVER.replace("@ST_SOURCE@", st)
                         .replace("@DIRECT_CHECKS@", "\n".join(direct))
                         .replace("@ST_CHECKS@", "\n".join(checks)), encoding="utf-8")
        executable = work / "extension_host"
        run([args.cxx, "-std=c++17", "-I" + str(root / "demo"), "-I" + str(source / "include"),
             "-I" + str(build / "generated/include"), driver, build / "HSTR.so",
             "-Wl,-rpath," + str(build), "-o", executable], log)
    result = subprocess.run([str(executable), str(work / "applications")], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (work / "runtime.log").write_text(result.stdout, encoding="utf-8")
    print(result.stdout, end="", flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
