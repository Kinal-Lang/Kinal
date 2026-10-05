"""Hosted startup must initialize process services even for Main() without args."""
from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipIf(os.name == "nt", "POSIX argv initialization; Windows uses GetCommandLineA")
class ProcessRuntimeTests(unittest.TestCase):
    def test_process_initialization_and_argument_boundaries(self) -> None:
        compiler = shutil.which(os.environ.get("CC", "cc"))
        if not compiler:
            self.skipTest("a C compiler is required")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "probe.c"
            source.write_text(r'''
#include <stdint.h>
#include <string.h>
extern void kn_native_process_initialize_arguments(int32_t argc, const char **argv);
extern const char *kn_native_system_command_line(void);
extern void __kn_sys_args(const char ***data, uint64_t *length);
int main(void) {
    const char *argv[] = {"probe", "first", "two words", 0};
    const char *other[] = {"replacement", 0};
    const char **data = 0; uint64_t length = 0;
    kn_native_process_initialize_arguments(0, 0);
    if (strcmp(kn_native_system_command_line(), "")) return 1;
    kn_native_process_initialize_arguments(3, argv);
    if (strcmp(kn_native_system_command_line(), "probe first two words")) return 2;
    __kn_sys_args(&data, &length);
    if (length != 2 || strcmp(data[0], "first") || strcmp(data[1], "two words")) return 3;
    kn_native_process_initialize_arguments(1, other);
    if (strcmp(kn_native_system_command_line(), "probe first two words")) return 4;
    return 0;
}
''', encoding="utf-8")
            output = root / "probe"
            subprocess.run([compiler, "-DKN_NO_MEM_OVERRIDE", "-DKN_NO_FLTUSED",
                            "-I", str(ROOT / "libs/runtime/include"), str(source),
                            str(ROOT / "libs/runtime/src/kn_runtime.c"),
                            "-lm", "-ldl", "-lpthread", "-o", str(output)], check=True)
            process = subprocess.run([str(output)], capture_output=True, text=True, timeout=15)
            self.assertEqual(process.returncode, 0, process.stdout + process.stderr)


if __name__ == "__main__":
    unittest.main()
