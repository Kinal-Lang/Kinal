from __future__ import annotations

import glob
import os
import shutil
import tempfile
import zipfile
from pathlib import Path

from .context import (
    CJSON_DIR,
    CJSON_INCLUDE,
    CJSON_LEGACY_PREBUILT_ROOT,
    CJSON_PREBUILT_CACHE_ROOT,
    CJSON_SRC,
    CJSON_VERSION_FILE,
    CHKSTK_SRC,
    CIVETWEB_DIR,
    CIVETWEB_INCLUDE,
    CIVETWEB_LEGACY_PREBUILT_ROOT,
    CIVETWEB_PREBUILT_CACHE_ROOT,
    CIVETWEB_SRC,
    CIVETWEB_SRC_ARCHIVE_URL,
    CIVETWEB_VERSION_FILE,
    IO_WEB_NATIVE_TARGETS,
    IO_JSON_NATIVE_TARGETS,
    JSON_BRIDGE_SRC,
    MATH_SRC,
    ROOT,
    RUNTIME_DIR,
    RUNTIME_INCLUDE,
    RUNTIME_SRC,
    SELFHOST_UTIL_HOSTLIB_SRC,
    WEB_BRIDGE_SRC,
    exe_name,
    host_tag,
    is_windows,
)
from .llvm import llvm_lib_dir
from .zig import detect_zig_path, ensure_zig_available
from .util import copy_tree, download_file, extract_zip_safely, run, write_text


def windows_drive_roots() -> list[Path]:
    roots: list[Path] = []
    for code in range(ord("A"), ord("Z") + 1):
        root = Path(f"{chr(code)}:\\")
        if root.exists():
            roots.append(root)
    return roots


def _pick_latest_path(paths: list[Path]) -> Path | None:
    existing = [p for p in paths if p.exists()]
    if not existing:
        return None
    existing.sort(key=lambda p: str(p).lower())
    return existing[-1]


def detect_windows_msvc_lib_dir(arch: str) -> Path | None:
    env_root = os.environ.get("VCToolsInstallDir")
    if env_root:
        p = Path(env_root) / "lib" / arch
        if p.is_dir():
            return p

    matches: list[Path] = []
    for root in windows_drive_roots():
        pattern = str(root / "Microsoft Visual Studio" / "*" / "*" / "VC" / "Tools" / "MSVC" / "*" / "lib" / arch)
        matches.extend(Path(m) for m in glob.glob(pattern) if Path(m).is_dir())
    return _pick_latest_path(matches)


def detect_windows_sdk_lib_dir(kind: str, arch: str) -> Path | None:
    env_root = os.environ.get("WindowsSdkDir")
    if env_root:
        lib_root = Path(env_root) / "Lib"
        version = os.environ.get("UCRTVersion") or os.environ.get("WindowsSDKVersion")
        if version:
            p = lib_root / version.strip("\\/") / kind / arch
            if p.is_dir():
                return p
        if lib_root.is_dir():
            matches = [p for p in lib_root.glob(f"*/{kind}/{arch}") if p.is_dir()]
            picked = _pick_latest_path(matches)
            if picked:
                return picked

    matches: list[Path] = []
    for root in windows_drive_roots():
        pattern = str(root / "Windows Kits" / "10" / "Lib" / "*" / kind / arch)
        matches.extend(Path(m) for m in glob.glob(pattern) if Path(m).is_dir())
    return _pick_latest_path(matches)


def copy_windows_hosted_lib_dirs(host_dir: Path, arch: str) -> None:
    lib_dirs = [
        detect_windows_msvc_lib_dir(arch),
        detect_windows_sdk_lib_dir("ucrt", arch),
        detect_windows_sdk_lib_dir("um", arch),
    ]
    seen: set[str] = set()
    for lib_dir in lib_dirs:
        if not lib_dir or not lib_dir.is_dir():
            continue
        for src in sorted(lib_dir.glob("*.lib")):
            name = src.name.lower()
            if name in seen:
                continue
            shutil.copy2(src, host_dir / src.name)
            seen.add(name)


