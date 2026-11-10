"""Embed the IEC support library and HSTR module ABI in libHSTR."""
import pathlib, sys
root, output, generated = map(pathlib.Path, sys.argv[1:])
files = [("lib/" + p.relative_to(root / "third_party/matiec/lib").as_posix(), p)
         for p in sorted((root / "third_party/matiec/lib").rglob("*")) if p.is_file()]
files += [("hstr_module.h", root / "include/HSTR/module.h")]
files += [("HSTR/generated/" + name, generated / name)
          for name in ("host_fields.inc", "interface_signature.h")]
with output.open("w") as out:
    out.write('#pragma once\n#include <cstddef>\nnamespace hstr_detail {\nstruct Resource { const char* name; const unsigned char* data; size_t size; };\n')
    for i, (_, path) in enumerate(files):
        data = path.read_bytes()
        out.write(f'static const unsigned char r{i}[] = {{' + ','.join(map(str, data)) + '};\n')
    out.write('static const Resource resources[] = {\n')
    for i, (name, _) in enumerate(files):
        out.write(f'{{"{name}",r{i},sizeof(r{i})}},\n')
    out.write('};\n}\n')
