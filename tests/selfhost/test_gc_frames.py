from __future__ import annotations

import unittest

from check_gc_frames import check_entry_roots


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


if __name__ == "__main__":
    unittest.main()
