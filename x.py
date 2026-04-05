#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from infra.scripts.x.context import (
    ARTIFACTS_ROOT,
    BUILD_GUI_LOCALES_DIR,
    BUILD_ROOT,
    CHKSTK_SRC,
    CIVETWEB_DIR,
    CIVETWEB_INCLUDE,
    CIVETWEB_LEGACY_PREBUILT_ROOT,
    CIVETWEB_PREBUILT_CACHE_ROOT,
    CIVETWEB_SRC,
    CIVETWEB_SRC_ARCHIVE_URL,
    CIVETWEB_VERSION_FILE,
    IO_WEB_NATIVE_TARGETS,
    LOCALES_DIR,
    MATH_SRC,
    OUT,
    RELEASE_ROOT,
    ROOT,
    RUNTIME_DIR,
    RUNTIME_INCLUDE,
    RUNTIME_SRC,
    SELFHOST_APP_DIR,
    SELFHOST_OUT,
    SELFHOST_SUPPORT_DIR,
    SELFHOST_UTIL_HOSTLIB_SRC,
    STAGE_ROOT,
    STDPKG_DIR,
    TEST_ROOT,
    THIRD_PARTY_REPO_URL,
    THIRD_PARTY_ROOT,
    VERSION_FILE,
    WEB_BRIDGE_SRC,
    build_dir,
    exe_name,
    host_tag,
    is_windows,
    read_version,
    release_bundle_ready,
    release_dir,
    release_fallback_dir,
    release_fallback_zip,
    release_zip,
    stage_dir,
)
from infra.scripts.x.llvm import detect_llvm_dir, llvm_bin_dir, llvm_lib_dir, preferred_cmake_compilers, resolve_cmake_env
from infra.scripts.x.release_ops import (
    build_official_stdpkg_klibs,
    clean_legacy_source_artifacts,
    stage_bundle,
    zip_directory,
)
from infra.scripts.x.runtime_build import (
    build_civetweb_for_available_targets,
    build_runtime_for_host,
)
from infra.scripts.x.selfhost_ops import (
    default_staged_compiler,
    prepare_selfhost_stage0,
    run_selfhost_bootstrap,
    run_selfhost_tests,
)
from infra.scripts.x.third_party import ensure_third_party_repo
from infra.scripts.x.zig import detect_zig_path, ensure_zig_available
from infra.scripts.x.util import copy_tree, download_file, find_artifact, read_json, resolve_tool, run, write_text


