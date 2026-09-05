from __future__ import annotations

import contextlib
import io
import subprocess
import tempfile
import unittest
from pathlib import Path

from run_tests import pe_machine, print_runtime_output


class RuntimeDiagnosticsTests(unittest.TestCase):
    def test_exit_failure_preserves_stdout_exception_and_stderr(self) -> None:
        process = subprocess.CompletedProcess(
            ["test-program"], 1, "Unhandled IO.Request: missing OpenSSL\n", "native detail\n"
        )
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            print_runtime_output(process)
        self.assertEqual(
            output.getvalue(),
            "stdout:\n'Unhandled IO.Request: missing OpenSSL\\n'\n"
            "stderr:\n'native detail\\n'\n",
        )

    def test_empty_streams_are_explicit(self) -> None:
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            print_runtime_output(subprocess.CompletedProcess(["test-program"], 1))
        self.assertEqual(output.getvalue(), "stdout:\n''\nstderr:\n''\n")

    def test_machine_comes_from_binary_header(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "probe.dll"
            for machine, expected in ((0x014C, "x86"), (0x8664, "x64"), (0xAA64, "arm64")):
                with self.subTest(machine=machine):
                    header = bytearray(64)
                    header[:2] = b"MZ"
                    header[60:64] = (64).to_bytes(4, "little")
                    path.write_bytes(header + b"PE\0\0" + machine.to_bytes(2, "little"))
                    self.assertEqual(pe_machine(path), expected)

    def test_invalid_or_missing_pe_remains_diagnostic_only(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "probe.dll"
            self.assertIn("unreadable:", pe_machine(path))
            path.write_bytes(b"not an executable")
            self.assertEqual(pe_machine(path), "not-PE")
            header = bytearray(64)
            header[:2] = b"MZ"
            header[60:64] = (64).to_bytes(4, "little")
            path.write_bytes(header)
            self.assertEqual(pe_machine(path), "invalid-PE")


if __name__ == "__main__":
    unittest.main()
