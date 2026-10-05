"""Package provenance distinguishes the Linux launcher from its executable."""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from infra.scripts.x import runtime_build

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("check_package_cli", ROOT / "tests/selfhost/check_package_cli.py")
package_cli = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(package_cli)


class PackageProducerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.compiler = self.root / "kinal"
        self.compiler.write_bytes(b"\x7fELF compiler fixture")

    def test_direct_executable_keeps_its_path(self):
        self.assertEqual(package_cli.compiler_producer_path(self.compiler), self.compiler.resolve())

    def test_sibling_payload_does_not_change_direct_executable(self):
        self.compiler.with_name("kinal.bin").write_bytes(b"other executable")
        self.assertEqual(package_cli.compiler_producer_path(self.compiler), self.compiler.resolve())

    def test_official_linux_launcher_names_payload(self):
        with patch.object(runtime_build, "host_tag", return_value="linux-x64"):
            runtime_build.write_linux_compiler_launcher(self.root)
        self.assertEqual(package_cli.compiler_producer_path(self.compiler),
                         self.compiler.with_name("kinal.bin").resolve())

    def test_linux_launcher_keeps_lf_on_windows_text_writers(self):
        original_write_text = Path.write_text

        def windows_write_text(path, text, *args, **kwargs):
            # Model Windows' native newline translation on every test host.
            kwargs.setdefault("newline", "\r\n")
            return original_write_text(path, text, *args, **kwargs)

        with patch.object(runtime_build, "host_tag", return_value="linux-x64"), \
             patch.object(Path, "write_text", windows_write_text):
            runtime_build.write_linux_compiler_launcher(self.root)
        launcher = self.compiler.read_bytes()
        self.assertTrue(launcher.startswith(b"#!/usr/bin/env sh\n"))
        self.assertNotIn(b"\r", launcher)
        self.assertEqual(package_cli.compiler_producer_path(self.compiler),
                         self.compiler.with_name("kinal.bin").resolve())

    def test_missing_launcher_payload_is_rejected(self):
        self.compiler.write_bytes(b'#!/usr/bin/env sh\nexec "$HERE/kinal.bin" "$@"\n')
        with self.assertRaisesRegex(AssertionError, "payload is missing"):
            package_cli.compiler_producer_path(self.compiler)

    def test_unrecognized_launcher_is_not_reinterpreted(self):
        self.compiler.write_bytes(b'#!/usr/bin/env sh\nexec "/some/other/compiler" "$@"\n')
        self.compiler.with_name("kinal.bin").write_bytes(b"other executable")
        self.assertEqual(package_cli.compiler_producer_path(self.compiler), self.compiler.resolve())


if __name__ == "__main__":
    unittest.main()
