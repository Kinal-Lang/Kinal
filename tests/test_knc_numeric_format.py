"""KNC numeric ABI fixtures, independent of the compiler's binary writer.

Run against a rebuilt VM with KINAL_VM_PATH=/path/to/kinalvm. Set
KINAL_V2_VM_PATH to an older version-2-only VM for the forward-rejection check.
Both the ordinary file loader and packed executable loader are exercised.
"""
from __future__ import annotations

from fractions import Fraction
import math
import os
import random
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def program(version: int, *, integers: tuple[int, ...] = (),
            floats: tuple[float, ...] = (), code: bytes = b"\x3b",
            flags: int = 0) -> bytes:
    header = b"KNC2" + struct.pack("<HH7i", version, flags, 0, 0,
                                  len(integers), len(floats), 0, 0, 1)
    integer_format = "<i" if version == 2 else "<q"
    constants = b"".join(struct.pack(integer_format, value) for value in integers)
    constants += b"".join(struct.pack("<d", value) for value in floats)
    name = b"NumericFixture.Main"
    function = struct.pack("<4i", 0, 8, 0, len(name)) + name
    function += struct.pack("<i", len(code)) + code
    return header + constants + function


def load(opcode: int, register: int, index: int) -> bytes:
    return bytes((opcode, register)) + struct.pack("<H", index)


def print_register(builtin: int, register: int) -> bytes:
    return bytes((44, 255)) + struct.pack("<H", builtin) + bytes((1, register, 255, 255, 255))


def integer_program(version: int, values: tuple[int, ...]) -> bytes:
    code = b"".join(load(0, 0, index) + print_register(8, 0)
                    for index in range(len(values))) + b"\x3b"
    return program(version, integers=values, code=code)


def f32(value: float) -> float:
    try:
        return struct.unpack("<f", struct.pack("<f", value))[0]
    except OverflowError:
        return math.copysign(math.inf, value)


def integer_f32(value: int) -> float:
    """Round an integer directly, without an intermediate binary64 rounding."""
    magnitude = abs(value)
    shift = max(0, magnitude.bit_length() - 24)
    if shift:
        significand, remainder = divmod(magnitude, 1 << shift)
        halfway = 1 << (shift - 1)
        if remainder > halfway or (remainder == halfway and significand & 1):
            significand += 1
        magnitude = significand << shift
    return math.copysign(float(magnitude), value)


def rational_f32(value: Fraction) -> float:
    """Round exact arithmetic to binary32, independently of host float arithmetic."""
    if not value:
        return 0.0
    sign = -1.0 if value < 0 else 1.0
    numerator, denominator = abs(value).as_integer_ratio()
    exponent = numerator.bit_length() - denominator.bit_length()
    if ((exponent >= 0 and numerator < denominator << exponent) or
            (exponent < 0 and numerator << -exponent < denominator)):
        exponent -= 1
    shift = max(exponent - 23, -149)
    if shift < 0:
        numerator <<= -shift
    else:
        denominator <<= shift
    significand, remainder = divmod(numerator, denominator)
    if 2 * remainder > denominator or (2 * remainder == denominator and significand & 1):
        significand += 1
    result = math.ldexp(float(significand), shift)
    return math.copysign(math.inf if result >= 2.0 ** 128 else result, sign)


class NumericFormatTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        default_vm = ROOT / "out/stage/host-debug" / ("kinalvm.exe" if os.name == "nt" else "kinalvm")
        cls.vm = Path(os.environ.get("KINAL_VM_PATH", str(default_vm))).resolve()
        if not cls.vm.is_file():
            raise unittest.SkipTest("build KinalVM or set KINAL_VM_PATH")
        cls.temp = tempfile.TemporaryDirectory(prefix="knc-numeric-")
        cls.directory = Path(cls.temp.name)
        cls.sequence = 0

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temp.cleanup()

    def run_fixture(self, data: bytes, *, embedded: bool = False,
                    vm: Path | None = None, disassemble: bool = False) -> subprocess.CompletedProcess[str]:
        type(self).sequence += 1
        vm = vm or self.vm
        name = f"fixture-{self.sequence}"
        if embedded:
            path = self.directory / (name + (".exe" if os.name == "nt" else ""))
            shutil.copyfile(vm, path)
            with path.open("ab") as output:
                output.write(data)
                output.write(struct.pack("<I", len(data)) + b"KNCE")
            path.chmod(path.stat().st_mode | 0o111)
            command = [str(path)]
        else:
            path = self.directory / (name + ".knc")
            path.write_bytes(data)
            command = [str(vm), *(["--disasm"] if disassemble else []), str(path)]
        return subprocess.run(command, capture_output=True, text=True, timeout=20)

    def assert_output(self, data: bytes, expected: list[str]) -> None:
        for embedded in (False, True):
            with self.subTest(embedded=embedded):
                result = self.run_fixture(data, embedded=embedded)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(result.stdout.splitlines(), expected)

    def assert_rejected(self, data: bytes, message: str, *, vm: Path | None = None) -> None:
        for embedded in (False, True):
            with self.subTest(embedded=embedded):
                result = self.run_fixture(data, embedded=embedded, vm=vm)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn(message, result.stdout + result.stderr)

    def test_v2_signed_i32_compatibility(self) -> None:
        values = (-2147483648, -2147483647, -1, 0, 1, 2147483646, 2147483647)
        self.assert_output(integer_program(2, values), list(map(str, values)))

    def test_v3_signed_i64_boundaries(self) -> None:
        values = (-(1 << 63), -(1 << 63) + 1, -(1 << 32), -(1 << 31) - 1,
                  -(1 << 31), -1, 0, 1, (1 << 31) - 1, 1 << 31,
                  (1 << 32) - 1, 1 << 32, (1 << 63) - 2, (1 << 63) - 1)
        self.assert_output(integer_program(3, values), list(map(str, values)))

    def test_unknown_versions_rejected(self) -> None:
        for version in (0, 1, 4, 65535):
            with self.subTest(version=version):
                self.assert_rejected(program(version), f"Unsupported .knc version: {version}")

    def test_flags_rejected_for_both_versions(self) -> None:
        for version in (2, 3):
            with self.subTest(version=version):
                self.assert_rejected(program(version, flags=1), "Unsupported .knc flags: 1")

    def test_truncated_i64_rejected(self) -> None:
        data = integer_program(3, ((1 << 63) - 1,))
        # A complete header and only seven of the eight integer-constant bytes.
        self.assert_rejected(data[:43], "Invalid or truncated KNC2 payload")

    def test_old_vm_rejects_v3_cleanly(self) -> None:
        path = os.environ.get("KINAL_V2_VM_PATH")
        if not path:
            self.skipTest("set KINAL_V2_VM_PATH to a saved version-2-only VM")
        old_vm = Path(path).resolve()
        self.assert_rejected(integer_program(3, (1 << 32,)),
                             "Unsupported .knc version: 3", vm=old_vm)

    def test_float_to_f32_rounding(self) -> None:
        halfway = 1.0 + 2.0 ** -24
        values = (0.1, -0.1, 16777217.0, -16777217.0,
                  math.nextafter(halfway, 0.0), halfway, math.nextafter(halfway, math.inf),
                  1.0 + 3.0 * 2.0 ** -24, 2.0 ** -149, 2.0 ** -150,
                  math.nextafter(2.0 ** -150, math.inf), -2.0 ** -150,
                  2.0 ** -126, 2.0 ** 128, -2.0 ** 128, math.inf, -math.inf)
        constants: list[float] = []
        code = bytearray()
        for value in values:
            index = len(constants)
            constants.extend((value, f32(value)))
            code += load(1, 0, index) + bytes((137, 0, 0))  # aliasing is valid
            code += load(1, 1, index + 1) + bytes((16, 2, 0, 1))
            code += print_register(9, 2)
        code += b"\x3b"
        self.assert_output(program(3, floats=tuple(constants), code=bytes(code)), ["true"] * len(values))

    def test_float_to_f32_preserves_nan_and_negative_zero(self) -> None:
        code = load(1, 0, 0) + bytes((137, 0, 0, 17, 2, 0, 0)) + print_register(9, 2)
        code += load(1, 0, 1) + bytes((137, 0, 0))
        code += load(1, 1, 2) + bytes((13, 0, 1, 0))
        code += load(1, 1, 3) + bytes((16, 2, 0, 1)) + print_register(9, 2) + b"\x3b"
        data = program(3, floats=(math.nan, -0.0, 1.0, -math.inf), code=code)
        self.assert_output(data, ["true", "true"])

    def test_int_to_f32_avoids_double_rounding(self) -> None:
        signed = (0, 1, -1, (1 << 62) + (1 << 38) + 1,
                  -((1 << 62) + (1 << 38) + 1), -(1 << 63), (1 << 63) - 1)
        unsigned = ((1 << 63) + (1 << 39) + 1, (1 << 64) - 1)
        values = signed + unsigned
        integers = tuple(value if value < 1 << 63 else value - (1 << 64) for value in values)
        floats = tuple(integer_f32(value) for value in values)
        code = bytearray()
        for index in range(len(values)):
            code += load(0, 0, index) + bytes((138, 0, 0, int(index >= len(signed))))
            code += load(1, 1, index) + bytes((16, 2, 0, 1)) + print_register(9, 2)
        code += b"\x3b"
        self.assert_output(program(3, integers=integers, floats=floats, code=bytes(code)),
                           ["true"] * len(values))

    def test_f32_arithmetic_rounds_after_each_operation(self) -> None:
        rng = random.Random(0xF32)
        smallest = 2.0 ** -149
        largest = float.fromhex("0x1.fffffep127")
        pairs = [(1.0, 2.0 ** -24), (largest, largest), (smallest, 2.0),
                 (1.0, 3.0), (smallest, largest), (1.0, -1.0)]
        for _ in range(64):
            pair = []
            for _ in range(2):
                raw = rng.randrange(1, 0x7f800000) | (rng.randrange(2) << 31)
                pair.append(struct.unpack("<f", struct.pack("<I", raw))[0])
            pairs.append(tuple(pair))
        constants: list[float] = []
        code = bytearray()
        for lhs, rhs in pairs:
            left, right = Fraction(lhs), Fraction(rhs)
            for opcode, exact in ((10, left + right), (11, left - right),
                                  (12, left * right), (13, left / right)):
                index = len(constants)
                constants.extend((lhs, rhs, rational_f32(exact)))
                code += load(1, 0, index) + load(1, 1, index + 1)
                code += bytes((opcode, 0, 0, 1, 137, 0, 0))
                code += load(1, 1, index + 2) + bytes((16, 2, 0, 1))
                code += print_register(9, 2)
        code += b"\x3b"
        self.assert_output(program(3, floats=tuple(constants), code=bytes(code)),
                           ["true"] * (len(pairs) * 4))

    def test_f32_opcodes_reject_invalid_inputs(self) -> None:
        self.assert_rejected(program(3, integers=(1,), code=load(0, 0, 0) + bytes((137, 0, 0, 59))),
                             "Expected float register")
        self.assert_rejected(program(3, floats=(1.0,), code=load(1, 0, 0) + bytes((138, 0, 0, 0, 59))),
                             "Expected int register")
        self.assert_rejected(program(3, integers=(1,), code=load(0, 0, 0) + bytes((138, 0, 0, 2, 59))),
                             "Unsupported IntToF32 signedness: 2")

    def test_disassembly_names_sizes_and_operands(self) -> None:
        code = load(0, 0, 0) + bytes((138, 1, 0, 0, 138, 2, 0, 1, 137, 3, 2, 59))
        result = self.run_fixture(program(3, integers=(1 << 32,), code=code), disassemble=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("4294967296", result.stdout)
        self.assertRegex(result.stdout, r"IntToF32\s+r1, r0, signed")
        self.assertRegex(result.stdout, r"IntToF32\s+r2, r0, unsigned")
        self.assertRegex(result.stdout, r"FloatToF32\s+r3, r2")
        self.assertRegex(result.stdout, r"0015:\s+Halt")


if __name__ == "__main__":
    unittest.main()
