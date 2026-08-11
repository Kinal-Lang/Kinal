from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path

from .context import (
    ROOT,
    SELFHOST_APP_DIR,
    SELFHOST_OUT,
    exe_name,
    host_tag,
    is_windows,
    release_dir,
    release_fallback_dir,
    stage_dir,
)
from .llvm import detect_llvm_dir, llvm_bin_dir
from .util import run


def default_staged_compiler(release: bool) -> Path:
    return stage_dir("Release" if release else "Debug") / exe_name("kinal")


def selfhost_stage0_dir() -> Path:
    return SELFHOST_OUT / "stage0-host"


def selfhost_stage0_exe() -> Path:
    return selfhost_stage0_dir() / exe_name("kinal")


def selfhost_stage1_dir() -> Path:
    return SELFHOST_OUT / "stage1"


def selfhost_stage1_exe() -> Path:
    return selfhost_stage1_dir() / exe_name("kinal-selfhost")


def selfhost_test_dir() -> Path:
    return SELFHOST_OUT / "tests"


def selfhost_bridge_object() -> Path:
    return SELFHOST_OUT / "bridge" / "kn_selfhost_llvm.o"


def build_selfhost_bridge() -> Path:
    llvm_dir = detect_llvm_dir()
    llvm_root = llvm_dir.resolve().parents[2]
    llvm_bin = llvm_bin_dir(llvm_dir)
    clang = llvm_bin / exe_name("clang")
    if not clang.is_file():
        raise SystemExit(f"LLVM clang was not found: {clang}")
    current_path = os.environ.get("PATH", "")
    llvm_bin_text = str(llvm_bin)
    if llvm_bin_text not in current_path.split(os.pathsep):
        os.environ["PATH"] = llvm_bin_text + (os.pathsep + current_path if current_path else "")
    output = selfhost_bridge_object()
    output.parent.mkdir(parents=True, exist_ok=True)
    run(
        [
            clang,
            "-std=c11",
            "-O2",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            SELFHOST_APP_DIR / "bridge" / "include",
            "-I",
            llvm_root / "include",
            "-c",
            SELFHOST_APP_DIR / "bridge" / "src" / "kn_selfhost_llvm.c",
            "-o",
            output,
        ]
    )
    runtime_output = output.with_name("kn_selfhost_runtime.o")
    run(
        [
            clang,
            "-std=c11",
            "-O2",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            SELFHOST_APP_DIR / "bridge" / "include",
            "-c",
            SELFHOST_APP_DIR / "bridge" / "src" / "kn_selfhost_runtime.c",
            "-o",
            runtime_output,
        ]
    )
    return output


def copy_selfhost_llvm_runtime(stage0: Path, stage1: Path) -> None:
    if is_windows():
        for name in ("LLVM-C.dll", "LTO.dll", "Remarks.dll"):
            source = stage0.parent / name
            if source.is_file():
                shutil.copy2(source, stage1.parent / name)
        return

    # Non-Windows stage1 links with an rpath into the frozen stage0 bundle.
    # Keep a local copy as well so the stage can later be packaged in isolation.
    source_dir = stage0.parent / "llvm" / "lib"
    target_dir = stage1.parent / "llvm" / "lib"
    if source_dir.is_dir():
        shutil.copytree(source_dir, target_dir, dirs_exist_ok=True)


def sync_selfhost_stdlib_native_assets(stage0: Path) -> None:
    packaged = stage0.parent / "stdpkg"
    if not packaged.is_dir():
        return
    for native_dir in sorted(packaged.glob("*/*/native")):
        relative = native_dir.relative_to(packaged)
        shutil.copytree(native_dir, ROOT / "libs" / "std" / relative, dirs_exist_ok=True)


def write_selfhost_clang_wrapper(stage1_root: Path) -> None:
    if is_windows():
        return
    llvm_dir = detect_llvm_dir()
    clang = llvm_bin_dir(llvm_dir) / "clang"
    if not clang.is_file():
        raise SystemExit(f"LLVM clang was not found: {clang}")
    wrapper = stage1_root / "linker" / "clang"
    wrapper.parent.mkdir(parents=True, exist_ok=True)
    escaped = str(clang).replace("'", "'\\''")
    wrapper.write_text(f"#!/bin/sh\nexec '{escaped}' \"$@\"\n", encoding="utf-8")
    wrapper.chmod(0o755)

    linker_name = "ld64.lld" if host_tag().startswith("macos-") else "ld.lld"
    source = llvm_bin_dir(llvm_dir) / linker_name
    if not source.is_file():
        raise SystemExit(f"LLVM linker was not found: {source}")
    shutil.copy2(source, stage1_root / "linker" / linker_name)


