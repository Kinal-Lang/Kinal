#!/usr/bin/env python3
"""Check that builtin metadata, Native lowering, KNC, and the VM stay aligned."""

from __future__ import annotations

import re
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def enum_body(text: str, name: str) -> str:
    match = re.search(r"typedef\s+enum\s*\{(?P<body>.*?)\}\s*" + re.escape(name) + r"\s*;", text, re.S)
    if not match:
        raise AssertionError(f"missing enum {name}")
    return match.group("body")


def assigned_enum_body(text: str, name: str) -> str:
    match = re.search(r"Enum\s+" + re.escape(name) + r"\b[^\{]*\{(?P<body>.*?)\}", text, re.S)
    if not match:
        raise AssertionError(f"missing enum {name}")
    return match.group("body")


def duplicates(values: list[int]) -> list[int]:
    return sorted(value for value, count in Counter(values).items() if count > 1)


def check_desktop_intrinsics(core_ids: set[int]) -> int:
    ui = ROOT / "libs/std/IO.UI/2.0.0/src"
    leaves = {}
    declaration = re.compile(
        r"\[IO\.Kinal\.Runtime\.KncIntrinsic\((\d+)\)\]\s*"
        r"(?:Trusted|Extern) Function [\w.*]+ (\w+)\(([^)]*)\)"
    )
    for source in ui.rglob("*.kn"):
        for number, name, params in declaration.findall(source.read_text(encoding="utf-8")):
            number = int(number)
            if number in leaves or number in core_ids:
                raise AssertionError(f"duplicate IO.UI intrinsic ID {number}")
            arity = len(params.split(",")) if params.strip() else 0
            leaves[number] = (name, arity if arity > 4 else None)

    handlers = {}
    desktop = read("apps/kinalvm/windows/Desktop.kn")
    for kind, number, name, arity in re.findall(
        r'\[IO\.Kinal\.VM\.Stdlib\.Vm(Range)?Builtin\((\d+),\s*"IO\.UI\.(\w+)"(?:,\s*(\d+))?\)\]',
        desktop,
    ):
        number = int(number)
        if number in handlers or bool(kind) != bool(arity):
            raise AssertionError(f"invalid Windows VM handler registration for {number}")
        handlers[number] = (name, int(arity) if arity else None)
    if not leaves or leaves != handlers:
        raise AssertionError(f"IO.UI/Windows VM intrinsic mismatch: {leaves.items() ^ handlers.items()}")
    return len(handlers)


