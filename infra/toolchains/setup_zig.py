#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
import sys
import tarfile
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from infra.scripts.x.zig import (  # noqa: E402
    ZIG_VERSION,
    detect_zig_path,
    host_default_zig_url,
    zig_binary_in,
    zig_cache_root,
)


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def download(url: str, dest: Path) -> None:
    print(f"+ download {url}")
    with urllib.request.urlopen(url) as resp, dest.open("wb") as fh:
        shutil.copyfileobj(resp, fh)


def extract_archive(archive: Path, dest: Path) -> None:
    if archive.name.endswith(".tar.xz"):
        with tarfile.open(archive, "r:xz") as tf:
            tf.extractall(dest)
        return
    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(dest)
        return
    raise SystemExit(f"unsupported archive format: {archive.name}")


def fetch_prebuilt(url: str) -> Path:
    cache_root = zig_cache_root()
    ensure_dir(cache_root)
    archive = cache_root / Path(url).name
    if not archive.exists():
        download(url, archive)
    extract_root = cache_root / "_extract"
    if extract_root.exists():
        shutil.rmtree(extract_root)
    extract_root.mkdir(parents=True, exist_ok=True)
    extract_archive(archive, extract_root)
    extracted_roots = sorted(p for p in extract_root.iterdir() if p.is_dir())
    if not extracted_roots:
        raise SystemExit(f"downloaded Zig archive did not contain a root directory: {url}")
    upstream_root = extracted_roots[0]
    zig_bin = zig_binary_in(upstream_root)
    if not zig_bin.exists():
        raise SystemExit(f"downloaded Zig archive is missing binary: {zig_bin}")
    for child in upstream_root.iterdir():
        target = cache_root / child.name
        if target.exists():
            if target.is_dir():
                shutil.rmtree(target)
            else:
                target.unlink()
        shutil.move(str(child), str(target))
    shutil.rmtree(extract_root, ignore_errors=True)
    return zig_binary_in(cache_root)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Bootstrap local Zig dependencies for Kinal.")
    parser.add_argument("--fetch-prebuilt", action="store_true", help="download and extract the host Zig toolchain")
    parser.add_argument("--print-path", action="store_true", help="print the detected Zig binary path")
    parser.add_argument("--prebuilt-url", default=host_default_zig_url(), help="override the host Zig archive URL")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.fetch_prebuilt:
        zig = fetch_prebuilt(args.prebuilt_url)
    else:
        zig = detect_zig_path()
    if args.print_path and zig:
        print(zig)
    if zig:
        print(f"zig ready: {zig}")
        return 0
    print(
        f"zig {ZIG_VERSION} was not found. Use --fetch-prebuilt or install Zig and expose it via PATH/KINAL_ZIG.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