def build_windows_import_lib(def_text: str, out_lib: Path, machine: str, llvm_bin: Path) -> None:
    llvm_lib = llvm_bin / "llvm-lib.exe"
    if not llvm_lib.exists():
        raise SystemExit(f"missing llvm-lib.exe in {llvm_bin}")
    with tempfile.TemporaryDirectory(prefix="kinal-importlib-") as tmp:
        tmp_dir = Path(tmp)
        def_path = tmp_dir / (out_lib.stem + ".def")
        def_path.write_text(def_text, encoding="utf-8")
        run([llvm_lib, "/nologo", f"/machine:{machine}", f"/def:{def_path}", f"/out:{out_lib}"])


def windows_host_arch_names(host: str) -> tuple[str, str]:
    if host == "win-x64":
        return "x64", "x64"
    if host == "win-x86":
        return "x86", "x86"
    if host == "win-arm64":
        return "ARM64", "arm64"
    raise SystemExit(f"unsupported Windows host architecture: {host}")


def build_runtime_for_host(stage_runtime: Path, llvm_bin: Path) -> None:
    stage_runtime.mkdir(parents=True, exist_ok=True)
    copy_tree(RUNTIME_DIR / "src", stage_runtime / "src")
    copy_tree(RUNTIME_INCLUDE, stage_runtime / "include")

    host = host_tag()
    if host.startswith("win-"):
        clang = llvm_bin / "clang.exe"
        if not clang.exists():
            raise SystemExit(f"missing clang.exe in {llvm_bin}")

        machine, lib_arch = windows_host_arch_names(host)
        host_dir = stage_runtime / host
        host_dir.mkdir(parents=True, exist_ok=True)

        run(
            [
                clang,
                "-c",
                str(RUNTIME_SRC),
                "-o",
                str(host_dir / "kn_runtime.obj"),
                "-std=c11",
                "-O2",
                "-ffreestanding",
                "-fno-builtin",
                "-fno-stack-protector",
                "-fno-exceptions",
                "-fno-asynchronous-unwind-tables",
                "-I",
                str(RUNTIME_INCLUDE),
            ]
        )

        run(
            [
                clang,
                "-c",
                str(CHKSTK_SRC),
                "-o",
                str(host_dir / "kn_chkstk.obj"),
                "-std=c11",
                "-O2",
                "-ffreestanding",
                "-fno-builtin",
                "-fno-stack-protector",
                "-fno-exceptions",
                "-fno-asynchronous-unwind-tables",
            ]
        )

        run([clang, "-c", str(MATH_SRC), "-o", str(host_dir / "kn_math.obj"), "-std=c11", "-O2"])

        if SELFHOST_UTIL_HOSTLIB_SRC.exists():
            run(
                [
                    clang,
                    "-c",
                    str(SELFHOST_UTIL_HOSTLIB_SRC),
                    "-o",
                    str(host_dir / "kn_util_hostlib.obj"),
                    "-std=c11",
                    "-O2",
                    "-ffreestanding",
                    "-fno-builtin",
                    "-fno-stack-protector",
                    "-fno-exceptions",
                    "-fno-asynchronous-unwind-tables",
                    "-I",
                    str(RUNTIME_INCLUDE),
                ]
            )

        kernel32_def = """LIBRARY KERNEL32.dll
EXPORTS
  GetStdHandle
  WriteFile
  ReadFile
  ExitProcess
  GetProcessHeap
  HeapAlloc
  HeapFree
  CreateFileA
  CreateFileMappingA
  MapViewOfFile
  UnmapViewOfFile
  CloseHandle
  GetFileSizeEx
  GetCommandLineA
  CreateProcessA
  WaitForSingleObject
  GetExitCodeProcess
  Sleep
  GetTickCount64
  QueryPerformanceCounter
  QueryPerformanceFrequency
  GetSystemTimeAsFileTime
  FileTimeToSystemTime
  FindFirstFileA
  FindNextFileA
  FindClose
  CreateDirectoryA
  RemoveDirectoryA
  DeleteFileA
  GetEnvironmentVariableA
  GetCurrentDirectoryA
    GetModuleFileNameA
  GetModuleHandleA
  GetLastError
  GetConsoleMode
  SetConsoleMode
  SetConsoleOutputCP
  SetConsoleCP
  SetConsoleCursorPosition
  MultiByteToWideChar
  WideCharToMultiByte
"""
        user32_def = """LIBRARY USER32.dll
EXPORTS
  GetAsyncKeyState
  MessageBoxA
  MessageBoxW
  RegisterClassA
  RegisterClassW
  CreateWindowExA
  CreateWindowExW
  ShowWindow
  UpdateWindow
  GetMessageA
  GetMessageW
  TranslateMessage
  DispatchMessageA
  DispatchMessageW
  PostQuitMessage
  DefWindowProcA
  DefWindowProcW
  CallWindowProcW
  LoadCursorA
  LoadCursorW
  MoveWindow
  SendMessageA
  SendMessageW
  SetCapture
  ReleaseCapture
  SetWindowLongPtrW
  SetWindowTextW
  InvalidateRect
  GetClientRect
  FillRect
  GetWindowTextLengthW
  GetWindowTextW
  GetParent
  SetFocus
  EnableWindow
"""
        gdi32_def = """LIBRARY GDI32.dll
EXPORTS
  SetTextColor
  SetBkColor
  CreateSolidBrush
  DeleteObject
  CreateFontA
  CreateFontW
"""
        comctl32_def = """LIBRARY COMCTL32.dll
EXPORTS
  InitCommonControlsEx
"""
        ucrtbase_def = """LIBRARY ucrtbase.dll
EXPORTS
  sin
  cos
  tan
  asin
  acos
  atan
  atan2
  sinh
  cosh
  tanh
  sqrt
  cbrt
  pow
  exp
  log
  log2
  log10
  floor
  ceil
  round
  trunc
  fabs
  fmod
  fmin
  fmax
  copysign
"""
        build_windows_import_lib(kernel32_def, host_dir / "kernel32.lib", machine, llvm_bin)
        build_windows_import_lib(user32_def, host_dir / "user32.lib", machine, llvm_bin)
        build_windows_import_lib(gdi32_def, host_dir / "gdi32.lib", machine, llvm_bin)
        build_windows_import_lib(comctl32_def, host_dir / "comctl32.lib", machine, llvm_bin)
        build_windows_import_lib(ucrtbase_def, host_dir / "ucrtbase.lib", machine, llvm_bin)
        copy_windows_hosted_lib_dirs(host_dir, lib_arch)
        return

    if host.startswith("linux-"):
        cc = llvm_bin / "clang"
        if not cc.exists():
            cc_path = shutil.which("clang") or shutil.which("cc")
            if not cc_path:
                raise SystemExit("clang/cc not found for Linux runtime build")
            cc = Path(cc_path)
        host_dir = stage_runtime / host
        host_dir.mkdir(parents=True, exist_ok=True)
        run([cc, "-c", str(RUNTIME_SRC), "-o", str(host_dir / "kn_runtime.o"), "-std=c11", "-O2", "-D_POSIX_C_SOURCE=200809L", "-I", str(RUNTIME_INCLUDE)])
        run([cc, "-c", str(MATH_SRC), "-o", str(host_dir / "kn_math.o"), "-std=c11", "-O2", "-lm"])
        return

    if host.startswith("macos-"):
        cc = llvm_bin / "clang"
        if not cc.exists():
            cc_path = shutil.which("clang") or shutil.which("cc")
            if not cc_path:
                raise SystemExit("clang/cc not found for macOS runtime build")
            cc = Path(cc_path)
        host_dir = stage_runtime / host
        host_dir.mkdir(parents=True, exist_ok=True)
        run([cc, "-c", str(RUNTIME_SRC), "-o", str(host_dir / "kn_runtime.o"), "-std=c11", "-O2", "-I", str(RUNTIME_INCLUDE)])
        run([cc, "-c", str(MATH_SRC), "-o", str(host_dir / "kn_math.o"), "-std=c11", "-O2"])
        return

    raise SystemExit(f"host runtime packaging is not implemented for {host}")


