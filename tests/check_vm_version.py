"""Check KinalVM's embedded banner and its optional packaged VERSION metadata."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from infra.scripts.x.vm_metadata import generate_kinalvm_build_info, read_vm_version


def check_vm_version(vm: Path, root: Path = ROOT, *,
                     require_packaged_version: bool = False) -> dict[str, object]:
    vm, root = vm.resolve(), root.resolve()
    generate_kinalvm_build_info(root, check=True)
    expected = "KinalVM " + read_vm_version(root) + "\n"
    # Run away from the repository and any VERSION file. The banner belongs to
    # the executable, and must not depend on the user's current directory.
    with tempfile.TemporaryDirectory(prefix="kinalvm-version-") as directory:
        for flag in ("--version", "-V"):
            result = subprocess.run([str(vm), flag], cwd=directory, text=True,
                                    capture_output=True, timeout=30)
            assert result.returncode == 0, (vm, flag, result.returncode, result.stderr)
            assert result.stdout.replace("\r\n", "\n") == expected, (
                vm, flag, expected, result.stdout)
            assert not result.stderr, (vm, flag, result.stderr)
    packaged = vm.parent / "VERSION"
    assert not require_packaged_version or packaged.is_file(), (
        "missing packaged VERSION", packaged)
    if packaged.is_file():
        assert packaged.read_bytes() == (root / "VERSION").read_bytes(), (
            "packaged VERSION differs from canonical VERSION", packaged)
    return {"name": "vm_version", "ok": True, "vm": str(vm),
            "version": read_vm_version(root), "cases": 2,
            "packaged_version": packaged.is_file()}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vm", type=Path, action="append", required=True)
    parser.add_argument("--require-packaged-version", action="store_true")
    args = parser.parse_args()
    for vm in args.vm:
        print(json.dumps(check_vm_version(vm,
            require_packaged_version=args.require_packaged_version)))
