#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
cd "$script_dir/../third_party/matiec/stage1_2"
bison -d -Wno-yacc --defines=iec_bison.hh -o iec_bison.cc iec_bison.yy
flex -o iec_flex.cc iec_flex.ll