def civetweb_pinned_commit() -> str:
    if not CIVETWEB_VERSION_FILE.exists():
        raise SystemExit(f"missing CivetWeb version file: {CIVETWEB_VERSION_FILE}")

    lines = CIVETWEB_VERSION_FILE.read_text(encoding="utf-8").splitlines()
    for index, line in enumerate(lines):
        if line.strip() != "Commit:":
            continue
        if index + 1 >= len(lines):
            break
        commit = lines[index + 1].strip()
        if len(commit) == 40 and all(ch in "0123456789abcdefABCDEF" for ch in commit):
            return commit

    raise SystemExit(f"failed to parse pinned CivetWeb commit from {CIVETWEB_VERSION_FILE}")


def ensure_civetweb_source() -> None:
    if (CIVETWEB_INCLUDE / "civetweb.h").exists() and CIVETWEB_SRC.exists():
        return

    commit = civetweb_pinned_commit()
    archive_url = CIVETWEB_SRC_ARCHIVE_URL.format(commit=commit)

    with tempfile.TemporaryDirectory(prefix="kinal-civetweb-") as temp_dir:
        temp_root = Path(temp_dir)
        archive_path = temp_root / f"civetweb-{commit}.zip"
        extract_root = temp_root / "extract"
        extract_root.mkdir(parents=True, exist_ok=True)

        download_file(archive_url, archive_path)
        extract_zip_safely(archive_path, extract_root)

        extracted_roots = sorted(path for path in extract_root.iterdir() if path.is_dir())
        if not extracted_roots:
            raise SystemExit(f"downloaded CivetWeb archive did not contain a root directory: {archive_url}")

        upstream_root = extracted_roots[0]
        include_src = upstream_root / "include"
        src_src = upstream_root / "src"
        if not include_src.exists() or not src_src.exists():
            raise SystemExit(f"downloaded CivetWeb archive is missing include/src: {archive_url}")

        CIVETWEB_INCLUDE.mkdir(parents=True, exist_ok=True)
        (CIVETWEB_DIR / "src").mkdir(parents=True, exist_ok=True)
        shutil.copytree(include_src, CIVETWEB_INCLUDE, dirs_exist_ok=True)
        shutil.copytree(src_src, CIVETWEB_DIR / "src", dirs_exist_ok=True)

    print(f"[OK] bootstrapped CivetWeb source into {CIVETWEB_DIR}")


