"""32-bit ABI/layout contracts, with stage0 differential IR and COFF checks.

Foreign code is never executed. The current-host runtime error-layout probe is
executed separately; object emission also exercises LLVM's verifier/lowerer.
"""
from __future__ import annotations

import argparse
import json
import platform
import re
from pathlib import Path


ABI_SOURCE = '''Unit Tests.Target32Abi;
Extern Function usize probe_unsigned_size(usize value) By C;
Extern Function isize probe_signed_size(isize value) By C;
Extern Function int probe_c_int(int value) By C;
Extern Function bool probe_c_bool(bool value) By C;
Extern Function int probe_system(int value, usize size) By System;
Extern Function int probe_default(int value);
Delegate Function usize SizeDelegate(usize value) By C;
Struct LayoutValue { byte Marker; byte* Address; i64 Value; }
Class Reference { Public byte Marker; Public byte* Address; }
Static Trusted Function usize CallUnsigned(usize value) { Return probe_unsigned_size(value); }
Static Trusted Function isize CallSigned(isize value) { Return probe_signed_size(value); }
Static Trusted Function int CallSystem(int value, usize size) { Return probe_system(value, size); }
Static Trusted Function int CallDefault(int value) { Return probe_default(value); }
Static Trusted Function bool CallBool(bool value) { Return probe_c_bool(value); }
Static Trusted Function byte** PointerStep(byte** base, isize index) { Return base + index; }
Static Function usize Identity(usize value) { Return value; }
Static Trusted Function usize CallDelegate(usize value) { SizeDelegate = Identity; Return SizeDelegate(value); }
Static Function LayoutValue[] AllocateLayoutValues(int count) { LayoutValue[2] values; Return values; }
Static Function Reference AllocateReference() { Return New Reference(); }
Static Function int Main() { Return 0; }
Static Function usize AddSize(usize left, usize right) { Return left + right; }
Static Function isize DivideSize(isize left, isize right) { Return left / right; }
Static Function usize RemainderSize(usize left, usize right) { Return left % right; }
Static Function usize CompoundSize(usize left, usize right) { left += right; Return left; }
Static Function bool CompareSize(usize left, usize right) { Return left < right; }
Static Function bool CompareWide(usize left, u64 right) { Return left < right; }
Static Function usize ShiftSize(usize left, usize right) { Return left >> right; }
Static Function usize CastSize(u64 value) { Return [usize](value); }
Static Function usize LiteralSize() { Return [usize](4294967296); }
Enum NativeCode By int { Value = 1 }
Enum NativeSize By usize { Value = 1 }
Extern Function NativeCode enum_default(NativeCode value);
Extern Function NativeCode enum_c(NativeCode value) By C;
Extern Function NativeCode enum_system(NativeCode value) By System;
Extern Function NativeSize enum_size(NativeSize value) By C;
Static Trusted Function NativeCode CallEnumDefault(NativeCode value) { Return enum_default(value); }
Static Trusted Function NativeCode CallEnumC(NativeCode value) { Return enum_c(value); }
Static Trusted Function NativeCode CallEnumSystem(NativeCode value) { Return enum_system(value); }
Static Trusted Function NativeSize CallEnumSize(NativeSize value) { Return enum_size(value); }
'''

RUNTIME_SOURCE = '''Unit Tests.Runtime32;
Get IO.Kinal.Runtime;
Get IO.GC;
Static Trusted Function int Main() {
    byte* value = IO.Kinal.Runtime.ErrorNew(55, "title", "message");
    If (IO.Kinal.Runtime.ErrorTitle(value) != "title") Return 1;
    If (IO.Kinal.Runtime.ErrorMessage(value) != "message") Return 2;
    IO.Kinal.Runtime.ErrorSetTrace(value, "trace");
    If (IO.Kinal.Runtime.ErrorTrace(value) != "trace") Return 3;
    byte* inner = IO.Kinal.Runtime.ErrorNew(56, "inner", "message");
    IO.Kinal.Runtime.ErrorLinkInner(value, inner);
    IO.GC.Collect();
    If (IO.Kinal.Runtime.ErrorInner(value) != inner) Return 4;
    Return 0;
}
'''

def body(ir: str, name: str) -> str:
    # Symbols differ between stage0 and selfhost; source function names do not.
    match = re.search(r'^define [^\n]*@[^\n(]*' + re.escape(name) +
                      r'(?:_\d+)?\([^\n]*\).*?\n}\n', ir, re.M | re.S)
    assert match, f"missing function {name}"
    return match.group()


