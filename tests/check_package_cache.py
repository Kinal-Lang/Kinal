"""CLI-private archive extraction must not accumulate across invocations."""
from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import subprocess
import tempfile
from pathlib import Path

from check_project_packages import archive


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True, type=Path)
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    with tempfile.TemporaryDirectory(prefix="kn-cache-") as directory:
        root = Path(directory)
        temp = root / "tmp"
        foreign = temp / "kinal-cache/klib/process-foreign"
        foreign.mkdir(parents=True)
        sentinel = foreign / "sentinel"
        sentinel.write_text("preserve")
        env = {**os.environ, "TMP": str(temp), "TEMP": str(temp), "TMPDIR": str(temp)}
        source = root / "Main.kn"
        source.write_text("Unit Tests.Cache;\nGet IO.Console;\nStatic Function int Main() { Return 0; }\n")
        bad = root / "Bad.kn"
        bad.write_text("Unit Tests.BadCache;\nGet IO.Console;\nStatic Function int Main() { Return Missing; }\n")
        def compile_case(index: int, failing: bool = False) -> None:
            result = subprocess.run([str(compiler), "build", "--no-module-discovery", "--emit", "ir",
                                     str(bad if failing else source), "-o", str(root / f"case-{index}.ll")],
                                    env=env, capture_output=True, text=True, timeout=120)
            if (result.returncode != 0) != failing:
                raise AssertionError(f"cache probe: {result}")
        for index in range(3):
            compile_case(index)
        compile_case(3, True)
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as workers:
            list(workers.map(compile_case, range(4, 8)))
        leftovers = list(foreign.parent.glob("process-*"))
        if leftovers != [foreign] or sentinel.read_text() != "preserve":
            raise AssertionError(f"CLI caches leaked or foreign cache changed: {leftovers}")
        print("[OK] package_cache_success_failure_and_concurrency")
        result = subprocess.run([str(compiler), "build", "--keep-temps", "--no-module-discovery",
                                 "--emit", "ir", str(source), "-o", str(root / "kept.ll")],
                                env=env, capture_output=True, text=True, timeout=120)
        if result.returncode != 0 or len(list(foreign.parent.glob("process-*"))) != 2:
            raise AssertionError(f"explicit --keep-temps cache was removed: {result}")
        print("[OK] package_cache_explicit_keep_temps")
        # Runtime native libraries may remain linked by path; retain their cache.
        package = root / "packages/probe"
        package.mkdir(parents=True)
        (package / "package.knpkg.json").write_text(json.dumps(
            {"name": "Probe.Cache", "version": "1", "klib": "lib/probe.klib"}))
        archive(package / "lib/probe.klib", "Probe.Cache", {
            "src/Probe.kn": "Unit Probe.Cache;\nFunction int Value() { Return 7; }\n",
            "native/libprobe.so.1": "runtime asset fixture",
        })
        source.write_text("Unit Tests.Cache;\nGet Probe.Cache;\nStatic Function int Main() { Return Probe.Cache.Value(); }\n")
        result = subprocess.run([str(compiler), "build", "--pkg-root", str(root / "packages"),
                                 "--emit", "ir", str(source),
                                 "-o", str(root / "runtime.ll")], env=env, capture_output=True, text=True, timeout=120)
        if result.returncode != 0 or not list(temp.rglob("libprobe.so.1")):
            raise AssertionError(f"runtime asset cache was removed: {result}")
        print("[OK] package_cache_runtime_asset_retention")


if __name__ == "__main__":
    main()