def export_gui_locales(dest_dir: Path) -> list[Path]:
    gui_dir = dest_dir
    gui_dir.mkdir(parents=True, exist_ok=True)
    files = [
        gui_dir / "en-US.json",
        gui_dir / "zh-CN.json",
    ]
    for file in files:
        if not file.exists():
            if file.name == "en-US.json":
                content = {
                    "window.title": "Kinal Build Dashboard",
                    "toolbar.build_mode": "Build mode",
                    "toolbar.test_mode": "Test mode",
                    "toolbar.language": "UI language",
                    "toolbar.clean_build_dir": "Clean build dir",
                    "toolbar.clear_output": "Clear Output",
                    "toolbar.cancel_task": "Cancel Task",
                    "status.idle": "Idle",
                    "status.running": "Running",
                    "frame.status": "Status",
                    "frame.actions": "Actions",
                    "frame.output": "Output",
                    "status.refresh": "Refresh Status",
                    "status.tree.key": "Key",
                    "status.tree.value": "Value",
                    "actions.group.status_fetch": "Status & Fetch",
                    "actions.group.build": "Build",
                    "actions.group.validation": "Validation",
                    "actions.update_third_party": "Update third-party on fetch",
                    "actions.doctor": "Doctor",
                    "actions.fetch_third_party": "Fetch Third-Party",
                    "actions.fetch_llvm": "Fetch LLVM",
                    "actions.fetch_zig": "Fetch Zig",
                    "actions.dev": "Dev",
                    "actions.dist": "Dist",
                    "actions.stdpkg": "StdPkg",
                    "actions.tests": "Tests",
                    "actions.selfhost": "Selfhost",
                    "actions.selfhost_bootstrap": "Selfhost Bootstrap",
                    "actions.open_artifacts": "Open Artifacts",
                    "dialog.title": "Kinal Build Dashboard",
                    "dialog.running_command": "A command is already running.",
                    "dialog.refresh_failed": "Failed to refresh status:\n{error}",
                    "log.kinal_root": "Kinal root: {path}\\n",
                    "log.python": "Python: {path}\\n",
                    "log.command.done": "exit code {code}\\n",
                    "log.cancel_requested": "cancel requested for running task\\n",
                    "log.cancel_failed": "failed to cancel task: {error}\\n",
                    "command.doctor": "Doctor",
                    "command.fetch_third_party": "Fetch Third-Party",
                    "command.fetch_llvm": "Fetch LLVM",
                    "command.fetch_zig": "Fetch Zig",
                    "command.dev": "Dev Build",
                    "command.dist": "Release Build",
                    "command.stdpkg": "StdPkg Build",
                    "command.tests": "Tests",
                    "command.selfhost": "Selfhost",
                    "command.selfhost_bootstrap": "Selfhost Bootstrap",
                    "command.open_artifacts": "Open Artifacts",
                }
            else:
                content = {
                    "window.title": "Kinal Build Dashboard",
                    "toolbar.build_mode": "Build mode",
                    "toolbar.test_mode": "Test mode",
                    "toolbar.language": "UI language",
                    "toolbar.clean_build_dir": "Clean build dir",
                    "toolbar.clear_output": "Clear Output",
                    "toolbar.cancel_task": "Cancel Task",
                    "status.idle": "Idle",
                    "status.running": "Running",
                    "frame.status": "Status",
                    "frame.actions": "Actions",
                    "frame.output": "Output",
                    "status.refresh": "Refresh Status",
                    "status.tree.key": "Key",
                    "status.tree.value": "Value",
                    "actions.group.status_fetch": "Status & Fetch",
                    "actions.group.build": "Build",
                    "actions.group.validation": "Validation",
                    "actions.update_third_party": "Update third-party on fetch",
                    "actions.doctor": "Doctor",
                    "actions.fetch_third_party": "Fetch Third-Party",
                    "actions.fetch_llvm": "Fetch LLVM",
                    "actions.fetch_zig": "Fetch Zig",
                    "actions.dev": "Dev",
                    "actions.dist": "Dist",
                    "actions.stdpkg": "StdPkg",
                    "actions.tests": "Tests",
                    "actions.selfhost": "Selfhost",
                    "actions.selfhost_bootstrap": "Selfhost Bootstrap",
                    "actions.open_artifacts": "Open Artifacts",
                    "dialog.title": "Kinal Build Dashboard",
                    "dialog.running_command": "A command is already running.",
                    "dialog.refresh_failed": "Failed to refresh status:\n{error}",
                    "log.kinal_root": "Kinal root: {path}\\n",
                    "log.python": "Python: {path}\\n",
                    "log.command.done": "exit code {code}\\n",
                    "log.cancel_requested": "cancel requested for running task\\n",
                    "log.cancel_failed": "failed to cancel task: {error}\\n",
                    "command.doctor": "Doctor",
                    "command.fetch_third_party": "Fetch Third-Party",
                    "command.fetch_llvm": "Fetch LLVM",
                    "command.fetch_zig": "Fetch Zig",
                    "command.dev": "Dev Build",
                    "command.dist": "Release Build",
                    "command.stdpkg": "StdPkg Build",
                    "command.tests": "Tests",
                    "command.selfhost": "Selfhost",
                    "command.selfhost_bootstrap": "Selfhost Bootstrap",
                    "command.open_artifacts": "Open Artifacts",
                }
            write_text(file, json.dumps(content, indent=2, ensure_ascii=False) + "\n")
    return files


def ensure_build_prereqs() -> None:
    ensure_third_party_repo(update=False)


