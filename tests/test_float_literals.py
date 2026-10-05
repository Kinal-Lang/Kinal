"""Exact decimal-to-binary64 rounding, independent of LLVM and the host locale."""
from __future__ import annotations

import ctypes
import decimal
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def bits(value: float) -> int:
    return struct.unpack("=Q", struct.pack("=d", value))[0]


def fixed(value: decimal.Decimal) -> str:
    text = format(value, "f")
    return text if "." in text else text + ".0"


class FloatLiteralTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        compiler = shutil.which(os.environ.get("CC", "cc"))
        if not compiler:
            raise unittest.SkipTest("a C compiler is required")
        cls.temp = tempfile.TemporaryDirectory()
        source = Path(cls.temp.name) / "float_literals.c"
        source.write_text('''#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <fenv.h>
#define kn_memcpy memcpy
#include "kn_parser_float.inc"
uint64_t parse_bits(const char *text, size_t length) {
    double value = parse_float_decimal(text, length);
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}
int rounding_mode(int mode) {
    return fesetround(mode == 1 ? FE_UPWARD : mode == 2 ? FE_DOWNWARD : FE_TONEAREST);
}
''', encoding="utf-8")
        library = Path(cls.temp.name) / ("float_literals.dylib" if sys.platform == "darwin" else "float_literals.so")
        subprocess.run([compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                        "-shared", "-fPIC", "-ffreestanding", "-fno-builtin",
                        "-I", str(ROOT / "apps/kinal/src/parser"), str(source), "-lm",
                        "-o", str(library)], check=True)
        cls.library = ctypes.CDLL(str(library))
        cls.parse = cls.library.parse_bits
        cls.parse.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        cls.parse.restype = ctypes.c_uint64
        cls.rounding_mode = cls.library.rounding_mode
        cls.rounding_mode.argtypes = [ctypes.c_int]
        cls.rounding_mode.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temp.cleanup()

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


if __name__ == "__main__":
    unittest.main()
