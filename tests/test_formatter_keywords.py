"""Formatting must preserve token boundaries for every supported keyword."""
from __future__ import annotations

import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class FormatterKeywordTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        configured = os.environ.get("KINAL_TEST_COMPILER")
        cls.compiler = Path(configured) if configured else ROOT / "out/build/host-debug/apps/kinal/kinal"
        if not cls.compiler.is_file():
            raise unittest.SkipTest("build kinal or set KINAL_TEST_COMPILER")
        cls.compiler = cls.compiler.resolve()

    def test_adjacent_operators_preserve_execution(self) -> None:
        source = (b"Unsafe Static Function int Main(){int x=7;int a=- - x;"
                  b"int c=- - - x;int* p=&x;int** q=&p;int d=* * q;"
                  b"Return a==7 && c==-7 && d==7 && x==7 ? 0 : 1;}\n")
        formatted = subprocess.run([str(self.compiler), "fmt", "--stdin"], input=source,
                                   cwd=ROOT, capture_output=True, timeout=60)
        self.assertEqual(formatted.returncode, 0, formatted.stderr)
        self.assertIn(b"- -", formatted.stdout)
        with tempfile.TemporaryDirectory(prefix="kinal-fmt-operators-") as directory:
            for label, content in (("original", source), ("formatted", formatted.stdout)):
                path = Path(directory) / (label + ".kn")
                output = Path(directory) / (label + (".exe" if os.name == "nt" else ""))
                path.write_bytes(content)
                built = subprocess.run([str(self.compiler), "build", str(path), "--no-module-discovery",
                                        "-o", str(output)], cwd=ROOT, capture_output=True, timeout=120)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                execution = subprocess.run([str(output)], cwd=ROOT, capture_output=True, timeout=30)
                self.assertEqual(execution.returncode, 0, execution.stdout + execution.stderr)

    def test_operator_and_number_boundaries(self) -> None:
        for original, required in ((b"- -", b"- -"), (b"+ +", b"+ +"),
                                   (b"/ /", b"/ /"), (b"/ *", b"/ *"),
                                   (b"1 . 2", b"1 .2"), (b"< <", b"< <"),
                                   (b"> >", b"> >"), (b"& &", b"& &"),
                                   (b"| |", b"| |"), (b"! =", b"! =")):
            with self.subTest(original=original):
                formatted = subprocess.run([str(self.compiler), "fmt", "--stdin"], input=original,
                                           cwd=ROOT, capture_output=True, timeout=60)
                self.assertEqual(formatted.returncode, 0, formatted.stderr)
                self.assertIn(required, formatted.stdout)

    def test_alias_foreach_in_preserve_program(self) -> None:
        source = (b"Unit Tests.FormatKeywords;\n"
                  b"Get IO.Console; Alias Print By IO.Console.PrintLine;\n"
                  b"Static Function int Main(){int[] values={1,2};int total=0;"
                  b"Foreach(int value In values){total+=value;}Print(total);Return total-3;}\n")
        formatted = subprocess.run([str(self.compiler), "fmt", "--stdin"], input=source,
                                   cwd=ROOT, capture_output=True, timeout=60)
        self.assertEqual(formatted.returncode, 0, formatted.stderr)
        self.assertIn(b"Alias Print By IO.Console.PrintLine;", formatted.stdout)
        self.assertIn(b"Foreach (int value In values)", formatted.stdout)
        again = subprocess.run([str(self.compiler), "fmt", "--stdin"], input=formatted.stdout,
                               cwd=ROOT, capture_output=True, timeout=60)
        self.assertEqual(again.returncode, 0, again.stderr)
        self.assertEqual(again.stdout, formatted.stdout)
        with tempfile.TemporaryDirectory(prefix="kinal-fmt-keywords-") as directory:
            path = Path(directory) / "Main.kn"
            output = Path(directory) / ("app.exe" if os.name == "nt" else "app")
            path.write_bytes(formatted.stdout)
            built = subprocess.run([str(self.compiler), "build", str(path), "--no-module-discovery",
                                    "-o", str(output)], cwd=ROOT, capture_output=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            execution = subprocess.run([str(output)], cwd=ROOT, capture_output=True, timeout=30)
            self.assertEqual(execution.returncode, 0, execution.stdout + execution.stderr)
            self.assertEqual(execution.stdout.replace(b"\r\n", b"\n"), b"3\n")


if __name__ == "__main__":
    unittest.main()
