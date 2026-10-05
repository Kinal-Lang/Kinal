"""Generate KinalVM's embedded component version from the canonical VERSION.

The small generated Unit is checked in so direct source/project builds work
with a clean checkout and a published bootstrap compiler. Build helpers refresh
it automatically; --check and the regression test reject stale checked-in data.
This module intentionally depends on no compiler or VM build helpers.
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

from .context import ROOT


BUILD_INFO = Path("apps/kinalvm/src/IO/Kinal/VM/BuildInfo.kn")


def read_vm_version(root: Path = ROOT) -> str:
    versions = [line.partition("=")[2].strip()
                for line in (root / "VERSION").read_text(encoding="utf-8").splitlines()
                if line.partition("=")[0].strip() == "kinalvm"]
    if len(versions) != 1 or not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", versions[0]):
        raise ValueError("VERSION must contain exactly one kinalvm=MAJOR.MINOR.PATCH entry")
    return versions[0]


def render_build_info(version: str) -> str:
    # Only VERSION values accepted by read_vm_version are interpolated here.
    return ("// Generated from VERSION:kinalvm. Do not edit by hand.\n"
            "// Regenerate: python -m infra.scripts.x.vm_metadata\n"
            "Unit IO.Kinal.VM.BuildInfo;\n\n"
            "Safe Function string Version()\n"
            "{\n"
            f'    Return "{version}";\n'
            "}\n")


def generate_kinalvm_build_info(root: Path = ROOT, *, check: bool = False) -> Path:
    output = root / BUILD_INFO
    expected = render_build_info(read_vm_version(root))
    current = output.read_text(encoding="utf-8") if output.is_file() else None
    if current != expected:
        if check:
            raise ValueError("KinalVM BuildInfo.kn is stale; run "
                             "python -m infra.scripts.x.vm_metadata")
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(expected, encoding="utf-8", newline="\n")
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="reject stale generated source")
    args = parser.parse_args()
    try:
        generate_kinalvm_build_info(check=args.check)
    except ValueError as error:
        parser.exit(1, f"{error}\n")
