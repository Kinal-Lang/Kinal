from __future__ import annotations

import argparse
import os
import re
import subprocess
from pathlib import Path

from check_targets import TARGETS, function_body


def check_native_memory_abi(ir: str) -> None:
    # The C leaves intentionally use fixed uint64_t counts, not size_t.
    # An x64-only executable test cannot catch an incorrect usize declaration.
    signatures = {
        "kn_native_heap_allocate": ("ptr", "i64"),
        "kn_native_memory_copy": ("void", "ptr, ptr, i64"),
        "kn_native_memory_set": ("void", "ptr, i8, i64"),
        "kn_native_memory_compare": ("i32", "ptr, ptr, i64"),
    }
    for name, (result, arguments) in signatures.items():
        declaration = re.search(r"^declare (\w+) @" + name + r"\(([^)]*)\)",
                                ir, re.MULTILINE)
        assert declaration, f"missing native memory declaration: {name}"
        assert declaration.groups() == (result, arguments), \
            f"incorrect fixed-width native memory ABI: {declaration[0]}"


def check_scalar_calls(ir: str) -> None:
    for name, targets in (("ScalarEven", ("ScalarStep", "ScalarOdd")),
                          ("ScalarOdd", ("ScalarEven",)), ("ScalarStep", ())):
        worker = re.search(
            rf'^define private i64 @"[^"]*_{name}_1\$scalar"\([^\n]*\).*?^}}',
            ir, re.M | re.S)
        assert worker, name
        body = worker.group()
        assert "GarbageCollector_MaybeCollect_0()" in body, name
        assert "Exception_Has_" not in body and "global_init" not in body, name
        for target in targets:
            assert re.search(rf'call i64 @"[^"]*_{target}_1\$scalar"\(', body), target
        if targets:
            entry = function_body(ir, name)
            assert entry.index("ensure_global_init") < entry.index("Exception_Has_"), name
            assert "pending_exception" in entry and "$scalar" in entry, name
    for name in ("FallibleOdd", "FallibleEven", "Failable", "ViaCallback", "ViaNative"):
        assert name + "_1$scalar" not in ir, name
        if name in ("FallibleOdd", "FallibleEven", "ViaCallback"):
            assert "Exception_Has_0()" in function_body(ir, name), name
    tls = re.search(r"^declare ptr @kn_native_runtime_thread_storage\(\) #(\d+)", ir, re.M)
    assert tls, "missing native TLS accessor attributes"
    attrs = re.search(r"^attributes #" + tls[1] + r" = \{ ([^}]+) \}", ir, re.M)
    assert attrs and all(attr in attrs[1] for attr in ("memory(none)", "nounwind", "willreturn"))


def check_converted_argument_roots(ir: str, *, function_name: str = "CheckConvertedArguments",
                                  expected_boxes: int = 7) -> None:
    body = function_body(ir, function_name)
    boxes = list(re.finditer(
        r"(%[\w.]+) = insertvalue \{ i64, i64 \} \{ i64 6, i64 undef \},"
        r" i64 %[^\n]+, 1", body))
    assert len(boxes) == expected_boxes, "fixture must exercise every implicit argument conversion"
    for box in boxes:
        remaining = body[box.end():]
        collection = re.search(r"call i64 @\w+_CollectCallArgument_0\(\)", remaining)
        assert collection, "boxed argument must precede another collecting argument"
        stored = re.search(r"store \{ i64, i64 \} " + re.escape(box[1]) +
                           r", ptr (%[\w.]+),", remaining[:collection.start()])
        assert stored, f"converted argument has no root before collection: {box[1]}"
        slot = re.escape(stored[1])
        assert re.search(r"@__kn_sh_IO_Kinal_Runtime_GarbageCollector_SetFrameRoot_4"
                         r"\(ptr [^,]+, ptr " + slot + r",", body[:box.start()]), \
            f"converted argument slot is unregistered: {stored[1]}"


def check_character_argument_root(ir: str, *, function_name: str = "CheckConvertedArguments",
                                  collector: str = "CollectCallArgument",
                                  expected_conversions: int = 1) -> None:
    body = function_body(ir, function_name)
    conversions = list(re.finditer(r"(%[\w.]+) = call ptr "
                                   r"@__kn_sh_IO_Kinal_Runtime_CharToString_1\(i8 120\)", body))
    assert len(conversions) == expected_conversions, \
        "fixture must exercise every implicit char-to-string conversion"
    for conversion in conversions:
        remaining = body[conversion.end():]
        collection = re.search(r"call (?:i64|ptr) @\w+_" + re.escape(collector) + r"_0\(\)",
                               remaining)
        assert collection, "converted character must precede another collecting argument"
        stored = re.search(r"store ptr " + re.escape(conversion[1]) +
                           r", ptr (%[\w.]+),", remaining[:collection.start()])
        assert stored, "converted character has no root before collection"
        assert re.search(r"@__kn_sh_IO_Kinal_Runtime_GarbageCollector_SetFrameRoot_4"
                         r"\(ptr [^,]+, ptr " + re.escape(stored[1]) + r",",
                         body[:conversion.start()]), \
            "converted character slot is unregistered"


