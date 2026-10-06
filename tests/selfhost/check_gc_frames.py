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
        assert re.search(r"@__kn_sh_IO_Kinal_Runtime_GarbageCollector_AddRoot_3"
                         r"\(ptr [^,]+, ptr " + slot + r",", body[:box.start()]), \
            f"converted argument slot is unregistered: {stored[1]}"


def check_character_argument_root(ir: str) -> None:
    body = function_body(ir, "CheckConvertedArguments")
    conversion = re.search(r"(%[\w.]+) = call ptr "
                           r"@__kn_sh_IO_Kinal_Runtime_CharToString_1\(i8 120\)", body)
    assert conversion, "fixture must exercise implicit char-to-string conversion"
    remaining = body[conversion.end():]
    collection = re.search(r"call i64 @\w+_CollectCallArgument_0\(\)", remaining)
    assert collection, "converted character must precede another collecting argument"
    stored = re.search(r"store ptr " + re.escape(conversion[1]) +
                       r", ptr (%[\w.]+),", remaining[:collection.start()])
    assert stored, "converted character has no root before collection"
    assert re.search(r"@__kn_sh_IO_Kinal_Runtime_GarbageCollector_AddRoot_3"
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
        pattern = (r"@__kn_sh_IO_Kinal_Runtime_GarbageCollector_AddRoot_3"
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
        output = out / ("roots-" + target + ".ll")
        build = subprocess.run(
            [str(compiler), "build", "--project", str(project), "--profile", "test",
             "--target", target, "--emit", "ir", "-o", str(output)],
            cwd=root, capture_output=True, text=True, timeout=180,
        )
        assert build.returncode == 0, (target, build.returncode, build.stdout, build.stderr)
        ir = output.read_text(encoding="utf-8")
        check_entry_roots(ir)
        check_converted_argument_roots(ir)
        check_converted_argument_roots(ir, function_name="CheckDynamicConvertedArguments",
                                       expected_boxes=1)
        check_character_argument_root(ir)
        check_native_memory_abi(ir)
        print(f"[OK] GC entry roots and fixed-width native memory ABI {target}", flush=True)
    return {"name": "gc_frames", "ok": True, "compilers": len(tools),
            "roots": 257, "repeats": 3, "ir_targets": len(TARGETS),
            "stage0_reference": stage0_reference}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    print(check_gc_frames(args.compiler.resolve(), args.stage0.resolve(),
                          Path(__file__).resolve().parents[2], args.out_dir.resolve()))
