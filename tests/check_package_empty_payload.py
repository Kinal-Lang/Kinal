"""Trusted empty, text, and binary payloads must survive package roundtrips."""
from __future__ import annotations

import argparse
import json
import struct
import subprocess
import tempfile
from pathlib import Path


def read_archive(path: Path) -> tuple[bytes, dict[str, bytes]]:
    data = path.read_bytes()
    assert data[:8] == b"KNKLIB1\0", path
    version, producer_size, manifest_size, count = struct.unpack_from("<IIII", data, 8)
    assert version == 1
    cursor = 24 + producer_size
    manifest = data[cursor:cursor + manifest_size]
    cursor += manifest_size
    payload = {}
    for _ in range(count):
        path_size, size = struct.unpack_from("<IQ", data, cursor)
        cursor += 12
        name = data[cursor:cursor + path_size].decode().replace("\\", "/")
        cursor += path_size
        assert name not in payload
        payload[name] = data[cursor:cursor + size]
        cursor += size
    assert cursor == len(data)
    return manifest, payload


def run(compiler: Path, work: Path) -> None:
    def invoke(*arguments: str | Path) -> str:
        command = [str(compiler), "pkg", *map(str, arguments)]
        process = subprocess.run(command, capture_output=True, text=True, timeout=30)
        assert process.returncode == 0, (command, process.returncode, process.stdout, process.stderr)
        return (process.stdout + process.stderr).replace("\r\n", "\n")

    for name, payload in (
        ("empty-only", {"empty.txt": b""}),
        ("mixed", {"assets/empty.txt": b"", "notes.txt": "payload café\n".encode(),
                   "assets/data.bin": bytes(range(256)) * 513}),
    ):
        package = work / name
        package.mkdir()
        manifest = package / "package.knpkg.json"
        manifest.write_text(json.dumps({"kind": "package", "name": "Payload.Empty",
                                        "version": "1.0", "source_root": "."}) + "\n",
                            encoding="utf-8")
        expected_manifest = manifest.read_bytes()
        for relative, contents in payload.items():
            path = package / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(contents)
        archive = work / f"{name}.klib"
        invoke("build", "--manifest", manifest, "-o", archive)
        assert read_archive(archive) == (expected_manifest, payload)
        info = invoke("info", archive)
        assert f"  Sources: {len(payload)}\n" in info, info
        assert f"  SourceBytes: {sum(map(len, payload.values()))}\n" in info, info
        recovered = work / f"{name}-unpacked"
        invoke("unpack", archive, "-o", recovered)
        actual = {path.relative_to(recovered).as_posix(): path.read_bytes()
                  for path in recovered.rglob("*") if path.is_file()}
        assert actual == {"package.knpkg.json": expected_manifest, **payload}
        repacked = work / f"{name}-repacked.klib"
        invoke("build", "--manifest", recovered, "-o", repacked)
        assert read_archive(repacked) == (expected_manifest, payload)
        print(f"[OK] {name}: build, info, unpack, repack")
    print("package empty payload checks passed: 2")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="kn-empty-payload-") as directory:
        run(args.compiler.resolve(), Path(directory))


if __name__ == "__main__":
    main()
