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
            for suffix, expected in (("/absent.txt", 2), ("/absent/file.txt", 3),
                                     ("/one.txt/*", 3), ("/one.txt/child/*", 3)):
                with self.subTest(suffix=suffix):
                    self.assertEqual(find((directory + suffix).encode(), data), ctypes.c_void_p(-1).value)
                    self.assertEqual(self.host.GetLastError(), expected)

    def test_overlong_input_is_rejected(self) -> None:
        buf = ctypes.create_string_buffer(4096)
        self.assertEqual(self.fullpath(b"/" + b"a" * 8192, len(buf), buf, None), 0)


@unittest.skipIf(os.name == "nt", "POSIX host shim")
class PlatformGlobErrorTests(unittest.TestCase):
    """Exercise BSD and GNU glob error outcomes on every POSIX test host."""

    @classmethod
    def setUpClass(cls) -> None:
        compiler = shutil.which(os.environ.get("CC", "cc"))
        if not compiler:
            raise unittest.SkipTest("a C compiler is required")
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        stub = Path(cls.temp.name) / "glob_errors.c"
        stub.write_text('''#include <errno.h>
#include <glob.h>
static int mode;
void set_glob_error_mode(int value) { mode = value; }
int glob(const char *pattern, int flags,
         int (*on_error)(const char *, int), glob_t *matches) {
    (void)pattern; (void)flags; (void)on_error; (void)matches;
    errno = mode == 1 || mode == 3 ? ENOENT
          : mode == 2 || mode == 4 ? ENOTDIR
          : mode == 5 ? EACCES : mode == 6 ? ENOMEM : 0;
    return mode == 6 ? GLOB_NOSPACE : mode >= 3 ? GLOB_ABORTED : GLOB_NOMATCH;
}
''', encoding="utf-8")
        library = Path(cls.temp.name) / ("glob_errors.dylib" if sys.platform == "darwin" else "glob_errors.so")
        subprocess.run([compiler, "-shared", "-fPIC", "-Dglob=kn_test_glob",
                        "-I", str(ROOT / "libs/runtime/include"),
                        str(ROOT / "libs/runtime/src/kn_platform_host.c"), str(stub),
                        "-o", str(library)], check=True)
        cls.host = ctypes.CDLL(str(library))
        cls.host.FindFirstFileA.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
        cls.host.FindFirstFileA.restype = ctypes.c_void_p
        cls.host.set_glob_error_mode.argtypes = [ctypes.c_int]

    def assert_error(self, pattern: str, expected: int) -> None:
        self.assertEqual(self.host.FindFirstFileA(pattern.encode(), None), ctypes.c_void_p(-1).value)
        self.assertEqual(self.host.GetLastError(), expected)

    def test_no_match_distinguishes_parent_from_leaf_without_relying_on_errno(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "file").touch()
            for mode in (0, 1, 2):
                self.host.set_glob_error_mode(mode)
                for suffix, expected in (("/*", 2), ("/missing.txt", 2), ("/absent/*", 3),
                                         ("/absent/file", 3), ("/file/*", 3), ("/file/child/*", 3)):
                    with self.subTest(mode=mode, suffix=suffix):
                        self.assert_error(directory + suffix, expected)
                self.assert_error("missing-relative-file", 2)
                self.assert_error("/*", 2)

    def test_aborted_and_out_of_memory_errors_are_preserved(self) -> None:
        for mode, expected in ((3, 3), (4, 3), (5, 5), (6, 8)):
            with self.subTest(mode=mode):
                self.host.set_glob_error_mode(mode)
                self.assert_error("unused/*", expected)


if __name__ == "__main__":
    unittest.main()
