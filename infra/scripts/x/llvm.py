from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

from .context import (
    LLVM_LEGACY_PREBUILT_ROOT,
    LLVM_PREBUILT_CACHE_ROOT,
    is_windows,
)


def llvm_candidate_roots() -> list[Path]:
    roots: list[Path] = []
    if LLVM_PREBUILT_CACHE_ROOT.exists():
        roots.append(LLVM_PREBUILT_CACHE_ROOT)
    if LLVM_LEGACY_PREBUILT_ROOT.exists():
        roots.append(LLVM_LEGACY_PREBUILT_ROOT)
    return roots


def detect_llvm_dir() -> Path:
    candidates: list[Path] = []
    for env_name in ("LLVM_DIR", "KN_LLVM_DIR"):
        raw = os.environ.get(env_name, "").strip()
        if raw:
            candidates.append(Path(raw).expanduser())

    for root in llvm_candidate_roots():
        for child in root.iterdir():
            cand = child / "lib" / "cmake" / "llvm"
            if cand.exists():
                candidates.append(cand)

    if not is_windows():
        for tool in ("llvm-config", "llvm-config-21", "llvm-config-19", "llvm-config-18", "llvm-config-17"):
            tool_path = shutil.which(tool)
            if not tool_path:
                continue
            try:
                out = subprocess.check_output([tool_path, "--cmakedir"], text=True).strip()
            except Exception:
                continue
            if out:
                candidates.append(Path(out).expanduser())

    for cand in candidates:
        if cand.exists():
            return cand.resolve()

    raise SystemExit(
        "LLVM_DIR was not found. Set LLVM_DIR/KN_LLVM_DIR, install a system LLVM, "
        "or run `python x.py fetch llvm-prebuilt`."
    )


def llvm_bin_dir(llvm_dir: Path) -> Path:
    return llvm_dir.resolve().parents[2] / "bin"


def llvm_lib_dir(llvm_dir: Path) -> Path:
    return llvm_dir.resolve().parents[1]


def resolve_cmake_env() -> tuple[dict[str, str], Path]:
    env = os.environ.copy()
    llvm_dir = detect_llvm_dir()
    env["LLVM_DIR"] = str(llvm_dir)
    return env, llvm_dir


def preferred_cmake_compilers(llvm_dir: Path) -> tuple[Path | None, Path | None]:
    llvm_bin = llvm_bin_dir(llvm_dir)
    if is_windows():
        c = llvm_bin / "clang.exe"
        cxx = llvm_bin / "clang++.exe"
    else:
        c = llvm_bin / "clang"
        cxx = llvm_bin / "clang++"
    if c.exists() and cxx.exists():
        return c, cxx
    return None, None