def main() -> int:
    std_header = read("apps/kinal/include/kn/std.h")
    std_source = read("apps/kinal/src/kn_std.c")
    native_source = read("apps/kinal/src/codegen/kn_codegen_expr.inc")
    knc_source = read("apps/kinal/src/kn_knc.c")
    bytecode_source = read("apps/kinalvm/src/IO/Kinal/VM/Bytecode.kn")
    vm_source = read("apps/kinalvm/src/IO/Kinal/VM/Stdlib.kn")

    enum_names = re.findall(r"\bKN_BUILTIN_[A-Z0-9_]+\b", enum_body(std_header, "KnBuiltinKind"))
    enum_names = list(dict.fromkeys(enum_names))
    if not enum_names or enum_names[-1] != "KN_BUILTIN_COUNT":
        raise AssertionError("KnBuiltinKind must end with KN_BUILTIN_COUNT")
    builtin_names = [name for name in enum_names if name not in {"KN_BUILTIN_NONE", "KN_BUILTIN_COUNT"}]
    builtin_set = set(builtin_names)
    builtin_index = {name: index for index, name in enumerate(builtin_names)}

    ranges_match = re.search(
        r"g_builtin_lowering_ranges\s*\[\s*\]\s*=\s*\{(?P<body>.*?)\};",
        std_source,
        re.S,
    )
    if not ranges_match:
        raise AssertionError("missing builtin lowering registry")
    ranges = re.findall(
        r"\{\s*(KN_BUILTIN_[A-Z0-9_]+)\s*,\s*(KN_BUILTIN_[A-Z0-9_]+)\s*,"
        r"\s*KN_BUILTIN_LOWERING_([A-Z0-9_]+)\s*\}",
        ranges_match.group("body"),
    )
    classified: dict[str, str] = {}
    for first, last, group in ranges:
        if first not in builtin_index or last not in builtin_index:
            raise AssertionError(f"unknown builtin range boundary: {first}..{last}")
        start = builtin_index[first]
        end = builtin_index[last]
        if start > end:
            raise AssertionError(f"reversed builtin range: {first}..{last}")
        for name in builtin_names[start : end + 1]:
            if name in classified:
                raise AssertionError(f"builtin classified more than once: {name}")
            classified[name] = group

    helper_groups = ("platform", "collections", "system", "text", "filesystem", "dynamic")
    native_owner: dict[str, str] = {}
    for helper in helper_groups:
        start_match = re.search(r"^static CGValue gen_builtin_" + helper + r"\b", native_source, re.M)
        if not start_match:
            raise AssertionError(f"missing Native builtin helper: {helper}")
        next_starts = [
            match.start()
            for match in re.finditer(r"^static CGValue gen_builtin_", native_source[start_match.end() :], re.M)
        ]
        end = start_match.end() + next_starts[0] if next_starts else len(native_source)
        body = native_source[start_match.start() : end]
        cases = set(re.findall(r"\bcase\s+(KN_BUILTIN_[A-Z0-9_]+)\s*:", body))
        cases &= builtin_set
        for name in cases:
            previous = native_owner.get(name)
            if previous and previous != helper:
                raise AssertionError(f"Native builtin appears in two helpers: {name} ({previous}, {helper})")
            native_owner[name] = helper

    missing_classification = sorted(builtin_set - classified.keys())
    missing_native = sorted(builtin_set - native_owner.keys())
    if missing_classification:
        raise AssertionError(f"builtins without lowering group: {missing_classification}")
    if missing_native:
        raise AssertionError(f"builtins without Native lowering: {missing_native}")
    for name in builtin_names:
        expected = native_owner[name].upper()
        if classified[name] != expected:
            raise AssertionError(
                f"builtin lowering mismatch: {name} registry={classified[name]} native={expected}"
            )

    mapping_match = re.search(
        r"g_builtin_vm_mappings\s*\[\s*\]\s*=\s*\{(?P<body>.*?)\};",
        std_source,
        re.S,
    )
    if not mapping_match:
        raise AssertionError("missing builtin VM mapping registry")
    mapped = re.findall(
        r"\{\s*(KN_BUILTIN_[A-Z0-9_]+)\s*,\s*(\d+)\s*\}",
        mapping_match.group("body"),
    )
    unknown_mapped = sorted(name for name, _ in mapped if name not in builtin_set)
    if unknown_mapped:
        raise AssertionError(f"KNC maps unknown builtins: {unknown_mapped}")
    duplicate_mapped_names = sorted(
        name for name, count in Counter(name for name, _ in mapped).items() if count > 1
    )
    if duplicate_mapped_names:
        raise AssertionError(f"KNC maps builtin kinds more than once: {duplicate_mapped_names}")
    mapped_ids = [int(value) for _, value in mapped]
    duplicate_mapped_ids = duplicates(mapped_ids)
    if duplicate_mapped_ids:
        raise AssertionError(f"KNC builtin ids are duplicated: {duplicate_mapped_ids}")

    handler_ids = [int(value) for value in re.findall(r"\[VmBuiltin\(\s*(\d+)\s*,", vm_source)]
    duplicate_handler_ids = duplicates(handler_ids)
    if duplicate_handler_ids:
        raise AssertionError(f"VM builtin handlers are duplicated: {duplicate_handler_ids}")
    declared_ids = [
        int(value)
        for value in re.findall(r"\b[A-Za-z_][A-Za-z0-9_]*\s*=\s*(\d+)\s*,?",
                                assigned_enum_body(bytecode_source, "BuiltinId"))
    ]
    duplicate_declared_ids = duplicates(declared_ids)
    if duplicate_declared_ids:
        raise AssertionError(f"VM BuiltinId values are duplicated: {duplicate_declared_ids}")

    missing_handlers = sorted(set(mapped_ids) - set(handler_ids))
    undeclared_handlers = sorted(set(handler_ids) - set(declared_ids))
    if missing_handlers:
        raise AssertionError(f"KNC emits builtin ids without VM handlers: {missing_handlers}")
    if undeclared_handlers:
        raise AssertionError(f"VM handlers use undeclared BuiltinId values: {undeclared_handlers}")
    if not re.search(
        r"static int map_builtin_id\s*\([^)]*\)\s*\{\s*"
        r"return kn_builtin_vm_id\(\(KnBuiltinKind\)builtin_id\);\s*\}",
        knc_source,
        re.S,
    ):
        raise AssertionError("KNC must consume the centralized builtin VM mapping registry")

    json_source = read("libs/std/IO.Json/1.0.0/src/IO/Json.kn")
    package_leaves = re.findall(
        r"\[IO\.Kinal\.Runtime\.KncIntrinsic\((\d+)\)\]\s*Trusted Function \w+ (\w+)\(", json_source
    )
    package_handlers = re.findall(r'\[VmBuiltin\((\d+), "IO\.Json\.(\w+)"\)\]', vm_source)
    if not package_leaves or sorted(package_leaves) != sorted(package_handlers):
        raise AssertionError("IO.Json package and VM handlers must expose matching intrinsic IDs and names")

    math_source = read("libs/std/IO.Core/1.0.0/src/IO/Math.kn")
    math_externs = re.findall(r"Extern Unsafe Function \w+ __kn_math_(\w+)\(", math_source)
    math_leaves = re.findall(
        r"\[IO\.Kinal\.Runtime\.KncIntrinsic\((\d+)\)\]\s*"
        r"Extern Unsafe Function \w+ __kn_math_(\w+)\(", math_source
    )
    math_handlers = [(number, name.lower()) for number, name in
                     re.findall(r'\[VmBuiltin\((\d+), "IO\.Math\.(\w+)"\)\]', vm_source)]
    if (not math_externs or sorted(math_externs) != sorted(name for _, name in math_leaves)
            or sorted(math_leaves) != sorted(math_handlers)):
        raise AssertionError("IO.Math externs and VM handlers must expose matching intrinsic IDs and names")

    # KNC instance slots follow declaration order on both the guest and host.
    time_source = read("libs/std/IO.Core/1.0.0/src/IO/Time.kn")
    date_class = re.search(r"Class DateTime\s*\{([^}]*)\}", time_source)
    if not date_class or re.findall(r"Public int (\w+);", date_class.group(1)) != [
        "Ticks", "Year", "Month", "Day", "Hour", "Minute", "Second", "Millisecond"
    ]:
        raise AssertionError("IO.Time.DateTime field layout diverged from the KNC host ABI")
    time_leaves = re.findall(
        r"\[IO\.Kinal\.Runtime\.KncIntrinsic\((\d+)\)\]\s*Trusted Function \w+ (\w+)\(", time_source
    )
    time_handlers = re.findall(r'\[VmBuiltin\((\d+), "IO\.Time\.(\w+)"\)\]', vm_source)
    if len(time_leaves) != 5 or sorted(time_leaves) != sorted(time_handlers):
        raise AssertionError("IO.Time package and VM clock handlers must match")

    builder_source = read("libs/std/IO.Core/1.0.0/src/IO/Text/Builder.kn")
    builder_leaves = re.findall(
        r"\[IO\.Kinal\.Runtime\.KncIntrinsic\((\d+)\)\]\s*Trusted Function \w+ __Builder(\w+)\(", builder_source
    )
    builder_handlers = re.findall(r'\[VmBuiltin\((\d+), "IO\.Text\.Builder\.(\w+)"\)\]', vm_source)
    builder_names = [(number, "EnsureCapacity" if name == "Reserve" else name) for number, name in builder_leaves]
    if len(builder_names) != 7 or sorted(builder_names) != sorted(builder_handlers):
        raise AssertionError("IO.Text.Builder package bridges and VM handlers must match")

    desktop_count = check_desktop_intrinsics(set(declared_ids))
    print(
        f"[OK] builtin_registry native={len(native_owner)} "
        f"knc={len(mapped_ids)} vm={len(handler_ids)} declared={len(declared_ids)} desktop={desktop_count}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
