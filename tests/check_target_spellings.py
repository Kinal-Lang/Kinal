"""Canonical Windows aliases and freestanding spellings: IR + object regression.

Works with either compiler. No emitted foreign executable is linked or run.
Vendorless Windows input must be normalized before reaching LLVM: passing the
raw three-component spelling previously produced ELF rather than Windows COFF.
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import subprocess
from pathlib import Path


CASES = (
    ("i686-windows-msvc", "i686-pc-windows-msvc", 1, 1),
    ("x86_64-windows-msvc", "x86_64-pc-windows-msvc", 1, 2),
    ("aarch64-windows-msvc", "aarch64-pc-windows-msvc", 1, 3),
    ("bare64", "x86_64-unknown-none-elf", 0, 2),
    ("freestanding64", "x86_64-unknown-none-elf", 0, 2),
    ("kernel64", "x86_64-unknown-none-elf", 0, 2),
    ("bare-arm64", "aarch64-unknown-none-elf", 0, 3),
    ("freestanding-arm64", "aarch64-unknown-none-elf", 0, 3),
    ("kernel-arm64", "aarch64-unknown-none-elf", 0, 3),
    ("x86_64-unknown-none-elf", "x86_64-unknown-none-elf", 0, 2),
    ("aarch64-unknown-none-elf", "aarch64-unknown-none-elf", 0, 3),
    ("x86_64-unknown-none", "x86_64-unknown-none", 0, 2),
    ("aarch64-unknown-none", "aarch64-unknown-none", 0, 3),
)

SOURCE = '''Unit Tests.TargetSpellings;
Get IO.Target;
Static Function int probe_os() { Return IO.Target.OS; }
Static Function int probe_arch() { Return IO.Target.Arch; }
Static Function int probe_bits() { Return IO.Target.PointerBits; }
Static Function int probe_env() { Return IO.Target.Env; }
Static Function int Main() { Return 0; }
Static Function void KMain() {}
'''


def invoke(command: list[str], root: Path) -> str:
    process = subprocess.run(command, cwd=root, text=True, capture_output=True, timeout=180)
    output = process.stdout + process.stderr
    if process.returncode:
        raise AssertionError(f"command failed: {command}\n{output}")
    return output


def function_body(ir: str, name: str) -> str:
    match = re.search(r'^define [^\n]*@[^\n(]*' + re.escape(name) +
                      r'(?:_\d+)?\([^\n]*\).*?\n}\n', ir, re.M | re.S)
    assert match, f"missing function {name}"
    return match.group()


def check_target_spellings(compiler: Path, root: Path, out: Path,
                           *, windows_only: bool = False,
                           bare_only: bool = False) -> dict[str, object]:
    out.mkdir(parents=True, exist_ok=True)
    source = out / "Main.kn"
    source.write_text(SOURCE, encoding="utf-8")
    cases = [case for case in CASES if (not windows_only or case[2] == 1) and
             (not bare_only or case[2] == 0)]
    records = []
    for alias, triple, os_id, arch in cases:
        command = [str(compiler), "build", str(source), "--no-module-discovery", "--target", alias]
        ir_path = out / (alias + ".ll")
        invoke(command + ["--emit", "ir", "-o", str(ir_path)], root)
        ir = ir_path.read_text(encoding="utf-8")
        assert f'target triple = "{triple}"' in ir, (alias, triple)
        layout_match = re.search(r'^target datalayout = "([^"\n]+)"', ir, re.M)
        assert layout_match, alias
        layout = layout_match.group(1)
        for name, value in (("os", os_id), ("arch", arch),
                            ("bits", 32 if arch == 1 else 64),
                            ("env", 1 if os_id == 1 else 2)):
            assert f"ret i64 {value}" in function_body(ir, "probe_" + name), (alias, name)
        if arch == 1:
            assert "-p:32:32" in layout and "-i64:64" in layout, layout
        obj = out / (alias + ".obj")
        invoke(command + ["--emit", "obj", "-o", str(obj)], root)
        data = obj.read_bytes()
        if os_id == 1:
            machine = struct.unpack_from("<H", data)[0]
            assert machine == {1: 0x14C, 2: 0x8664, 3: 0xAA64}[arch], (alias, machine)
            object_format = "COFF"
        else:
            assert data[:5] == b"\x7fELF\x02", alias
            machine = struct.unpack_from("<H", data, 18)[0]
            assert machine == (183 if arch == 3 else 62), (alias, machine)
            object_format = "ELF64"
            assert "@__kn_entry(" in ir and not re.search(r'^define .*@main\(', ir, re.M)
        records.append({"input": alias, "triple": triple, "layout": layout,
                        "os": os_id, "arch": arch, "pointer_bits": 32 if arch == 1 else 64,
                        "object_format": object_format, "machine": machine})
        print(f"[OK] target spelling {alias}: {triple}, {object_format}", flush=True)
    report = {"name": "target_spellings", "ok": True, "cases": records}
    (out / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    subset = parser.add_mutually_exclusive_group()
    subset.add_argument("--windows-only", action="store_true")
    subset.add_argument("--bare-only", action="store_true")
    args = parser.parse_args()
    report = check_target_spellings(args.compiler.resolve(), Path(__file__).resolve().parents[1],
                                    args.out_dir.resolve(), windows_only=args.windows_only,
                                    bare_only=args.bare_only)
    print(json.dumps(report))


if __name__ == "__main__":
    main()
