from __future__ import annotations

import argparse
from pathlib import Path

from .x.context import host_tag, read_version, release_dir
from .x.release_ops import sha256_file, tar_directory, zip_directory


POSIX_HOST_PREFIXES = ("linux-", "macos-")


def asset_prefix(version: str, release_host_tag: str) -> str:
    return f"Kinal-{version}-{release_host_tag}"


def write_checksums(paths: list[Path], output: Path) -> Path:
    lines = [f"{sha256_file(path)}  {path.name}" for path in sorted(paths, key=lambda value: value.name)]
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return output


def prepare_assets(version: str, release_host_tag: str, bundle_dir: Path, output_dir: Path) -> list[Path]:
    prefix = asset_prefix(version, release_host_tag)
    archives: list[Path] = []
    output_dir.mkdir(parents=True, exist_ok=True)
    archives.append(zip_directory(bundle_dir, output_dir / f"{prefix}.zip", root_name=prefix))
    if release_host_tag.startswith(POSIX_HOST_PREFIXES):
        archives.append(tar_directory(bundle_dir, output_dir / f"{prefix}.tar.gz", compression="gz", root_name=prefix))
        archives.append(tar_directory(bundle_dir, output_dir / f"{prefix}.tar.xz", compression="xz", root_name=prefix))
    checksums = write_checksums(archives, output_dir / f"SHA256SUMS-{release_host_tag}.txt")
    return archives + [checksums]


def merge_checksums(input_dir: Path, output: Path) -> Path:
    checksum_files = sorted(input_dir.rglob("SHA256SUMS-*.txt"))
    if not checksum_files:
        raise SystemExit(f"no checksum manifests found under {input_dir}")
    lines: list[str] = []
    seen: set[str] = set()
    for checksum_file in checksum_files:
        for raw_line in checksum_file.read_text(encoding="utf-8").splitlines():
            line = raw_line.strip()
            if not line or line in seen:
                continue
            seen.add(line)
            lines.append(line)
    lines.sort(key=lambda line: line.split(None, 1)[1] if "  " in line else line)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return output


def cmd_prepare(args: argparse.Namespace) -> int:
    version = args.version or read_version()
    release_host_tag = args.host_tag or host_tag()
    bundle_dir = args.bundle_dir.resolve() if args.bundle_dir else release_dir()
    if not bundle_dir.is_dir():
        raise SystemExit(f"release bundle directory does not exist: {bundle_dir}")
    output_dir = args.output_dir.resolve()
    created = prepare_assets(version, release_host_tag, bundle_dir, output_dir)
    for path in created:
        print(f"[OK] created: {path}")
    return 0


def cmd_merge(args: argparse.Namespace) -> int:
    merged = merge_checksums(args.input_dir.resolve(), args.output.resolve())
    print(f"[OK] merged checksums: {merged}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Prepare versioned release archives and checksum manifests")
    sub = parser.add_subparsers(dest="cmd", required=True)

    prepare = sub.add_parser("prepare", help="create versioned archives and per-host checksums")
    prepare.add_argument("--version", help="release version, defaults to VERSION:kinal")
    prepare.add_argument("--host-tag", help="release host tag, defaults to current host")
    prepare.add_argument("--bundle-dir", type=Path, help="bundle directory produced by x.py dist")
    prepare.add_argument("--output-dir", type=Path, required=True, help="directory for generated release assets")
    prepare.set_defaults(func=cmd_prepare)

    merge = sub.add_parser("merge", help="merge per-host checksum manifests into one file")
    merge.add_argument("--input-dir", type=Path, required=True, help="directory containing SHA256SUMS-*.txt")
    merge.add_argument("--output", type=Path, required=True, help="output checksum file")
    merge.set_defaults(func=cmd_merge)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())