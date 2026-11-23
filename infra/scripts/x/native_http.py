"""Build the IO.Request and IO.Web 2.0 platform objects; no bundled HTTP or TLS libraries."""
import argparse
import os
import platform
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TRIPLES = {"win-x64": "x86_64-windows-gnu", "win-arm64": "aarch64-windows-gnu",
           "linux-x64": "x86_64-linux-gnu", "linux-arm64": "aarch64-linux-gnu"}

def host_target():
    arch = "arm64" if platform.machine().lower() in {"arm64", "aarch64"} else "x64"
    return ("win-" if os.name == "nt" else "linux-") + arch

def build(std, targets, packages, zig=None):
    src = ROOT / "libs/runtime/src"
    for target in targets:
        win = target.startswith("win-")
        if win and os.name == "nt" and shutil.which("clang"):
            triple = TRIPLES[target].replace("windows-gnu", "pc-windows-msvc")
            cc = ["clang", "--target=" + triple]
        elif not win and platform.system() == "Linux" and target == host_target():
            cc = [os.environ.get("CC", "cc")]
        else:
            cc = [str(zig or "zig"), "cc", "-target", TRIPLES[target]]
        flags = ["-O2", "-g0", "-std=c11", "-fno-stack-protector"]
        if win:
            flags += ["-D_CRT_SECURE_NO_WARNINGS"]
        if win and cc[1] == "cc":
            flags += ["-D_UCRT=1", "-D__USE_MINGW_ANSI_STDIO=0", "-fno-emulated-tls"]
        if not win:
            flags += ["-D_GNU_SOURCE", "-fPIC", "-pthread"]
        for name in packages:
            leaf = "kn_" + name.split(".")[1].lower() + "_native"
            obj = std / name / "2.0.0/native" / target / (leaf + (".obj" if win else ".o"))
            obj.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.TemporaryDirectory(dir=obj.parent) as tmp:
                out = Path(tmp) / obj.name
                subprocess.run([*cc, *flags, "-c", src / (leaf + ".c"), "-o", out], check=True)
                out.replace(obj)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--std", type=Path, default=ROOT / "libs/std")
    parser.add_argument("--target", action="append", choices=TRIPLES)
    parser.add_argument("--package", action="append", choices=["IO.Request", "IO.Web"])
    args = parser.parse_args()
    build(args.std, args.target or [host_target()], args.package or ["IO.Request", "IO.Web"])
