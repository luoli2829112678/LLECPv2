"""从唯一接口清单生成 C/C++ 和 ST 桥接代码；只在构建阶段运行。"""
import argparse
import hashlib
import json
from pathlib import Path
import re


# IEC 类型只在这里映射一次。新增普通接口不需要修改生成器。
TYPES = {
    "VOID": "void", "BOOL": "bool",
    "SINT": "int8_t", "USINT": "uint8_t", "BYTE": "uint8_t",
    "INT": "int16_t", "UINT": "uint16_t", "WORD": "uint16_t",
    "DINT": "int32_t", "UDINT": "uint32_t", "DWORD": "uint32_t",
    "LINT": "int64_t", "ULINT": "uint64_t", "LWORD": "uint64_t",
    "REAL": "float", "LREAL": "double",
}
IDENTIFIER = re.compile(r"[A-Za-z][A-Za-z0-9]*(?:_[A-Za-z0-9]+)*\Z")
CPP_IDENTIFIER = re.compile(r"[A-Za-z][A-Za-z0-9_]*\Z")
SDK_NAME = re.compile(r"[A-Za-z_][A-Za-z_0-9]*(?:::[A-Za-z_][A-Za-z_0-9]*)*\Z")
IEC_RESERVED = set((
    "ACTION AND ARRAY AT BY CASE CONFIGURATION CONSTANT DO ELSE ELSIF END_ACTION "
    "END_CASE END_CONFIGURATION END_FOR END_FUNCTION END_FUNCTION_BLOCK END_IF "
    "END_PROGRAM END_REPEAT END_RESOURCE END_STEP END_STRUCT END_TRANSITION "
    "END_TYPE END_VAR END_WHILE EN ENO EXIT FALSE FOR FROM FUNCTION FUNCTION_BLOCK "
    "IF INITIAL_STEP MOD NOT OF ON OR PROGRAM REPEAT RESOURCE RETAIN RETURN STEP "
    "STRUCT TASK THEN TO TRANSITION TRUE TYPE UNTIL VAR VAR_ACCESS VAR_CONFIG "
    "VAR_EXTERNAL VAR_GLOBAL VAR_INPUT VAR_IN_OUT VAR_OUTPUT VAR_TEMP WHILE WITH XOR"
).split()) | set(TYPES)
CPP_RESERVED = set((
    "alignas alignof and and_eq asm auto bitand bitor bool break case catch char "
    "char16_t char32_t class compl const constexpr const_cast continue decltype "
    "default delete do double dynamic_cast else enum explicit export extern false "
    "float for friend goto if inline int long mutable namespace new noexcept not "
    "not_eq nullptr operator or or_eq private protected public register "
    "reinterpret_cast return short signed sizeof static static_assert static_cast "
    "struct switch template this thread_local throw true try typedef typeid "
    "typename union unsigned using virtual void volatile wchar_t while xor xor_eq"
).split())
BANNER = "// 自动生成自 interfaces/llpc.json，请修改清单后重新构建。\n"


def fields(value, allowed, required, where):
    """尽早报告拼错的字段，避免配置被静默忽略。"""
    if not isinstance(value, dict):
        raise ValueError(f"{where}: expected an object")
    extra, missing = set(value) - allowed, required - set(value)
    if extra or missing:
        raise ValueError(f"{where}: unknown fields {sorted(extra)}, missing fields {sorted(missing)}")


def iec_name(value, where):
    if (not isinstance(value, str) or not IDENTIFIER.fullmatch(value)
            or value.upper() in IEC_RESERVED or value.upper().startswith("HSTR")):
        raise ValueError(f"{where}: invalid/reserved IEC identifier {value!r}")
    return value.upper()


def parameters(value, where):
    if not isinstance(value, list):
        raise ValueError(f"{where}: params must be an array of [name, IEC type]")
    result, seen = [], set()
    for pair in value:
        if not isinstance(pair, list) or len(pair) != 2:
            raise ValueError(f"{where}: expected [name, IEC type], got {pair!r}")
        name = iec_name(pair[0], where)
        kind = pair[1]
        if not isinstance(kind, str) or kind not in TYPES or kind == "VOID":
            raise ValueError(f"{where}: unsupported input type {kind!r}")
        if name in seen:
            raise ValueError(f"{where}: duplicate parameter {name}")
        seen.add(name)
        result.append([name, kind])
    return result