def check_abi_ir(ir: str, bits: int) -> None:
    size_type = f"i{bits}"
    if bits == 32:
        assert re.search(r'target datalayout = "[^"\n]*-p:32:32', ir)
    for name in ("probe_unsigned_size", "probe_signed_size"):
        assert f"declare {size_type} @{name}({size_type})" in ir, name
    assert "declare i32 @probe_default(i32)" in ir
    assert "declare i32 @probe_c_bool(i32)" in ir
    cc = "x86_stdcallcc " if bits == 32 else ""
    assert f"declare {cc}i32 @probe_system(i32, {size_type})" in ir
    assert f"call {cc}i32 @probe_system(" in body(ir, "CallSystem")
    for name in ("enum_default", "enum_c"):
        assert f"declare i32 @{name}(i32)" in ir, name
    assert f"declare {cc}i32 @enum_system(i32)" in ir
    assert f"call {cc}i32 @enum_system(" in body(ir, "CallEnumSystem")
    assert f"declare {size_type} @enum_size({size_type})" in ir
    for name in ("CallEnumDefault", "CallEnumC", "CallEnumSystem"):
        value = body(ir, name)
        assert re.search(r"trunc i64 .* to i32", value), value
        assert re.search(r"sext i32 .* to i64", value), value
    if bits == 32:
        value = body(ir, "CallEnumSize")
        assert re.search(r"trunc i64 .* to i32", value), value
        assert re.search(r"zext i32 .* to i64", value), value
    for name, extension in (("CallUnsigned", "zext"), ("CallSigned", "sext")):
        value = body(ir, name)
        assert re.search(r"^define i64 .+\(i64 ", value), value
        if bits == 32:
            assert re.search(r"trunc i64 .* to i32", value), value
            assert re.search(extension + r" i32 .* to i64", value), value
        else:
            assert not re.search(r"(?:trunc|zext|sext) i(?:32|64)", value), value
    for name, instruction in (("AddSize", "add"), ("CompoundSize", "add"),
                              ("DivideSize", "sdiv"), ("RemainderSize", "urem"),
                              ("ShiftSize", "lshr"), ("CompareSize", "icmp ult")):
        value = body(ir, name)
        assert re.search(r"\b" + instruction + " " + size_type + r"\b", value), value
    assert "icmp ult i64" in body(ir, "CompareWide")
    assert "ret i64 4294967296" in body(ir, "LiteralSize")
    assert "trunc" not in body(ir, "CastSize")
    assert re.search(r"getelementptr inbounds (?:nuw )?ptr, ptr .* i64 ",
                     body(ir, "PointerStep"))
    assert re.search(r"%[^\n]*LayoutValue = type \{ i8, ptr, i64 \}", ir)
    assert re.search(r"^define \{ ptr, i64 \} .*AllocateLayoutValues", ir, re.M)
    # Both adapters and indirect calls must carry pointer-sized C size_t.
    assert re.search(r"call " + size_type + r" %[^(]+\(" + size_type + r" ",
                     body(ir, "CallDelegate"))
    adapters = re.findall(r'^define [^\n]+\([^\n]*\).*?\n}\n', ir, re.M | re.S)
    adapter = next((value for value in adapters
                    if re.search(r"call i64 @[^\n(]*Identity(?:_\d+)?\(i64 ", value)), None)
    assert adapter is not None, "missing C delegate adapter"
    assert re.search(r"^define (?:internal )?" + size_type + r" .*\(" + size_type + r" ", adapter)
    if bits == 32:
        assert re.search(r"zext i32 .* to i64", adapter), adapter
        assert re.search(r"trunc i64 .* to i32", adapter), adapter


def check_runtime_layout(ir: str) -> None:
    scan = body(ir, "ScanRegion")
    assert re.search(r"getelementptr inbounds (?:nuw )?ptr, ptr ", scan), scan
    assert re.search(r"load ptr, ptr ", scan), scan
    assert "%IO.Kinal.Runtime.RuntimeErrorStorage = type { ptr, i64, ptr, ptr, ptr, ptr }" in ir
    for name, field in (("ErrorTitle", 2), ("ErrorMessage", 3),
                        ("ErrorTrace", 4), ("ErrorInner", 5)):
        value = body(ir, name)
        assert re.search(r"getelementptr inbounds (?:nuw )?%IO\.Kinal\.Runtime\.RuntimeErrorStorage, "
                         r"ptr [^\n]+, i32 0, i32 " + str(field), value), value
    value = body(ir, "ErrorNew")
    assert "getelementptr inbounds %IO.Kinal.Runtime.RuntimeErrorStorage" in value
    assert re.search(r"getelementptr inbounds (?:nuw )?%IO\.Kinal\.Runtime\.RuntimeErrorStorage, "
                     r"ptr [^\n]+, i32 0, i32 1", value), value