def check_entry_roots(ir: str, *, legacy_stage0: bool = False) -> None:
    body = function_body(ir, "CheckLoopRoots")
    stack_slots = set(re.findall(r"(%[\w.]+) = alloca ", body))
    registered: set[str] = set()
    block = ""
    if legacy_stage0:
        frame = re.search(r"(%[\w.]+) = call ptr @__kn_gc_push_frame\(", body)
        assert frame, "fixture no longer uses a C-stage0 GC frame"
        pattern = (r"@__kn_gc_add_root\(ptr " + re.escape(frame[1]) +
                   r", ptr (%[\w.]+),")
    else:
        pattern = (r"@__kn_sh_IO_Kinal_Runtime_GarbageCollector_SetFrameRoot_4"
                   r"\(ptr [^,]+, ptr (%[\w.]+),")
    for line in body.splitlines():
        label = re.match(r"([\w.]+):", line)
        if label:
            block = label[1]
        root = re.search(pattern, line)
        if root:
            slot = root[1]
            assert block == "entry", f"root registration repeats in {block}: {line}"
            assert slot in stack_slots, f"root is not an entry-owned slot: {line}"
            assert slot not in registered, f"duplicate root registration: {slot}"
            assert re.search(r"store [^\n]*, ptr " + re.escape(slot) + r",", body[:body.index(line)]), \
                f"uninitialized entry root: {slot}"
            registered.add(slot)
    assert len(registered) >= 5, "loop fixture no longer exercises managed declarations"
    if not legacy_stage0:
        frame = re.search(
            r"@__kn_sh_IO_Kinal_Runtime_GarbageCollector_PushPlannedFrame_3"
            r"\(ptr (%[\w.]+), ptr (%[\w.]+), i64 (\d+)\)", body)
        assert frame, "generated GC frame must use caller-owned storage"
        slots = re.findall(r"@__kn_sh_IO_Kinal_Runtime_GarbageCollector_SetFrameRoot_4"
                           r"\(ptr " + re.escape(frame[2]) + r", ptr %[\w.]+, [^\n]+, i64 (\d+)\)",
                           body[:frame.start()])
        assert list(map(int, slots)) == list(range(len(registered))), "incomplete planned root table"
        assert "GarbageCollector_AddRoot_3(" not in body, "planned frame used dynamic root growth"
        assert int(frame[3]) == len(registered), "stack root table must fit the planned roots exactly"
        assert re.search(re.escape(frame[1]) + r" = alloca \[6 x i64\]", body)
        assert re.search(re.escape(frame[2]) + r" = alloca \[" +
                         str(2 * len(registered)) + r" x i64\]", body)
        assert "@__kn_sh_IO_Kinal_Runtime_GarbageCollector_PushFrame_0(" not in body


def check_assignment_address_roots(ir: str, *, legacy_stage0: bool = False) -> None:
    body = function_body(ir, "CheckAssignmentAddressRoots")
    prefix = r"gc\.assignment\.address" if legacy_stage0 else "gc_assignment_address"
    register = "__kn_gc_add_root" if legacy_stage0 else "__kn_sh_IO_Kinal_Runtime_GarbageCollector_SetFrameRoot_4"
    slots = re.findall(r"(%" + prefix + r"[\w.]*) = alloca ptr", body)
    assert len(slots) == 4, "fixture must preserve all four assignment addresses"
    for slot in slots:
        escaped = re.escape(slot)
        initialized = re.search(r"store ptr null, ptr " + escaped + r",", body)
        registrations = list(re.finditer(r"@" + register + r"\(ptr [^,]+, ptr " + escaped + r",", body))
        assert len(registrations) == 1, f"assignment address must register once: {slot}"
        registered = registrations[0]
        assert initialized and registered and initialized.start() < registered.start(), \
            f"assignment address slot is uninitialized/unregistered: {slot}"
        preceding_labels = re.findall(r"^([\w.]+):", body[:registered.start()], re.MULTILINE)
        assert preceding_labels and preceding_labels[-1] == "entry", \
            f"assignment address must register at entry: {slot}"
        stored = re.search(r"store ptr (%[\w.]+), ptr " + escaped + r",", body)
        assert stored, f"assignment address has no root before collection: {slot}"
        remainder = body[stored.end():]
        collection = re.search(r"call i64 @[\w.]+[._](CollectCallArgument|CollectAndReplaceAssignment)(?:_0)?\(\)", remainder)
        assert collection, "assignment must evaluate a collecting RHS after the address"
        written = re.search(r"store i64 [^,]+, ptr " + re.escape(stored[1]) + r",",
                            remainder[collection.end():])
        assert written, "rooted address must be the actual assignment destination"
        if collection[1] == "CollectAndReplaceAssignment":
            assert re.search(r"load i64, ptr " + re.escape(stored[1]) + r",",
                             remainder[:collection.start()]), \
                "compound assignment must read its old value before the RHS"


