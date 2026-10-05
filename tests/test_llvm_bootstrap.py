from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from infra.toolchains.setup_llvm import ensure_linux_shared_runtime


class LlvmBootstrapTests(unittest.TestCase):
    def toolchain(self, root: Path) -> Path:
        for name in ("bin/llvm-config", "bin/clang++", "lib/libLLVMCore.a"):
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
        return root

    def test_existing_shared_runtime_is_not_relinked(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = self.toolchain(Path(directory))
            (root / "lib/libLLVM.so.21.1").touch()
            with mock.patch("platform.system", return_value="Linux"), \
                 mock.patch("subprocess.check_output") as query:
                ensure_linux_shared_runtime(root)
                query.assert_not_called()
            self.assertTrue((root / "lib/libLLVM.so").is_file())

    def test_static_archive_builds_shared_runtime_atomically(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = self.toolchain(Path(directory))
            core = str(root / "lib/libLLVMCore.a")
            def link(command: list[str]) -> None:
                self.assertIn("-Wl,--whole-archive", command)
                self.assertIn("-Wl,-z,defs", command)
                self.assertIn("-l:libxml2.so.2", command)
                self.assertEqual(command[-1], str(root / "lib/libLLVM.so.tmp"))
                Path(command[-1]).write_bytes(b"linked")
            with mock.patch("platform.system", return_value="Linux"), \
                 mock.patch("subprocess.check_output", side_effect=[core, "-lxml2"]), \
                 mock.patch("ctypes.util.find_library", return_value="libxml2.so.2"), \
                 mock.patch("infra.toolchains.setup_llvm.run", side_effect=link):
                ensure_linux_shared_runtime(root)
            self.assertEqual((root / "lib/libLLVM.so").read_bytes(), b"linked")
            self.assertFalse((root / "lib/libLLVM.so.tmp").exists())

    def test_failed_link_removes_partial_library(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = self.toolchain(Path(directory))
            temporary = root / "lib/libLLVM.so.tmp"
            temporary.write_bytes(b"partial")
            with mock.patch("platform.system", return_value="Linux"), \
                 mock.patch("subprocess.check_output", side_effect=[str(root / "lib/libLLVMCore.a"), ""]), \
                 mock.patch("infra.toolchains.setup_llvm.run", side_effect=subprocess.CalledProcessError(1, "clang++")):
                with self.assertRaisesRegex(SystemExit, "failed to build"):
                    ensure_linux_shared_runtime(root)
            self.assertFalse(temporary.exists())
            self.assertFalse((root / "lib/libLLVM.so").exists())

    def test_non_linux_toolchain_unchanged(self) -> None:
        with mock.patch("platform.system", return_value="Windows"), \
             mock.patch("subprocess.check_output") as query:
            ensure_linux_shared_runtime(Path("missing"))
            query.assert_not_called()


if __name__ == "__main__":
    unittest.main()