def cmake_configure(build_type: str, *, clean_first: bool = False) -> Path:
    bdir = build_dir(build_type)
    if clean_first and bdir.exists():
        shutil.rmtree(bdir)
    env, llvm_dir = resolve_cmake_env()
    c_compiler, cxx_compiler = preferred_cmake_compilers(llvm_dir)
    cache_path = bdir / "CMakeCache.txt"
    if cache_path.exists() and c_compiler and cxx_compiler:
        cache_text = cache_path.read_text(encoding="utf-8", errors="ignore")
        if str(c_compiler).replace("\\", "/") not in cache_text or str(cxx_compiler).replace("\\", "/") not in cache_text:
            shutil.rmtree(bdir)
    preset = f"host-{build_type.lower()}"
    cmake_args: list[str | Path] = [
        resolve_tool("cmake"),
        "--preset",
        preset,
        f"-DLLVM_DIR={llvm_dir}",
    ]
    if c_compiler and cxx_compiler:
        cmake_args.extend(
            [
                f"-DCMAKE_C_COMPILER={c_compiler}",
                f"-DCMAKE_CXX_COMPILER={cxx_compiler}",
            ]
        )
    run(
        cmake_args,
        env=env,
    )
    return bdir


def cmake_build(build_type: str) -> Path:
    env, _ = resolve_cmake_env()
    preset = f"build-host-{build_type.lower()}"
    run([resolve_tool("cmake"), "--build", "--preset", preset], env=env)
    return build_dir(build_type)


def cmd_doctor(_: argparse.Namespace) -> int:
    print(f"kinal_version={read_version()}")
    print(f"kinalvm_version={read_version('kinalvm')}")
    print(f"host={host_tag()}")
    print(f"python={sys.executable}")
    print(f"third_party_root={THIRD_PARTY_ROOT}")
    print(f"third_party_repo={THIRD_PARTY_REPO_URL}")
    print(f"cmake={resolve_tool('cmake')}")
    print(f"ninja={resolve_tool('ninja')}")
    print(f"node={resolve_tool('node')}")
    print(f"npm={resolve_tool('npm')}")
    print(f"npx={resolve_tool('npx')}")
    doctor_ok = True
    try:
        llvm_dir = detect_llvm_dir()
        print(f"llvm_dir={llvm_dir}")
        print(f"llvm_bin={llvm_bin_dir(llvm_dir)}")
        c_compiler, cxx_compiler = preferred_cmake_compilers(llvm_dir)
        if c_compiler and cxx_compiler:
            print(f"cmake_c={c_compiler}")
            print(f"cmake_cxx={cxx_compiler}")
    except SystemExit as exc:
        doctor_ok = False
        print(f"llvm_dir=<missing>")
        print(f"doctor_hint={exc}")
        print("next_step=python x.py fetch llvm-prebuilt")
    zig = detect_zig_path()
    if zig:
        print(f"zig={zig}")
    else:
        print("zig=<missing>")
        print("zig_hint=run python x.py fetch zig-prebuilt or install Zig and expose it via PATH/KINAL_ZIG")
    print(f"build_debug={build_dir('debug')}")
    print(f"stage_debug={stage_dir('debug')}")
    print(f"release={release_dir()}")
    return 0 if doctor_ok else 1


def collect_status() -> dict[str, object]:
    zig_path = detect_zig_path()
    status: dict[str, object] = {
        "kinal_version": read_version(),
        "kinalvm_version": read_version("kinalvm"),
        "host": host_tag(),
        "python": sys.executable,
        "third_party_root": str(THIRD_PARTY_ROOT),
        "third_party_repo": THIRD_PARTY_REPO_URL,
        "third_party_present": THIRD_PARTY_ROOT.exists(),
        "zig": str(zig_path) if zig_path else "",
        "zig_ready": bool(zig_path),
        "build_debug": str(build_dir("debug")),
        "stage_debug": str(stage_dir("debug")),
        "release": str(release_dir()),
    }
    try:
        llvm_dir = detect_llvm_dir()
        status["llvm_ready"] = True
        status["llvm_dir"] = str(llvm_dir)
        status["llvm_bin"] = str(llvm_bin_dir(llvm_dir))
        c_compiler, cxx_compiler = preferred_cmake_compilers(llvm_dir)
        status["cmake_c"] = str(c_compiler) if c_compiler else ""
        status["cmake_cxx"] = str(cxx_compiler) if cxx_compiler else ""
    except SystemExit as exc:
        status["llvm_ready"] = False
        status["llvm_dir"] = ""
        status["doctor_hint"] = str(exc)
        status["next_step"] = "python x.py fetch llvm-prebuilt"
    return status


