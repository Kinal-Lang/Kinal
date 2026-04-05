#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_COMPILER = ROOT / "out" / "selfhost" / "stage0-host" / "selfhost-stage0.exe"
DEFAULT_OUT_DIR = ROOT / "out" / "selfhost" / "tests"


def run(cmd: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=str(cwd), text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def expect_case(name: str, cmd: list[str], cwd: Path, expect_code: int, expect_substring: str | None = None) -> bool:
    proc = run(cmd, cwd)
    ok = proc.returncode == expect_code
    if expect_substring is not None:
        ok = ok and (expect_substring in proc.stdout)
    status = "PASS" if ok else "FAIL"
    print(f"[{status}] {name}")
    if not ok:
        print("  CMD :", " ".join(cmd))
        print("  CODE:", proc.returncode, "(expect", expect_code, ")")
        if expect_substring:
            print("  NEED:", expect_substring)
        print("  OUT :")
        print(proc.stdout.rstrip())
    return ok


def fx(root: Path, rel: str) -> str:
    return str((root / rel).resolve())


def stage_bundle_copy(compiler: Path, out_dir: Path, name: str) -> Path:
    bundle_dir = compiler.parent
    dst = out_dir / name
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(bundle_dir, dst)
    return dst


def main() -> int:
    ap = argparse.ArgumentParser(description="Run selfhost stage regression tests")
    ap.add_argument("--compiler", default=str(DEFAULT_COMPILER))
    ap.add_argument("--root", default=str(ROOT))
    ap.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR))
    args = ap.parse_args()

    root = Path(args.root).resolve()
    compiler = Path(args.compiler).resolve()
    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    if not compiler.exists():
        print(f"[FATAL] compiler not found: {compiler}")
        return 2

    ok = True
    comp = str(compiler)

    ok &= expect_case(
        "parse/missing_semicolon",
        [comp, "--in", fx(root, "selfhost/examples/ParseMissingSemicolon.fx"), "--phase", "parse"],
        root,
        1,
        "Expected Semicolon",
    )
    ok &= expect_case(
        "parse/try_requires_catch",
        [comp, "--in", fx(root, "selfhost/examples/ParseTryNoCatch.fx"), "--phase", "parse"],
        root,
        1,
        "Expected Catch",
    )
    ok &= expect_case(
        "sema/simple_return",
        [comp, "--in", fx(root, "selfhost/examples/SemaSimpleReturn.fx"), "--phase", "sema"],
        root,
        0,
    )
    ok &= expect_case(
        "sema/if_else_return",
        [comp, "--in", fx(root, "selfhost/examples/SemaValidReturn.fx"), "--phase", "sema"],
        root,
        0,
    )
    ok &= expect_case(
        "sema/missing_return",
        [comp, "--in", fx(root, "selfhost/examples/SemaMissingReturn.fx"), "--phase", "sema"],
        root,
        1,
        "Missing Return",
    )
    ok &= expect_case(
        "sema/main_signature",
        [comp, "--in", fx(root, "selfhost/examples/SemaBadMainSig.fx"), "--phase", "sema"],
        root,
        1,
        "Entry Signature",
    )
    ok &= expect_case(
        "sema/scoped_symbol_required",
        [comp, "--in", fx(root, "selfhost/examples/SemaScopedSymbol.fx"), "--phase", "sema"],
        root,
        1,
        "Scoped Symbol Required",
    )

    out_exe = out_dir / "selfhost-lexdemo-test.exe"
    ok &= expect_case(
        "full/compile_lexdemo",
        [comp, "--in", fx(root, "selfhost/examples/LexDemo.fx"), "--phase", "full", "-o", str(out_exe)],
        root,
        0,
    )
    if out_exe.exists():
        proc = run([str(out_exe)], out_dir)
        full_ok = proc.returncode == 0 and ("kinal" in proc.stdout)
    else:
        proc = subprocess.CompletedProcess([str(out_exe)], 1, "", "")
        full_ok = False
    print(f"[{'PASS' if full_ok else 'FAIL'}] full/run_lexdemo")
    if not full_ok:
        print("  CODE:", proc.returncode)
        print("  OUT :")
        print(proc.stdout.rstrip())
    ok &= full_ok

    out_exe_self = out_dir / "selfhost-lexdemo-self-test.exe"
    ok &= expect_case(
        "full_self/compile_lexdemo",
        [comp, "--in", fx(root, "selfhost/examples/LexDemo.fx"), "--phase", "full", "--backend", "self", "-o", str(out_exe_self)],
        root,
        0,
    )
    if out_exe_self.exists():
        proc = run([str(out_exe_self)], out_dir)
        full_ok = proc.returncode == 0 and ("kinal" in proc.stdout)
    else:
        proc = subprocess.CompletedProcess([str(out_exe_self)], 1, "", "")
        full_ok = False
    print(f"[{'PASS' if full_ok else 'FAIL'}] full_self/run_lexdemo")
    if not full_ok:
        print("  CODE:", proc.returncode)
        print("  OUT :")
        print(proc.stdout.rstrip())
    ok &= full_ok

    stage1_bundle = stage_bundle_copy(compiler, out_dir, "stage1-host")
    out_boot = stage1_bundle / "selfhost-stage1.exe"
    ok &= expect_case(
        "native/selfhost_stage1",
        [comp, "--in", fx(root, "selfhost/src/Main.kn"), "--phase", "full", "--backend", "host", "-o", str(out_boot)],
        root,
        0,
    )
    if out_boot.exists():
        out_boot_exe = out_dir / "selfhost-bootstrap-lexdemo.exe"
        ok &= expect_case(
            "native/stage1_compile_lexdemo",
            [str(out_boot), "--in", fx(root, "selfhost/examples/LexDemo.fx"), "-o", str(out_boot_exe)],
            root,
            0,
        )
        if out_boot_exe.exists():
            proc = run([str(out_boot_exe)], out_dir)
            full_ok = proc.returncode == 0 and ("kinal" in proc.stdout)
        else:
            proc = subprocess.CompletedProcess([str(out_boot_exe)], 1, "", "")
            full_ok = False
        print(f"[{'PASS' if full_ok else 'FAIL'}] native/stage1_run_lexdemo")
        if not full_ok:
            print("  CODE:", proc.returncode)
            print("  OUT :")
            print(proc.stdout.rstrip())
        ok &= full_ok
    else:
        print("[FAIL] native/selfhost_stage1_exists")
        ok = False

    print()
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
