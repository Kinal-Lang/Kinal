"""Cross-compiler shared metadata ABI, callbacks and lifecycle regression.

Only host binaries execute. The Kinal consumers deliberately use the language
callback ABI rather than imposing a platform's C aggregate-return ABI on it.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


def check_metadata_artifacts(compiler: Path, stage0: Path, root: Path, out: Path,
                             *, reference_only: bool = False) -> dict[str, object]:
    out.mkdir(parents=True, exist_ok=True)
    fixtures = root / "tests/selfhost/fixtures/native_metadata"
    suffix = ".exe" if os.name == "nt" else ""
    library_suffix = ".dll" if os.name == "nt" else ".dylib" if sys.platform == "darwin" else ".so"
    records = []
    sequence = 0

    def invoke(label: str, command: list[Path | str], *, expected: int = 0) -> None:
        nonlocal sequence
        sequence += 1
        result = subprocess.run([str(value) for value in command], cwd=root,
                                text=True, capture_output=True, timeout=240)
        record = {"label": label, "command": result.args, "returncode": result.returncode,
                  "stdout": result.stdout, "stderr": result.stderr}
        (out / f"{sequence:02d}-{label}.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        assert result.returncode == expected, record

    roles = [("stage0", stage0)] if reference_only else [("stage0", stage0), ("selfhost", compiler)]
    producers, consumers = {}, {}
    for role, binary in roles:
        library = out / (role + "-metadata" + library_suffix)
        library.unlink(missing_ok=True)
        invoke(role + "-library", [binary, "build", "--no-module-discovery", "--kind", "shared",
                                   fixtures / "Library.kn", "-o", library])
        assert library.is_file(), library
        producers[role] = library
        consumer_source = fixtures / "Consumer.kn"
        if role == "selfhost":
            # C stage0 leaves callbacks borrowed from a loaded module. Selfhost
            # additionally invalidates its adapters when unloading succeeds.
            consumer_source = out / "SelfhostConsumer.kn"
            source = (fixtures / "Consumer.kn").read_text(encoding="utf-8")
            source = source.replace("    Return 0;", """    If ([int](invoke(19, 23)) != 0) Return 15;
    If ([string](add.Fetch("arg.label")) != "add") Return 16;
    Return 0;""")
            consumer_source.write_text(source, encoding="utf-8")
        consumer = out / (role + "-metadata-consumer" + suffix)
        consumer.unlink(missing_ok=True)
        invoke(role + "-consumer", [binary, "build", "--no-module-discovery",
                                    consumer_source, "-o", consumer])
        assert consumer.is_file(), consumer
        consumers[role] = consumer
    for producer, library in producers.items():
        for consumer, executable in consumers.items():
            invoke(consumer + "-loads-" + producer, [executable, library])
            records.append({"producer": producer, "consumer": consumer, "host_executed": True})
            print(f"[OK] {consumer} consumer loads {producer} shared metadata: all owner kinds, "
                  "retention, defaults, callback arity, repeated-load references and unload", flush=True)
    enum_checked = False
    if not reference_only:
        # This accepted selfhost extension is intentionally separate from the
        # C interoperability matrix: current C rejects enum-typed Meta params.
        enum_exe = out / ("selfhost-enum-metadata" + suffix)
        enum_exe.unlink(missing_ok=True)
        invoke("selfhost-enum-build", [compiler, "build", "--no-module-discovery",
                                      fixtures / "EnumConsumer.kn", fixtures / "EnumDefinitions.kn",
                                      "-o", enum_exe])
        invoke("selfhost-enum-values", [enum_exe])
        enum_checked = True
        print("[OK] selfhost enum metadata: supplied/default values and declaration scope", flush=True)
    return {"name": "native_metadata_artifacts", "ok": True, "checks": records,
            "selfhost_enum_metadata": enum_checked, "foreign_execution": "not tested"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    print(json.dumps(check_metadata_artifacts(args.compiler.resolve(), args.stage0.resolve(),
                                              args.root.resolve(), args.out_dir.resolve(),
                                              reference_only=args.reference_only)))