def cmd_status(args: argparse.Namespace) -> int:
    status = collect_status()
    if args.json:
        print(json.dumps(status, indent=2, ensure_ascii=False))
        return 0
    for key, value in status.items():
        print(f"{key}={value}")
    return 0


def cmd_open_artifacts(_: argparse.Namespace) -> int:
    target = ARTIFACTS_ROOT
    if sys.platform == "win32":
        os.startfile(target)  # type: ignore[attr-defined]
    elif sys.platform == "darwin":
        subprocess.Popen(["open", str(target)])
    else:
        subprocess.Popen(["xdg-open", str(target)])
    print(f"[OK] opened artifacts: {target}")
    return 0


def cmd_export_gui_locales(args: argparse.Namespace) -> int:
    base_dir = Path(args.out_dir).resolve() if args.out_dir else BUILD_GUI_LOCALES_DIR
    files = export_gui_locales(base_dir)
    for file in files:
        print(f"[OK] gui locale: {file}")
    return 0


def cmd_fetch(args: argparse.Namespace) -> int:
    target = args.target
    if target in ("third-party", "all"):
        print(f"[INFO] ensuring third-party repository at {THIRD_PARTY_ROOT}")
        repo = ensure_third_party_repo(update=args.update)
        print(f"[OK] third-party repo: {repo}")
    if target in ("llvm-prebuilt", "llvm-src", "llvm", "all"):
        ensure_third_party_repo(update=False)
        script = ROOT / "infra" / "toolchains" / "setup_llvm.py"
        print(f"[INFO] fetching LLVM assets via {script}")
        cmd: list[str | Path] = [sys.executable, script]
        if target in ("llvm-prebuilt", "llvm", "all"):
            cmd.append("--fetch-prebuilt")
        if target in ("llvm-src", "all"):
            cmd.append("--fetch-src")
        run(cmd)
        print("[OK] LLVM fetch completed")
    if target in ("zig-prebuilt", "zig", "all"):
        script = ROOT / "infra" / "toolchains" / "setup_zig.py"
        print(f"[INFO] fetching Zig via {script}")
        run([sys.executable, script, "--fetch-prebuilt"])
        zig = ensure_zig_available(fetch=False)
        if zig:
            print(f"[OK] Zig ready: {zig}")
        else:
            raise SystemExit("Zig bootstrap reported success but Zig could not be located afterwards.")
    return 0


def cmd_dev(args: argparse.Namespace) -> int:
    ensure_build_prereqs()
    build_type = "Release" if args.release else "Debug"
    cmake_configure(build_type, clean_first=args.clean)
    cmake_build(build_type)
    dest = stage_bundle(build_type, stage_dir(build_type))
    print(f"[OK] staged bundle: {dest}")
    return 0


def cmd_stdpkg(args: argparse.Namespace) -> int:
    ensure_build_prereqs()
    build_type = "Release" if args.release else "Debug"
    if args.build:
        cmake_configure(build_type, clean_first=args.clean)
        cmake_build(build_type)
    compiler = Path(args.compiler) if args.compiler else find_artifact(build_dir(build_type), exe_name("kinal"))
    build_official_stdpkg_klibs(compiler, llvm_bin_dir(detect_llvm_dir()))
    print(f"[OK] rebuilt official stdpkg klibs with: {compiler}")
    return 0


