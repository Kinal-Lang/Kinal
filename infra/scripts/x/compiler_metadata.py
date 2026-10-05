"""Keep the selfhost compiler's embedded version synchronized with VERSION.

The generated Unit is checked in so direct source/project builds do not depend
on a sidecar at runtime. Bootstrap helpers refresh it before compiling.
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

from .context import ROOT
from .vm_metadata import read_vm_version

BUILD_INFO = Path("apps/kinal-selfhost/src/IO/Kinal/Compiler/Core/BuildInfo.kn")


def read_compiler_version(root: Path = ROOT) -> str:
    versions = [line.partition("=")[2].strip()
                for line in (root / "VERSION").read_text(encoding="utf-8").splitlines()
                if line.partition("=")[0].strip() == "kinal"]
    if len(versions) != 1 or not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", versions[0]):
        raise ValueError("VERSION must contain exactly one kinal=MAJOR.MINOR.PATCH entry")
    return versions[0]


def render_build_info(version: str, vm_version: str) -> str:
    return ("// Generated from VERSION:kinal and VERSION:kinalvm. Do not edit by hand.\n"
            "// Regenerate: python -m infra.scripts.x.compiler_metadata\n"
            "Unit IO.Kinal.Compiler.Core.BuildInfo;\n\n"
            "Safe Function string Version()\n"
            "{\n"
            f'    Return "{version}";\n'
            "}\n\n"
            "Safe Function string VMVersion()\n"
            "{\n"
            f'    Return "{vm_version}";\n'
            "}\n")


def generate_selfhost_build_info(root: Path = ROOT, *, check: bool = False) -> Path:
    output = root / BUILD_INFO
    expected = render_build_info(read_compiler_version(root), read_vm_version(root))
    current = output.read_text(encoding="utf-8") if output.is_file() else None
    if current != expected:
        if check:
            raise ValueError("Selfhost BuildInfo.kn is stale; run "
                             "python -m infra.scripts.x.compiler_metadata")
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(expected, encoding="utf-8", newline="\n")
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="reject stale generated source")
    args = parser.parse_args()
    try:
        generate_selfhost_build_info(check=args.check)
    except ValueError as error:
        parser.exit(1, f"{error}\n")
