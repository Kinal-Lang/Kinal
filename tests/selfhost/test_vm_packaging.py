"""Selfhost distribution wiring for the matching VM runner and version metadata."""
from __future__ import annotations
import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from infra.scripts.x import selfhost_ops

spec = importlib.util.spec_from_file_location("selfhost_bootstrap", ROOT / "tests/selfhost/run_bootstrap.py")
bootstrap = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(bootstrap)


class VmPackagingTests(unittest.TestCase):
    def test_runner_is_built_by_the_selfhost_compiler(self):
        compiler = Path("/isolated/stage/kinal-selfhost")
        with patch.object(selfhost_ops, "run") as invoke:
            output = selfhost_ops.build_selfhost_vm_runner(compiler)
        self.assertEqual(output.parent, compiler.parent)
        command = invoke.call_args.args[0]
        self.assertEqual(command[0], compiler)
        self.assertEqual(command[1:3], ["build", "--project"])
        self.assertEqual(command[3], ROOT / "apps/kinalvm")
        self.assertEqual(command[4:6], ["--profile", "release"])
        self.assertEqual(command[-1], output)

    def test_later_stage_keeps_runner_and_version(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "source"; target = Path(temporary) / "target"
            source.mkdir(); target.mkdir()
            for name in ("kinalvm", "kinalvm.exe", "VERSION"):
                (source / name).write_bytes(name.encode("ascii"))
            (source / "unrelated.txt").write_text("do not package")
            bootstrap.copy_stage_support(source, target)
            for name in ("kinalvm", "kinalvm.exe", "VERSION"):
                self.assertEqual((target / name).read_bytes(), name.encode("ascii"))
            self.assertFalse((target / "unrelated.txt").exists())

    def test_toolchain_keeps_runtime_sources_and_official_archive_tools(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, target, llvm, bridge = [root / name for name in ("stage0", "stage1", "llvm", "bridge")]
            for directory in (source / "runtime/linux-x64", source / "runtime/src",
                              source / "runtime/include/kn", source / "stdpkg",
                              source / "linker", target, llvm, bridge):
                directory.mkdir(parents=True, exist_ok=True)
            runtime_files = {
                "linux-x64/kn_runtime.o": b"runtime-leaf",
                "linux-x64/kn_math.o": b"math-leaf",
                "src/kn_runtime.c": b"runtime-source",
                "include/kn/platform.h": b"platform-header",
            }
            for name, contents in runtime_files.items():
                (source / "runtime" / name).write_bytes(contents)
            for name in ("llvm-ar", "llvm-lib", "lld-link", "ld64.lld"):
                file = llvm / name
                file.write_bytes(name.encode())
                file.chmod(0o755)
            (bridge / "kn_selfhost_llvm.o").write_bytes(b"bridge")
            with (patch.object(selfhost_ops, "is_windows", return_value=False),
                  patch.object(selfhost_ops, "exe_name", side_effect=lambda name: name),
                  patch.object(selfhost_ops, "host_tag", return_value="linux-x64"),
                  patch.object(selfhost_ops, "selfhost_bridge_object", return_value=bridge / "kn_selfhost_llvm.o"),
                  patch.object(selfhost_ops, "detect_llvm_dir", return_value=llvm),
                  patch.object(selfhost_ops, "llvm_bin_dir", return_value=llvm),
                  patch.object(selfhost_ops, "write_selfhost_clang_wrapper")):
                selfhost_ops.package_selfhost_toolchain(source / "kinal", target / "kinal-selfhost")
            for name, contents in runtime_files.items():
                self.assertEqual((target / "runtime" / name).read_bytes(), contents)
            for name in ("llvm-ar", "llvm-lib", "lld-link", "ld64.lld"):
                packaged = target / "linker" / name
                self.assertEqual(packaged.read_bytes(), name.encode())
                # copy2 preserves source permissions. Windows cannot model
                # POSIX execute bits even for this simulated Linux bundle.
                self.assertEqual(packaged.stat().st_mode & 0o777,
                                 (llvm / name).stat().st_mode & 0o777)
            # This operation must not fabricate a VM or use stage0 as a callback.
            self.assertFalse((target / "kinalvm").exists())

    def test_later_stage_preserves_cross_runtime_inputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "source"; target = Path(temporary) / "target"
            for path in ("runtime/src/kn_runtime.c", "runtime/include/kn/freestanding.h", "linker/llvm-ar"):
                file = source / path
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_text(path)
            target.mkdir()
            bootstrap.copy_stage_support(source, target)
            for path in ("runtime/src/kn_runtime.c", "runtime/include/kn/freestanding.h", "linker/llvm-ar"):
                self.assertEqual((target / path).read_text(), path)


if __name__ == "__main__":
    unittest.main()