def check_gc_frames(compiler: Path, stage0: Path, root: Path, out: Path,
                    *, stage0_reference: bool = True) -> dict[str, object]:
    out.mkdir(parents=True, exist_ok=True)
    tools = [("stage0", stage0)] if stage0_reference else []
    tools.append(("selfhost", compiler))
    project = root / "tests/selfhost/fixtures/gc_frames/kinal.knproj"
    suffix = ".exe" if os.name == "nt" else ""
    for label, tool in tools:
        executable = out / (label + suffix)
        command = [str(tool), "build", "--project", str(project), "--profile", "test",
                   "-o", str(executable)]
        build = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=180)
        assert build.returncode == 0, (label, build.returncode, build.stdout, build.stderr)
        for repeat in range(3):
            result = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                    text=True, timeout=60)
            assert result.returncode == 0 and result.stdout == "gc-frames-ok\n" and not result.stderr, \
                (label, repeat, result.returncode, result.stdout, result.stderr)
        print(f"[OK] {label} Kinal GC frame growth/nesting/collection/loop slots", flush=True)
    has_paired_buffers = False
    for target, *_ in TARGETS:
        if stage0_reference:
            output = out / ("roots-stage0-" + target + ".ll")
            build = subprocess.run(
                [str(stage0), "build", "--project", str(project), "--profile", "test",
                 "--target", target, "--emit", "ir", "-o", str(output)],
                cwd=root, capture_output=True, text=True, timeout=180,
            )
            assert build.returncode == 0, (target, "stage0", build.stdout, build.stderr)
            ir = output.read_text(encoding="utf-8")
            check_entry_roots(ir, legacy_stage0=True)
            check_native_memory_abi(ir)
            check_assignment_address_roots(ir, legacy_stage0=True)
        output = out / ("roots-" + target + ".ll")
        build = subprocess.run(
            [str(compiler), "build", "--project", str(project), "--profile", "test",
             "--target", target, "--emit", "ir", "-o", str(output)],
            cwd=root, capture_output=True, text=True, timeout=180,
        )
        assert build.returncode == 0, (target, build.returncode, build.stdout, build.stderr)
        ir = output.read_text(encoding="utf-8")
        check_entry_roots(ir)
        for name in ("FoldByteArray", "FoldGenerator", "FoldLiteral", "FoldUnsigned32", "FoldSigned16"):
            body = function_body(ir, name)
            assert "modular_group_body" in body and "modular_group_check" in body, (target, name)
            assert "urem i" in body and "while_poll" in body, (target, name)
        for name in ("DependentAddend", "WideProduct", "ShortenedString", "ExposedRecurrence", "ThrowingRecurrence"):
            assert "modular_group_body" not in function_body(ir, name), (target, name)
        assert "gc_expression" not in function_body(ir, "FoldLiteral"), target
        assert "gc_batch_end" not in function_body(ir, "ShortenedString"), target
        literal_body = function_body(ir, "CheckLiteralRoots")
        roots = set(re.findall(r"GarbageCollector_SetFrameRoot_4\(ptr [^,]+, ptr %([\w.]+),", literal_body))
        assert not any(name.startswith("fixedText") for name in roots), (target, roots)
        assert {"reboundText", "escapedText", "appendedText"} <= roots, (target, roots)
        if "define ptr @__kn_sh_IO_Kinal_Runtime_StringAppendLocal_3(" in ir:
            appends = function_body(ir, "CheckLocalAppends")
            entry = re.split(r"^  (?:br|switch|ret|invoke|unreachable)\b", appends, maxsplit=1, flags=re.M)[0]
            states = re.findall(r"(%string_append_state\d+) = alloca", appends)
            assert states and all(f"store i64 0, ptr {slot}," in entry for slot in states), target
            assert appends.count("@__kn_sh_IO_Kinal_Runtime_StringAppendLocal_3(") >= 4, target
            assert "@__kn_sh_IO_Kinal_Runtime_StringConcat_2(" in appends, target
            literal = function_body(ir, "CheckLiteralAppendBytes")
            assert "append_fitting" in literal and "append_grow" in literal, target
            assert len(re.findall(r"call void @kn_native_memory_copy\([^\n]+i64 3\)", literal)) == 2, target
        if "@__kn_sh_IO_Kinal_Runtime_GarbageCollector_TakeLocalArray_2(" in ir:
            take = "call ptr @__kn_sh_IO_Kinal_Runtime_GarbageCollector_TakeLocalArray_2("
            give = "call void @__kn_sh_IO_Kinal_Runtime_GarbageCollector_ReturnLocalArray_1("
            for name, arrays in (("NestedFixedArray", 3), ("ReenteredFixedArray", 1),
                                 ("ThrowFromFixedArray", 1), ('OverlappingFixedArray_1$async_body"', 2)):
                body = function_body(ir, name)
                assert body.count(take) == arrays and body.count(give) >= arrays, (target, name)
            for name in ("PublishedFixedArray", "PublishedArrayElement", "CapturedFixedArray"):
                body = function_body(ir, name)
                assert take not in body and give not in body, (target, name)
            globals_prefix = "@__kn_sh_global_IO_Kinal_Runtime_GarbageCollector_"
            if globals_prefix + "OlderSpareLocalArray" in ir:
                has_paired_buffers = True
                collect = function_body(ir, "GarbageCollector_Collect_0")
                before_scan = collect[:collect.index(globals_prefix + "GlobalRoots")]
                for slot in ("SpareLocalArray", "OlderSpareLocalArray"):
                    assert "store ptr null, ptr " + globals_prefix + slot + "," in before_scan, (target, slot)
        check_scalar_calls(ir)
        for name in ("Runtime_StringLength", "Runtime_StringEqual"):
            assert " call " not in function_body(ir, name), (target, name)
        concat = function_body(ir, "Runtime_StringConcat")
        assert "GarbageCollector_PushPlannedFrame" in concat and "Runtime_AllocateText" in concat
        plain = function_body(ir, "CheckPlainArrayAccess")
        assert "gc_assignment_address" not in plain and "Runtime_MemoryZero" not in plain
        assert "while_poll" in plain and "gc_batch_end" in plain
        assert "GarbageCollector_MaybeCollect_0()" in plain
        search = function_body(ir, "FindValue")
        assert "gc_loop_budget" in search and "while_poll" in search, target
        limits = function_body(ir, "CheckBatchLimits")
        assert "gc_loop_budget" in limits, target
        for name, mode in (("CountWithBranch", "gc_batch_end"),
                           ("CountWithChangedBound", "gc_loop_budget"),
                           ("CountWithChangedIndex", "gc_loop_budget")):
            body = function_body(ir, name)
            assert mode in body and "while_poll" in body, (target, name)
            assert "GarbageCollector_MaybeCollect_0()" in body, (target, name)
        handlers = function_body(ir, "CheckPropagationBlocks")
        assert len(re.findall(r"^exception_error\d+:", handlers, re.MULTILINE)) == 3, target
        collector = function_body(ir, "CollectCallArgument")
        assert "gc_frame_storage" not in collector and "GarbageCollector_PopFrame_" not in collector
        assert "GarbageCollector_MaybeCollect_0()" in collector
        assert "GarbageCollector_Collect_0()" in collector and "Exception_Has_0()" in collector
        check_converted_argument_roots(ir)
        check_converted_argument_roots(ir, function_name="CheckDynamicConvertedArguments",
                                       expected_boxes=1)
        check_character_argument_root(ir)
        check_character_argument_root(ir, function_name="CheckBuiltinConvertedArguments",
                                      collector="CollectBuiltinArgument", expected_conversions=4)
        check_native_memory_abi(ir)
        check_assignment_address_roots(ir)
        print(f"[OK] GC entry roots and fixed-width native memory ABI {target}", flush=True)
    if has_paired_buffers:
        probe = out / ("weak-buffers" + suffix)
        subprocess.run([str(compiler), "build", "--no-module-discovery", str(project.with_name("WeakBuffers.kn")),
                        "-o", str(probe)], cwd=root, check=True, timeout=180)
        subprocess.run([str(probe)], cwd=root, check=True, timeout=60)
        print("[OK] weak array buffers match, clear on reuse and expire at collection", flush=True)
    return {"name": "gc_frames", "ok": True, "compilers": len(tools),
            "roots": 4097, "repeats": 3, "ir_targets": len(TARGETS),
            "stage0_reference": stage0_reference}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    print(check_gc_frames(args.compiler.resolve(), args.stage0.resolve(),
                          Path(__file__).resolve().parents[2], args.out_dir.resolve()))
