"""Archive collection must be complete or fail without producing an archive."""
from __future__ import annotations

import argparse
import json
import struct
import subprocess
import tempfile
from pathlib import Path


def run(compiler: Path, work: Path) -> None:
    checks = 0
    for name, count, nested, empty in [
        ("root-at-cap", 256, False, False),
        ("nested-at-cap", 256, True, False),
        ("root-over-cap", 257, False, False),
        ("nested-over-cap", 257, True, False),
        ("empty-subdirectory", 1, True, True),
    ]:
        root = work / name
        root.mkdir(parents=True)
        manifest = root / "package.knpkg.json"
        manifest.write_text(json.dumps({"kind": "package", "name": "Payload.Test", "version": "1.0", "source_root": "."}))
        directory = root / "nested" / "deep" if nested else root
        directory.mkdir(parents=True, exist_ok=True)
        for index in range(count):
            (directory / f"file-{index:03}.txt").write_text(str(index))
        if empty:
            (root / "empty").mkdir()
        output = work / f"{name}.klib"
        process = subprocess.run([str(compiler), "pkg", "build", "--manifest", str(manifest),
                                  "-o", str(output)], capture_output=True, text=True, timeout=30)
        if count > 256 and process.returncode != 0:
            if output.exists():
                raise AssertionError(f"{name}: overflowing collection left a partial archive: {process}")
        else:
            if process.returncode != 0:
                raise AssertionError(f"{name}: valid collection failed: {process}")
            data = output.read_bytes()
            if data[:8] != b"KNKLIB1\0":
                raise AssertionError(f"{name}: invalid archive header")
            version, producer_size, manifest_size, actual = struct.unpack_from("<IIII", data, 8)
            if version != 1 or actual != count:
                raise AssertionError(f"{name}: expected {count} archived files, got {actual} (version {version})")
            cursor = 24 + producer_size + manifest_size
            contents = {}
            for _ in range(actual):
                path_size, size = struct.unpack_from("<IQ", data, cursor)
                cursor += 12
                path = data[cursor:cursor + path_size].decode().replace("\\", "/")
                cursor += path_size
                contents[path] = data[cursor:cursor + size]
                cursor += size
            prefix = "nested/deep/" if nested else ""
            expected = {f"{prefix}file-{index:03}.txt": str(index).encode() for index in range(count)}
            if cursor != len(data) or contents != expected:
                raise AssertionError(f"{name}: archive payload is incomplete, duplicated, or malformed")
        checks += 1
        print(f"[OK] {name}")
    print(f"package payload checks passed: {checks}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="kn-payload-") as directory:
        run(args.compiler.resolve(), Path(directory))


if __name__ == "__main__":
    main()
