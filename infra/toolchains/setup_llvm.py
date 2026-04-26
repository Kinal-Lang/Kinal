#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
THIRD_PARTY_ROOT = ROOT.parent / "Kinal-ThirdParty"
LLVM_ROOT = THIRD_PARTY_ROOT / "llvm"
PREBUILT_ROOT = THIRD_PARTY_ROOT / ".cache" / "llvm" / "prebuilt"
SRC_ROOT = LLVM_ROOT / "src"
LLVM_PROJECT_DIR = SRC_ROOT / "llvm-project"
LLVM_RELEASE = "21.1.8"
LLVM_BRANCH = "release/21.x"
LLVM_SRC_REPO = "https://github.com/llvm/llvm-project.git"


def host_default_prebuilt_url() -> str:
    system = platform.system().lower()
    machine = platform.machine().lower()
    base = "https://github.com/llvm/llvm-project/releases/download/"

    if system == "windows":
        if machine in ("amd64", "x86_64"):
            return f"{base}llvmorg-{LLVM_RELEASE}/clang+llvm-{LLVM_RELEASE}-x86_64-pc-windows-msvc.tar.xz"
        if machine in ("arm64", "aarch64"):
            return f"{base}llvmorg-{LLVM_RELEASE}/clang+llvm-{LLVM_RELEASE}-aarch64-pc-windows-msvc.tar.xz"
        raise SystemExit(f"unsupported Windows host architecture for LLVM prebuilt: {platform.machine()}")

    if system == "linux":
        if machine in ("amd64", "x86_64"):
            return f"{base}llvmorg-{LLVM_RELEASE}/LLVM-{LLVM_RELEASE}-Linux-X64.tar.xz"
        if machine in ("arm64", "aarch64"):
            return f"{base}llvmorg-{LLVM_RELEASE}/LLVM-{LLVM_RELEASE}-Linux-ARM64.tar.xz"
        raise SystemExit(f"unsupported Linux host architecture for LLVM prebuilt: {platform.machine()}")

    if system == "darwin":
        if machine in ("amd64", "x86_64"):
            return f"{base}llvmorg-{LLVM_RELEASE}/LLVM-{LLVM_RELEASE}-macOS-X64.tar.xz"
        if machine in ("arm64", "aarch64"):
            return f"{base}llvmorg-{LLVM_RELEASE}/LLVM-{LLVM_RELEASE}-macOS-ARM64.tar.xz"
        raise SystemExit(f"unsupported macOS host architecture for LLVM prebuilt: {platform.machine()}")

    raise SystemExit(f"unsupported host platform: system={platform.system()} arch={platform.machine()}")


def run(cmd: list[str], cwd: Path = ROOT) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.check_call(cmd, cwd=str(cwd))


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def format_size(size: int) -> str:
    units = ["B", "KB", "MB", "GB", "TB"]
    value = float(size)
    unit = units[0]
    for unit in units:
        if value < 1024.0 or unit == units[-1]:
            break
        value /= 1024.0
    if unit == "B":
        return f"{int(value)} {unit}"
    return f"{value:.1f} {unit}"