def civetweb_prebuilt_root() -> Path:
    if CIVETWEB_LEGACY_PREBUILT_ROOT.exists():
        return CIVETWEB_LEGACY_PREBUILT_ROOT
    return CIVETWEB_PREBUILT_CACHE_ROOT


def cjson_prebuilt_root() -> Path:
    if CJSON_LEGACY_PREBUILT_ROOT.exists():
        return CJSON_LEGACY_PREBUILT_ROOT
    return CJSON_PREBUILT_CACHE_ROOT


def ensure_cjson_source() -> None:
    if CJSON_SRC.exists() and (CJSON_INCLUDE / "cJSON.h").exists():
        return
    raise SystemExit(
        f"cJSON source is missing under {CJSON_DIR}; ensure Kinal-ThirdParty provides cjson/include and cjson/src."
    )


def build_civetweb_for_target(target: str, llvm_bin: Path) -> bool:
    ensure_civetweb_source()
    out_dir = civetweb_prebuilt_root() / target
    out_dir.mkdir(parents=True, exist_ok=True)
    if target.startswith("win-"):
        if not is_windows() or host_tag() != target:
            return False
        clang = llvm_bin / "clang.exe"
        llvm_lib = llvm_bin / "llvm-lib.exe"
        if not clang.exists() or not llvm_lib.exists():
            return False

        civet_obj = out_dir / "CivetWeb.obj"
        civet_lib = out_dir / "CivetWeb.lib"
        bridge_obj = out_dir / "kn_web_civet.obj"

        run([clang, "-c", str(CIVETWEB_SRC), "-o", str(civet_obj), "-std=c11", "-O2", "-DNO_SSL", "-DNO_CGI", "-DNO_CACHING", "-I", str(CIVETWEB_INCLUDE)])
        run([llvm_lib, "/nologo", f"/out:{civet_lib}", str(civet_obj)])
        run([clang, "-c", str(WEB_BRIDGE_SRC), "-o", str(bridge_obj), "-std=c11", "-O2", "-I", str(CIVETWEB_INCLUDE), "-I", str(RUNTIME_INCLUDE)])
        return True

    if target.startswith("linux-"):
        zig_target = "x86_64-linux-gnu" if target == "linux-x64" else "aarch64-linux-gnu"
        zig = ensure_zig_available(fetch=True)
        if zig:
            cc_cmd: list[str | Path] = [zig, "cc", "-target", zig_target]
        else:
            clang = llvm_bin / "clang"
            if host_tag().startswith("linux-") and host_tag() == target:
                if not clang.exists():
                    cc_path = shutil.which("clang") or shutil.which("cc")
                    if not cc_path:
                        return False
                    clang = Path(cc_path)
                cc_cmd = [clang]
            else:
                return False

        civet_obj = out_dir / "CivetWeb.o"
        bridge_obj = out_dir / "kn_web_civet.o"

        run(cc_cmd + ["-c", str(CIVETWEB_SRC), "-o", str(civet_obj), "-std=c11", "-O2", "-Wno-error=date-time", "-fPIC", "-pthread", "-D_POSIX_C_SOURCE=200809L", "-DNO_SSL", "-DNO_CGI", "-DNO_CACHING", "-I", str(CIVETWEB_INCLUDE)])
        run(cc_cmd + ["-c", str(WEB_BRIDGE_SRC), "-o", str(bridge_obj), "-std=c11", "-O2", "-Wno-error=date-time", "-fPIC", "-pthread", "-D_POSIX_C_SOURCE=200809L", "-I", str(CIVETWEB_INCLUDE), "-I", str(RUNTIME_INCLUDE)])
        return True

    return False


