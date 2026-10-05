"""Hosted startup must initialize process services even for Main() without args."""
from __future__ import annotations

import os
import json
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


class KinalProcessArgumentTests(unittest.TestCase):
    def test_process_run_keeps_exact_argv(self) -> None:
        configured = os.environ.get("KINAL_TEST_COMPILER")
        if not configured or not Path(configured).is_file():
            self.skipTest("set KINAL_TEST_COMPILER to test the Kinal package")
        from infra.scripts.x.llvm import detect_llvm_dir, llvm_bin_dir
        clang = llvm_bin_dir(detect_llvm_dir()) / ("clang.exe" if os.name == "nt" else "clang")
        arguments = ["two words", "", 'quoted "value"', "trailing\\", '\\"',
                     "\\\\", "tab\tvalue", "plain", "&|<>%!"]
        expected = ",".join(json.dumps(value) for value in arguments)
        with tempfile.TemporaryDirectory(prefix="kinal process arguments ") as directory:
            root = Path(directory)
            child_source = root / "probe.c"
            child_source.write_text('''#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    const char *expected[] = {''' + expected + '''};
    if (argc != 10) return 80;
    for (int i = 0; i < 9; i++) if (strcmp(argv[i + 1], expected[i])) return 81 + i;
    puts("arguments-ok");
    return 0;
}
''', encoding="utf-8")
            child = root / ("argv probe.exe" if os.name == "nt" else "argv probe")
            subprocess.run([str(clang), str(child_source), "-o", str(child)], check=True)
            parent_source = root / "Main.kn"
            parent_source.write_text('''Get IO.Kinal.Runtime;
Trusted Static Function int Main(string[] args) {
    string[] values = {''' + expected + '''};
    Return IO.Kinal.Runtime.ProcessRun(args[0], values);
}
''', encoding="utf-8")
            parent = root / ("parent program.exe" if os.name == "nt" else "parent program")
            built = subprocess.run([configured, "build", "--no-module-discovery", str(parent_source),
                                    "-o", str(parent)], cwd=ROOT, capture_output=True, timeout=180)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            execution = subprocess.run([str(parent), str(child)], capture_output=True, timeout=30)
            self.assertEqual(execution.returncode, 0, execution.stdout + execution.stderr)
            self.assertEqual(execution.stdout.replace(b"\r\n", b"\n"), b"arguments-ok\n")


if __name__ == "__main__":
    unittest.main()
