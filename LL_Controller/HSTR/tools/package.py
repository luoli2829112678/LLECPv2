"""Create the distributable, excluding compiler build intermediates."""
from pathlib import Path
import argparse
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
# 发布约束：预编译目录只允许唯一 demo，禁止旧启动器或独立测试程序混入。
binary_dir = root / "prebuilt/linux-x86_64/bin"
if binary_dir.exists():
    binary_names = {path.name for path in binary_dir.iterdir() if path.is_file()}
    if binary_names != {"hstr_demo"}:
        raise RuntimeError(f"Expected only hstr_demo in bin, got: {sorted(binary_names)}")
args.output.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(args.output, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root)
        if any(part in {"build", ".git", "__pycache__"} or part.startswith(".hstr-build-") for part in relative.parts):
            continue
        if path.is_file() and path.resolve() != args.output.resolve():
            archive.write(path, Path("HSTR") / relative)
print(args.output.resolve())
