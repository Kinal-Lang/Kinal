#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
X_PY = ROOT / "x.py"
SELFHOST_OUT = ROOT / "out" / "selfhost"
BOOTSTRAP_OUT = SELFHOST_OUT / "bootstrap"


def exe_name(name: str) -> str:
    return f"{name}.exe" if sys.platform.startswith("win") else name


def run_capture(cmd: list[str], cwd: Path = ROOT) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=str(cwd), text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def stage0_path() -> Path:
    return SELFHOST_OUT / "stage0-host" / exe_name("selfhost-stage0")


def ensure_stage0(*, clean: bool = False) -> Path:
    cmd = [sys.executable, str(X_PY), "selfhost"]
    if clean:
        cmd.append("--clean")
    proc = run_capture(cmd)
    print(proc.stdout.rstrip())
    if proc.returncode != 0:
        raise SystemExit(proc.returncode)
    compiler = stage0_path()
    if not compiler.exists():
        raise SystemExit(f"stage0 compiler not found after build: {compiler}")
    return compiler


def parse_completion(text: str) -> float:
    match = re.search(r"COMPLETION:\s+\d+/\d+\s+\(([\d.]+)%\)", text)
    if not match:
        return 0.0
    try:
        return float(match.group(1))
    except ValueError:
        return 0.0


def prepare_stage_bundle(base_bundle: Path, dst_bundle: Path) -> None:
    if dst_bundle.exists():
        shutil.rmtree(dst_bundle)
    shutil.copytree(base_bundle, dst_bundle)


def build_next_stage(current_compiler: Path, stage_index: int, backend: str) -> Path:
    bundle = BOOTSTRAP_OUT / f"stage{stage_index}-{backend}"
    prepare_stage_bundle(current_compiler.parent, bundle)
    next_compiler = bundle / exe_name(f"selfhost-stage{stage_index}")
    proc = run_capture(
        [
            str(current_compiler),
            "--in",
            str(ROOT / "selfhost" / "src" / "Main.kn"),
            "--phase",
            "full",
            "--backend",
            backend,
            "-o",
            str(next_compiler),
        ]
    )
    print(proc.stdout.rstrip())
    if proc.returncode != 0 or not next_compiler.exists():
        raise SystemExit(f"bootstrap failed while building {next_compiler}")
    return next_compiler


def measure_completion(compiler: Path, backend: str, stage_index: int) -> float:
    csuite_out = BOOTSTRAP_OUT / "csuite" / f"stage{stage_index}-{backend}"
    proc = run_capture(
        [
            sys.executable,
            str(ROOT / "selfhost" / "tests" / "run_csuite_selfhost.py"),
            "--compiler",
            str(compiler),
            "--root",
            str(ROOT),
            "--backend",
            backend,
            "--out-dir",
            str(csuite_out),
        ]
    )
    print(proc.stdout.rstrip())
    return parse_completion(proc.stdout)


def main() -> int:
    ap = argparse.ArgumentParser(description="Bootstrap selfhost compiler until target completion or max stage")
    ap.add_argument("--target", type=float, default=98.0, help="target completion percent")
    ap.add_argument("--max-stage", type=int, default=3, help="maximum stage index to build (inclusive)")
    ap.add_argument("--bootstrap-backend", choices=["host", "self"], default="host", help="backend used for next-stage bootstrap")
    ap.add_argument("--metric-backend", choices=["host", "self"], default="self", help="backend used for completion metric")
    ap.add_argument("--clean", action="store_true", help="rebuild stage0 and wipe bootstrap output first")
    args = ap.parse_args()

    if args.clean and BOOTSTRAP_OUT.exists():
        shutil.rmtree(BOOTSTRAP_OUT)
    BOOTSTRAP_OUT.mkdir(parents=True, exist_ok=True)

    stage = 0
    compiler = ensure_stage0(clean=args.clean)
    reached = False
    last_pct = 0.0

    while True:
        print(f"\n=== Stage {stage}: {compiler} ===")
        last_pct = measure_completion(compiler, args.metric_backend, stage)
        if last_pct >= args.target:
            reached = True
            print(f"[STOP] target reached: {last_pct:.2f}% >= {args.target:.2f}%")
            break

        if stage >= args.max_stage:
            print(f"[STOP] max stage reached: {stage} (target not met)")
            break

        stage += 1
        compiler = build_next_stage(compiler, stage, args.bootstrap_backend)

    print("\nSUMMARY")
    print(f"  final_stage: {stage}")
    print(f"  final_completion: {last_pct:.2f}%")
    print(f"  target: {args.target:.2f}%")
    print(f"  reached_target: {'yes' if reached else 'no'}")
    return 0 if reached else 1


if __name__ == "__main__":
    raise SystemExit(main())