def extract_archive(archive: Path, dest: Path) -> None:
    print(f"[INFO] extracting {archive.name} into {dest}", flush=True)
    suffixes = archive.suffixes
    if suffixes[-2:] == [".tar", ".xz"] or archive.name.endswith(".tar.xz"):
        with tarfile.open(archive, "r:xz") as tf:
            members = tf.getmembers()
            total = len(members)
            last_report = 0.0
            last_bucket = -1
            base = dest.resolve()
            for member in members:
                member_name = member.name.replace("\\", "/")
                if Path(member_name).is_absolute():
                    raise SystemExit(f"archive entry uses an absolute path: {member.name}")
                target = (dest / member_name).resolve()
                try:
                    common = os.path.commonpath([str(base), str(target)])
                except ValueError as exc:
                    raise SystemExit(f"archive entry escapes destination: {member.name}") from exc
                if common != str(base):
                    raise SystemExit(f"archive entry escapes destination: {member.name}")
                if member.issym() or member.islnk():
                    link_name = member.linkname.replace("\\", "/")
                    if Path(link_name).is_absolute():
                        raise SystemExit(f"archive link escapes destination: {member.name} -> {member.linkname}")
                    resolved_link = (target.parent / link_name).resolve()
                    try:
                        common = os.path.commonpath([str(base), str(resolved_link)])
                    except ValueError as exc:
                        raise SystemExit(f"archive link escapes destination: {member.name} -> {member.linkname}") from exc
                    if common != str(base):
                        raise SystemExit(f"archive link escapes destination: {member.name} -> {member.linkname}")
                elif not (member.isdir() or member.isfile()):
                    raise SystemExit(f"archive entry type is not allowed: {member.name}")
            for index, member in enumerate(members, start=1):
                tf.extract(member, dest)
                now = time.monotonic()
                if total:
                    percent = index / total * 100.0
                    bucket = int(percent // 5)
                    if bucket > last_bucket or now - last_report >= 2.0 or index == total:
                        print(
                            f"[INFO] extract progress: {percent:5.1f}% ({index} / {total})",
                            flush=True,
                        )
                        last_report = now
                        last_bucket = bucket
        print(f"[OK] extracted {archive.name}", flush=True)
        return
    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as zf:
            members = zf.infolist()
            total = len(members)
            last_report = 0.0
            last_bucket = -1
            base = dest.resolve()
            for index, member in enumerate(members, start=1):
                member_name = member.filename.replace("\\", "/")
                if Path(member_name).is_absolute():
                    raise SystemExit(f"archive entry uses an absolute path: {member.filename}")
                target = (dest / member_name).resolve()
                try:
                    common = os.path.commonpath([str(base), str(target)])
                except ValueError as exc:
                    raise SystemExit(f"archive entry escapes destination: {member.filename}") from exc
                if common != str(base):
                    raise SystemExit(f"archive entry escapes destination: {member.filename}")
                if member.is_dir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with zf.open(member) as src, target.open("wb") as fh:
                        shutil.copyfileobj(src, fh)
                    mode = (member.external_attr >> 16) & 0o777
                    if mode:
                        target.chmod(mode)
                now = time.monotonic()
                if total:
                    percent = index / total * 100.0
                    bucket = int(percent // 5)
                    if bucket > last_bucket or now - last_report >= 2.0 or index == total:
                        print(
                            f"[INFO] extract progress: {percent:5.1f}% ({index} / {total})",
                            flush=True,
                        )
                        last_report = now
                        last_bucket = bucket
        print(f"[OK] extracted {archive.name}", flush=True)
        return
    raise SystemExit(f"unsupported archive format: {archive.name}")


def download(url: str, dest: Path) -> None:
    print(f"[INFO] downloading {url}", flush=True)
    temp = dest.with_name(f"{dest.name}.tmp")
    temp.unlink(missing_ok=True)
    try:
        with urllib.request.urlopen(url, timeout=30) as resp, temp.open("wb") as fh:
            total = resp.headers.get("Content-Length")
            total_size = int(total) if total and total.isdigit() else 0
            chunk_size = 1024 * 1024
            downloaded = 0
            last_report = 0.0
            while True:
                chunk = resp.read(chunk_size)
                if not chunk:
                    break
                fh.write(chunk)
                downloaded += len(chunk)
                now = time.monotonic()
                if total_size and (now - last_report >= 0.5 or downloaded == total_size):
                    percent = downloaded / total_size * 100.0
                    print(
                        f"[INFO] download progress: {percent:5.1f}% ({format_size(downloaded)} / {format_size(total_size)})",
                        flush=True,
                    )
                    last_report = now
                elif not total_size and now - last_report >= 0.5:
                    print(f"[INFO] download progress: {format_size(downloaded)}", flush=True)
                    last_report = now
        temp.replace(dest)
    except (OSError, urllib.error.URLError) as exc:
        temp.unlink(missing_ok=True)
        raise SystemExit(f"download failed: {url}: {exc}") from exc
    print(f"[OK] download complete: {dest}", flush=True)


def fetch_prebuilt(url: str) -> None:
    ensure_dir(PREBUILT_ROOT)
    archive = PREBUILT_ROOT / Path(url).name
    print(f"[INFO] prebuilt cache root: {PREBUILT_ROOT}", flush=True)
    if not archive.exists():
        download(url, archive)
    else:
        print(f"[INFO] archive already present: {archive}", flush=True)
    extract_archive(archive, PREBUILT_ROOT)
    print(f"[OK] prebuilt LLVM ready under {PREBUILT_ROOT}", flush=True)


def fetch_src(depth: int) -> None:
    ensure_dir(SRC_ROOT)
    if LLVM_PROJECT_DIR.exists():
        print(f"[INFO] llvm-project already exists: {LLVM_PROJECT_DIR}", flush=True)
        return
    cmd = ["git", "clone", "--branch", LLVM_BRANCH]
    if depth > 0:
        cmd.extend(["--depth", str(depth)])
    cmd.extend([LLVM_SRC_REPO, str(LLVM_PROJECT_DIR)])
    run(cmd)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Bootstrap local LLVM dependencies for Kinal.")
    parser.add_argument("--fetch-prebuilt", action="store_true", help="download and extract the host LLVM toolchain")
    parser.add_argument("--fetch-src", action="store_true", help="clone llvm-project source into Kinal-ThirdParty/llvm/src")
    parser.add_argument("--prebuilt-url", default=host_default_prebuilt_url(), help="override the prebuilt archive URL")
    parser.add_argument("--src-depth", type=int, default=1, help="clone depth for llvm-project source")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.fetch_prebuilt and not args.fetch_src:
        print("nothing selected; use --fetch-prebuilt and/or --fetch-src", flush=True)
        return 1
    if args.fetch_prebuilt:
        fetch_prebuilt(args.prebuilt_url)
    if args.fetch_src:
        fetch_src(args.src_depth)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