def build_civetweb_for_available_targets(llvm_bin: Path) -> list[str]:
    built: list[str] = []
    for target in IO_WEB_NATIVE_TARGETS:
        if build_civetweb_for_target(target, llvm_bin):
            built.append(target)
    return built


def build_cjson_for_target(target: str, llvm_bin: Path) -> bool:
    ensure_cjson_source()
    out_dir = cjson_prebuilt_root() / target
    out_dir.mkdir(parents=True, exist_ok=True)

    if target.startswith("win-"):
        if not is_windows():
            return False
        clang = llvm_bin / "clang.exe"
        if not clang.exists():
            return False
        if target == "win-x64":
            triple = "x86_64-pc-windows-msvc"
        elif target == "win-arm64":
            triple = "aarch64-pc-windows-msvc"
        else:
            return False
        cjson_obj = out_dir / "cJSON.obj"
        bridge_obj = out_dir / "kn_json_cjson.obj"
        run([clang, "--target=" + triple, "-c", str(CJSON_SRC), "-o", str(cjson_obj), "-std=c11", "-O2", "-I", str(CJSON_INCLUDE)])
        run([clang, "--target=" + triple, "-c", str(JSON_BRIDGE_SRC), "-o", str(bridge_obj), "-std=c11", "-O2", "-I", str(CJSON_INCLUDE)])
        return True

    if target.startswith("linux-"):
        zig_target = "x86_64-linux-gnu" if target == "linux-x64" else "aarch64-linux-gnu" if target == "linux-arm64" else ""
        if not zig_target:
            return False
        zig = ensure_zig_available(fetch=True)
        if zig:
            cc_cmd: list[str | Path] = [zig, "cc", "-target", zig_target]
        else:
            clang = llvm_bin / "clang"
            if host_tag().startswith("linux-") and host_tag() == target:
                if not clang.exists():
                    cc_path = shutil.which("clang") or shutil.which("cc")
                    if not cc_path:
                        return False
                    clang = Path(cc_path)
                cc_cmd = [clang]
            else:
                return False
        cjson_obj = out_dir / "cJSON.o"
        bridge_obj = out_dir / "kn_json_cjson.o"
        run(cc_cmd + ["-c", str(CJSON_SRC), "-o", str(cjson_obj), "-std=c11", "-O2", "-fPIC", "-I", str(CJSON_INCLUDE)])
        run(cc_cmd + ["-c", str(JSON_BRIDGE_SRC), "-o", str(bridge_obj), "-std=c11", "-O2", "-fPIC", "-I", str(CJSON_INCLUDE)])
        return True

    if target.startswith("macos-"):
        zig_target = "x86_64-macos-none" if target == "macos-x64" else "aarch64-macos-none" if target == "macos-arm64" else ""
        if not zig_target:
            return False
        zig = ensure_zig_available(fetch=True)
        if zig:
            cc_cmd: list[str | Path] = [zig, "cc", "-target", zig_target]
        else:
            if not host_tag().startswith("macos-") or host_tag() != target:
                return False
            clang = llvm_bin / "clang"
            if not clang.exists():
                cc_path = shutil.which("clang") or shutil.which("cc")
                if not cc_path:
                    return False
                clang = Path(cc_path)
            cc_cmd = [clang]
        cjson_obj = out_dir / "cJSON.o"
        bridge_obj = out_dir / "kn_json_cjson.o"
        run(cc_cmd + ["-c", str(CJSON_SRC), "-o", str(cjson_obj), "-std=c11", "-O2", "-fPIC", "-I", str(CJSON_INCLUDE)])
        run(cc_cmd + ["-c", str(JSON_BRIDGE_SRC), "-o", str(bridge_obj), "-std=c11", "-O2", "-fPIC", "-I", str(CJSON_INCLUDE)])
        return True

    return False


