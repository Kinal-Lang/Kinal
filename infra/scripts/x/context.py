from __future__ import annotations

import platform
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
ARTIFACTS_ROOT = ROOT / "artifacts"
OUT = ROOT / "out"
BUILD_ROOT = OUT / "build"
STAGE_ROOT = OUT / "stage"
TEST_ROOT = OUT / "test"
SELFHOST_OUT = OUT / "selfhost"
RELEASE_ROOT = ARTIFACTS_ROOT / "release"

THIRD_PARTY_ROOT = ROOT.parent / "Kinal-ThirdParty"
THIRD_PARTY_REPO_URL = "https://github.com/Kinal-Lang/Kinal-ThirdParty/"
THIRD_PARTY_CACHE_ROOT = THIRD_PARTY_ROOT / ".cache"

LLVM_ROOT = THIRD_PARTY_ROOT / "llvm"
LLVM_MANIFESTS_ROOT = LLVM_ROOT / "manifests"
LLVM_SRC_ROOT = LLVM_ROOT / "src"
LLVM_PREBUILT_CACHE_ROOT = THIRD_PARTY_CACHE_ROOT / "llvm" / "prebuilt"
LLVM_LEGACY_PREBUILT_ROOT = LLVM_ROOT / "prebuilt"

VERSION_FILE = ROOT / "VERSION"

LOCALES_DIR = ROOT / "infra" / "assets" / "locales"
BUILD_GUI_LOCALES_DIR = ROOT / "infra" / "assets" / "locales.build-gui"
RUNTIME_DIR = ROOT / "libs" / "runtime"
STDPKG_DIR = ROOT / "libs" / "std"
RUNTIME_SRC = RUNTIME_DIR / "src" / "kn_runtime.c"
CHKSTK_SRC = RUNTIME_DIR / "src" / "kn_chkstk.c"
MATH_SRC = RUNTIME_DIR / "src" / "kn_math.c"
RUNTIME_INCLUDE = RUNTIME_DIR / "include"

SELFHOST_SUPPORT_DIR = ROOT / "libs" / "selfhost" / "support"
SELFHOST_UTIL_HOSTLIB_SRC = SELFHOST_SUPPORT_DIR / "kn_util_hostlib.c"
SELFHOST_APP_DIR = ROOT / "libs" / "selfhost"

CIVETWEB_DIR = THIRD_PARTY_ROOT / "civetweb"
CIVETWEB_INCLUDE = CIVETWEB_DIR / "include"
CIVETWEB_SRC = CIVETWEB_DIR / "src" / "civetweb.c"
CIVETWEB_MANIFESTS_ROOT = CIVETWEB_DIR / "manifests"
CIVETWEB_PREBUILT_CACHE_ROOT = THIRD_PARTY_CACHE_ROOT / "civetweb" / "artifacts"
CIVETWEB_LEGACY_PREBUILT_ROOT = CIVETWEB_DIR / "prebuilt"
CIVETWEB_VERSION_FILE = CIVETWEB_DIR / "VERSION.txt"
CIVETWEB_SRC_ARCHIVE_URL = "https://github.com/civetweb/civetweb/archive/{commit}.zip"
WEB_BRIDGE_SRC = RUNTIME_DIR / "src" / "kn_web_civet.c"
IO_WEB_NATIVE_TARGETS = ("win-x64", "linux-x64", "linux-arm64")


def host_tag() -> str:
    system = platform.system().lower()
    machine = platform.machine().lower()
    if system == "windows":
        if machine in ("amd64", "x86_64"):
            return "win-x64"
        if machine in ("x86", "i386", "i686"):
            return "win-x86"
    if system == "linux":
        if machine in ("x86_64", "amd64"):
            return "linux-x64"
        if machine in ("aarch64", "arm64"):
            return "linux-arm64"
    if system == "darwin":
        if machine in ("x86_64", "amd64"):
            return "macos-x64"
        if machine in ("aarch64", "arm64"):
            return "macos-arm64"
    raise SystemExit(f"unsupported host platform: system={platform.system()} arch={platform.machine()}")


def is_windows() -> bool:
    return host_tag().startswith("win-")


def exe_name(name: str) -> str:
    return f"{name}.exe" if is_windows() else name


def build_dir(build_type: str) -> Path:
    return BUILD_ROOT / f"host-{build_type.lower()}"


def stage_dir(build_type: str) -> Path:
    return STAGE_ROOT / f"host-{build_type.lower()}"


def release_dir() -> Path:
    return RELEASE_ROOT / f"Kinal-{host_tag()}"


def release_zip() -> Path:
    return RELEASE_ROOT / f"Kinal-{host_tag()}.zip"


def release_fallback_dir() -> Path:
    return RELEASE_ROOT / f"Kinal-{host_tag()}.next"


def release_fallback_zip() -> Path:
    return RELEASE_ROOT / f"Kinal-{host_tag()}.next.zip"




def release_bundle_ready() -> bool:
    bundle = release_dir()
    required = [
        bundle / exe_name("kinal"),
        bundle / exe_name("kinalvm"),
        bundle / "kinal-host.lib",
        bundle / "llvm" / "lib" / ("LLVM-C.lib" if is_windows() else "libLLVM-C.so"),
    ]
    return all(path.exists() for path in required)


def read_version(key: str = "kinal") -> str:
    """Read a version from the unified VERSION file by key (kinal or kinalvm)."""
    for line in VERSION_FILE.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line.startswith(f"{key}="):
            return line.split("=", 1)[1].strip()
    raise ValueError(f"Version key '{key}' not found in VERSION")
