from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path

from .context import ROOT, host_tag, is_windows
from .util import run

ZIG_VERSION = "0.14.1"


def host_default_zig_url() -> str:
    host = host_tag()
    if host == "win-x64":
        return f"https://ziglang.org/download/{ZIG_VERSION}/zig-windows-x86_64-{ZIG_VERSION}.zip"
    if host == "linux-x64":
        return f"https://ziglang.org/download/{ZIG_VERSION}/zig-linux-x86_64-{ZIG_VERSION}.tar.xz"
    if host == "linux-arm64":
        return f"https://ziglang.org/download/{ZIG_VERSION}/zig-linux-aarch64-{ZIG_VERSION}.tar.xz"
    if host == "macos-arm64":
        return f"https://ziglang.org/download/{ZIG_VERSION}/zig-macos-aarch64-{ZIG_VERSION}.tar.xz"
    if host == "macos-x64":
        return f"https://ziglang.org/download/{ZIG_VERSION}/zig-macos-x86_64-{ZIG_VERSION}.tar.xz"
    raise SystemExit(f"unsupported host platform for Zig bootstrap: {host}")


def user_cache_root() -> Path:
    override = os.environ.get("KINAL_TOOLCHAIN_CACHE", "").strip()
    if override:
        return Path(override).expanduser()
    if is_windows():
        base = os.environ.get("LOCALAPPDATA", "").strip()
        if base:
            return Path(base) / "Kinal" / "toolchains"
        return Path.home() / "AppData" / "Local" / "Kinal" / "toolchains"
    xdg = os.environ.get("XDG_CACHE_HOME", "").strip()
    if xdg:
        return Path(xdg) / "kinal" / "toolchains"
    return Path.home() / ".cache" / "kinal" / "toolchains"


def zig_cache_root() -> Path:
    return user_cache_root() / "zig" / f"{host_tag()}-{ZIG_VERSION}"


def zig_binary_in(root: Path) -> Path:
    if is_windows():
        return root / "zig.exe"
    return root / "zig"


def common_zig_candidates() -> list[Path]:
    candidates: list[Path] = []
    for env_name in ("KINAL_LINKER_ZIG", "KINAL_ZIG", "ZIG"):
        raw = os.environ.get(env_name, "").strip()
        if raw:
            candidates.append(Path(raw).expanduser())
    path_hit = shutil.which("zig.exe") or shutil.which("zig")
    if path_hit:
        candidates.append(Path(path_hit))
    if is_windows():
        candidates.extend(
            [
                Path(r"C:\Program Files\Zig\zig.exe"),
                Path(r"C:\Zig\zig.exe"),
                Path(r"K:\Zig\zig.exe"),
            ]
        )
    else:
        candidates.extend([Path("/usr/local/bin/zig"), Path("/usr/bin/zig")])
    candidates.append(zig_binary_in(zig_cache_root()))
    return candidates


def detect_zig_path() -> Path | None:
    for candidate in common_zig_candidates():
        if candidate and candidate.exists():
            return candidate.resolve()
    return None


def ensure_zig_available(*, fetch: bool = False) -> Path | None:
    zig = detect_zig_path()
    if zig or not fetch:
        return zig
    script = ROOT / "infra" / "toolchains" / "setup_zig.py"
    if not script.exists():
        raise SystemExit(f"missing Zig bootstrap script: {script}")
    run([sys.executable, script, "--fetch-prebuilt"])
    return detect_zig_path()