def description(value, where):
    if not isinstance(value, str) or any(c in value for c in "\r\n\\\x00"):
        raise ValueError(f"{where}: description must be one line without backslashes")
    return value


def load_manifest(path):
    data = json.loads(path.read_text(encoding="utf-8"))
    fields(data, {"functions"}, {"functions"}, "manifest")
    if not isinstance(data["functions"], list):
        raise ValueError("functions must be an array")
    functions, names, callbacks = [], set(), {"user", "trace"}
    allowed = {"name", "return", "callback", "sdk", "params", "description", "aliases"}
    for entry in data["functions"]:
        fields(entry, allowed, {"name", "return", "callback", "sdk", "params"}, "function")
        name = iec_name(entry["name"], "function")
        kind, callback, sdk = entry["return"], entry["callback"], entry["sdk"]
        if not isinstance(kind, str) or kind not in TYPES:
            raise ValueError(f"{name}: unsupported return type {kind!r}")
        if (not isinstance(callback, str) or not CPP_IDENTIFIER.fullmatch(callback)
                or callback in CPP_RESERVED or callback in callbacks or "__" in callback):
            raise ValueError(f"{name}: invalid/duplicate callback {callback!r}")
        if not isinstance(sdk, str) or not SDK_NAME.fullmatch(sdk):
            raise ValueError(f"{name}: sdk must be a C++ function name, e.g. AXIS_Enable")
        callbacks.add(callback)
        params = parameters(entry["params"], name)
        variants = [{"name": name, "params": params,
                     "description": description(entry.get("description", ""), name)}]
        aliases = entry.get("aliases", [])
        if not isinstance(aliases, list):
            raise ValueError(f"{name}: aliases must be an array")
        for alias in aliases:
            if isinstance(alias, str):
                alias = {"name": alias}
            fields(alias, {"name", "params", "description"}, {"name"}, f"{name} alias")
            alias_name = iec_name(alias["name"], f"{name} alias")
            alias_params = parameters(alias.get("params", params), alias_name)
            if [p[1] for p in alias_params] != [p[1] for p in params]:
                raise ValueError(f"{alias_name}: alias parameter types/order must match {name}")
            variants.append({"name": alias_name, "params": alias_params,
                             "description": description(alias.get("description", f"{name} 的别名。"), alias_name)})
        for variant in variants:
            if variant["name"] in names:
                raise ValueError(f"duplicate ST function/alias: {variant['name']}")
            if variant["name"] in {p[0] for p in variant["params"]}:
                raise ValueError(f"{variant['name']}: parameter conflicts with function result")
            names.add(variant["name"])
        functions.append({"return": kind, "callback": callback, "sdk": sdk, "variants": variants})
    return functions


