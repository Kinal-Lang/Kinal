from __future__ import annotations

import re
import unittest

from check_gc_frames import (check_character_argument_root, check_converted_argument_roots,
                             check_entry_roots, check_native_memory_abi)


def entry_ir(*, legacy: bool = False) -> str:
    lines = ["define i1 @Tests_CheckLoopRoots_0() {", "entry:"]
    for index in range(5):
        lines.append(f"  %slot{index} = alloca ptr, align 8")
    if legacy:
        lines.append("  %gc.frame = call ptr @__kn_gc_push_frame()")
    for index in range(5):
        lines.append(f"  store ptr null, ptr %slot{index}, align 8")
        function = "__kn_gc_add_root" if legacy else "__kn_sh_IO_Kinal_Runtime_GarbageCollector_AddRoot_3"
        frame = "%gc.frame" if legacy else "%frame"
        lines.append(f"  call void @{function}(ptr {frame}, ptr %slot{index}, i64 8)")
    return "\n".join(lines + ["  ret i1 true", "}", ""])


class EntryRootTests(unittest.TestCase):
    def test_accepts_initialized_entry_slots(self):
        check_entry_roots(entry_ir())
        check_entry_roots(entry_ir(legacy=True), legacy_stage0=True)

    def test_rejects_repeated_loop_registration(self):
        for legacy in (False, True):
            with self.subTest(legacy=legacy), self.assertRaisesRegex(AssertionError, "repeats"):
                ir = entry_ir(legacy=legacy).replace("  store ptr null, ptr %slot3", "loop_body:\n  store ptr null, ptr %slot3")
                check_entry_roots(ir, legacy_stage0=legacy)

    def test_rejects_uninitialized_slots(self):
        ir = entry_ir().replace("  store ptr null, ptr %slot2, align 8\n", "")
        with self.assertRaisesRegex(AssertionError, "uninitialized"):
            check_entry_roots(ir)

    def test_rejects_duplicate_regions(self):
        ir = entry_ir().replace("ptr %slot4, i64 8", "ptr %slot3, i64 8")
        with self.assertRaisesRegex(AssertionError, "duplicate"):
            check_entry_roots(ir)

    def test_rejects_heap_scan_regions(self):
        ir = entry_ir().replace("ptr %slot4, i64 8", "ptr %heap_cell, i64 8")
        with self.assertRaisesRegex(AssertionError, "entry-owned"):
            check_entry_roots(ir)


class ConvertedArgumentRootTests(unittest.TestCase):
    @staticmethod
    def fixture_ir() -> str:
        lines = ["define i1 @Tests_CheckConvertedArguments_0() {", "entry:"]
        for index in range(7):
            lines.extend([
                f"  %root{index} = alloca {{ i64, i64 }}, align 8",
                f"  call void @__kn_sh_IO_Kinal_Runtime_GarbageCollector_AddRoot_3(ptr %frame, ptr %root{index}, i64 16)",
                f"  %box{index} = insertvalue {{ i64, i64 }} {{ i64 6, i64 undef }}, i64 %payload{index}, 1",
                f"  store {{ i64, i64 }} %box{index}, ptr %root{index}, align 8",
                f"  %ignored{index} = call i64 @Tests_CollectCallArgument_0()",
            ])
        return "\n".join(lines + ["  ret i1 true", "}", ""])

    def test_accepts_rooted_conversions(self):
        check_converted_argument_roots(self.fixture_ir())

    def test_rejects_original_value_only_root(self):
        ir = self.fixture_ir().replace(
            "  store { i64, i64 } %box3, ptr %root3, align 8\n", "")
        with self.assertRaisesRegex(AssertionError, "no root before collection"):
            check_converted_argument_roots(ir)

    def test_rejects_unregistered_converted_slot(self):
        ir = self.fixture_ir().replace("ptr %root3, i64 16", "ptr %original3, i64 16")
        with self.assertRaisesRegex(AssertionError, "unregistered"):
            check_converted_argument_roots(ir)


class CharacterArgumentRootTests(unittest.TestCase):
    IR = "\n".join([
        "define i1 @Tests_CheckConvertedArguments_0() {", "entry:",
        "  %root = alloca ptr, align 8",
        "  call void @__kn_sh_IO_Kinal_Runtime_GarbageCollector_AddRoot_3(ptr %frame, ptr %root, i64 8)",
        "  %text = call ptr @__kn_sh_IO_Kinal_Runtime_CharToString_1(i8 120)",
        "  store ptr %text, ptr %root, align 8",
        "  %ignored = call i64 @Tests_CollectCallArgument_0()",
        "  ret i1 true", "}", "",
    ])

    def test_accepts_rooted_character_conversion(self):
        check_character_argument_root(self.IR)

    def test_rejects_unrooted_character_conversion(self):
        ir = self.IR.replace("  store ptr %text, ptr %root, align 8\n", "")
        with self.assertRaisesRegex(AssertionError, "no root before collection"):
            check_character_argument_root(ir)

    def test_rejects_unregistered_character_slot(self):
        ir = self.IR.replace("ptr %root, i64 8", "ptr %original, i64 8")
        with self.assertRaisesRegex(AssertionError, "unregistered"):
            check_character_argument_root(ir)

    def test_checks_each_builtin_conversion(self):
        body = self.IR.split("entry:\n", 1)[1].split("  ret i1", 1)[0]
        body = body.replace("call i64 @Tests_CollectCallArgument", "call ptr @Tests_CollectBuiltinArgument")
        ir = "define i1 @Tests_CheckBuiltinConvertedArguments_0() {\nentry:\n"
        for index in range(4):
            ir += re.sub(r"%(root|text|ignored)\b", lambda m: m[0] + str(index), body)
        ir += "  ret i1 true\n}\n"
        options = dict(function_name="CheckBuiltinConvertedArguments",
                       collector="CollectBuiltinArgument", expected_conversions=4)
        check_character_argument_root(ir, **options)
        with self.assertRaisesRegex(AssertionError, "no root before collection"):
            check_character_argument_root(ir.replace(
                "  store ptr %text2, ptr %root2, align 8\n", ""), **options)


class NativeMemoryAbiTests(unittest.TestCase):
    ABI = "\n".join([
        "declare ptr @kn_native_heap_allocate(i64)",
        "declare void @kn_native_memory_copy(ptr, ptr, i64)",
        "declare void @kn_native_memory_set(ptr, i8, i64)",
        "declare i32 @kn_native_memory_compare(ptr, ptr, i64)",
    ])

    def test_fixed_width_counts(self):
        check_native_memory_abi(self.ABI)

    def test_rejects_target_width_counts(self):
        for name in ("heap_allocate", "memory_copy", "memory_set", "memory_compare"):
            with self.subTest(name=name), self.assertRaisesRegex(AssertionError, "fixed-width"):
                ir = re.sub(r"(@kn_native_" + name + r"\([^\n]*?)i64", r"\1i32", self.ABI)
                check_native_memory_abi(ir)

    def test_rejects_missing_leaf(self):
        with self.assertRaisesRegex(AssertionError, "missing"):
            check_native_memory_abi(self.ABI.split("\n", 1)[1])


if __name__ == "__main__":
    unittest.main()
