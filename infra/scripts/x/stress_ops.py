from __future__ import annotations

import os
import sys
from pathlib import Path

from .context import OUT, ROOT
from .llvm import detect_llvm_dir, llvm_bin_dir
from .util import run


def run_stress_suite(
    compiler: Path,
    *,
    out_dir: Path | None = None,
    profile: str = "full",
    cases: list[str] | None = None,
    keep_generated: bool = False,
) -> None:
    cmd = [
        sys.executable,
        str(ROOT / "tests" / "run_stress.py"),
        "--compiler",
        str(compiler),
        "--out-dir",
        str((out_dir or (OUT / "stress")).resolve()),
        "--profile",
        profile,
    ]
    for case in cases or []:
        cmd.extend(["--case", case])
    if keep_generated:
        cmd.append("--keep-generated")

    env = os.environ.copy()
    if not env.get("KN_LLVM_BIN"):
        try:
            env["KN_LLVM_BIN"] = str(llvm_bin_dir(detect_llvm_dir()))
        except SystemExit:
            pass
    run(cmd, env=env)