def build_cjson_for_available_targets(llvm_bin: Path) -> list[str]:
    built: list[str] = []
    for target in IO_JSON_NATIVE_TARGETS:
        if build_cjson_for_target(target, llvm_bin):
            built.append(target)
    return built


def copy_civetweb_bundle_files(dest: Path) -> None:
    civet_dst = dest / "third_party" / "civetweb"
    civet_dst.mkdir(parents=True, exist_ok=True)
    for name in ("LICENSE.md", "SECURITY.md", "VERSION.txt"):
        src = CIVETWEB_DIR / name
        if src.exists():
            shutil.copy2(src, civet_dst / name)
    prebuilt_root = civetweb_prebuilt_root()
    if not prebuilt_root.exists():
        return
    for prebuilt_src in sorted(prebuilt_root.iterdir()):
        if prebuilt_src.is_dir():
            copy_tree(prebuilt_src, civet_dst / "prebuilt" / prebuilt_src.name)


def copy_cjson_bundle_files(dest: Path) -> None:
    cjson_dst = dest / "third_party" / "cjson"
    cjson_dst.mkdir(parents=True, exist_ok=True)
    for name in ("LICENSE", "VERSION.txt"):
        src = CJSON_DIR / name
        if src.exists():
            shutil.copy2(src, cjson_dst / name)
    prebuilt_root = cjson_prebuilt_root()
    if not prebuilt_root.exists():
        return
    for prebuilt_src in sorted(prebuilt_root.iterdir()):
        if prebuilt_src.is_dir():
            copy_tree(prebuilt_src, cjson_dst / "prebuilt" / prebuilt_src.name)


def sync_io_web_native_assets(stdpkg_dir: Path, targets: list[str] | None = None) -> list[str]:
    copied: list[str] = []
    prebuilt_root = civetweb_prebuilt_root()
    if targets:
        target_names = list(targets)
    else:
        if not prebuilt_root.exists():
            return copied
        target_names = [p.name for p in sorted(prebuilt_root.iterdir()) if p.is_dir()]
    if not target_names:
        return copied

    version_dirs = sorted((stdpkg_dir / "IO.Web").glob("*/"))
    for version_dir in version_dirs:
        if not version_dir.is_dir():
            continue
        for target in target_names:
            prebuilt_src = prebuilt_root / target
            if not prebuilt_src.exists():
                continue
            native_dir = version_dir / "native" / target
            native_dir.mkdir(parents=True, exist_ok=True)
            for src in sorted(prebuilt_src.iterdir()):
                if src.is_file():
                    shutil.copy2(src, native_dir / src.name)
            if target not in copied:
                copied.append(target)
    return copied


def sync_io_json_native_assets(stdpkg_dir: Path, targets: list[str] | None = None) -> list[str]:
    copied: list[str] = []
    prebuilt_root = cjson_prebuilt_root()
    if targets:
        target_names = list(targets)
    else:
        if not prebuilt_root.exists():
            return copied
        target_names = [p.name for p in sorted(prebuilt_root.iterdir()) if p.is_dir()]
    if not target_names:
        return copied

    version_dirs = sorted((stdpkg_dir / "IO.Json").glob("*/"))
    for version_dir in version_dirs:
        if not version_dir.is_dir():
            continue
        for target in target_names:
            prebuilt_src = prebuilt_root / target
            if not prebuilt_src.exists():
                continue
            native_dir = version_dir / "native" / target
            native_dir.mkdir(parents=True, exist_ok=True)
            for src in sorted(prebuilt_src.iterdir()):
                if src.is_file():
                    shutil.copy2(src, native_dir / src.name)
            if target not in copied:
                copied.append(target)
    return copied