def package_selfhost_toolchain(stage0: Path, stage1: Path) -> None:
    stage0_root = stage0.parent
    stage1_root = stage1.parent
    bridge_dir = selfhost_bridge_object().parent
    shutil.copytree(bridge_dir, stage1_root / "bridge", dirs_exist_ok=True)
    shutil.copytree(ROOT / "libs" / "std", stage1_root / "stdlib-src", dirs_exist_ok=True)

    linker_source = stage0_root / "linker"
    if linker_source.is_dir():
        shutil.copytree(linker_source, stage1_root / "linker", dirs_exist_ok=True)
    write_selfhost_clang_wrapper(stage1_root)

    runtime_source = stage0_root / "runtime" / host_tag()
    runtime_target = stage1_root / "runtime" / host_tag()
    if runtime_source.is_dir():
        shutil.copytree(runtime_source, runtime_target, dirs_exist_ok=True)
    required_runtime = (
        (
            "kn_runtime.obj",
            "kn_math.obj",
            "kernel32.lib",
            "user32.lib",
            "gdi32.lib",
            "oldnames.lib",
            "msvcrt.lib",
            "vcruntime.lib",
            "ucrt.lib",
        )
        if is_windows()
        else ("kn_runtime.o", "kn_math.o")
    )
    for name in required_runtime:
        source = runtime_target / name
        if not source.is_file():
            raise SystemExit(f"Selfhost toolchain input is missing: {source}")

    if is_windows():
        llvm_import = stage0_root / "llvm" / "lib" / "LLVM-C.lib"
        llvm_target = stage1_root / "llvm" / "lib"
        llvm_target.mkdir(parents=True, exist_ok=True)
        shutil.copy2(llvm_import, llvm_target / llvm_import.name)


def freeze_selfhost_stage0_bundle(bundle_dir: Path) -> Path:
    bundle = bundle_dir.expanduser().resolve()
    compiler = bundle / exe_name("kinal")
    required = [
        compiler,
        bundle / "llvm" / "lib",
        bundle / "runtime",
        bundle / "stdpkg",
    ]
    # Windows release bundles carry lld-link. POSIX bundles may omit the linker
    # because package_selfhost_toolchain creates a small clang/lld wrapper from
    # the LLVM installation selected by the workflow.
    if is_windows():
        required.append(bundle / "linker")
    missing = [path for path in required if not path.exists()]
    if missing:
        details = "\n".join(f"  - {path}" for path in missing)
        raise SystemExit(f"Selfhost stage0 bundle is incomplete:\n{details}")

    stage0_dir = selfhost_stage0_dir()
    if stage0_dir.exists():
        shutil.rmtree(stage0_dir)
    shutil.copytree(bundle, stage0_dir)
    frozen = selfhost_stage0_exe()
    sync_selfhost_stdlib_native_assets(frozen)
    return frozen


def prepare_selfhost_stage0(*, clean_first: bool, cmd_dist, bundle_dir: Path | None = None) -> Path:
    if bundle_dir is not None:
        return freeze_selfhost_stage0_bundle(bundle_dir)

    # Selfhosting is a compiler correctness boundary. Rebuild before freezing so
    # stage0 never carries stale runtime/compiler objects from an older bundle.
    cmd_dist(type("Args", (), {"clean": clean_first})())
    # A running Windows compiler can keep the stable release directory locked;
    # cmd_dist then publishes the fresh bundle as *.next. Always freeze the
    # newest available compiler bundle, otherwise bootstrap silently uses stale
    # stage0 code after a successful rebuild.
    candidates = [release_dir(), release_fallback_dir()]
    available = [path for path in candidates if (path / exe_name("kinal")).is_file()]
    if not available:
        raise SystemExit("No release compiler bundle is available for selfhost stage0.")
    bundle = max(available, key=lambda path: (path / exe_name("kinal")).stat().st_mtime_ns)
    return freeze_selfhost_stage0_bundle(bundle)


def build_selfhost_stage1(stage0: Path) -> Path:
    bridge_llvm = build_selfhost_bridge()
    bridge_runtime = bridge_llvm.with_name("kn_selfhost_runtime.o")
    stage1 = selfhost_stage1_exe()
    stage1.parent.mkdir(parents=True, exist_ok=True)
    command: list[str | Path] = [
        stage0,
        "build",
        "--project",
        SELFHOST_APP_DIR,
        "--profile",
        "stage1",
        "-o",
        stage1,
        "--lib-dir",
        stage0.parent / "llvm" / "lib",
        "--lib",
        "LLVM-C" if is_windows() else "LLVM",
        "--link-file",
        bridge_llvm,
        "--link-file",
        bridge_runtime,
    ]
    if is_windows():
        command.extend(["--link-arg", "/stack:16777216"])
    elif host_tag().startswith("macos-"):
        command.extend(
            [
                "--link-arg",
                "-rpath",
                "--link-arg",
                "@loader_path/llvm/lib",
            ]
        )
    else:
        command.extend(
            [
                "--link-arg",
                "-rpath",
                "--link-arg",
                r"\$ORIGIN/llvm/lib",
            ]
        )
    run(command)
    copy_selfhost_llvm_runtime(stage0, stage1)
    package_selfhost_toolchain(stage0, stage1)
    return stage1


def run_selfhost_tests(stage0: Path, stage1: Path) -> Path:
    out_dir = selfhost_test_dir()
    out_dir.mkdir(parents=True, exist_ok=True)
    run(
        [
            sys.executable,
            str(ROOT / "tests" / "selfhost" / "run_selfhost_tests.py"),
            "--stage0",
            str(stage0),
            "--compiler",
            str(stage1),
            "--root",
            str(ROOT),
            "--out-dir",
            str(out_dir),
        ]
    )
    return out_dir


def run_selfhost_bootstrap(*, target: float, max_stage: int, bootstrap_backend: str, metric_backend: str, clean: bool) -> None:
    cmd = [
        sys.executable,
        str(ROOT / "tests" / "selfhost" / "run_bootstrap.py"),
        "--target",
        str(target),
        "--max-stage",
        str(max_stage),
        "--bootstrap-backend",
        bootstrap_backend,
        "--metric-backend",
        metric_backend,
    ]
    if clean:
        cmd.append("--clean")
    run(cmd)
