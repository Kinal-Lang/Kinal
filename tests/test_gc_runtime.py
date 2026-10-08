"""Bounds of legacy GC scan regions, independent of conservative stack roots."""
from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class GcRegionTests(unittest.TestCase):
    def test_only_complete_pointer_words_are_scanned(self) -> None:
        compiler = shutil.which(os.environ.get("CC", "clang" if os.name == "nt" else "cc"))
        if not compiler:
            self.skipTest("a C compiler is required")
        with tempfile.TemporaryDirectory(prefix="kinal-gc-region-") as directory:
            output = Path(directory) / ("probe.exe" if os.name == "nt" else "probe")
            libraries = ["-luser32", "-lgdi32"] if os.name == "nt" else ["-lm", "-lpthread"]
            if os.name == "posix" and os.uname().sysname != "Darwin":
                libraries.append("-ldl")
            build = subprocess.run(
                [compiler, "-O2", "-std=c11", "-ffreestanding", "-fno-builtin",
                 "-D_GNU_SOURCE", "-I", str(ROOT / "libs/runtime/include"),
                 str(ROOT / "tests/ffi_native/gc_region_probe.c"),
                 *libraries, "-o", str(output)],
                capture_output=True, text=True, timeout=120,
            )
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
