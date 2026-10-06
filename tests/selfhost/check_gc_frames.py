from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path


def check_gc_frames(compiler: Path, stage0: Path, root: Path, out: Path,
                    *, stage0_reference: bool = True) -> dict[str, object]:
    out.mkdir(parents=True, exist_ok=True)
    tools = [("stage0", stage0)] if stage0_reference else []
    tools.append(("selfhost", compiler))
    project = root / "tests/selfhost/fixtures/gc_frames/kinal.knproj"
    suffix = ".exe" if os.name == "nt" else ""
    for label, tool in tools:
        executable = out / (label + suffix)
        command = [str(tool), "build", "--project", str(project), "--profile", "test",
                   "-o", str(executable)]
        build = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=180)
        assert build.returncode == 0, (label, build.returncode, build.stdout, build.stderr)
        for repeat in range(3):
            result = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                    text=True, timeout=60)
            assert result.returncode == 0 and result.stdout == "gc-frames-ok\n" and not result.stderr, \
                (label, repeat, result.returncode, result.stdout, result.stderr)
        print(f"[OK] {label} Kinal GC frame growth/nesting/collection", flush=True)
    return {"name": "gc_frames", "ok": True, "compilers": len(tools),
            "roots": 257, "repeats": 3, "stage0_reference": stage0_reference}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    print(check_gc_frames(args.compiler.resolve(), args.stage0.resolve(),
                          Path(__file__).resolve().parents[2], args.out_dir.resolve()))
