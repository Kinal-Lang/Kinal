"""Exact decimal-to-binary64 rounding, independent of LLVM and the host locale."""
from __future__ import annotations

import _ctypes
import ctypes
import decimal
import gc
import os
import random
import shutil
import struct
import subprocess
import sys
import sysconfig
import tempfile
import unittest
import weakref
from pathlib import Path
from unittest import mock

from run_tests import pe_machine

ROOT = Path(__file__).resolve().parents[1]


def bits(value: float) -> int:
    return struct.unpack("=Q", struct.pack("=d", value))[0]


def fixed(value: decimal.Decimal) -> str:
    text = format(value, "f")
    return text if "." in text else text + ".0"


def library_build_command(compiler: str, source: Path, library: Path, windows_gnu: bool = False) -> list[str]:
    # This harness calls hosted CRT fenv/memcpy APIs. In freestanding mode,
    # Clang's float.h omits SDK definitions required by Windows UCRT fenv.h.
    command = [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
               "-shared", "-fhosted", "-fno-builtin"]
    if sys.platform == "win32" and not windows_gnu:
        # Match the Python process, not a possibly emulated compiler or the OS.
        # Generic cc on Windows ARM64 runners can be an x64 MinGW installation.
        targets = {"win-arm64": "aarch64-pc-windows-msvc",
                   "win-amd64": "x86_64-pc-windows-msvc", "win32": "i686-pc-windows-msvc"}
        python_platform = sysconfig.get_platform()
        if python_platform not in targets:
            raise RuntimeError(f"unsupported Windows Python platform: {python_platform}")
        command.append(f"--target={targets[python_platform]}")
    elif sys.platform != "win32":
        command.append("-fPIC")
    command.extend(["-I", str(ROOT / "apps/kinal/src/parser"), str(source)])
    if sys.platform != "win32" or windows_gnu:
        command.append("-lm")
    return [*command, "-o", str(library)]


def validate_library_architecture(library: Path) -> None:
    if sys.platform != "win32":
        return
    python_machine = pe_machine(Path(sys.executable))
    library_machine = pe_machine(library)
    if python_machine not in {"x86", "x64", "arm64"} or library_machine != python_machine:
        raise RuntimeError(f"float fixture architecture mismatch: Python={python_machine}, "
                           f"library={library_machine} ({library})")


class FloatLiteralTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        compiler = shutil.which(os.environ.get("CC", "clang" if sys.platform == "win32" else "cc"))
        if not compiler:
            if sys.platform == "win32":
                raise RuntimeError("the Windows float fixture requires Clang and the MSVC/Windows SDK environment")
            raise unittest.SkipTest("a C compiler is required")
        windows_gnu = False
        if sys.platform == "win32" and os.environ.get("CC"):
            # Retain explicitly selected MinGW compilers. The PE check below
            # still rejects an ABI mismatch rather than silently skipping tests.
            target = subprocess.check_output([compiler, "-dumpmachine"], text=True).strip().lower()
            windows_gnu = "mingw" in target or "windows-gnu" in target
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        source = Path(cls.temp.name) / "float_literals.c"
        source.write_text('''#if !__STDC_HOSTED__
#error "the float test harness requires hosted CRT headers"
#endif
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <fenv.h>
#define kn_memcpy memcpy
#include "kn_parser_float.inc"
#ifdef _WIN32
#define TEST_EXPORT __declspec(dllexport)
#else
#define TEST_EXPORT
#endif
TEST_EXPORT uint64_t parse_bits(const char *text, size_t length) {
    double value = parse_float_decimal(text, length);
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}
TEST_EXPORT int rounding_mode(int mode) {
    return fesetround(mode == 1 ? FE_UPWARD : mode == 2 ? FE_DOWNWARD : FE_TONEAREST);
}
''', encoding="utf-8")
        extension = ".dll" if sys.platform == "win32" else ".dylib" if sys.platform == "darwin" else ".so"
        library = Path(cls.temp.name) / ("float_literals" + extension)
        subprocess.run(library_build_command(compiler, source, library, windows_gnu), check=True)
        validate_library_architecture(library)
        cls.parse = cls.rounding_mode = None
        cls.library = ctypes.CDLL(str(library))
        # Class cleanups also run when a later setup step fails. LIFO ordering
        # releases the DLL before Windows is asked to delete its directory.
        cls.addClassCleanup(cls.unload_library)
        cls.parse = cls.library.parse_bits
        cls.parse.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        cls.parse.restype = ctypes.c_uint64
        cls.rounding_mode = cls.library.rounding_mode
        cls.rounding_mode.argtypes = [ctypes.c_int]
        cls.rounding_mode.restype = ctypes.c_int

    @classmethod
    def unload_library(cls) -> None:
        library = cls.library
        handle = library._handle
        references = [weakref.ref(library)]
        # CDLL caches each _FuncPtr, and each pointer keeps the CDLL alive.
        # Break those cycles explicitly; dropping class aliases alone leaves
        # callable objects pointing at unmapped code until cyclic GC runs.
        for name in ("parse_bits", "rounding_mode"):
            function = library.__dict__.pop(name, None)
            if function is not None:
                references.append(weakref.ref(function))
        for name in ("parse", "rounding_mode"):
            function = vars(cls).get(name)
            if function is not None:
                references.append(weakref.ref(function))
            setattr(cls, name, None)
        del function
        cls.library = None
        del library
        if sys.platform == "win32":
            # _FuncPtr can also contain internal self-cycles. Collect those
            # explicitly, then refuse to unmap code still retained by a caller
            # or a failure traceback. Never rely on a future GC pass.
            gc.collect()
            if any(reference() is not None for reference in references):
                raise RuntimeError("cannot unload float fixture: library or function references remain")
            _ctypes.FreeLibrary(handle)

    def check_literal(self, text: str, expected: int | None = None) -> None:
        encoded = text.encode("ascii")
        if expected is None:
            expected = bits(float(text.rstrip("fF")))
        self.assertEqual(self.parse(encoded, len(encoded)), expected, text)

    def test_exact_decimals_and_suffixes(self) -> None:
        for text in ("0.0", "000.000", "0.75", "0.25", "0.125", "0.0625", "1.875",
                     "1f", "0.75F", "0000000.75000000f", "9007199254740991.0",
                     "9007199254740992.0", "9007199254740993.0", "18446744073709551615.0"):
            with self.subTest(text=text):
                self.check_literal(text)

    def test_nonterminated_token_is_length_bounded(self) -> None:
        self.assertEqual(self.parse(b"0.759999999", 4), bits(0.75))
        self.assertEqual(self.parse(b"0.75F12345", 5), bits(0.75))

    def test_rounding_boundaries(self) -> None:
        with decimal.localcontext() as context:
            context.prec = 1600
            two = decimal.Decimal(2)
            # Ordinary, smallest-normal, subnormal and maximum-finite boundaries.
            for raw in (0, 1, 2, 0x000fffffffffffff, 0x0010000000000000,
                        0x3fefffffffffffff, 0x3ff0000000000000, 0x3ff0000000000001,
                        0x4340000000000000, 0x7feffffffffffffe):
                low = decimal.Decimal.from_float(struct.unpack("=d", struct.pack("=Q", raw))[0])
                high = decimal.Decimal.from_float(struct.unpack("=d", struct.pack("=Q", raw + 1))[0])
                midpoint = (low + high) / two
                delta = two ** -1200
                for value in (low, midpoint - delta, midpoint, midpoint + delta, high):
                    self.check_literal(fixed(value))
            maximum = decimal.Decimal.from_float(sys.float_info.max)
            overflow_midpoint = maximum + two ** 970
            for value in (maximum, overflow_midpoint - 1, overflow_midpoint, overflow_midpoint + 1):
                self.check_literal(fixed(value))

    def test_extreme_lengths_and_sticky_tail(self) -> None:
        for text in ("0." + "0" * 20000, "0." + "0" * 20000 + "1",
                     "1" + "0" * 20000 + ".0", "0" * 20000 + "0.75",
                     "0.75" + "0" * 20000, "0.75" + "0" * 20000 + "1"):
            self.check_literal(text)
        # A digit beyond our retained decimal window must still break a tie.
        with decimal.localcontext() as context:
            context.prec = 1600
            for midpoint in (decimal.Decimal(2) ** -1075,
                             decimal.Decimal(1) + decimal.Decimal(2) ** -53):
                text = fixed(midpoint)
                text += "0" * (1400 - len(text)) + "1"
                self.check_literal(text)

    def test_seeded_decimal_corpus(self) -> None:
        rng = random.Random(0xDEC1A1)
        for _ in range(2000):
            length = rng.randrange(1, 90)
            digits = "".join(str(rng.randrange(10)) for _ in range(length))
            point = rng.randrange(length + 1)
            text = (digits[:point] or "0") + "." + (digits[point:] or "0")
            self.check_literal(text)
        for _ in range(300):
            raw = rng.randrange(0x7ff0000000000000)
            value = struct.unpack("=d", struct.pack("=Q", raw))[0]
            self.check_literal(fixed(decimal.Decimal.from_float(value)), raw)

    def test_process_rounding_mode_does_not_change_literals(self) -> None:
        samples = [(text, bits(float(text))) for text in
                   ("0.75", "0.1", "9007199254740993.0", "1.00000000000000011102230246251565404236316680908203125")]
        try:
            for mode in (1, 2):
                if self.rounding_mode(mode) != 0:
                    self.skipTest("host does not support changing the floating-point rounding mode")
                for text, expected in samples:
                    self.check_literal(text, expected)
        finally:
            self.rounding_mode(0)