def copy_host_linker_tools(stage_linker: Path, llvm_bin: Path) -> None:
    stage_linker.mkdir(parents=True, exist_ok=True)
    host = host_tag()
    if host.startswith("win-"):
        for name in ("llc.exe", "llvm-mc.exe", "llvm-as.exe", "lld-link.exe", "ld.lld.exe", "lld.exe", "llvm-lib.exe"):
            src = llvm_bin / name
            if src.exists():
                shutil.copy2(src, stage_linker / name)
        return

    if host.startswith("linux-"):
        for name in ("ld.lld", "lld"):
            src = llvm_bin / name
            if src.exists():
                shutil.copy2(src, stage_linker / name)
        return

    if host.startswith("macos-"):
        for name in ("ld.lld", "lld"):
            src = llvm_bin / name
            if src.exists():
                shutil.copy2(src, stage_linker / name)


def copy_llvm_runtime_files(dest: Path, llvm_bin: Path, llvm_dir: Path) -> None:
    if is_windows():
        for name in ("LLVM-C.dll", "LTO.dll", "Remarks.dll"):
            src = llvm_bin / name
            if src.exists():
                shutil.copy2(src, dest / name)
        return

    lib_dir = llvm_lib_dir(llvm_dir)
    runtime_dest = dest / "llvm" / "lib"
    runtime_dest.mkdir(parents=True, exist_ok=True)
    copied = False
    host = host_tag()
    patterns = ["libLLVM.so*"]
    if host.startswith("macos-"):
        patterns = ["libLLVM*.dylib", "libLTO*.dylib", "libRemarks*.dylib"]
    for pattern in patterns:
        for src in sorted(lib_dir.glob(pattern)):
            if src.is_file():
                shutil.copy2(src, runtime_dest / src.name)
                copied = True
    if not copied:
        expected = "libLLVM*.dylib" if host.startswith("macos-") else "libLLVM.so*"
        raise SystemExit(f"failed to locate {expected} in {lib_dir}")


def copy_llvm_dev_files(dest: Path, llvm_dir: Path) -> None:
    if is_windows():
        lib_dir = llvm_dir.resolve().parents[1]
        src = lib_dir / "LLVM-C.lib"
        if src.exists():
            target = dest / "llvm" / "lib"
            target.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, target / "LLVM-C.lib")


def write_linux_compiler_launcher(bundle_dir: Path) -> None:
    if not host_tag().startswith("linux-"):
        return

    compiler = bundle_dir / "kinal"
    real_compiler = bundle_dir / "kinal.bin"
    if not compiler.exists():
        raise SystemExit(f"missing compiler binary for launcher generation: {compiler}")

    if real_compiler.exists():
        real_compiler.unlink()
    compiler.rename(real_compiler)

    launcher = """#!/usr/bin/env sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
LLVM_LIB="$HERE/llvm/lib"
if [ -n "${LD_LIBRARY_PATH:-}" ]; then
    export LD_LIBRARY_PATH="$LLVM_LIB:$LD_LIBRARY_PATH"
else
    export LD_LIBRARY_PATH="$LLVM_LIB"
fi
exec "$HERE/kinal.bin" "$@"
"""
    write_text(compiler, launcher)
    compiler.chmod(0o755)


def build_kinal_vm_binary(compiler: Path, bundle_dir: Path) -> Path:
    vm_src = ROOT / "apps" / "kinalvm" / "src" / "Main.kn"
    vm_out = bundle_dir / exe_name("kinalvm")
    cmd: list[str | Path] = [compiler, "build", vm_src, "-o", vm_out]
    if host_tag().startswith("linux-"):
        zig_path = ensure_zig_available(fetch=True) or detect_zig_path()
        if zig_path:
            cmd.extend(["--linker", "zig", "--linker-path", zig_path])
    run(cmd, cwd=bundle_dir)
    return vm_out