def signature(functions):
    """完整 SHA-256 同时保护回调布局、ST 签名和接口顺序，不需要手工改版本号。"""
    shape = [{"return": f["return"], "callback": f["callback"],
              "variants": [{"name": v["name"], "params": v["params"]} for v in f["variants"]]}
             for f in functions]
    payload = json.dumps({"generator_abi": 1, "functions": shape}, sort_keys=True).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def generate(functions):
    """生成强类型直连代码；Cycle 中没有按名字查询、JSON 解析或参数装箱。"""
    output = {name: BANNER for name in (
        "callbacks.inc", "host_fields.inc", "host_bindings.inc",
        "runtime_declarations.inc", "runtime_definitions.inc", "llpc_adapter.inc")}
    st = []
    rows = ["# 自动生成的接口表", "", "修改 `interfaces/llpc.json` 后重新构建。", "",
            "| ST / Runtime 函数 | 返回类型 | 回调字段 | SDK 函数 |",
            "|---|---|---|---|"]
    for func in functions:
        result, callback = TYPES[func["return"]], func["callback"]
        first = func["variants"][0]
        types = [TYPES[p[1]] for p in first["params"]]
        # C++ 形参使用固定前缀，以免与 SDK、宏或局部变量名称发生冲突。
        args = [f"hstr_arg_{i}" for i in range(len(types))]
        declarations = [f"{kind} {arg}" for kind, arg in zip(types, args)]
        callback_types = ", ".join(["void*"] + types)
        host_params = ", ".join(["void* hstr_context"] + declarations)
        actual = ", ".join(args)
        forwarded = ", ".join(["h.user"] + args)
        doc = first["description"] or first["name"]
        output["callbacks.inc"] += f"/// {doc}\n{result} (*{callback})({callback_types}) = nullptr;\n"
        output["host_fields.inc"] += f"{result} (*{callback})({callback_types});\n"
        output["host_bindings.inc"] += (
            f"host.{callback} = []({host_params}) -> {result}\n{{\n"
            "    auto& h = static_cast<Impl*>(hstr_context)->hardware;\n"
            f"    if (!h.{callback})\n    {{\n"
            f'        throw Error(ErrorCode::HardwareUnavailable, "{first["name"]}: callback {callback} missing");\n'
            f"    }}\n    return h.{callback}({forwarded});\n}};\n")
        # return void-expression 在 C++ 中合法；非 void 值也由声明返回类型转换。
        sdk_call = f'{func["sdk"]}({actual})'
        sdk_body = f"{sdk_call};" if result == "void" else f"return {sdk_call};"
        output["llpc_adapter.inc"] += (
            f"h.{callback} = []({', '.join(['void*'] + declarations)}) -> {result}\n{{\n"
            f"    {sdk_body}\n}};\n")
        for variant in func["variants"]:
            name, params = variant["name"], variant["params"]
            declaration = ", ".join(declarations)
            output["runtime_declarations.inc"] += (
                f'/// {variant["description"] or name}\n'
                f"{result} {name}({declaration});\n")
            output["runtime_definitions.inc"] += (
                f"{result} Runtime::{name}({declaration})\n{{\n"
                "    std::lock_guard<std::mutex> lock(p_->mutex);\n"
                f"    return p_->host.{callback}({', '.join(['p_.get()'] + args)});\n}}\n")
            st.append(f"FUNCTION {name} : {func['return']}")
            if params:
                st += ["VAR_INPUT"] + [f"    {n} : {t};" for n, t in params] + ["END_VAR"]
            else:
                # 此版 matiec 的函数声明必须有变量区；局部占位变量不增加调用参数。
                st += ["VAR", "    HSTR_UNUSED : BOOL;", "END_VAR"]
            call = f"hstr_host.{callback}({', '.join(['hstr_host.user'] + [p[0] for p in params])})"
            st.append("{" + (f"{name} = " if result != "void" else "") + call + ";}")
            st.append("END_FUNCTION\n")
            readable = ", ".join(f"{n}: {t}" for n, t in params)
            rows.append(f"| `{name}({readable})` | {func['return']} | `{callback}` | `{func['sdk']}` |")
    fingerprint = signature(functions)
    output["interface_signature.h"] = (
        BANNER + '#ifndef HSTR_INTERFACE_SIGNATURE_H\n#define HSTR_INTERFACE_SIGNATURE_H\n'
        f'#define HSTR_INTERFACE_SIGNATURE "{fingerprint}"\n#endif\n')
    text = "\n".join(st) + "\n"
    output["hardware_st.hpp"] = (
        BANNER + '#pragma once\nnamespace hstr_detail\n{\n'
        'inline constexpr char hardware_st[] = R"HSTR_ST(\n' + text + ')HSTR_ST";\n}\n')
    output["hardware.st"] = text
    output["INTERFACES.md"] = "\n".join(rows) + "\n"
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output_directory", type=Path)
    args = parser.parse_args()
    try:
        rendered = generate(load_manifest(args.manifest))
    except (ValueError, OSError) as error:
        parser.exit(1, f"Interface generation failed: {error}\n")
    args.output_directory.mkdir(parents=True, exist_ok=True)
    for name, text in rendered.items():
        path = args.output_directory / name
        # 不改写内容相同的产物，减少重编译；stamp 由 CMake 单独更新。
        if not path.exists() or path.read_text(encoding="utf-8") != text:
            path.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