class FloatLibraryFixtureTests(unittest.TestCase):
    def test_fixture_uses_hosted_headers_without_builtin_substitution(self) -> None:
        for system, windows_gnu in (("win32", False), ("win32", True), ("linux", False), ("darwin", False)):
            with self.subTest(system=system, windows_gnu=windows_gnu), \
                 mock.patch.object(sys, "platform", system), \
                 mock.patch.object(sysconfig, "get_platform", return_value="win-arm64"):
                command = library_build_command("cc", Path("input.c"), Path("output"), windows_gnu)
                self.assertIn("-fhosted", command)
                self.assertIn("-fno-builtin", command)
                self.assertNotIn("-ffreestanding", command)

    def test_windows_target_matches_python_architecture(self) -> None:
        for python_platform, target in (("win-arm64", "aarch64"), ("win-amd64", "x86_64"),
                                        ("win32", "i686")):
            with self.subTest(python_platform=python_platform), \
                 mock.patch.object(sys, "platform", "win32"), \
                 mock.patch.object(sysconfig, "get_platform", return_value=python_platform):
                command = library_build_command("clang", Path("input.c"), Path("output.dll"))
                self.assertIn(f"--target={target}-pc-windows-msvc", command)
                self.assertNotIn("-lm", command)
                self.assertNotIn("-fPIC", command)

    def test_windows_library_machine_matches_python_executable(self) -> None:
        def write_pe(path: Path, machine: int) -> None:
            header = bytearray(64)
            header[:2] = b"MZ"
            header[60:64] = (64).to_bytes(4, "little")
            path.write_bytes(header + b"PE\0\0" + machine.to_bytes(2, "little"))

        with tempfile.TemporaryDirectory() as directory:
            executable, library = Path(directory) / "python.exe", Path(directory) / "fixture.dll"
            for python_machine in (0x014C, 0x8664, 0xAA64):
                write_pe(executable, python_machine)
                with self.subTest(python_machine=python_machine), \
                     mock.patch.object(sys, "platform", "win32"), \
                     mock.patch.object(sys, "executable", str(executable)):
                    write_pe(library, python_machine)
                    validate_library_architecture(library)
                    write_pe(library, 0x8664 if python_machine != 0x8664 else 0xAA64)
                    with self.assertRaisesRegex(RuntimeError, "fixture architecture mismatch"):
                        validate_library_architecture(library)
                    library.write_bytes(b"not a DLL")
                    with self.assertRaisesRegex(RuntimeError, "library=not-PE"):
                        validate_library_architecture(library)

    def test_posix_build_retains_pic_and_math_library(self) -> None:
        with mock.patch.object(sys, "platform", "linux"):
            command = library_build_command("cc", Path("input.c"), Path("output.so"))
        self.assertIn("-fPIC", command)
        self.assertIn("-lm", command)
        self.assertFalse(any(argument.startswith("--target=") for argument in command))

    def test_explicit_mingw_compiler_retains_its_native_target(self) -> None:
        with mock.patch.object(sys, "platform", "win32"):
            command = library_build_command("gcc", Path("input.c"), Path("output.dll"), windows_gnu=True)
        self.assertIn("-lm", command)
        self.assertNotIn("-fPIC", command)
        self.assertFalse(any(argument.startswith("--target=") for argument in command))

    def test_unknown_windows_architecture_is_reported(self) -> None:
        with mock.patch.object(sys, "platform", "win32"), \
             mock.patch.object(sysconfig, "get_platform", return_value="unknown"):
            with self.assertRaisesRegex(RuntimeError, "unsupported Windows Python platform"):
                library_build_command("clang", Path("input.c"), Path("output.dll"))

    def fixture(self) -> type[FloatLiteralTests]:
        class Fixture(FloatLiteralTests):
            pass
        class Library:
            _handle = 123
        class Function:
            def __init__(self, library: Library) -> None:
                self.library = library
        Fixture.library = Library()
        Fixture.parse = Fixture.library.parse_bits = Function(Fixture.library)
        Fixture.rounding_mode = Fixture.library.rounding_mode = Function(Fixture.library)
        return Fixture

    def test_real_ctypes_references_are_released_before_unload(self) -> None:
        release = _ctypes.FreeLibrary if sys.platform == "win32" else _ctypes.dlclose
        for state in ("complete", "cached_only", "library_only"):
            with self.subTest(state=state):
                fixture = self.fixture()
                fixture.setUpClass()
                path = Path(fixture.temp.name)
                references = [weakref.ref(fixture.library), weakref.ref(fixture.parse),
                              weakref.ref(fixture.rounding_mode)]
                # Model setup failing before class aliases are assigned, or
                # before any function lookup. Cleanup must work in both cases.
                if state != "complete":
                    fixture.parse = fixture.rounding_mode = None
                if state == "library_only":
                    fixture.library.__dict__.pop("parse_bits")
                    fixture.library.__dict__.pop("rounding_mode")

                def unload(handle: int) -> None:
                    self.assertTrue(all(reference() is None for reference in references))
                    release(handle)

                enabled = gc.isenabled()
                gc.disable()
                try:
                    with mock.patch.object(sys, "platform", "win32"), \
                         mock.patch.object(_ctypes, "FreeLibrary", side_effect=unload, create=True) as free:
                        fixture.doClassCleanups()
                    self.assertEqual(fixture.tearDown_exceptions, [])
                    free.assert_called_once()
                    self.assertFalse(path.exists())
                finally:
                    if enabled:
                        gc.enable()

    def test_real_ctypes_retained_alias_or_traceback_blocks_unload(self) -> None:
        release = _ctypes.FreeLibrary if sys.platform == "win32" else _ctypes.dlclose

        def fail_with_alias(function: object) -> None:
            raise ValueError("fixture call failed")

        for state in ("alias", "traceback"):
            with self.subTest(state=state):
                fixture = self.fixture()
                fixture.setUpClass()
                handle = fixture.library._handle
                references = [weakref.ref(fixture.library), weakref.ref(fixture.parse)]
                retained = fixture.parse
                if state == "traceback":
                    try:
                        fail_with_alias(retained)
                    except ValueError as error:
                        retained = error
                try:
                    with mock.patch.object(sys, "platform", "win32"), \
                         mock.patch.object(_ctypes, "FreeLibrary", create=True) as free:
                        with self.assertRaisesRegex(RuntimeError, "function references remain"):
                            fixture.unload_library()
                        free.assert_not_called()
                    self.assertTrue(all(reference() is not None for reference in references))
                finally:
                    # Release the deliberately retained alias/traceback before
                    # unloading; avoid re-running the already consumed cleanup.
                    retained = None
                    gc.collect()
                    self.assertTrue(all(reference() is None for reference in references))
                    release(handle)
                    fixture._class_cleanups.clear()
                    fixture.temp.cleanup()

    def test_windows_unloads_before_removing_temporary_files(self) -> None:
        fixture = self.fixture()
        events = []

        def unload(handle: int) -> None:
            self.assertEqual(handle, 123)
            self.assertIsNone(fixture.parse)
            self.assertIsNone(fixture.rounding_mode)
            self.assertIsNone(fixture.library)
            events.append("unload")

        def remove_directory() -> None:
            # Model the Windows file lock even on Linux/macOS test hosts.
            if events != ["unload"]:
                raise PermissionError("DLL is still loaded")
            events.append("cleanup")

        fixture.addClassCleanup(remove_directory)
        fixture.addClassCleanup(fixture.unload_library)
        with mock.patch.object(sys, "platform", "win32"), \
             mock.patch.object(_ctypes, "FreeLibrary", side_effect=unload, create=True):
            fixture.doClassCleanups()
        self.assertEqual(fixture.tearDown_exceptions, [])
        self.assertEqual(events, ["unload", "cleanup"])

    def test_windows_unload_failure_is_not_suppressed(self) -> None:
        fixture = self.fixture()
        with mock.patch.object(sys, "platform", "win32"), \
             mock.patch.object(_ctypes, "FreeLibrary", side_effect=OSError("unload failed"), create=True):
            with self.assertRaisesRegex(OSError, "unload failed"):
                fixture.unload_library()


if __name__ == "__main__":
    unittest.main()
