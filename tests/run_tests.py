#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = ROOT / "out" / "test"
DEFAULT_MANIFEST = ROOT / "tests" / "manifest.json"


def is_windows() -> bool:
    return platform.system().lower() == "windows"


def host_platform() -> str:
    system = platform.system().lower()
    if system == "darwin":
        return "macos"
    return system


def normalize_platform_name(name: object) -> str:
    value = str(name).strip().lower()
    if value in {"win", "windows", "win32"}:
        return "windows"
    if value in {"mac", "macos", "osx", "darwin"}:
        return "macos"
    if value in {"linux", "gnu/linux"}:
        return "linux"
    return value


def case_runs_on_host(case: dict[str, object]) -> bool:
    current = host_platform()

    platforms = case.get("platforms")
    if platforms is not None:
        allowed = {normalize_platform_name(name) for name in platforms}
        if current not in allowed:
            return False

    skip_platforms = case.get("skip_platforms")
    if skip_platforms is not None:
        blocked = {normalize_platform_name(name) for name in skip_platforms}
        if current in blocked:
            return False

    return True


def find_zig() -> str | None:
    return shutil.which("zig.exe") or shutil.which("zig")


def find_wsl_ubuntu() -> str | None:
    if not is_windows():
        return None
    wsl = shutil.which("wsl.exe")
    if not wsl:
        return None
    probe = subprocess.run(
        [wsl, "-l", "-q"],
        cwd=str(ROOT),
        text=True,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    if probe.returncode != 0:
        return None
    for line in (probe.stdout or "").splitlines():
        name = line.replace("\x00", "").strip()
        if not name:
            continue
        if name.lower() == "ubuntu":
            smoke = subprocess.run(
                [wsl, "-d", name, "bash", "-lc", "true"],
                cwd=str(ROOT),
                text=True,
                capture_output=True,
                encoding="utf-8",
                errors="replace",
            )
            if smoke.returncode == 0:
                return name
            return None
    return None


def to_wsl_path(path: Path) -> str:
    resolved = path.resolve()
    if not is_windows():
        return resolved.as_posix()
    drive = resolved.drive.rstrip(":")
    tail = resolved.as_posix()[len(resolved.drive) :]
    if tail.startswith("/"):
        tail = tail[1:]
    return f"/mnt/{drive.lower()}/{tail}"


def exe_path(base: Path) -> Path:
    return base.with_suffix(".exe") if is_windows() else base


def obj_path(base: Path) -> Path:
    return base.with_suffix(".obj") if is_windows() else base.with_suffix(".o")


def artifact_path(base: Path, output_ext: str = "") -> Path:
    if output_ext:
        suffix = output_ext if output_ext.startswith(".") else f".{output_ext}"
        return base.with_suffix(suffix)
    return exe_path(base)


def dylib_suffix() -> str:
    if is_windows():
        return ".dll"
    if platform.system().lower() == "darwin":
        return ".dylib"
    return ".so"


def case_file_paths(case: dict[str, object]) -> list[Path]:
    files = case.get("files")
    if files:
        return [ROOT / Path(str(f)) for f in files]
    file = case.get("file")
    return [ROOT / Path(str(file))] if file else []


def rewrite_legacy_test_path(value: str, out_dir: Path) -> str:
    norm = value.replace("\\", "/")
    for prefix in ("build/tests/", "out/test/"):
        if norm.startswith(prefix):
            return str(out_dir / Path(norm).name)
    return value


def run(cmd: list[str], cwd: Path = ROOT, capture: bool = False) -> subprocess.CompletedProcess | str:
    if capture:
        return subprocess.run(cmd, cwd=str(cwd), text=True, capture_output=True, encoding="utf-8", errors="replace")
    subprocess.check_call(cmd, cwd=str(cwd))
    return ""


def matches_expected_error(output: str, expected: str) -> bool:
    if expected in output:
        return True

    if expected.startswith("[Parser] "):
        suffix = expected[len("[Parser] ") :]
        return "[Parser]" in output and suffix in output
    if expected.startswith("[Sema] "):
        suffix = expected[len("[Sema] ") :]
        return "[Sema]" in output and suffix in output
    if expected.startswith("[Codegen] "):
        suffix = expected[len("[Codegen] ") :]
        return "[Codegen]" in output and suffix in output
    return False


ANSI_ESCAPE_RE = re.compile(r"\x1b\[[0-9;]*m")


def strip_ansi(text: str) -> str:
    return ANSI_ESCAPE_RE.sub("", text)


def normalize_unhandled_runtime_output(output: str) -> str:
    text = strip_ansi(output).replace("\r\n", "\n")
    if "Unhandled Error:" not in text or "Stack Trace" not in text:
        return text

    lines = [line.rstrip() for line in text.split("\n")]
    message = ""
    frames: list[str] = []

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("Unhandled Error:"):
            message = stripped[len("Unhandled Error:") :].strip()
            continue
        if not stripped.startswith("at "):
            continue
        frame = stripped[3:]
        if "  " in frame:
            frame = frame.split("  ", 1)[0].rstrip()
        if frame.endswith("()"):
            frame = frame[:-2]
        if frame:
            frames.append(frame)

    if not message or not frames:
        return text
    return f"{message}\n{' -> '.join(frames)}\n"


def matches_expected_runtime_output(output: str, expected: str) -> bool:
    normalized_output = output.replace("\r\n", "\n")
    normalized_expected = expected.replace("\r\n", "\n")
    if normalized_output == normalized_expected:
        return True
    return normalize_unhandled_runtime_output(normalized_output) == normalized_expected


def run_driver_integration_tests(compiler: Path, out_dir: Path) -> int:
    hello_fx = ROOT / "tests" / "hello.kn"
    build_native_ffi_assets(out_dir)

    pipe_ll = out_dir / "pipeline_hello.ll"
    pipe_s = out_dir / "pipeline_hello.s"
    pipe_obj = obj_path(out_dir / "pipeline_hello")
    pipe_exe = exe_path(out_dir / "pipeline_hello")
    run([str(compiler), "build", "--no-module-discovery", "--emit", "ir", str(hello_fx), "-o", str(pipe_ll)], cwd=ROOT)
    run([str(compiler), "build", "--no-module-discovery", "--emit", "asm", str(pipe_ll), "-o", str(pipe_s)], cwd=ROOT)
    run([str(compiler), "build", "--no-module-discovery", "--emit", "obj", str(pipe_s), "-o", str(pipe_obj)], cwd=ROOT)
    run([str(compiler), "build", "--no-module-discovery", str(pipe_obj), "-o", str(pipe_exe)], cwd=ROOT)
    p_out = run([str(pipe_exe)], cwd=ROOT, capture=True)
    assert isinstance(p_out, subprocess.CompletedProcess)
    if (p_out.stdout or "").replace("\r\n", "\n") != "hello\n":
        print("[FAIL] pipeline_resume")
        return 1
    print("[OK] pipeline_resume")

    lto_exe = exe_path(out_dir / "lto_hello")
    run([str(compiler), "build", "--no-module-discovery", "--lto=thin", str(hello_fx), "-o", str(lto_exe)], cwd=ROOT)
    lto_out = run([str(lto_exe)], cwd=ROOT, capture=True)
    assert isinstance(lto_out, subprocess.CompletedProcess)
    if (lto_out.stdout or "").replace("\r\n", "\n") != "hello\n":
        print("[FAIL] lto_thin")
        return 1
    print("[OK] lto_thin")

    lto_full_exe = exe_path(out_dir / "lto_hello_full")
    run([str(compiler), "build", "--no-module-discovery", "--lto=full", str(hello_fx), "-o", str(lto_full_exe)], cwd=ROOT)
    lto_full_out = run([str(lto_full_exe)], cwd=ROOT, capture=True)
    assert isinstance(lto_full_out, subprocess.CompletedProcess)
    if (lto_full_out.stdout or "").replace("\r\n", "\n") != "hello\n":
        print("[FAIL] lto_full")
        return 1
    print("[OK] lto_full")

    bundled_vm = compiler.parent / exe_path(Path("kinalvm"))
    if not bundled_vm.exists():
        print("[FAIL] bundled_kinalvm")
        return 1
    print("[OK] bundled_kinalvm")
    vm_exe = bundled_vm

    hello_knc = out_dir / "hello.knc"
    run([str(compiler), "vm", "build", "--no-module-discovery", str(hello_fx), "-o", str(hello_knc)], cwd=ROOT)
    if hello_knc.read_bytes()[:4] != b"KNC2":
        print("[FAIL] knc_binary_header")
        return 1
    print("[OK] knc_binary_header")
    knc_hello_out = run([str(vm_exe), str(hello_knc)], cwd=ROOT, capture=True)
    assert isinstance(knc_hello_out, subprocess.CompletedProcess)
    if knc_hello_out.returncode != 0 or (knc_hello_out.stdout or "").replace("\r\n", "\n") != "hello\n":
        print("[FAIL] knc_vm_hello")
        return 1
    print("[OK] knc_vm_hello")

    knc_cli_out = run([str(compiler), "vm", "run", str(hello_knc)], cwd=ROOT, capture=True)
    assert isinstance(knc_cli_out, subprocess.CompletedProcess)
    if knc_cli_out.returncode != 0 or (knc_cli_out.stdout or "").replace("\r\n", "\n") != "hello\n":
        print("[FAIL] knc_vm_cli_run")
        return 1
    print("[OK] knc_vm_cli_run")

    native_source_out = run([str(compiler), "run", "--no-module-discovery", str(hello_fx)], cwd=ROOT, capture=True)
    assert isinstance(native_source_out, subprocess.CompletedProcess)
    if native_source_out.returncode != 0 or (native_source_out.stdout or "").replace("\r\n", "\n") != "hello\n":
        print("[FAIL] native_source_run")
        return 1
    print("[OK] native_source_run")

    vm_source_out = run([str(compiler), "vm", "run", "--no-module-discovery", str(hello_fx)], cwd=ROOT, capture=True)
    assert isinstance(vm_source_out, subprocess.CompletedProcess)
    if vm_source_out.returncode != 0 or (vm_source_out.stdout or "").replace("\r\n", "\n") != "hello\n":
        print("[FAIL] knc_vm_source_run")
        return 1
    print("[OK] knc_vm_source_run")

    vm_exe_from_source = exe_path(out_dir / "hello_vm_exe")
    run([str(compiler), "vm", "pack", "--no-module-discovery", str(hello_fx), "-o", str(vm_exe_from_source)], cwd=ROOT)
    vm_exe_from_source_out = run([str(vm_exe_from_source)], cwd=ROOT, capture=True)
    assert isinstance(vm_exe_from_source_out, subprocess.CompletedProcess)
    if vm_exe_from_source_out.returncode != 0 or (vm_exe_from_source_out.stdout or "").replace("\r\n", "\n") != "hello\n":
        print("[FAIL] knc_vm_exe_from_source")
        return 1
    print("[OK] knc_vm_exe_from_source")

    vm_exe_from_knc = exe_path(out_dir / "hello_knc_vm_exe")
    run([str(compiler), "vm", "pack", str(hello_knc), "-o", str(vm_exe_from_knc)], cwd=ROOT)
    vm_exe_from_knc_out = run([str(vm_exe_from_knc)], cwd=ROOT, capture=True)
    assert isinstance(vm_exe_from_knc_out, subprocess.CompletedProcess)
    if vm_exe_from_knc_out.returncode != 0 or (vm_exe_from_knc_out.stdout or "").replace("\r\n", "\n") != "hello\n":
        print("[FAIL] knc_vm_exe_from_knc")
        return 1
    print("[OK] knc_vm_exe_from_knc")

    vm_exe_alias = exe_path(out_dir / "hello_knc_alias_vm_exe")
    run([str(compiler), "vm", "pack", str(hello_knc), "-o", str(vm_exe_alias)], cwd=ROOT)
    vm_exe_alias_out = run([str(vm_exe_alias)], cwd=ROOT, capture=True)
    assert isinstance(vm_exe_alias_out, subprocess.CompletedProcess)
    if vm_exe_alias_out.returncode != 0 or (vm_exe_alias_out.stdout or "").replace("\r\n", "\n") != "hello\n":
        print("[FAIL] knc_vm_exe_alias")
        return 1
    print("[OK] knc_vm_exe_alias")

    def run_knc_case(label: str, source_name: str, expected: str, expected_exit_code: int = 0) -> int:
        src = ROOT / "tests" / source_name
        out = out_dir / f"{Path(source_name).stem}.knc"
        run([str(compiler), "vm", "build", "--no-module-discovery", str(src), "-o", str(out)], cwd=ROOT)
        proc = run([str(vm_exe), str(out)], cwd=ROOT, capture=True)
        assert isinstance(proc, subprocess.CompletedProcess)
        actual = (proc.stdout or "").replace("\r\n", "\n")
        if proc.returncode != expected_exit_code or actual != expected:
            print(f"[FAIL] {label}")
            return 1
        print(f"[OK] {label}")
        return 0

    knc_arith_fx = ROOT / "tests" / "knc_arith.kn"
    knc_arith = out_dir / "knc_arith.knc"
    run([str(compiler), "vm", "build", "--no-module-discovery", str(knc_arith_fx), "-o", str(knc_arith)], cwd=ROOT)
    knc_arith_out = run([str(vm_exe), str(knc_arith)], cwd=ROOT, capture=True)
    assert isinstance(knc_arith_out, subprocess.CompletedProcess)
    if knc_arith_out.returncode != 0 or (knc_arith_out.stdout or "").replace("\r\n", "\n") != "42\n":
        print("[FAIL] knc_vm_arith")
        return 1
    print("[OK] knc_vm_arith")

    knc_std_fx = ROOT / "tests" / "knc_std_semantics.kn"
    knc_std = out_dir / "knc_std_semantics.knc"
    run([str(compiler), "vm", "build", "--no-module-discovery", str(knc_std_fx), "-o", str(knc_std)], cwd=ROOT)
    knc_std_out = run([str(vm_exe), str(knc_std)], cwd=ROOT, capture=True)
    assert isinstance(knc_std_out, subprocess.CompletedProcess)
    expected_std = "3\ntrue\nabc\nfalse\ntrue\n42\nfalse\n"
    if knc_std_out.returncode != 0 or (knc_std_out.stdout or "").replace("\r\n", "\n") != expected_std:
        print("[FAIL] knc_vm_std_semantics")
        return 1
    print("[OK] knc_vm_std_semantics")

    knc_control_fx = ROOT / "tests" / "control.kn"
    knc_control = out_dir / "control.knc"
    run([str(compiler), "vm", "build", "--no-module-discovery", str(knc_control_fx), "-o", str(knc_control)], cwd=ROOT)
    knc_control_out = run([str(vm_exe), str(knc_control)], cwd=ROOT, capture=True)
    assert isinstance(knc_control_out, subprocess.CompletedProcess)
    if knc_control_out.returncode != 0 or (knc_control_out.stdout or "").replace("\r\n", "\n") != "ok\n":
        print("[FAIL] knc_vm_control")
        return 1
    print("[OK] knc_vm_control")

    knc_functions_fx = ROOT / "tests" / "functions.kn"
    knc_functions = out_dir / "functions.knc"
    run([str(compiler), "vm", "build", "--no-module-discovery", str(knc_functions_fx), "-o", str(knc_functions)], cwd=ROOT)
    knc_functions_out = run([str(vm_exe), str(knc_functions)], cwd=ROOT, capture=True)
    assert isinstance(knc_functions_out, subprocess.CompletedProcess)
    if knc_functions_out.returncode != 0 or (knc_functions_out.stdout or "").replace("\r\n", "\n") != "ok\n":
        print("[FAIL] knc_vm_functions")
        return 1
    print("[OK] knc_vm_functions")

    knc_expr_fx = ROOT / "tests" / "expr_ops.kn"
    knc_expr = out_dir / "expr_ops.knc"
    run([str(compiler), "vm", "build", "--no-module-discovery", str(knc_expr_fx), "-o", str(knc_expr)], cwd=ROOT)
    knc_expr_out = run([str(vm_exe), str(knc_expr)], cwd=ROOT, capture=True)
    assert isinstance(knc_expr_out, subprocess.CompletedProcess)
    expected_expr = "1\n3\n9\n9\n3\n"
    if knc_expr_out.returncode != 0 or (knc_expr_out.stdout or "").replace("\r\n", "\n") != expected_expr:
        print("[FAIL] knc_vm_expr_ops")
        return 1
    print("[OK] knc_vm_expr_ops")

    knc_char_fx = ROOT / "tests" / "char_literals.kn"
    knc_char = out_dir / "char_literals.knc"
    run([str(compiler), "vm", "build", "--no-module-discovery", str(knc_char_fx), "-o", str(knc_char)], cwd=ROOT)
    knc_char_out = run([str(vm_exe), str(knc_char)], cwd=ROOT, capture=True)
    assert isinstance(knc_char_out, subprocess.CompletedProcess)
    expected_char = "A\n65\n10\n39\n"
    if knc_char_out.returncode != 0 or (knc_char_out.stdout or "").replace("\r\n", "\n") != expected_char:
        print("[FAIL] knc_vm_char_literals")
        return 1
    print("[OK] knc_vm_char_literals")

    knc_char_str_fx = ROOT / "tests" / "char_to_string.kn"
    knc_char_str = out_dir / "char_to_string.knc"
    run([str(compiler), "vm", "build", "--no-module-discovery", str(knc_char_str_fx), "-o", str(knc_char_str)], cwd=ROOT)
    knc_char_str_out = run([str(vm_exe), str(knc_char_str)], cwd=ROOT, capture=True)
    assert isinstance(knc_char_str_out, subprocess.CompletedProcess)
    expected_char_str = "a\nxa\nay\na\n"
    if knc_char_str_out.returncode != 0 or (knc_char_str_out.stdout or "").replace("\r\n", "\n") != expected_char_str:
        print("[FAIL] knc_vm_char_to_string")
        return 1
    print("[OK] knc_vm_char_to_string")

    knc_bitwise_fx = ROOT / "tests" / "bitwise.kn"
    knc_bitwise = out_dir / "bitwise.knc"
    run([str(compiler), "vm", "build", "--no-module-discovery", str(knc_bitwise_fx), "-o", str(knc_bitwise)], cwd=ROOT)
    knc_bitwise_out = run([str(vm_exe), str(knc_bitwise)], cwd=ROOT, capture=True)
    assert isinstance(knc_bitwise_out, subprocess.CompletedProcess)
    expected_bitwise = "1\n7\n6\n10\n2\n-6\n"
    if knc_bitwise_out.returncode != 0 or (knc_bitwise_out.stdout or "").replace("\r\n", "\n") != expected_bitwise:
        print("[FAIL] knc_vm_bitwise")
        return 1
    print("[OK] knc_vm_bitwise")

    knc_any_cast_fx = ROOT / "tests" / "any_cast.kn"
    knc_any_cast = out_dir / "any_cast.knc"
    run([str(compiler), "vm", "build", "--no-module-discovery", str(knc_any_cast_fx), "-o", str(knc_any_cast)], cwd=ROOT)
    knc_any_cast_out = run([str(vm_exe), str(knc_any_cast)], cwd=ROOT, capture=True)
    assert isinstance(knc_any_cast_out, subprocess.CompletedProcess)
    expected_any_cast = "123\n456\ntrue\n42\ntrue\nA\n"
    if knc_any_cast_out.returncode != 0 or (knc_any_cast_out.stdout or "").replace("\r\n", "\n") != expected_any_cast:
        print("[FAIL] knc_vm_any_cast")
        return 1
    print("[OK] knc_vm_any_cast")

    if run_knc_case("knc_vm_cast_more", "knc_cast_more.kn", "42\n2\n1\n0\nfalse\ntrue\nB\n") != 0:
        return 1

    knc_time_file_fx = ROOT / "tests" / "knc_time_file.kn"
    knc_time_file = out_dir / "knc_time_file.knc"
    run([str(compiler), "vm", "build", "--no-module-discovery", str(knc_time_file_fx), "-o", str(knc_time_file)], cwd=ROOT)
    knc_time_file_out = run([str(vm_exe), str(knc_time_file)], cwd=ROOT, capture=True)
    assert isinstance(knc_time_file_out, subprocess.CompletedProcess)
    expected_time_file = "ok\ntrue\ntrue\nalpha\nfallback\ntrue\nalphabeta\ntrue\nfalse\n"
    if knc_time_file_out.returncode != 0 or (knc_time_file_out.stdout or "").replace("\r\n", "\n") != expected_time_file:
        print("[FAIL] knc_vm_time_file")
        return 1
    print("[OK] knc_vm_time_file")

    if run_knc_case("knc_vm_typeof", "typeof.kn", "int\nint[]\nint*\n") != 0:
        return 1
    if run_knc_case("knc_vm_pointers", "pointers.kn", "ok\n") != 0:
        return 1
    if run_knc_case("knc_vm_pointer_depth", "pointer_depth.kn", "ok\n") != 0:
        return 1
    if run_knc_case("knc_vm_function_pointer", "function_pointer.kn", "false\nfalse\n") != 0:
        return 1
    if run_knc_case("knc_vm_string_to_char_ptr", "string_to_char_ptr.kn", "false\nfalse\n") != 0:
        return 1
    if run_knc_case("knc_vm_global_null_sugar", "global_null_sugar.kn", "41\n0\nnull\nblk\nIO.Type.Object.Class\n") != 0:
        return 1
    if run_knc_case("knc_vm_const_if", "const_if.kn", "win\n") != 0:
        return 1
    if run_knc_case("knc_vm_console_varargs", "console_varargs.kn", "\nA 1 true\nB 2\n") != 0:
        return 1
    if run_knc_case("knc_vm_superloop", "knc_superloop.kn", "5\n0\n") != 0:
        return 1
    if run_knc_case("knc_vm_superloop_branches", "knc_superloop_branches.kn", "1\n5\n-1\n0\n") != 0:
        return 1
    knc_superloop_src = ROOT / "tests" / "knc_superloop.kn"
    knc_superloop_no_fuse = out_dir / "knc_superloop_nofuse.knc"
    run(
        [str(compiler), "vm", "build", "--no-module-discovery", "--no-superloop", str(knc_superloop_src), "-o", str(knc_superloop_no_fuse)],
        cwd=ROOT,
    )
    knc_superloop_no_fuse_out = run([str(vm_exe), str(knc_superloop_no_fuse)], cwd=ROOT, capture=True)
    assert isinstance(knc_superloop_no_fuse_out, subprocess.CompletedProcess)
    if knc_superloop_no_fuse_out.returncode != 0 or (knc_superloop_no_fuse_out.stdout or "").replace("\r\n", "\n") != "5\n0\n":
        print("[FAIL] knc_vm_superloop_nofuse")
        return 1
    print("[OK] knc_vm_superloop_nofuse")
    knc_loop_backedge_src = ROOT / "tests" / "knc_loop_backedge.kn"
    knc_loop_backedge = out_dir / "knc_loop_backedge.knc"
    run(
        [str(compiler), "vm", "build", "--no-module-discovery", "--no-superloop", str(knc_loop_backedge_src), "-o", str(knc_loop_backedge)],
        cwd=ROOT,
    )
    knc_loop_backedge_out = run([str(vm_exe), str(knc_loop_backedge)], cwd=ROOT, capture=True)
    assert isinstance(knc_loop_backedge_out, subprocess.CompletedProcess)
    if knc_loop_backedge_out.returncode != 0 or (knc_loop_backedge_out.stdout or "").replace("\r\n", "\n") != "40\n15\n4\n1\n":
        print("[FAIL] knc_vm_loop_backedge")
        return 1
    print("[OK] knc_vm_loop_backedge")
    knc_superloop_branches_src = ROOT / "tests" / "knc_superloop_branches.kn"
    knc_superloop_branches_no_fuse = out_dir / "knc_superloop_branches_nofuse.knc"
    run(
        [str(compiler), "vm", "build", "--no-module-discovery", "--no-superloop", str(knc_superloop_branches_src), "-o", str(knc_superloop_branches_no_fuse)],
        cwd=ROOT,
    )
    knc_superloop_branches_no_fuse_out = run([str(vm_exe), str(knc_superloop_branches_no_fuse)], cwd=ROOT, capture=True)
    assert isinstance(knc_superloop_branches_no_fuse_out, subprocess.CompletedProcess)
    if knc_superloop_branches_no_fuse_out.returncode != 0 or (knc_superloop_branches_no_fuse_out.stdout or "").replace("\r\n", "\n") != "1\n5\n-1\n0\n":
        print("[FAIL] knc_vm_superloop_branches_nofuse")
        return 1
    print("[OK] knc_vm_superloop_branches_nofuse")
    if run_knc_case("knc_vm_gc_new_arg_root", "gc_new_arg_root.kn", "ok\n") != 0:
        return 1
    if run_knc_case("knc_vm_gc_call_arg_root", "gc_call_arg_root.kn", "AB\n") != 0:
        return 1
    if run_knc_case("knc_vm_runtime_ffi", "ffi.kn", "true\ntrue\n") != 0:
        return 1

    if is_windows():
        zig = find_zig()
        ubuntu = find_wsl_ubuntu()
        if zig and ubuntu:
            linux_exe = out_dir / "cross_linux_hello"
            run(
                [
                    str(compiler),
                    "build",
                    "--no-module-discovery",
                    "--target",
                    "linux64",
                    "--linker",
                    "zig",
                    "--linker-path",
                    zig,
                    str(hello_fx),
                    "-o",
                    str(linux_exe),
                ],
                cwd=ROOT,
            )
            linux_exe_wsl = to_wsl_path(linux_exe)
            linux_run = run(
                [
                    "wsl.exe",
                    "-d",
                    ubuntu,
                    "bash",
                    "-lc",
                    f"chmod +x '{linux_exe_wsl}' && '{linux_exe_wsl}'",
                ],
                cwd=ROOT,
                capture=True,
            )
            assert isinstance(linux_run, subprocess.CompletedProcess)
            if linux_run.returncode != 0 or (linux_run.stdout or "").replace("\r\n", "\n") != "hello\n":
                print("[FAIL] cross_linux_hello")
                return 1
            print("[OK] cross_linux_hello")
        else:
            print("[SKIP] cross_linux_hello")

        if zig:
            linux_web_exe = out_dir / "cross_linux_web_compile"
            run(
                [
                    str(compiler),
                    "build",
                    "--target",
                    "linux64",
                    "--linker",
                    "zig",
                    "--linker-path",
                    zig,
                    str(ROOT / "tests" / "stdlib_web_compile.kn"),
                    "-o",
                    str(linux_web_exe),
                ],
                cwd=ROOT,
            )
            print("[OK] cross_linux_web_compile")
        else:
            print("[SKIP] cross_linux_web_compile")

    locale_en = out_dir / "locale-en-template.json"
    run([str(compiler), "build", "--dump-locale-en", str(locale_en)], cwd=ROOT)
    locale_text = locale_en.read_text(encoding="utf-8")
    if "\"text\"" not in locale_text or "\"ui.help.full\"" not in locale_text:
        print("[FAIL] locale_export")
        return 1
    print("[OK] locale_export")

    warn_fx = ROOT / "tests" / "warn_entry_unsafe.kn"
    warn_exe = exe_path(out_dir / "warn_entry_unsafe")
    warn_proc = run([str(compiler), "build", "--no-module-discovery", str(warn_fx), "-o", str(warn_exe)], cwd=ROOT, capture=True)
    assert isinstance(warn_proc, subprocess.CompletedProcess)
    warn_text = ((warn_proc.stdout or "") + (warn_proc.stderr or "")).replace("\r\n", "\n")
    if warn_proc.returncode != 0 or "Entry Unsafe" not in warn_text or "[warning]" not in warn_text:
        print("[FAIL] warning_entry_unsafe")
        return 1
    warn_run = run([str(warn_exe)], cwd=ROOT, capture=True)
    assert isinstance(warn_run, subprocess.CompletedProcess)
    if (warn_run.stdout or "").replace("\r\n", "\n") != "ok\n":
        print("[FAIL] warning_entry_unsafe_runtime")
        return 1
    print("[OK] warning_entry_unsafe")

    sema_multi_proc = run(
        [
            str(compiler),
            "build",
            "--no-module-discovery",
            str(ROOT / "tests" / "error_sema_multi.kn"),
            "-o",
            str(exe_path(out_dir / "error_sema_multi_dummy")),
        ],
        cwd=ROOT,
        capture=True,
    )
    assert isinstance(sema_multi_proc, subprocess.CompletedProcess)
    sema_multi_text = ((sema_multi_proc.stdout or "") + (sema_multi_proc.stderr or "")).replace("\r\n", "\n")
    if (
        sema_multi_proc.returncode == 0
        or sema_multi_text.count("[Sema][") < 2
        or "[Diag] 2 error(s), 0 warning(s)" not in sema_multi_text
    ):
        print("[FAIL] sema_multi_errors")
        return 1
    print("[OK] sema_multi_errors")

    zh_proc = run(
        [
            str(compiler),
            "build",
            "--no-module-discovery",
            "--lang",
            "zh",
            str(ROOT / "tests" / "error_parser.kn"),
            "-o",
            str(exe_path(out_dir / "zh_error_dummy")),
        ],
        cwd=ROOT,
        capture=True,
    )
    assert isinstance(zh_proc, subprocess.CompletedProcess)
    zh_text = ((zh_proc.stdout or "") + (zh_proc.stderr or "")).replace("\r\n", "\n")
    if zh_proc.returncode == 0 or "[error]" in zh_text or "[Parser][" in zh_text:
        print("[FAIL] locale_zh")
        return 1
    print("[OK] locale_zh")

    zh_help_proc = run([str(compiler), "--lang", "zh", "-h"], cwd=ROOT, capture=True)
    assert isinstance(zh_help_proc, subprocess.CompletedProcess)
    zh_help_text = ((zh_help_proc.stdout or "") + (zh_help_proc.stderr or "")).replace("\r\n", "\n")
    if zh_help_proc.returncode != 0 or "Kinal 编译器" not in zh_help_text or "--link-root <dir>" not in zh_help_text:
        print("[FAIL] help_zh")
        return 1
    print("[OK] help_zh")

    help_proc = run([str(compiler), "-h"], cwd=ROOT, capture=True)
    assert isinstance(help_proc, subprocess.CompletedProcess)
    help_text = ((help_proc.stdout or "") + (help_proc.stderr or "")).replace("\r\n", "\n")
    if (
        help_proc.returncode != 0
        or "Build quick reference:" not in help_text
        or "Run quick reference:" not in help_text
        or "VM / package quick reference:" not in help_text
        or "Project:" not in help_text
        or "Link:" not in help_text
        or "--emit <bin|obj|asm|ir>" not in help_text
        or "kinal vm run <file.kn|file.knc> [-- program-args...]" not in help_text
        or "kinal pkg build --manifest <file|dir> -o <file.klib>" not in help_text
        or "--project <file|dir>" not in help_text
        or "--crt <auto|dynamic|static>" not in help_text
        or "--link-file <file>" not in help_text
        or '.knc             Use "kinal vm run" or "kinal vm pack"' not in help_text
        or "--no-module-discovery" not in help_text
        or "--vm" in help_text
        or "--emit-klib" in help_text
        or "--emit-package" in help_text
    ):
        print("[FAIL] help_en")
        return 1
    print("[OK] help_en")

    fmt_src = out_dir / "fmt_sample.kn"
    fmt_src.write_text("Unit   App;Get IO.Console;Static Function int Main(){Return 0;}", encoding="utf-8")
    run([str(compiler), "fmt", str(fmt_src)], cwd=ROOT)
    fmt_expected = "Unit App;\nGet IO.Console;\nStatic Function int Main()\n{\n    Return 0;\n}\n"
    if fmt_src.read_text(encoding="utf-8").replace("\r\n", "\n") != fmt_expected:
        print("[FAIL] fmt_file")
        return 1
    print("[OK] fmt_file")

    fmt_check_ok = run([str(compiler), "fmt", "--check", str(fmt_src)], cwd=ROOT, capture=True)
    assert isinstance(fmt_check_ok, subprocess.CompletedProcess)
    if fmt_check_ok.returncode != 0:
        print("[FAIL] fmt_check_clean")
        return 1
    print("[OK] fmt_check_clean")

    fmt_dirty = out_dir / "fmt_dirty.kn"
    fmt_dirty.write_text("Unit   App;Static Function int Main(){Return 0;}", encoding="utf-8")
    fmt_check_bad = subprocess.run(
        [str(compiler), "fmt", "--check", str(fmt_dirty)],
        cwd=str(ROOT),
        text=True,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    if fmt_check_bad.returncode == 0:
        print("[FAIL] fmt_check_dirty")
        return 1
    print("[OK] fmt_check_dirty")

    fmt_stdin = subprocess.run(
        [str(compiler), "fmt", "--stdin", "--stdout", "--stdin-filepath", "stdin.kn"],
        cwd=str(ROOT),
        input="Unit   App;Static Function int Main(){Return 0;}",
        text=True,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    if fmt_stdin.returncode != 0 or (fmt_stdin.stdout or "").replace("\r\n", "\n") != "Unit App;\nStatic Function int Main()\n{\n    Return 0;\n}\n":
        print("[FAIL] fmt_stdin_stdout")
        return 1
    print("[OK] fmt_stdin_stdout")

    if is_windows():
        same_unit_rel_exe = exe_path(out_dir / "driver_same_unit_autolink")
        same_unit_rel_proc = run(
            [
                str(compiler),
                "build",
                "same_unit_autolink_main.kn",
                "-o",
                str(same_unit_rel_exe),
            ],
            cwd=ROOT / "tests",
            capture=True,
        )
        assert isinstance(same_unit_rel_proc, subprocess.CompletedProcess)
        same_unit_rel_text = ((same_unit_rel_proc.stdout or "") + (same_unit_rel_proc.stderr or "")).replace("\r\n", "\n")
        if same_unit_rel_proc.returncode != 0:
            print("[FAIL] same_unit_autolink_relative")
            print(same_unit_rel_text)
            return 1
        same_unit_rel_run = run([str(same_unit_rel_exe)], cwd=ROOT, capture=True)
        assert isinstance(same_unit_rel_run, subprocess.CompletedProcess)
        if (same_unit_rel_run.stdout or "").replace("\r\n", "\n") != "ok\n":
            print("[FAIL] same_unit_autolink_relative_runtime")
            return 1
        print("[OK] same_unit_autolink_relative")

        ffi_lib_exe = exe_path(out_dir / "driver_ffi_lib")
        ffi_lib_proc = run(
            [
                str(compiler),
                "build",
                "--no-module-discovery",
                "--show-link",
                "--show-link-search",
                "-L",
                str(out_dir),
                "-l",
                "native_ffi",
                str(ROOT / "tests" / "ffi_lib.kn"),
                "-o",
                str(ffi_lib_exe),
            ],
            cwd=ROOT,
            capture=True,
        )
        assert isinstance(ffi_lib_proc, subprocess.CompletedProcess)
        ffi_lib_text = ((ffi_lib_proc.stdout or "") + (ffi_lib_proc.stderr or "")).replace("\r\n", "\n")
        if ffi_lib_proc.returncode != 0 or "[LinkSearch]" not in ffi_lib_text or "native_ffi.lib" not in ffi_lib_text:
            print("[FAIL] link_lib_dir")
            return 1
        ffi_lib_run = run([str(ffi_lib_exe)], cwd=ROOT, capture=True)
        assert isinstance(ffi_lib_run, subprocess.CompletedProcess)
        if (ffi_lib_run.stdout or "").replace("\r\n", "\n") != "42\nffi-native\n":
            print("[FAIL] link_lib_dir_runtime")
            return 1
        print("[OK] link_lib_dir")

        ffi_root_exe = exe_path(out_dir / "driver_ffi_root")
        ffi_root_proc = run(
            [
                str(compiler),
                "build",
                "--no-module-discovery",
                "--show-link",
                "--show-link-search",
                "--link-root",
                str(out_dir),
                "-l",
                "native_ffi",
                "--link-arg",
                "/debug",
                str(ROOT / "tests" / "ffi_lib.kn"),
                "-o",
                str(ffi_root_exe),
            ],
            cwd=ROOT,
            capture=True,
        )
        assert isinstance(ffi_root_proc, subprocess.CompletedProcess)
        ffi_root_text = ((ffi_root_proc.stdout or "") + (ffi_root_proc.stderr or "")).replace("\r\n", "\n")
        if ffi_root_proc.returncode != 0 or "/debug" not in ffi_root_text or str(out_dir) not in ffi_root_text:
            print("[FAIL] link_root")
            return 1
        ffi_root_run = run([str(ffi_root_exe)], cwd=ROOT, capture=True)
        assert isinstance(ffi_root_run, subprocess.CompletedProcess)
        if (ffi_root_run.stdout or "").replace("\r\n", "\n") != "42\nffi-native\n":
            print("[FAIL] link_root_runtime")
            return 1
        print("[OK] link_root")

        ffi_attr_lib_exe = exe_path(out_dir / "driver_ffi_attr_lib")
        ffi_attr_lib_proc = run(
            [
                str(compiler),
                "build",
                "--no-module-discovery",
                "-L",
                str(out_dir),
                str(ROOT / "tests" / "ffi_attr_lib.kn"),
                "-o",
                str(ffi_attr_lib_exe),
            ],
            cwd=ROOT,
            capture=True,
        )
        assert isinstance(ffi_attr_lib_proc, subprocess.CompletedProcess)
        if ffi_attr_lib_proc.returncode != 0:
            print("[FAIL] ffi_attr_lib")
            return 1
        ffi_attr_lib_run = run([str(ffi_attr_lib_exe)], cwd=ROOT, capture=True)
        assert isinstance(ffi_attr_lib_run, subprocess.CompletedProcess)
        if (ffi_attr_lib_run.stdout or "").replace("\r\n", "\n") != "42\nffi-native\n":
            print("[FAIL] ffi_attr_lib_runtime")
            return 1
        print("[OK] ffi_attr_lib")

        ffi_attr_file_exe = exe_path(out_dir / "driver_ffi_attr_file")
        ffi_attr_file_proc = run(
            [
                str(compiler),
                "build",
                "--no-module-discovery",
                str(ROOT / "tests" / "ffi_attr_file.kn"),
                "-o",
                str(ffi_attr_file_exe),
            ],
            cwd=ROOT,
            capture=True,
        )
        assert isinstance(ffi_attr_file_proc, subprocess.CompletedProcess)
        if ffi_attr_file_proc.returncode != 0:
            print("[FAIL] ffi_attr_file")
            return 1
        ffi_attr_file_run = run([str(ffi_attr_file_exe)], cwd=ROOT, capture=True)
        assert isinstance(ffi_attr_file_run, subprocess.CompletedProcess)
        if (ffi_attr_file_run.stdout or "").replace("\r\n", "\n") != "42\nffi-native\n":
            print("[FAIL] ffi_attr_file_runtime")
            return 1
        print("[OK] ffi_attr_file")

        build_hosted_probe_assets(out_dir)

        hosted_dynamic_exe = exe_path(out_dir / "driver_hosted_probe_dynamic")
        hosted_dynamic_proc = run(
            [
                str(compiler),
                "build",
                "--no-module-discovery",
                str(ROOT / "tests" / "ffi_hosted_auto.kn"),
                "--link-file",
                str(out_dir / "native_hosted_probe_md.obj"),
                "-o",
                str(hosted_dynamic_exe),
            ],
            cwd=ROOT,
            capture=True,
        )
        assert isinstance(hosted_dynamic_proc, subprocess.CompletedProcess)
        if hosted_dynamic_proc.returncode != 0:
            print("[FAIL] hosted_probe_dynamic_link")
            return 1
        hosted_dynamic_run = run([str(hosted_dynamic_exe)], cwd=ROOT, capture=True)
        assert isinstance(hosted_dynamic_run, subprocess.CompletedProcess)
        if (hosted_dynamic_run.stdout or "").replace("\r\n", "\n") != "77\n":
            print("[FAIL] hosted_probe_dynamic_runtime")
            return 1
        print("[OK] hosted_probe_dynamic")

        hosted_static_exe = exe_path(out_dir / "driver_hosted_probe_static")
        hosted_static_proc = run(
            [
                str(compiler),
                "build",
                "--no-module-discovery",
                "--crt",
                "static",
                str(ROOT / "tests" / "ffi_hosted_auto.kn"),
                "--link-file",
                str(out_dir / "native_hosted_probe_mt.obj"),
                "-o",
                str(hosted_static_exe),
            ],
            cwd=ROOT,
            capture=True,
        )
        assert isinstance(hosted_static_proc, subprocess.CompletedProcess)
        if hosted_static_proc.returncode != 0:
            print("[FAIL] hosted_probe_static_link")
            return 1
        hosted_static_run = run([str(hosted_static_exe)], cwd=ROOT, capture=True)
        assert isinstance(hosted_static_run, subprocess.CompletedProcess)
        if (hosted_static_run.stdout or "").replace("\r\n", "\n") != "77\n":
            print("[FAIL] hosted_probe_static_runtime")
            return 1
        print("[OK] hosted_probe_static")

    shared_artifact = out_dir / (("driver_dynlib" if is_windows() else "libdriver_dynlib") + dylib_suffix())
    shared_proc = run(
        [
            str(compiler),
            "build",
            "--no-module-discovery",
            "--kind",
            "shared",
            str(ROOT / "tests" / "dynlib_shared.kn"),
            "-o",
            str(shared_artifact),
        ],
        cwd=ROOT,
        capture=True,
    )
    assert isinstance(shared_proc, subprocess.CompletedProcess)
    if shared_proc.returncode != 0 or not shared_artifact.exists():
        print("[FAIL] shared_build")
        return 1
    if is_windows() and not shared_artifact.with_suffix(".lib").exists():
        print("[FAIL] shared_import_lib")
        return 1

    loader_exe = exe_path(out_dir / "driver_dynlib_loader")
    loader_proc = run(
        [
            str(compiler),
            "build",
            "--no-module-discovery",
            str(ROOT / "tests" / "dynlib_loader.kn"),
            "-o",
            str(loader_exe),
        ],
        cwd=ROOT,
        capture=True,
    )
    assert isinstance(loader_proc, subprocess.CompletedProcess)
    if loader_proc.returncode != 0:
        print("[FAIL] dynlib_loader_build")
        return 1
    loader_run = run([str(loader_exe), str(shared_artifact)], cwd=ROOT, capture=True)
    assert isinstance(loader_run, subprocess.CompletedProcess)
    if (loader_run.stdout or "").replace("\r\n", "\n") != "42\nshared-hi\n":
        print("[FAIL] dynlib_loader_runtime")
        return 1
    print("[OK] dynlib_loader")

    meta_loader_exe = exe_path(out_dir / "driver_meta_dynlib_loader")
    meta_loader_proc = run(
        [
            str(compiler),
            "build",
            "--no-module-discovery",
            str(ROOT / "tests" / "meta_dynlib_loader.kn"),
            "-o",
            str(meta_loader_exe),
        ],
        cwd=ROOT,
        capture=True,
    )
    assert isinstance(meta_loader_proc, subprocess.CompletedProcess)
    if meta_loader_proc.returncode != 0:
        print("[FAIL] meta_dynlib_loader_build")
        return 1
    meta_loader_run = run([str(meta_loader_exe), str(shared_artifact)], cwd=ROOT, capture=True)
    assert isinstance(meta_loader_run, subprocess.CompletedProcess)
    if (meta_loader_run.stdout or "").replace("\r\n", "\n") != "true\n1\nTests.Dynamic.Shared\nhello\nfalse\n":
        print("[FAIL] meta_dynlib_loader_runtime")
        return 1
    print("[OK] meta_dynlib_loader")

    if run_package_integration_tests(compiler, out_dir) != 0:
        return 1
    return 0


def run_package_integration_tests(compiler: Path, out_dir: Path) -> int:
    pkg_source_exe = exe_path(out_dir / "driver_pkg_source")
    pkg_source_proc = run(
        [
            str(compiler),
            "build",
            "--project",
            str(ROOT / "tests" / "pkg" / "project_source"),
            "-o",
            str(pkg_source_exe),
        ],
        cwd=ROOT,
        capture=True,
    )
    assert isinstance(pkg_source_proc, subprocess.CompletedProcess)
    if pkg_source_proc.returncode != 0:
        print("[FAIL] package_project_source_build")
        print((pkg_source_proc.stdout or "") + (pkg_source_proc.stderr or ""))
        return 1
    pkg_source_run = run([str(pkg_source_exe)], cwd=ROOT, capture=True)
    assert isinstance(pkg_source_run, subprocess.CompletedProcess)
    if (pkg_source_run.stdout or "").replace("\r\n", "\n") != "greeter-020\nA\n":
        print("[FAIL] package_project_source_runtime")
        return 1
    print("[OK] package_project_source")

    klib_path = out_dir / "Acme.Greeter.klib"
    klib_pack_proc = run(
        [
            str(compiler),
            "pkg",
            "build",
            "--manifest",
            str(ROOT / "tests" / "pkg" / "local" / "Acme.Greeter" / "0.1.0"),
            "-o",
            str(klib_path),
        ],
        cwd=ROOT,
        capture=True,
    )
    assert isinstance(klib_pack_proc, subprocess.CompletedProcess)
    if klib_pack_proc.returncode != 0 or not klib_path.exists():
        print("[FAIL] package_emit_klib")
        return 1
    print("[OK] package_emit_klib")

    klib_info_proc = run([str(compiler), "pkg", "info", str(klib_path)], cwd=ROOT, capture=True)
    assert isinstance(klib_info_proc, subprocess.CompletedProcess)
    klib_info_text = ((klib_info_proc.stdout or "") + (klib_info_proc.stderr or "")).replace("\r\n", "\n")
    if klib_info_proc.returncode != 0 or "[Klib]" not in klib_info_text or "Sources: 1" not in klib_info_text:
        print("[FAIL] package_klib_info")
        return 1
    print("[OK] package_klib_info")

    klib_extract_dir = out_dir / "klib_extract"
    klib_extract_proc = run(
        [
            str(compiler),
            "pkg",
            "unpack",
            str(klib_path),
            "-o",
            str(klib_extract_dir),
        ],
        cwd=ROOT,
        capture=True,
    )
    assert isinstance(klib_extract_proc, subprocess.CompletedProcess)
    extracted_source = klib_extract_dir / "src" / "Acme" / "Greeter.kn"
    if klib_extract_proc.returncode != 0 or not extracted_source.exists():
        print("[FAIL] package_extract_klib")
        return 1
    if "Unit Acme.Greeter;" not in extracted_source.read_text(encoding="utf-8"):
        print("[FAIL] package_extract_klib_contents")
        return 1
    print("[OK] package_extract_klib")

    emitted_pkg_root = out_dir / "generated-kpkg"
    emit_package_proc = run(
        [
            str(compiler),
            "pkg",
            "build",
            "--manifest",
            str(ROOT / "tests" / "pkg" / "local" / "Acme.Greeter" / "0.1.0"),
            "--layout",
            str(emitted_pkg_root),
        ],
        cwd=ROOT,
        capture=True,
    )
    assert isinstance(emit_package_proc, subprocess.CompletedProcess)
    emitted_pkg_dir = emitted_pkg_root / "Acme.Greeter" / "0.1.0"
    emitted_pkg_manifest = emitted_pkg_dir / "package.knpkg.json"
    emitted_pkg_klib = emitted_pkg_dir / "lib" / "Acme.Greeter.klib"
    if emit_package_proc.returncode != 0 or not emitted_pkg_manifest.exists() or not emitted_pkg_klib.exists():
        print("[FAIL] package_emit_layout")
        print((emit_package_proc.stdout or "") + (emit_package_proc.stderr or ""))
        return 1
    if "\"klib\": \"lib/Acme.Greeter.klib\"" not in emitted_pkg_manifest.read_text(encoding="utf-8"):
        print("[FAIL] package_emit_layout_manifest")
        return 1
    print("[OK] package_emit_layout")

    project_klib_dir = out_dir / "project-klib"
    (project_klib_dir / "src").mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "tests" / "pkg" / "project_source" / "src" / "Main.kn", project_klib_dir / "src" / "Main.kn")
    (project_klib_dir / "kinal.pkg.json").write_text(
        json.dumps(
            {
                "kind": "project",
                "name": "PkgProjectKlib",
                "version": "0.1.0",
                "entry": "src/Main.kn",
                "package_roots": [str(emitted_pkg_root.resolve())],
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )
    pkg_klib_exe = exe_path(out_dir / "driver_pkg_klib")
    pkg_klib_proc = run(
        [
            str(compiler),
            "build",
            "--project",
            str(project_klib_dir),
            "-o",
            str(pkg_klib_exe),
        ],
        cwd=ROOT,
        capture=True,
    )
    assert isinstance(pkg_klib_proc, subprocess.CompletedProcess)
    if pkg_klib_proc.returncode != 0:
        print("[FAIL] package_project_klib_build")
        print((pkg_klib_proc.stdout or "") + (pkg_klib_proc.stderr or ""))
        return 1
    pkg_klib_run = run([str(pkg_klib_exe)], cwd=ROOT, capture=True)
    assert isinstance(pkg_klib_run, subprocess.CompletedProcess)
    if (pkg_klib_run.stdout or "").replace("\r\n", "\n") != "greeter\nA\n":
        print("[FAIL] package_project_klib_runtime")
        return 1
    print("[OK] package_project_klib")

    hybrid_pkg_root = out_dir / "hybrid-kpkg"
    hybrid_pkg_dir = hybrid_pkg_root / "Acme.Greeter" / "0.1.0"
    hybrid_src_dir = hybrid_pkg_dir / "src" / "Acme"
    hybrid_lib_dir = hybrid_pkg_dir / "lib"
    hybrid_src_dir.mkdir(parents=True, exist_ok=True)
    hybrid_lib_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(emitted_pkg_klib, hybrid_lib_dir / "Acme.Greeter.klib")
    (hybrid_src_dir / "Greeter.kn").write_text(
        "Unit Acme.Greeter;\n\nSafe Function string Text()\n{\n    Return \"greeter-source\";\n}\n",
        encoding="utf-8",
    )
    (hybrid_pkg_dir / "package.knpkg.json").write_text(
        json.dumps(
            {
                "kind": "package",
                "name": "Acme.Greeter",
                "version": "0.1.0",
                "source_root": "src",
                "klib": "lib/Acme.Greeter.klib",
                "entry": "src/Acme/Greeter.kn",
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )

    project_hybrid_dir = out_dir / "project-hybrid"
    (project_hybrid_dir / "src").mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "tests" / "pkg" / "project_source" / "src" / "Main.kn", project_hybrid_dir / "src" / "Main.kn")
    (project_hybrid_dir / "kinal.pkg.json").write_text(
        json.dumps(
            {
                "kind": "project",
                "name": "PkgProjectHybrid",
                "version": "0.1.0",
                "entry": "src/Main.kn",
                "package_roots": [str(hybrid_pkg_root.resolve())],
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )

    hybrid_pref_exe = exe_path(out_dir / "driver_pkg_hybrid_prefers_klib")
    hybrid_pref_proc = run(
        [
            str(compiler),
            "build",
            "--project",
            str(project_hybrid_dir),
            "-o",
            str(hybrid_pref_exe),
        ],
        cwd=ROOT,
        capture=True,
    )
    assert isinstance(hybrid_pref_proc, subprocess.CompletedProcess)
    if hybrid_pref_proc.returncode != 0:
        print("[FAIL] package_hybrid_prefers_klib_build")
        print((hybrid_pref_proc.stdout or "") + (hybrid_pref_proc.stderr or ""))
        return 1
    hybrid_pref_run = run([str(hybrid_pref_exe)], cwd=ROOT, capture=True)
    assert isinstance(hybrid_pref_run, subprocess.CompletedProcess)
    if (hybrid_pref_run.stdout or "").replace("\r\n", "\n") != "greeter\nA\n":
        print("[FAIL] package_hybrid_prefers_klib_runtime")
        return 1
    print("[OK] package_hybrid_prefers_klib")

    (hybrid_lib_dir / "Acme.Greeter.klib").unlink()
    hybrid_fallback_exe = exe_path(out_dir / "driver_pkg_hybrid_fallback")
    hybrid_fallback_proc = run(
        [
            str(compiler),
            "build",
            "--project",
            str(project_hybrid_dir),
            "-o",
            str(hybrid_fallback_exe),
        ],
        cwd=ROOT,
        capture=True,
    )
    assert isinstance(hybrid_fallback_proc, subprocess.CompletedProcess)
    if hybrid_fallback_proc.returncode != 0:
        print("[FAIL] package_hybrid_fallback_build")
        print((hybrid_fallback_proc.stdout or "") + (hybrid_fallback_proc.stderr or ""))
        return 1
    hybrid_fallback_run = run([str(hybrid_fallback_exe)], cwd=ROOT, capture=True)
    assert isinstance(hybrid_fallback_run, subprocess.CompletedProcess)
    if (hybrid_fallback_run.stdout or "").replace("\r\n", "\n") != "greeter-source\nA\n":
        print("[FAIL] package_hybrid_fallback_runtime")
        return 1
    print("[OK] package_hybrid_fallback")

    pkg_bad_proc = run(
        [
            str(compiler),
            "build",
            "--project",
            str(ROOT / "tests" / "pkg" / "project_bad" / "kinal.pkg.json"),
            "-o",
            str(exe_path(out_dir / "driver_pkg_bad")),
        ],
        cwd=ROOT,
        capture=True,
    )
    assert isinstance(pkg_bad_proc, subprocess.CompletedProcess)
    pkg_bad_text = ((pkg_bad_proc.stdout or "") + (pkg_bad_proc.stderr or "")).replace("\r\n", "\n")
    if pkg_bad_proc.returncode == 0 or "reserved package namespace 'IO.*'" not in pkg_bad_text:
        print("[FAIL] package_reserved_namespace")
        return 1
    print("[OK] package_reserved_namespace")
    return 0


def llvm_bin_dir() -> Path:
    env = os.environ.get("KN_LLVM_BIN")
    candidates: list[Path] = []
    seen: set[str] = set()

    def add(path: Path) -> None:
        key = str(path)
        if key not in seen:
            seen.add(key)
            candidates.append(path)

    if env:
        add(Path(env).expanduser())

    for root in (
        ROOT.parent / "Kinal-ThirdParty" / ".cache" / "llvm" / "prebuilt",
        ROOT.parent / "Kinal-ThirdParty" / "llvm" / "prebuilt",
        ROOT / "third_party" / "llvm" / "prebuilt",
    ):
        if not root.exists():
            continue
        for child in sorted(root.iterdir(), key=lambda p: p.name.lower(), reverse=True):
            add(child / "bin")

    clang_on_path = shutil.which("clang.exe") or shutil.which("clang")
    if clang_on_path:
        add(Path(clang_on_path).resolve().parent)

    for candidate in candidates:
        clang = candidate / "clang.exe"
        llvm_lib = candidate / "llvm-lib.exe"
        lld_link = candidate / "lld-link.exe"
        if clang.exists() and llvm_lib.exists() and lld_link.exists():
            return candidate.resolve()

    return (ROOT / "third_party" / "llvm" / "prebuilt" / "clang+llvm-21.1.8-x86_64-pc-windows-msvc" / "bin").resolve()


def build_native_ffi_assets(out_dir: Path) -> None:
    if not is_windows():
        return

    src = ROOT / "tests" / "ffi_native" / "native_ffi.c"
    if not src.exists():
        return

    llvm_bin = llvm_bin_dir()
    clang = llvm_bin / "clang.exe"
    llvm_lib = llvm_bin / "llvm-lib.exe"
    lld_link = llvm_bin / "lld-link.exe"
    if not clang.exists() or not llvm_lib.exists() or not lld_link.exists():
        raise SystemExit("LLVM toolchain for FFI tests not found; check KN_LLVM_BIN or third_party/llvm/prebuilt")

    obj = out_dir / "native_ffi.obj"
    lib = out_dir / "native_ffi.lib"
    dll = out_dir / "native_ffi.dll"
    imp = out_dir / "native_ffi_import.lib"
    for p in (obj, lib, dll, imp):
        if p.exists():
            p.unlink()

    run(
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
        cwd=ROOT,
    )
    run([str(llvm_lib), f"/out:{lib}", str(obj)], cwd=ROOT)
    run(
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
        cwd=ROOT,
    )


def build_hosted_probe_assets(out_dir: Path) -> None:
    if not is_windows():
        return

    src = ROOT / "tests" / "ffi_native" / "native_hosted_probe.c"
    if not src.exists():
        return

    llvm_bin = llvm_bin_dir()
    clang = llvm_bin / "clang.exe"
    if not clang.exists():
        raise SystemExit("LLVM clang for hosted probe tests not found; check KN_LLVM_BIN or third_party/llvm/prebuilt")

    dynamic_obj = out_dir / "native_hosted_probe_md.obj"
    static_obj = out_dir / "native_hosted_probe_mt.obj"
    for p in (dynamic_obj, static_obj):
        if p.exists():
            p.unlink()

    run(
        [
            str(clang),
            "-c",
            str(src),
            "-o",
            str(dynamic_obj),
            "-fms-runtime-lib=dll",
            "-D_CRT_SECURE_NO_WARNINGS",
        ],
        cwd=ROOT,
    )
    run(
        [
            str(clang),
            "-c",
            str(src),
            "-o",
            str(static_obj),
            "-fms-runtime-lib=static",
            "-D_CRT_SECURE_NO_WARNINGS",
        ],
        cwd=ROOT,
    )


def manifest_needs_native_ffi(manifest: list[dict[str, object]]) -> bool:
    for case in manifest:
        if case_needs_native_ffi(case):
            return True
    return False


def case_needs_native_ffi(case: dict[str, object]) -> bool:
    for arg in case.get("compiler_args", []):
        if "native_ffi" in str(arg):
            return True
    for path in case.get("runtime_files", []):
        if "native_ffi" in str(path):
            return True
    for path in case_file_paths(case):
        if not path.exists():
            continue
        if "native_ffi" in path.read_text(encoding="utf-8"):
            return True
    return False


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description="Run Kinal compiler regression tests")
    ap.add_argument("--compiler", required=True, help="path to the compiler binary to test")
    ap.add_argument("--out-dir", default=str(DEFAULT_OUT), help="directory for generated test outputs")
    ap.add_argument("--manifest", default=str(DEFAULT_MANIFEST), help="manifest json to execute")
    ap.add_argument("--driver-checks", action="store_true", help="run slower driver integration checks")
    ap.add_argument("--package-checks", action="store_true", help="run local package and klib integration checks")
    return ap.parse_args()


def main() -> int:
    args = parse_args()
    compiler = Path(args.compiler).resolve()
    if not compiler.exists():
        raise SystemExit(f"compiler not found: {compiler}")

    manifest_path = Path(args.manifest).resolve()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    if manifest_needs_native_ffi(manifest):
        build_native_ffi_assets(out_dir)
    for case in manifest:
        name = case["name"]
        if not case_runs_on_host(case):
            print(f"[SKIP] {name} (not enabled for {host_platform()})")
            continue
        if not is_windows() and case_needs_native_ffi(case):
            print(f"[SKIP] {name} (native ffi assets are only wired for Windows in this harness)")
            continue
        kn_files = case_file_paths(case)
        expect_error = case.get("expect_error")
        compile_only = bool(case.get("compile_only", False))
        auto_link = case.get("auto_link", False)
        out_exe = artifact_path(out_dir / name, str(case.get("output_ext", "")))

        cmd = [str(compiler), "build"]
        if not auto_link:
            cmd.append("--no-module-discovery")
        for arg in case.get("compiler_args", []):
            a = rewrite_legacy_test_path(str(arg), out_dir)
            if not a.startswith("-"):
                p = Path(a)
                if not p.is_absolute():
                    cand = ROOT / p
                    if cand.exists():
                        a = str(cand)
            cmd.append(a)
        cmd.extend([str(f) for f in kn_files])
        cmd.extend(["-o", str(out_exe)])

        if expect_error:
            proc = run(cmd, cwd=ROOT, capture=True)
            assert isinstance(proc, subprocess.CompletedProcess)
            stderr = (proc.stderr or "").replace("\r\n", "\n")
            stdout = (proc.stdout or "").replace("\r\n", "\n")
            combined = stdout + stderr
            if proc.returncode == 0:
                print(f"[FAIL] {name}")
                print("Expected compiler error but it succeeded")
                return 1
            if not matches_expected_error(combined, expect_error):
                print(f"[FAIL] {name}")
                print("Expected error substring:")
                print(repr(expect_error))
                print("Got:")
                print(repr(combined))
                return 1
            print(f"[OK] {name}")
            continue

        run(cmd, cwd=ROOT)
        if compile_only:
            if not out_exe.exists():
                print(f"[FAIL] {name}")
                print(f"Expected compile-only artifact was not created: {out_exe}")
                return 1
            print(f"[OK] {name}")
            continue

        expected = case["expected"].replace("\r\n", "\n")
        expected_rc = int(case.get("expected_exit_code", 0))
        for runtime_file in case.get("runtime_files", []):
            runtime_file = rewrite_legacy_test_path(runtime_file, out_dir)
            src = Path(runtime_file)
            if not src.is_absolute():
                src = ROOT / runtime_file
            dst = out_dir / Path(runtime_file).name
            if src.resolve() != dst.resolve():
                shutil.copy2(src, dst)
        output = run([str(out_exe)], cwd=ROOT, capture=True)
        assert isinstance(output, subprocess.CompletedProcess)
        out_text = (output.stdout or "").replace("\r\n", "\n")

        if output.returncode != expected_rc:
            print(f"[FAIL] {name}")
            print("Expected exit code:")
            print(repr(expected_rc))
            print("Got:")
            print(repr(output.returncode))
            return 1

        if not matches_expected_runtime_output(out_text, expected):
            print(f"[FAIL] {name}")
            print("Expected:")
            print(repr(expected))
            print("Got:")
            print(repr(out_text))
            return 1
        print(f"[OK] {name}")

    if args.driver_checks and run_driver_integration_tests(compiler, out_dir) != 0:
        return 1
    if args.package_checks and run_package_integration_tests(compiler, out_dir) != 0:
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
