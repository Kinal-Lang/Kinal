"""Normal KNKLIB1 package CLI roundtrips and source dependency consumption.

This contract uses trusted generated fixtures. It is not an archive security or
malformed-input audit. Producer bytes, embedded manifests, payloads, metadata
and installed-package use are checked independently of archive entry order.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import subprocess
from pathlib import Path


def compiler_producer_path(compiler: Path) -> Path:
    """Archive provenance names the executable behind the packaged Linux launcher."""
    compiler = compiler.resolve()
    with compiler.open("rb") as source:
        header = source.read(4096)
    if (compiler.name == "kinal" and header.startswith(b"#!/usr/bin/env sh\n")
            and b'\nexec "$HERE/kinal.bin" "$@"\n' in header):
        payload = compiler.with_name("kinal.bin")
        assert payload.is_file(), f"packaged compiler payload is missing: {payload}"
        return payload.resolve()
    return compiler


def read_archive(path: Path) -> dict:
    data = path.read_bytes()
    assert data[:8] == b"KNKLIB1\0", path
    version, producer_length, manifest_length, count = struct.unpack_from("<IIII", data, 8)
    assert version == 1
    cursor = 24
    producer = data[cursor:cursor + producer_length].decode("utf-8")
    cursor += producer_length
    manifest = data[cursor:cursor + manifest_length]
    cursor += manifest_length
    payload = {}
    for _ in range(count):
        length, size = struct.unpack_from("<IQ", data, cursor)
        cursor += 12
        name = data[cursor:cursor + length].decode("utf-8").replace("\\", "/")
        cursor += length
        assert name not in payload
        payload[name] = data[cursor:cursor + size]
        cursor += size
    assert cursor == len(data)
    return {"producer": producer, "manifest": manifest, "payload": payload}


def check_package_cli(compiler: Path, stage0: Path, root: Path, out: Path,
                      *, stage0_reference: bool = True, compile_consumers: bool = True) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    logs = out / "logs"
    logs.mkdir(exist_ok=True)
    sequence = 0
    cases = 0
    suffix = ".exe" if os.name == "nt" else ""
    tools = [("stage0", stage0)] if stage0_reference else []
    tools.append(("selfhost", compiler))

    def invoke(label: str, command: list[str | Path], cwd: Path = out) -> subprocess.CompletedProcess:
        nonlocal sequence
        sequence += 1
        arguments = [str(part) for part in command]
        proc = subprocess.run(arguments, cwd=cwd, text=True, capture_output=True, timeout=180)
        (logs / f"{sequence:03d}-{label}.log").write_text(
            "command=" + repr(arguments) + "\ncwd=" + str(cwd) +
            "\nexit=" + str(proc.returncode) + "\n" + proc.stdout + proc.stderr, encoding="utf-8")
        assert proc.returncode == 0, (label, proc.returncode, proc.stdout, proc.stderr)
        return proc

    package = out / "package source"
    package.mkdir(exist_ok=True)
    payload = {
        "src/Acme/Greeter.kn": b"Unit Acme.Greeter; Function int Answer() { Return 42; }\n",
        "assets/native fixture.bin": bytes(range(256)) * 513,
        "assets/empty.txt": b"",
        "assets/notes.txt": "normal UTF-8 payload: café 雪\n".encode(),
        "build/readme.txt": b"build is an ordinary payload directory\n",
        "out/readme.txt": b"out is an ordinary payload directory\n",
    }
    for relative, content in payload.items():
        path = package / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
    # Existing archives and bookkeeping directories are excluded by stage0.
    for relative in ("old.klib", "assets/old.KLIB", ".git/config", ".kinal-cache/cache.txt"):
        path = package / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("previous generated artifact\n")
    metadata = {"kind": "library", "name": "Acme.Greeter", "version": "1.2.3",
                "summary": 'Greeting "helpers"\nwith café 雪', "url": "https://example.test/library",
                "entry": "src/Acme/Greeter.kn", "source_root": "src",
                "modules": ["Acme.Greeter"], "dependencies": ["Acme.Base"]}
    manifest = package / "package.knpkg.json"
    manifest.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    manifest_bytes = manifest.read_bytes()
    total = sum(map(len, payload.values()))

    def verify_archive(path: Path, producer: Path, expected_manifest: bytes = manifest_bytes,
                       expected_payload: dict = payload) -> dict:
        archive = read_archive(path)
        assert Path(archive["producer"]).resolve() == compiler_producer_path(producer), archive["producer"]
        assert archive["manifest"] == expected_manifest
        assert archive["payload"] == expected_payload, (path, archive["payload"].keys())
        return archive

    for label, producer in tools:
        source_package = package
        source_manifest = manifest
        tested_payload = payload
        archive = out / (label + " nested archives") / "Acme.Greeter.klib"
        invoke(label + "-build", [producer, "pkg", "build", "--manifest", source_package, "-o", archive])
        verify_archive(archive, producer, expected_payload=tested_payload)
        for consumer_label, consumer in tools:
            info = invoke(label + "-info-" + consumer_label, [consumer, "pkg", "info", archive])
            expected = ("[Klib]\n  File: " + str(archive) + "\n  Compiler: " + str(compiler_producer_path(producer)) +
                        f"\n  Sources: {len(tested_payload)}\n  SourceBytes: {sum(map(len, tested_payload.values()))}\n")
            assert (info.stdout + info.stderr).replace("\r\n", "\n") == expected
            recovered = out / (label + " unpack by " + consumer_label)
            invoke(label + "-unpack-" + consumer_label,
                   [consumer, "pkg", "unpack", archive, "--output", recovered])
            expected_files = dict(tested_payload, **{"package.knpkg.json": manifest_bytes})
            actual = {str(p.relative_to(recovered)).replace("\\", "/"): p.read_bytes()
                      for p in recovered.rglob("*") if p.is_file()}
            assert actual == expected_files
            cases += 1
        repacked = out / f"{label}-repacked.klib"
        invoke(label + "-repack", [compiler, "pkg", "build", "--manifest",
                                   out / (label + " unpack by selfhost"), "--output", repacked])
        verify_archive(repacked, compiler, expected_payload=tested_payload)
        default_dir = out / (label + " defaults")
        default_dir.mkdir(exist_ok=True)
        invoke(label + "-default-build", [producer, "pkg", "build", "--manifest", source_manifest], default_dir)
        default_archive = default_dir / "Acme.Greeter-1.2.3.klib"
        verify_archive(default_archive, producer, expected_payload=tested_payload)
        invoke(label + "-default-unpack", [producer, "pkg", "unpack", default_archive], default_dir)
        assert (default_dir / "Acme" / "package.knpkg.json").read_bytes() == manifest_bytes
        layout = out / (label + " package root")
        invoke(label + "-layout", [producer, "pkg", "build", "--manifest", source_manifest, "--layout", layout])
        installed = layout / "Acme.Greeter" / "1.2.3"
        wrapper = json.loads((installed / "package.knpkg.json").read_text(encoding="utf-8"))
        assert wrapper == dict({key: metadata[key] for key in
                              ("name", "version", "summary", "url", "entry", "modules", "dependencies")},
                              kind="package", klib="lib/Acme.Greeter.klib")
        verify_archive(installed / "lib" / "Acme.Greeter.klib", producer, expected_payload=tested_payload)
        cases += 4
        if compile_consumers:
            for consumer_label, consumer in tools:
                application = out / (label + " app by " + consumer_label)
                application.mkdir(exist_ok=True)
                main = application / "Main.kn"
                main.write_text("Unit Tests.PackageCli; Get Acme.Greeter; Get IO.Console; "
                                "Static Function int Main() { If (Acme.Greeter.Answer() != 42) Return 1; "
                                'IO.Console.PrintLine("package-cli-ok"); Return 0; }\n')
                executable = application / ("app" + suffix)
                invoke(label + "-consume-" + consumer_label,
                       [consumer, "build", main, "--pkg-root", layout, "-o", executable])
                result = invoke(label + "-execute-" + consumer_label, [executable])
                assert result.stdout.replace("\r\n", "\n") == "package-cli-ok\n"
                cases += 1

    repeat = out / "selfhost-repeat.klib"
    invoke("repeat-build", [compiler, "pkg", "build", "--manifest", manifest, "-o", repeat])
    expected = repeat.read_bytes()
    invoke("repeat-build-again", [compiler, "pkg", "build", "--manifest", manifest, "-o", repeat])
    assert repeat.read_bytes() == expected
    cases += 1

    # Explicit source_files, the legacy manifest name, and absent version all
    # roundtrip without rewriting the original embedded manifest.
    legacy = out / "legacy package"
    legacy.mkdir(exist_ok=True)
    legacy_manifest = legacy / "package.knpkg"
    legacy_manifest.write_text('{"kind":"package","name":"Acme.Legacy",'
                               '"source_files":["Legacy.kn"]}\n')
    legacy_source = legacy / "Legacy.kn"
    legacy_source.write_text("Unit Acme.Legacy; Function int Value() { Return 7; }\n")
    for label, producer in tools:
        cwd = out / (label + " legacy defaults")
        cwd.mkdir(exist_ok=True)
        invoke(label + "-legacy-build", [producer, "pkg", "build", "--manifest", legacy], cwd)
        verify_archive(cwd / "Acme.Legacy.klib", producer, legacy_manifest.read_bytes(),
                       {"Legacy.kn": legacy_source.read_bytes()})
        invoke(label + "-legacy-layout", [producer, "pkg", "build", "--manifest", legacy,
                                          "--layout", cwd / "kpkg"])
        wrapper = json.loads((cwd / "kpkg/Acme.Legacy/0.0.0/package.knpkg.json").read_text())
        assert wrapper == {"kind": "package", "name": "Acme.Legacy", "version": "0.0.0",
                           "entry": "Legacy.kn", "klib": "lib/Acme.Legacy.klib"}
        cases += 2
        if compile_consumers:
            (cwd / "Main.kn").write_text(
                "Unit Tests.LegacyPackageCli; Get Acme.Legacy; Get IO.Console; "
                "Static Function int Main() { If (Acme.Legacy.Value() != 7) Return 1; "
                'IO.Console.PrintLine("legacy-package-cli-ok"); Return 0; }\n')
            (cwd / "kinal.knproj").write_text(
                'Project PackageCli { DefaultProfile="native"; '
                'SourceSet "app" { Files=["Main.kn"]; RequireUnit=true; } '
                'Profile "native" { Source { Entry="Main.kn"; Sets=["app"]; '
                'Mode=ReachableUnits; } Build { Backend=Native; } } }\n')
            executable = cwd / ("app" + suffix)
            invoke(label + "-legacy-project", [compiler, "build", "--project", cwd,
                                               "-o", executable])
            executed = invoke(label + "-legacy-project-run", [executable])
            assert executed.stdout.replace("\r\n", "\n") == "legacy-package-cli-ok\n"
            cases += 1
    for command in ([], ["build"], ["info"], ["unpack"]):
        invoke("help-" + "-".join(command), [compiler, "pkg", *command, "--help"])
        cases += 1
    result = {"name": "selfhost_package_cli", "ok": True, "cases": cases, "commands": sequence,
              "stage0_reference": stage0_reference, "consumer_compilation": compile_consumers,
              "payload_files": len(payload), "payload_bytes": total,
              "stage0_empty_payload_packing": "supported",
              "selfhost_archive_sha256": hashlib.sha256(expected).hexdigest()}
    (out / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"[OK] package CLI: {cases} cases, {sequence} commands", flush=True)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--no-stage0-reference", action="store_true")
    parser.add_argument("--package-only", action="store_true")
    args = parser.parse_args()
    print(json.dumps(check_package_cli(args.compiler.resolve(), args.stage0.resolve(),
                                      Path(__file__).resolve().parents[2], args.out_dir.resolve(),
                                      stage0_reference=not args.no_stage0_reference,
                                      compile_consumers=not args.package_only)))
