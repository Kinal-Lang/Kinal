"""Compile the pure-Kinal KNC v3 writer and verify its binary ABI against the VM.

This focused check needs only KncModel and the fixture, not a full compiler build.
Every emitted byte is independently reconstructed below. The opcode declarations
and instruction widths are checked against the current VM source as well.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess


def _enum(source: str, name: str) -> dict[str, int]:
    match = re.search(r"\bEnum " + name + r" By u8\s*\{([^}]+)\}", source)
    assert match is not None, f"missing {name} declaration"
    values: dict[str, int] = {}
    value = 0
    for entry in match.group(1).split(","):
        entry = entry.strip()
        if not entry:
            continue
        parts = entry.split("=")
        if len(parts) == 2:
            value = int(parts[1].strip())
        values[parts[0].strip()] = value
        value += 1
    return values


def _string(value: str) -> bytes:
    data = value.encode("utf-8")
    return struct.pack("<I", len(data)) + data


def _expected_program() -> bytes:
    # Magic, revision, flags, entry, globals, ints, floats, strings, types, funcs.
    data = b"KNC2" + struct.pack("<HH7i", 3, 0, 0, 2, 3, 6, 2, 2, 1)
    data += struct.pack("<3q", -(2**63), 2**63 - 1, -1)
    data += struct.pack("<3d", 1.5, 0.0, -0.0)
    data += struct.pack("<3Q", 0x7FF8000000000042, 0x7FF0000000000000, 0xFFF0000000000000)
    data += _string("é雪") + _string("")
    data += struct.pack("<BBH4i", 3, 0, 0, 0, -1, 0, 0) + _string("Tests.Marker")
    data += struct.pack("<BBH4i", 1, 0, 0, 2, -1, 2, 1) + _string("Tests.Model")
    data += struct.pack("<3i", 0, -1, 0)
    call_print = bytes([44, 255, 0, 0, 1, 0, 255, 255, 255])
    code = bytes([0, 0, 0, 0]) + call_print + bytes([3, 0, 0, 0]) + call_print + bytes([59])
    data += struct.pack("<3i", 0, 1, 0) + _string("Main") + struct.pack("<i", len(code)) + code
    return data


def check_knc_model(compiler: Path, root: Path, out: Path, vm: Path | None = None,
                    *, link_args: tuple[str, ...] = ()) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    model = root / "apps/kinal-selfhost/src/IO/Kinal/Compiler/Backend/KncModel.kn"
    fixture = root / "tests/selfhost/fixtures/knc_model_writer.kn"
    vm_source = (root / "apps/kinalvm/src/IO/Kinal/VM/Bytecode.kn").read_text(encoding="utf-8")
    model_source = model.read_text(encoding="utf-8")
    assert _enum(model_source, "KncOpCode") == _enum(vm_source, "OpCode")
    assert _enum(model_source, "KncValueKind") == _enum(vm_source, "ValueKind")
    widths = re.findall(r"Case\(\[int\]\(OpCode\.([A-Za-z0-9]+)\)\)\s*\{ Return (\d+); \}", vm_source)
    expected_sizes = bytearray(139)
    opcodes = _enum(vm_source, "OpCode")
    for name, size in widths:
        expected_sizes[opcodes[name]] = int(size)
    assert len(widths) == 139 and all(expected_sizes)
    shutil.copyfile(model, out / "KncModel.kn")
    shutil.copyfile(fixture, out / "Main.kn")
    project = out / "kinal.knproj"
    project.write_text('Project KncModelCheck { DefaultProfile = "test"; '
                       'SourceSet "main" { Files = ["Main.kn", "KncModel.kn"]; RequireUnit = true; } '
                       'Profile "test" { Source { Entry = "Main.kn"; Sets = ["main"]; '
                       'Mode = ReachableUnits; } } }\n', encoding="utf-8")
    executable = out / ("knc-model-writer.exe" if os.name == "nt" else "knc-model-writer")
    command = [str(compiler), "build", "--project", str(project), "-o", str(executable)]
    for argument in link_args:
        command += ["--link-arg", argument]
    build = subprocess.run(command, cwd=root, text=True, capture_output=True, timeout=180)
    (out / "build.log").write_text(build.stdout + build.stderr, encoding="utf-8")
    assert build.returncode == 0, ("writer compilation failed", build.stdout, build.stderr)
    prefix = out / "model"
    execution = subprocess.run([str(executable), str(prefix)], cwd=root,
                               text=True, capture_output=True, timeout=30)
    (out / "writer.log").write_text(execution.stdout + execution.stderr, encoding="utf-8")
    assert execution.returncode == 0, ("writer fixture failed", execution.returncode,
                                       execution.stdout, execution.stderr)
    assert not execution.stdout and not execution.stderr
    expected_bytes = struct.pack("<BHiqdI", 171, 48879, -(2**31), -(2**63), -0.0, 5) + "é雪".encode()
    assert (out / "model.bytes").read_bytes() == expected_bytes
    assert (out / "model.sizes").read_bytes() == expected_sizes
    program = (out / "model.knc").read_bytes()
    assert program == _expected_program(), "KNC v3 serialization differs from wire ABI"
    assert (out / "model.repeat.knc").read_bytes() == program, "serialization is nondeterministic"
    assert (out / "model.annotated.knc").read_bytes() == program, "listing annotations changed binary serialization"
    listing = (out / "model.listing/nested/program.knasm").read_text(encoding="utf-8")
    assert "KNC2 v3" in listing and ".function #0 Main" in listing
    assert "r0(result)" in listing and "builtin#0, (r0(result))" in listing
    assert "; source line 7\\ncontinued" in listing and "; function end" in listing
    assert '"é雪"' in listing and "-9223372036854775808" in listing
    assert "ignored" not in listing
    assert (out / "model.listing/nested/repeat.knasm").read_text(encoding="utf-8") == listing
    coverage = (out / "model.listing/nested/all.knasm").read_text(encoding="utf-8")
    mnemonic_names = [name.replace("JumpOp", "Jump").replace("ThrowOp", "Throw") for name in opcodes]
    for name in mnemonic_names:
        assert re.search(r"^\d+: " + name + r"\s", coverage, flags=re.M), ("missing listing opcode", name)
    assert "width=64" in coverage and "unsigned" in coverage and "@257" in coverage
    assert "function#257" in coverage and "type#257" in coverage and "field#257" in coverage
    assert "capture#257" in coverage and "global#257" in coverage
    assert "invalid argument count 255" in coverage and "builtin#65535" in coverage
    assert not (out / "model.invalid.knasm").exists(), "invalid program listing was written"
    assert not (out / "model.invalid").exists(), "invalid program was written"
    if vm is not None:
        run = subprocess.run([str(vm), str(out / "model.knc")], cwd=root,
                             text=True, capture_output=True, timeout=30)
        assert run.returncode == 0, ("VM execution failed", run.stdout, run.stderr)
        assert run.stdout.replace("\r\n", "\n") == "-9223372036854775808\né雪\n"
        assert not run.stderr
        disasm = subprocess.run([str(vm), "--disasm", str(out / "model.knc")], cwd=root,
                                text=True, capture_output=True, timeout=30)
        (out / "model.disasm").write_text(disasm.stdout + disasm.stderr, encoding="utf-8")
        assert disasm.returncode == 0 and "LoadInt" in disasm.stdout and "Halt" in disasm.stdout
    return {"name": "knc_v3_model", "ok": True, "opcode_count": 139,
            "program_bytes": len(program), "vm_executed": vm is not None, "listing_opcode_count": 139}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--vm", type=Path)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--link-arg", action="append", default=[])
    args = parser.parse_args()
    print(json.dumps(check_knc_model(args.compiler.resolve(), Path(__file__).resolve().parents[2],
                                     args.out_dir.resolve(), args.vm.resolve() if args.vm else None,
                                     link_args=tuple(args.link_arg))))