def check_target_abi(compiler: Path, stage0: Path, root: Path, out: Path) -> dict[str, object]:
    from check_targets import invoke, check_object

    out.mkdir(parents=True, exist_ok=True)
    source = out / "Abi.kn"
    source.write_text(ABI_SOURCE, encoding="utf-8")
    for bits, target in ((32, "win86"), (64, "win64")):
        for role, executable in (("self", compiler), ("stage0", stage0)):
            stem = out / f"{role}-{bits}"
            command = [str(executable), "build", str(source), "--no-module-discovery",
                       "--target", target]
            path = stem.with_suffix(".ll")
            invoke(command + ["--emit", "ir", "-o", str(path)], root)
            check_abi_ir(path.read_text(encoding="utf-8"), bits)
            obj = stem.with_suffix(".obj")
            invoke(command + ["--emit", "obj", "-o", str(obj)], root)
            check_object(obj, 1, 1 if bits == 32 else 2)
            if bits == 32:
                contents = obj.read_bytes()
                assert b"_probe_system@8\0" in contents, obj
                assert b"_probe_default\0" in contents, obj
        print(f"[OK] {bits}-bit C/System ABI, size arithmetic, pointers and layouts", flush=True)

    capture = root / "tests/selfhost/fixtures/capture_alignment/kinal.knproj"
    for role, executable in (("self", compiler), ("stage0", stage0)):
        path = out / f"{role}-capture.ll"
        command = [str(executable), "build", "--project", str(capture), "--target", "win86"]
        invoke(command + ["--emit", "ir", "-o", str(path)], root)
        ir = path.read_text(encoding="utf-8")
        assert re.search(r"ptrtoint ptr [^\n]+ to i32", ir)
        obj = path.with_suffix(".obj")
        invoke(command + ["--emit", "obj", "-o", str(obj)], root)
        check_object(obj, 1, 1)

    # Every extern spelling shares the supported FFI contract. Unknown widths
    # and value aggregates must fail before LLVM construction, even unused.
    for abi in ("", " By C", " By System"):
        for invalid_type in ("Opaque", "any", "f16", "f128"):
            for position in ("return", "parameter"):
                name = f"invalid-{abi.strip().replace(' ', '-') or 'default'}-{invalid_type}-{position}"
                path = out / (name + ".kn")
                signature = (f"{invalid_type} External()" if position == "return" else
                             f"void External({invalid_type} value)")
                path.write_text("Unit Tests.InvalidFfi; Struct Opaque { int Value; }\n"
                                f"Extern Function {signature}{abi};\n"
                                "Static Function int Main() { Return 0; }\n", encoding="utf-8")
                for role, executable in (("self", compiler), ("stage0", stage0)):
                    rejected = out / f"{role}-{name}.ll"
                    invoke([str(executable), "build", str(path), "--no-module-discovery",
                            "--target", "win86", "--emit", "ir", "-o", str(rejected)],
                           root, error="C ABI " + position + " type is not supported")
                    assert not rejected.exists(), rejected
    print("[OK] default/C/System extern signature validation matches stage0", flush=True)
    for abi in ("", " By C", " By System"):
        for safety in ("", "Safe ", "Trusted ", "Unsafe "):
            name = f"safety-{abi.strip().replace(' ', '-') or 'default'}-{safety.strip() or 'default'}"
            path = out / (name + ".kn")
            path.write_text("Unit Tests.ExternSafety;\n"
                            f"Extern {safety}Function int Foreign(int value){abi};\n"
                            "Static Safe Function int Main() { Return Foreign(1); }\n", encoding="utf-8")
            for role, executable in (("self", compiler), ("stage0", stage0)):
                output = out / f"{role}-{name}.ll"
                invoke([str(executable), "build", str(path), "--no-module-discovery",
                        "--target", "win86", "--emit", "ir", "-o", str(output)], root,
                       error=None if safety == "Trusted " else "Safe function cannot call Unsafe")
                assert output.exists() == (safety == "Trusted "), output
    print("[OK] default/C/System extern safety and Trusted boundaries match stage0", flush=True)

    runtime = out / "Runtime.kn"
    runtime.write_text(RUNTIME_SOURCE, encoding="utf-8")
    for bits, target in ((32, "win86"), (64, "win64")):
        path = out / f"self-runtime-{bits}.ll"
        command = [str(compiler), "build", str(runtime), "--no-module-discovery",
                   "--target", target]
        invoke(command + ["--emit", "ir", "-o", str(path)], root)
        check_runtime_layout(path.read_text(encoding="utf-8"))
        obj = path.with_suffix(".obj")
        invoke(command + ["--emit", "obj", "-o", str(obj)], root)
        check_object(obj, 1, 1 if bits == 32 else 2)
    executable = out / ("runtime-host.exe" if platform.system() == "Windows" else "runtime-host")
    invoke([str(compiler), "build", str(runtime), "--no-module-discovery",
            "-o", str(executable)], root)
    invoke([str(executable)], root)
    print("[OK] 32-bit aligned captures; typed runtime error layout and host runtime", flush=True)
    return {"name": "target_abi", "ok": True, "pointer_bits": [32, 64]}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(check_target_abi(args.compiler.resolve(), args.stage0.resolve(),
                                     Path(__file__).resolve().parents[2], args.out_dir.resolve())))


if __name__ == "__main__":
    main()