def cmd_dist(args: argparse.Namespace) -> int:
    ensure_build_prereqs()
    build_type = "Release"
    cmake_configure(build_type, clean_first=args.clean)
    cmake_build(build_type)
    stable_dir = release_dir()
    stable_zip = release_zip()
    fallback_dir = release_fallback_dir()
    fallback_zip = release_fallback_zip()
    dest_path = stable_dir
    zip_path = stable_zip
    try:
        dest = stage_bundle(build_type, dest_path)
    except PermissionError:
        dest_path = fallback_dir
        zip_path = fallback_zip
        print(f"[WARN] release bundle is in use, staging fallback bundle: {dest_path}")
        dest = stage_bundle(build_type, dest_path)
    zip_path = zip_directory(dest, zip_path)
    if dest_path == stable_dir:
        if fallback_dir.exists():
            shutil.rmtree(fallback_dir)
        if fallback_zip.exists():
            fallback_zip.unlink()
    print(f"[OK] release bundle: {dest}")
    print(f"[OK] release zip: {zip_path}")
    return 0


def cmd_test(args: argparse.Namespace) -> int:
    ensure_build_prereqs()
    compiler = Path(args.compiler).resolve() if args.compiler else default_staged_compiler(args.release)
    if args.build or not compiler.exists():
        cmd_dev(argparse.Namespace(release=args.release, clean=False))
    compiler = Path(args.compiler).resolve() if args.compiler else default_staged_compiler(args.release)

    def run_manifest(manifest: Path, out_dir: Path, *, driver_checks: bool = False) -> None:
        out_dir.mkdir(parents=True, exist_ok=True)
        cmd = [
            sys.executable,
            str(ROOT / "tests" / "run_tests.py"),
            "--compiler",
            str(compiler),
            "--out-dir",
            str(out_dir),
            "--manifest",
            str(manifest),
        ]
        if driver_checks:
            cmd.append("--driver-checks")
        run(cmd)

    if args.full:
        run_manifest(ROOT / "tests" / "manifest.json", TEST_ROOT, driver_checks=True)
    else:
        run_manifest(ROOT / "tests" / "smoke.json", TEST_ROOT / "smoke")
        run_manifest(ROOT / "tests" / "freestanding.json", TEST_ROOT / "freestanding")
    return 0


def cmd_selfhost(args: argparse.Namespace) -> int:
    ensure_build_prereqs()
    stage0 = prepare_selfhost_stage0(clean_first=args.clean, cmd_dist=cmd_dist)
    print(f"[OK] selfhost stage0: {stage0}")
    if args.test:
        out_dir = run_selfhost_tests(stage0)
        print(f"[OK] selfhost tests: {out_dir}")
    return 0


def cmd_selfhost_bootstrap(args: argparse.Namespace) -> int:
    ensure_build_prereqs()
    run_selfhost_bootstrap(
        target=args.target,
        max_stage=args.max_stage,
        bootstrap_backend=args.bootstrap_backend,
        metric_backend=args.metric_backend,
        clean=args.clean,
    )
    return 0


