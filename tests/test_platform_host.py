"""POSIX host compatibility regressions; runs without a Kinal/LLVM build."""
from __future__ import annotations

import ctypes
import os
import posixpath
import random
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipIf(os.name == "nt", "POSIX host shim")
class PlatformHostTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        compiler = shutil.which(os.environ.get("CC", "cc"))
        if not compiler:
            raise unittest.SkipTest("a C compiler is required")
        cls.temp = tempfile.TemporaryDirectory()
        library = Path(cls.temp.name) / ("host.dylib" if sys.platform == "darwin" else "host.so")
        subprocess.run([compiler, "-shared", "-fPIC", "-I", str(ROOT / "libs/runtime/include"),
                        str(ROOT / "libs/runtime/src/kn_platform_host.c"), "-o", str(library)], check=True)
        cls.host = ctypes.CDLL(str(library))
        cls.fullpath = cls.host.GetFullPathNameA
        cls.fullpath.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.c_void_p,
                                 ctypes.POINTER(ctypes.c_void_p)]
        cls.fullpath.restype = ctypes.c_uint32

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temp.cleanup()

    def normalize(self, path: str) -> str:
        buf = ctypes.create_string_buffer(4096)
        result = self.fullpath(path.encode(), len(buf), buf, None)
        self.assertEqual(result, len(buf.value))
        return buf.value.decode()

    def test_parent_components_do_not_leave_separators(self) -> None:
        cases = {"/a/b/../c": "/a/c", "/a/b/..": "/a", "/a/../b/../c": "/c",
                 "/../../a": "/a", "/a//./b/../../": "/", "/a/b/../../../c": "/c"}
        for path, expected in cases.items():
            with self.subTest(path=path):
                self.assertEqual(self.normalize(path), expected)

    def test_seeded_normalization_and_idempotence(self) -> None:
        rng = random.Random(71021)
        for _ in range(1000):
            path = "/" + "/".join(rng.choices(["a", "b", "cc", ".", "..", ""], k=12))
            expected = "/" + posixpath.normpath(path).lstrip("/")
            actual = self.normalize(path)
            self.assertEqual(actual, expected, path)
            self.assertEqual(self.normalize(actual), actual)

    def test_small_buffer_reports_required_size_without_truncating(self) -> None:
        buf = ctypes.create_string_buffer(b"unchanged", 10)
        file_part = ctypes.c_void_p(1)
        result = self.fullpath(b"/long/component/../filename", 4, buf, ctypes.byref(file_part))
        self.assertEqual(result, len("/long/filename") + 1)
        self.assertEqual(buf.value, b"unchanged")
        self.assertIsNone(file_part.value)

    def test_size_query_and_exact_buffer(self) -> None:
        path = b"/a/b/../filename"
        expected = b"/a/filename"
        needed = self.fullpath(path, 0, None, None)
        self.assertEqual(needed, len(expected) + 1)
        buf = ctypes.create_string_buffer(needed)
        part = ctypes.c_void_p()
        self.assertEqual(self.fullpath(path, needed, buf, ctypes.byref(part)), len(expected))
        self.assertEqual(buf.value, expected)
        self.assertEqual(ctypes.string_at(part), b"filename")

    def test_input_and_output_may_alias(self) -> None:
        buf = ctypes.create_string_buffer(b"/a/b/../file", 100)
        self.assertEqual(self.fullpath(buf, len(buf), buf, None), len("/a/file"))
        self.assertEqual(buf.value, b"/a/file")

    def test_enumeration_distinguishes_empty_missing_and_end(self) -> None:
        find = self.host.FindFirstFileA
        find.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
        find.restype = ctypes.c_void_p
        next_file = self.host.FindNextFileA
        next_file.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        close = self.host.FindClose
        close.argtypes = [ctypes.c_void_p]
        data = ctypes.create_string_buffer(512)
        self.assertEqual(next_file(ctypes.c_void_p(-1), data), 0)
        self.assertEqual(self.host.GetLastError(), 6)
        self.assertFalse(close(ctypes.c_void_p(-1)))
        with tempfile.TemporaryDirectory() as directory:
            pattern = (directory + "/*").encode()
            self.assertEqual(find(pattern, data), ctypes.c_void_p(-1).value)
            self.assertEqual(self.host.GetLastError(), 2)
            Path(directory, "one.txt").touch()
            handle = find(pattern, data)
            self.assertNotEqual(handle, ctypes.c_void_p(-1).value)
            self.assertEqual(next_file(handle, data), 0)
            self.assertEqual(self.host.GetLastError(), 18)
            self.assertTrue(close(handle))
            self.assertEqual(find((directory + "/absent/*").encode(), data), ctypes.c_void_p(-1).value)
            self.assertEqual(self.host.GetLastError(), 3)

    def test_overlong_input_is_rejected(self) -> None:
        buf = ctypes.create_string_buffer(4096)
        self.assertEqual(self.fullpath(b"/" + b"a" * 8192, len(buf), buf, None), 0)


if __name__ == "__main__":
    unittest.main()
