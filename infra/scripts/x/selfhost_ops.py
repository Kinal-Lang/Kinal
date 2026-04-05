from __future__ import annotations

import shutil
import sys
from pathlib import Path

from .context import ROOT, SELFHOST_APP_DIR, SELFHOST_OUT, exe_name, release_bundle_ready, release_dir, stage_dir
from .util import run


def default_staged_compiler(release: bool) -> Path:
    return stage_dir("Release" if release else "Debug") / exe_name("kinal")


def selfhost_stage0_dir() -> Path:
    return SELFHOST_OUT / "stage0-host"


def selfhost_stage0_exe() -> Path:
    return selfhost_stage0_dir() / exe_name("selfhost-stage0")


def selfhost_test_dir() -> Path:
    return SELFHOST_OUT / "tests"


def prepare_selfhost_stage0(*, clean_first: bool, cmd_dist) -> Path:
    if clean_first or not release_bundle_ready():
        cmd_dist(type("Args", (), {"clean": clean_first})())
    bundle = release_dir()
    stage0_dir = selfhost_stage0_dir()
    if stage0_dir.exists():
        shutil.rmtree(stage0_dir)
    shutil.copytree(bundle, stage0_dir)

    compiler = stage0_dir / exe_name("kinal")
    hostlib = stage0_dir / "kinal-host.lib"
    llvm_c_lib = stage0_dir / "llvm" / "lib" / "LLVM-C.lib"
    stage0 = selfhost_stage0_exe()
    run([compiler, "build", SELFHOST_APP_DIR / "src" / "Main.kn", "-o", stage0, "--link-file", hostlib, "--link-file", llvm_c_lib])
    return stage0


def run_selfhost_tests(stage0: Path) -> Path:
    out_dir = selfhost_test_dir()
    out_dir.mkdir(parents=True, exist_ok=True)
    run([sys.executable, str(SELFHOST_APP_DIR / "tests" / "run_selfhost_tests.py"), "--compiler", str(stage0), "--root", str(ROOT), "--out-dir", str(out_dir)])
    return out_dir


def run_selfhost_bootstrap(*, target: float, max_stage: int, bootstrap_backend: str, metric_backend: str, clean: bool) -> None:
    cmd = [
        sys.executable,
        str(SELFHOST_APP_DIR / "tests" / "bootstrap_until_target.py"),
        "--target",
        str(target),
        "--max-stage",
        str(max_stage),
        "--bootstrap-backend",
        bootstrap_backend,
        "--metric-backend",
        metric_backend,
    ]
    if clean:
        cmd.append("--clean")
    run(cmd)