def cmd_clean(args: argparse.Namespace) -> int:
    if OUT.exists():
        shutil.rmtree(OUT)
        print(f"[OK] removed {OUT}")
    clean_legacy_source_artifacts()
    if args.dist:
        for bundle in (release_dir(), release_fallback_dir()):
            if bundle.exists():
                shutil.rmtree(bundle)
                print(f"[OK] removed {bundle}")
        for zip_path in (release_zip(), release_fallback_zip()):
            if zip_path.exists():
                zip_path.unlink()
                print(f"[OK] removed {zip_path}")
        legacy_plugin_dir = RELEASE_ROOT / "plugin"
        if legacy_plugin_dir.exists():
            shutil.rmtree(legacy_plugin_dir)
            print(f"[OK] removed {legacy_plugin_dir}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(description="Kinal host-only build entrypoint")
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("doctor", help="check host toolchain and paths")
    status = sub.add_parser("status", help="print current build status")
    status.add_argument("--json", action="store_true", help="emit machine-readable JSON")

    fetch = sub.add_parser("fetch", help="fetch third-party dependencies or LLVM bootstrap assets")
    fetch.add_argument("target", choices=["third-party", "llvm-prebuilt", "llvm-src", "llvm", "zig-prebuilt", "zig", "all"], help="what to fetch")
    fetch.add_argument("--update", action="store_true", help="pull the existing Kinal-ThirdParty repository before continuing")

    dev = sub.add_parser("dev", help="build and stage a local host bundle")
    dev.add_argument("--release", action="store_true", help="use release build instead of debug")
    dev.add_argument("--clean", action="store_true", help="wipe the CMake build dir before configure")

    dist = sub.add_parser("dist", help="build and package a host release bundle")
    dist.add_argument("--clean", action="store_true", help="wipe the CMake build dir before configure")

    stdpkg = sub.add_parser("stdpkg", help="rebuild official stdpkg .klib packages")
    stdpkg.add_argument("--compiler", type=str, default="", help="explicit compiler path")
    stdpkg.add_argument("--build", action="store_true", help="build the selected host compiler first")
    stdpkg.add_argument("--release", action="store_true", help="use the release host compiler")
    stdpkg.add_argument("--clean", action="store_true", help="wipe the CMake build dir before configure when combined with --build")

    test = sub.add_parser("test", help="run tests with the staged compiler")
    test.add_argument("--compiler", type=str, default="", help="explicit compiler path")
    test.add_argument("--build", action="store_true", help="build before running tests")
    test.add_argument("--release", action="store_true", help="use the release staged compiler")
    test.add_argument("--full", action="store_true", help="run the full legacy regression manifest")

    clean = sub.add_parser("clean", help="remove generated output")
    clean.add_argument("--dist", action="store_true", help="also remove the current host release bundle")

    export_gui = sub.add_parser("export-gui-locales", help="write default GUI locale JSON files")
    export_gui.add_argument("--out-dir", type=str, default="", help="optional output directory for GUI locale files")

    sub.add_parser("gui", help="launch the Tk build dashboard")
    sub.add_parser("open-artifacts", help="open the artifacts directory in the host file explorer")

    selfhost = sub.add_parser("selfhost", help="build selfhost stage0 and optionally run selfhost regressions")
    selfhost.add_argument("--clean", action="store_true", help="rebuild the release bundle before preparing stage0")
    selfhost.add_argument("--test", action="store_true", help="run selfhost regression tests after building stage0")

    selfhost_bootstrap = sub.add_parser("selfhost-bootstrap", help="build stage0 and advance bootstrap stages")
    selfhost_bootstrap.add_argument("--target", type=float, default=98.0, help="target completion percentage")
    selfhost_bootstrap.add_argument("--max-stage", type=int, default=3, help="maximum stage to build")
    selfhost_bootstrap.add_argument("--bootstrap-backend", choices=["host", "self"], default="host", help="backend used for next-stage compiler builds")
    selfhost_bootstrap.add_argument("--metric-backend", choices=["host", "self"], default="self", help="backend used for completion measurement")
    selfhost_bootstrap.add_argument("--clean", action="store_true", help="rebuild stage0 and wipe bootstrap output first")

    return ap


def main() -> int:
    args = build_parser().parse_args()
    if args.cmd == "doctor":
        return cmd_doctor(args)
    if args.cmd == "status":
        return cmd_status(args)
    if args.cmd == "fetch":
        return cmd_fetch(args)
    if args.cmd == "dev":
        return cmd_dev(args)
    if args.cmd == "dist":
        return cmd_dist(args)
    if args.cmd == "stdpkg":
        return cmd_stdpkg(args)
    if args.cmd == "test":
        return cmd_test(args)
    if args.cmd == "clean":
        return cmd_clean(args)
    if args.cmd == "export-gui-locales":
        return cmd_export_gui_locales(args)
    if args.cmd == "gui":
        from infra.scripts.kinal_gui import launch_gui
        launch_gui()
        return 0
    if args.cmd == "open-artifacts":
        return cmd_open_artifacts(args)
    if args.cmd == "selfhost":
        return cmd_selfhost(args)
    if args.cmd == "selfhost-bootstrap":
        return cmd_selfhost_bootstrap(args)
    raise SystemExit(f"unknown command: {args.cmd}")


if __name__ == "__main__":
    raise SystemExit(main())
