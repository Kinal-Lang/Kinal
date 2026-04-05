#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_COMPILER = ROOT / "out" / "selfhost" / "stage0-host" / "selfhost-stage0.exe"
DEFAULT_OUT_DIR = ROOT / "out" / "selfhost" / "tests_selfhost"
DEFAULT_MANIFEST = ROOT / "tests" / "manifest.json"


def run(cmd: list[str], cwd: Path, capture: bool = False) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=str(cwd), text=True, capture_output=capture)


def llvm_bin_dir() -> Path:
    env = os.environ.get("KN_LLVM_BIN", "").strip()
    if env:
        return Path(env).resolve()
    return (ROOT / "third_party" / "llvm" / "prebuilt" / "clang+llvm-21.1.8-x86_64-pc-windows-msvc" / "bin").resolve()


def build_native_ffi_assets(out_dir: Path) -> None:
    src = ROOT / "tests" / "ffi_native" / "native_ffi.c"
    if not src.exists():
        return

    llvm_bin = llvm_bin_dir()
    clang = llvm_bin / "clang.exe"
    llvm_lib = llvm_bin / "llvm-lib.exe"
    lld_link = llvm_bin / "lld-link.exe"
    if not clang.exists() or not llvm_lib.exists() or not lld_link.exists():
        raise SystemExit("LLVM toolchain for FFI tests not found; check KN_LLVM_BIN or third_party/llvm/prebuilt")

    out_dir.mkdir(parents=True, exist_ok=True)
    obj = out_dir / "native_ffi.obj"
    lib = out_dir / "native_ffi.lib"
    dll = out_dir / "native_ffi.dll"
    imp = out_dir / "native_ffi_import.lib"
    for path in (obj, lib, dll, imp):
        if path.exists():
            path.unlink()

    subprocess.check_call(
        [
            str(clang),
            "-c",
            str(src),
            "-o",
            str(obj),
            "-ffreestanding",
            "-fno-builtin",
            "-fno-stack-protector",
            "-fno-exceptions",
            "-fno-asynchronous-unwind-tables",
        ],
        cwd=str(ROOT),
    )
    subprocess.check_call([str(llvm_lib), f"/out:{lib}", str(obj)], cwd=str(ROOT))
    subprocess.check_call(
        [
            str(lld_link),
            "/nologo",
            "/dll",
            "/noentry",
            "/machine:x64",
            f"/out:{dll}",
            f"/implib:{imp}",
            str(obj),
        ],
        cwd=str(ROOT),
    )


def main() -> int:
    ap = argparse.ArgumentParser(description="Run core C-suite manifest through selfhost compiler")
    ap.add_argument("--compiler", default=str(DEFAULT_COMPILER))
    ap.add_argument("--root", default=str(ROOT))
    ap.add_argument("--backend", choices=["host", "self"], default="self")
    ap.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR))
    ap.add_argument("--manifest", default=str(DEFAULT_MANIFEST))
    args = ap.parse_args()

    root = Path(args.root).resolve()
    compiler = Path(args.compiler).resolve()
    out_dir = Path(args.out_dir).resolve()
    manifest_path = Path(args.manifest).resolve()
    if not compiler.exists():
        print(f"[FATAL] compiler not found: {compiler}")
        return 2
    if not manifest_path.exists():
        print(f"[FATAL] manifest not found: {manifest_path}")
        return 2

    out_dir.mkdir(parents=True, exist_ok=True)
    build_native_ffi_assets(out_dir)

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    passed = 0
    total = len(manifest)
    backend = args.backend

    print(f"[INFO] backend={backend}")

    for case in manifest:
        name = case["name"]
        files = case.get("files")
        if files:
            kn_files = [root / f for f in files]
        else:
            kn_files = [root / case["file"]]
        expect_error = case.get("expect_error")
        auto_link = case.get("auto_link", False)
        out_exe = out_dir / f"{name}.exe"
        if out_exe.exists():
            out_exe.unlink()

        cmd = [str(compiler), "--backend", backend, "--phase", "full"]
        if not auto_link:
            cmd.append("--no-auto-link")
        for arg in case.get("compiler_args", []):
            value = str(arg)
            if not value.startswith("-"):
                path = Path(value)
                if not path.is_absolute():
                    candidate = root / path
                    if candidate.exists():
                        value = str(candidate)
            cmd.append(value)
        cmd.extend([str(f) for f in kn_files])
        cmd.extend(["-o", str(out_exe)])

        proc = run(cmd, root, capture=True)
        combined = ((proc.stdout or "") + (proc.stderr or "")).replace("\r\n", "\n")

        if expect_error:
            if proc.returncode == 0:
                print(f"[FAIL] {name}")
                print("Expected compiler error but it succeeded")
                continue
            if expect_error not in combined:
                print(f"[FAIL] {name}")
                print("Expected error substring:")
                print(repr(expect_error))
                print("Got:")
                print(repr(combined))
                continue
            print(f"[OK] {name}")
            passed += 1
            continue

        if proc.returncode != 0 or not out_exe.exists():
            print(f"[FAIL] {name}")
            print(f"compile failed: rc={proc.returncode}")
            print(combined.rstrip())
            continue

        for runtime_file in case.get("runtime_files", []):
            src = root / runtime_file
            dst = out_dir / Path(runtime_file).name
            if src.resolve() != dst.resolve():
                shutil.copy2(src, dst)

        output = run([str(out_exe)], out_dir, capture=True)
        out_text = ((output.stdout or "") + (output.stderr or "")).replace("\r\n", "\n")
        expected = case["expected"].replace("\r\n", "\n")

        if out_text != expected:
            print(f"[FAIL] {name}")
            print("Expected:")
            print(repr(expected))
            print("Got:")
            print(repr(out_text))
            continue

        print(f"[OK] {name}")
        passed += 1

    pct = (passed * 100.0 / total) if total > 0 else 0.0
    print()
    print(f"COMPLETION: {passed}/{total} ({pct:.2f}%)")
    return 0 if passed == total else 1


if __name__ == "__main__":
    raise SystemExit(main())
