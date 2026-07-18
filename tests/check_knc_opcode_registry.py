#!/usr/bin/env python3
"""Keep the C KNC emitter and Kinal VM opcode ABI in lockstep."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_LAST_OPCODE = 136
UNSIGNED_TAIL = [
    ("KNC_OP_UDIV_INT", 130, "UDivInt", 5, "OpUDivInt"),
    ("KNC_OP_UREM_INT", 131, "URemInt", 5, "OpURemInt"),
    ("KNC_OP_ULT_INT", 132, "ULtInt", 5, "OpULtInt"),
    ("KNC_OP_ULE_INT", 133, "ULeInt", 5, "OpULeInt"),
    ("KNC_OP_UGT_INT", 134, "UGtInt", 5, "OpUGtInt"),
    ("KNC_OP_UGE_INT", 135, "UGeInt", 5, "OpUGeInt"),
    ("KNC_OP_LSHR_INT", 136, "LShrInt", 5, "OpLShrInt"),
]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def c_enum_body(text: str, name: str) -> str:
    match = re.search(r"typedef\s+enum\s*\{(?P<body>[^{}]*)\}\s*" + re.escape(name) + r"\s*;", text, re.S)
    if not match:
        raise AssertionError(f"missing C enum {name}")
    return match.group("body")


def kinal_enum_body(text: str, name: str) -> str:
    match = re.search(r"Enum\s+" + re.escape(name) + r"\b[^\{]*\{(?P<body>.*?)\}", text, re.S)
    if not match:
        raise AssertionError(f"missing Kinal enum {name}")
    return match.group("body")


def c_name_to_bytecode(name: str) -> str:
    words = {
        "BITAND": "BitAnd",
        "BITNOT": "BitNot",
        "BITOR": "BitOr",
        "BITXOR": "BitXor",
        "UDIV": "UDiv",
        "UREM": "URem",
        "ULT": "ULt",
        "ULE": "ULe",
        "UGT": "UGt",
        "UGE": "UGe",
        "LSHR": "LShr",
    }
    stem = name.removeprefix("KNC_OP_")
    converted = "".join(words.get(part, part.title()) for part in stem.split("_"))
    return {"Jump": "JumpOp", "Throw": "ThrowOp"}.get(converted, converted)


def parse_c_opcodes(text: str) -> list[tuple[str, int]]:
    body = c_enum_body(text, "KncOpCode")
    return [(name, int(value)) for name, value in re.findall(r"\b(KNC_OP_[A-Z0-9_]+)\s*=\s*(\d+)", body)]


def parse_bytecode_opcodes(text: str) -> list[tuple[str, int]]:
    body = re.sub(r"//.*?$|/\*.*?\*/", "", kinal_enum_body(text, "OpCode"), flags=re.M | re.S)
    result: list[tuple[str, int]] = []
    ordinal = 0
    for item in body.split(","):
        item = item.strip()
        if not item:
            continue
        match = re.fullmatch(r"([A-Za-z_][A-Za-z0-9_]*)(?:\s*=\s*(\d+))?", item)
        if not match:
            raise AssertionError(f"unrecognized OpCode entry: {item!r}")
        if match.group(2) is not None:
            ordinal = int(match.group(2))
        result.append((match.group(1), ordinal))
        ordinal += 1
    return result


def parse_instruction_sizes(text: str) -> list[tuple[str, int]]:
    start = text.find("Public Static Function int InstructionSize")
    end = text.find("Public Static Function int ReadU8", start)
    if start < 0 or end < 0:
        raise AssertionError("missing Bytecode.Program.InstructionSize")
    return [
        (name, int(size))
        for name, size in re.findall(
            r"Case\s*\(\s*\[int\]\s*\(\s*OpCode\.([A-Za-z0-9_]+)\s*\)\s*\)\s*"
            r"\{\s*Return\s+(\d+)\s*;\s*\}",
            text[start:end],
            re.S,
        )
    ]


def parse_dispatch_targets(text: str) -> list[str]:
    bodies = re.findall(r"dispatchTargets\s*=\s*\{(?P<body>.*?)\}\s*;", text, re.S)
    if not bodies:
        raise AssertionError("missing VM dispatchTargets assignment")
    body = max(bodies, key=len)
    return re.findall(r"\bDispatch\.([A-Za-z0-9_]+)\b", body)


def parse_c_listing_names(text: str) -> list[str]:
    match = re.search(
        r"static\s+const\s+char\s+\*knc_opcode_name\s*\([^)]*\)\s*\{.*?"
        r"static\s+const\s+char\s+\*names\s*\[\s*\]\s*=\s*\{(?P<body>.*?)\}\s*;",
        text,
        re.S,
    )
    if not match:
        raise AssertionError("missing C readable-listing opcode names")
    return re.findall(r'"([A-Za-z0-9_]+)"', match.group("body"))


def parse_kinal_disassembler_names(text: str) -> list[str]:
    match = re.search(
        r"Static\s+Function\s+string\[\]\s+OpcodeNames\s*\([^)]*\)\s*\{.*?"
        r"Return\s*\{(?P<body>.*?)\}\s*;",
        text,
        re.S,
    )
    if not match:
        raise AssertionError("missing KinalVM disassembler opcode names")
    return re.findall(r'"([A-Za-z0-9_]+)"', match.group("body"))


def main() -> int:
    knc_source = read("apps/kinal/src/kn_knc.c")
    bytecode_source = read("apps/kinalvm/src/IO/Kinal/VM/Bytecode.kn")
    vm_source = read("apps/kinalvm/src/IO/Kinal/VM/VM.kn")
    disassembler_source = read("apps/kinalvm/src/IO/Kinal/VM/Disassembler.kn")

    c_opcodes = parse_c_opcodes(knc_source)
    expected_values = list(range(EXPECTED_LAST_OPCODE + 1))
    if [value for _, value in c_opcodes] != expected_values:
        raise AssertionError("KncOpCode must explicitly and contiguously cover values 0..136")

    bytecode_opcodes = parse_bytecode_opcodes(bytecode_source)
    expected_bytecode = [(c_name_to_bytecode(name), value) for name, value in c_opcodes]
    if bytecode_opcodes != expected_bytecode:
        raise AssertionError("Bytecode.OpCode names/ordinals do not match the C KncOpCode ABI")

    expected_listing_names = [
        "Jump" if name == "JumpOp" else "Throw" if name == "ThrowOp" else name
        for name, _ in bytecode_opcodes
    ]
    if parse_c_listing_names(knc_source) != expected_listing_names:
        raise AssertionError("C readable-listing opcode names do not match the KNC ABI")
    if parse_kinal_disassembler_names(disassembler_source) != expected_listing_names:
        raise AssertionError("KinalVM disassembler opcode names do not match the KNC ABI")

    sizes = parse_instruction_sizes(bytecode_source)
    if [name for name, _ in sizes] != [name for name, _ in bytecode_opcodes]:
        raise AssertionError("InstructionSize must cover every opcode exactly once in ordinal order")

    dispatch_targets = parse_dispatch_targets(vm_source)
    expected_dispatch = [
        "OpJump" if name == "JumpOp" else "OpThrow" if name == "ThrowOp" else f"Op{name}"
        for name, _ in bytecode_opcodes
    ]
    if dispatch_targets != expected_dispatch:
        raise AssertionError("VM dispatchTargets must match Bytecode.OpCode ordinal order")

    actual_tail = [
        (c_opcodes[index][0], c_opcodes[index][1], bytecode_opcodes[index][0], sizes[index][1], dispatch_targets[index])
        for index in range(130, EXPECTED_LAST_OPCODE + 1)
    ]
    if actual_tail != UNSIGNED_TAIL:
        raise AssertionError(f"unsigned opcode ABI tail changed: {actual_tail!r}")

    print(f"[OK] knc_opcode_registry opcodes={len(c_opcodes)} unsigned_tail=130..136")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
