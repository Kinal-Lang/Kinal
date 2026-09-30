"""Runtime-free lowering checks. Foreign objects are inspected, never executed."""
from __future__ import annotations

import argparse
import json
import platform
import re
from pathlib import Path

from check_targets import TARGETS, check_object, function_body, invoke


SOURCE = '''Unit Tests.FreestandingSelf;
Get IO.Target;
Get IO.Host;
Get IO.Panic;
Get IO.Volatile;
Enum State By u8 { Ready = 7 }
Struct Pair { int Left; int Right; }
int GlobalValue = 5;
int AddressCalls = 0;
Trusted Function int* Address(int* pointer) { AddressCalls++; Return pointer; }
Static Function int probe_env() { Return IO.Target.Env; }
Static Function int probe_host_env() { Return IO.Host.Env; }
Static Function int probe_os() { Return IO.Target.OS; }
Static Function int probe_arch() { Return IO.Target.Arch; }
Safe Function Pair MakePair(int value) { Pair pair; pair.Left = value; pair.Right = 10; Return pair; }
Safe Function int Sum(Pair pair) { Return pair.Left + pair.Right; }
Safe Function int Add<T>(T value) { Return [int](value) + 1; }
Safe Function int Borrowed(int[] values) { Return values.Length(); }
Safe Function string Literal() { Return "static text"; }
Trusted Static Function void KMain(int* result)
{
    Pair pair = MakePair(19);
    Pair* pairAddress = &pair;
    (*pairAddress).Left = 20;
    int value = Sum(pair) + Add<int>(6);
    int index = 0;
    While (index < GlobalValue) { value = value + 1; index = index + 1; }
    If (State.Ready == [State](7) && probe_env() == IO.Target.Env.Freestanding)
    {
        *result = value;
        (*Address(result)) += 3;
        int old = (*Address(result))++;
        float number = 1.0;
        float oldNumber = number++;
        int* pointer = result;
        int* oldPointer = pointer++;
        bool advanced = pointer == result + 1 && oldPointer == result;
        pointer--;
        If (AddressCalls == 2 && old == 45 && *result == 46 &&
            oldNumber == 1.0 && number == 2.0 && advanced && pointer == result) *result = 42;
        Else *result = -2;
    }
    Else *result = -1;
}
Trusted Static Function void VolatileRoundtrip(byte* address)
{
    IO.Volatile.Write8([u8*](address), 12);
    IO.Volatile.Write16([u16*](address), IO.Volatile.Read8([u8*](address)));
    IO.Volatile.Write32([u32*](address), IO.Volatile.Read16([u16*](address)));
    IO.Volatile.Write64([u64*](address), IO.Volatile.Read32([u32*](address)));
    IO.Volatile.Write64([u64*](address), IO.Volatile.Read64([u64*](address)));
}
Trusted Static Function void Stop() { IO.Panic.Halt(); }
'''


def manifest(path: Path, *, target: str = "host", entry: str = "KMain",
             panic: str = "Trap", environment: str = "Freestanding",
             runtime: str = "None", source: str = "Main.kn") -> None:
    path.write_text(f'''Project FreestandingSelf {{
    DefaultProfile = "core";
    SourceSet "source" {{ Files = ["{source}"]; RequireUnit = true; }}
    Profile "core" {{
        Source {{ Entry = "{source}"; Sets = ["source"]; Mode = FileOnly; }}
        Build {{ Environment = {environment}; Runtime = {runtime}; Panic = {panic};
                 Target = "{target}"; EntrySymbol = "{entry}"; }}
        Link {{ NoCRT = true; NoDefaultLibs = true; }}
    }}
}}
''', encoding="utf-8")


def check_runtime_free(ir: str, *, wrapper: bool, panic: str) -> None:
    assert not re.search(r"^define .*@main\(", ir, re.M), "hosted main was emitted"
    for forbidden in ("GarbageCollector", "Exception", "kn_native_", "kn_sh_rt_", "__kn_gc"):
        assert forbidden not in ir, f"unexpected runtime dependency: {forbidden}"
    declarations = re.findall(r"^declare .*@([^ (]+)", ir, re.M)
    assert all(name.startswith("llvm.") for name in declarations), declarations
    assert ('@__kn_entry(' in ir) == wrapper
    for width in (8, 16, 32, 64):
        assert f"load volatile i{width}" in ir
    for width in (8, 16, 32, 64):
        assert f"store volatile i{width}" in ir
    stop = function_body(ir, "Stop")
    if panic == "Trap":
        assert "@llvm.trap()" in stop and "unreachable" in stop, stop
    else:
        assert "@llvm.trap()" not in stop and "panic_loop" in stop, stop


def check_freestanding(compiler: Path, stage0: Path, root: Path, out: Path,
                      *, stage0_reference: bool = True) -> dict[str, object]:
    out.mkdir(parents=True, exist_ok=True)
    (out / "Main.kn").write_text(SOURCE, encoding="utf-8")
    # Published stage0 compilers may predate the narrow-enum cast fix. Platform
    # facts remain differential on every run; the new semantic regression is
    # differential only when stage0 is explicitly the current reference.
    (out / "Reference.kn").write_text('''Unit Tests.FreestandingReference;
Get IO.Target;
Get IO.Host;
Static Function int probe_env() { Return IO.Target.Env; }
Static Function int probe_host_env() { Return IO.Host.Env; }
Static Function int probe_os() { Return IO.Target.OS; }
Static Function int probe_arch() { Return IO.Target.Arch; }
Trusted Static Function void KMain() {}
''', encoding="utf-8")
    project = out / "kinal.knproj"
    targets = TARGETS + (("bare64", "x86_64-unknown-none", 0, 2),
                         ("bare-arm64", "aarch64-unknown-none", 0, 3))
    for alias, triple, os_id, arch in targets:
        manifest(project, target=alias)
        command = [str(compiler), "build", "--project", str(project)]
        ir_path = out / f"{alias}.ll"
        invoke(command + ["--emit", "ir", "-o", str(ir_path)], root)
        ir = ir_path.read_text(encoding="utf-8")
        check_runtime_free(ir, wrapper=True, panic="Trap")
        assert f'target triple = "{triple}"' in ir
        for name, value in (("env", 2), ("host_env", 1), ("os", os_id), ("arch", arch)):
            assert f"ret i64 {value}" in function_body(ir, "probe_" + name)
        obj = out / f"{alias}.o"
        invoke(command + ["--emit", "obj", "-o", str(obj)], root)
        check_object(obj, os_id or 2, arch)
        reference = out / f"stage0-{alias}.ll"
        reference_project = out / "reference.knproj"
        manifest(reference_project, target=alias, source="Reference.kn")
        invoke([str(stage0), "build", "--project", str(reference_project), "--emit", "ir", "-o", str(reference)], root)
        reference_ir = reference.read_text(encoding="utf-8")
        assert '@__kn_entry(' in reference_ir
        for name, value in (("env", 2), ("host_env", 1), ("os", os_id), ("arch", arch)):
            assert f"ret i64 {value}" in function_body(reference_ir, "probe_" + name)
        print(f"[OK] runtime-free {alias}: constants, no runtime, object header", flush=True)

    # An independent stage0-hosted consumer calls the freestanding entry. The
    # object must not need any selfhost bridge, GC, exception or startup object.
    host_os = {"Windows": 1, "Linux": 2, "Darwin": 3}[platform.system()]
    host_arch = 3 if platform.machine().lower() in {"arm64", "aarch64"} else 2
    host_alias = next(alias for alias, _, os_id, arch in TARGETS
                      if (os_id, arch) == (host_os, host_arch))
    harness = out / "Harness.kn"
    harness.write_text('''[LinkName("__kn_entry")]
Extern Function void Run(int* result) By C;
Trusted Static Function int Main() {
    int result = 0;
    Run(&result);
    Return result == 42 ? 0 : 1;
}
''', encoding="utf-8")
    exe = out / ("consumer.exe" if host_os == 1 else "consumer")
    invoke([str(stage0), "build", "--no-module-discovery", str(harness), "--link-file", str(out / f"{host_alias}.o"),
            "-o", str(exe)], root)
    invoke([str(exe)], root)
    if stage0_reference:
        manifest(project)
        reference_obj = out / "stage0-host.o"
        invoke([str(stage0), "build", "--project", str(project), "--emit", "obj", "-o", str(reference_obj)], root)
        reference_exe = out / ("reference-consumer.exe" if host_os == 1 else "reference-consumer")
        invoke([str(stage0), "build", "--no-module-discovery", str(harness), "--link-file", str(reference_obj),
                "-o", str(reference_exe)], root)
        invoke([str(reference_exe)], root)

    hosted = out / "hosted-lvalues"
    hosted.mkdir(exist_ok=True)
    (hosted / "Main.kn").write_text(
        (root / "tests" / "common" / "compound_assignment_once.kn").read_text(encoding="utf-8"),
        encoding="utf-8")
    hosted_project = hosted / "kinal.knproj"
    hosted_project.write_text('''Project HostedLvalues {
    DefaultProfile = "native";
    SourceSet "source" { Files = ["Main.kn"]; RequireUnit = true; }
    Profile "native" { Source { Entry = "Main.kn"; Sets = ["source"]; Mode = FileOnly; } }
}
''', encoding="utf-8")
    hosted_exe = hosted / ("lvalues.exe" if host_os == 1 else "lvalues")
    invoke([str(compiler), "build", "--project", str(hosted_project), "-o", str(hosted_exe)], root)
    assert invoke([str(hosted_exe)], root).replace("\r\n", "\n") == "ok\n"

    for entry, panic in (("", "Trap"), ("Tests.FreestandingSelf.KMain", "Loop")):
        manifest(project, entry=entry, panic=panic)
        output = out / ("default-entry.ll" if not entry else "loop.ll")
        invoke([str(compiler), "build", "--project", str(project), "--emit", "ir", "-o", str(output)], root)
        check_runtime_free(output.read_text(encoding="utf-8"), wrapper=True, panic=panic)

    negatives = {
        "allocation": "Class Item {} Static Function void KMain() { Item item = New Item(); }",
        "async": "Async Static Function int KMain() { Return 0; }",
        "exception": 'Static Function void KMain() { Throw "error"; }',
        "any": "Static Function void KMain() { any value = 1; }",
        "fixed-local": "Static Function void KMain() { int[4] values; }",
        "fixed-global": "int[4] values; Static Function void KMain() {}",
        "fixed-field": "Struct Item { int[4] Values; } Static Function void KMain() {}",
        "array-literal": "Static Function void KMain() { int[] values = {1, 2}; }",
        "string-op": 'Static Function string KMain(string value) { Return value + "x"; }',
        "string-convert": "Static Function string KMain() { Return [string](42); }",
        "implicit-string": "Static Function string KMain() { Return 42; }",
        "closure": "Static Function void KMain() { Var f = Function int() { Return 1; }; }",
        "collection": "Static Function void KMain() { list values = list.Create(); }",
    }
    for name, source in negatives.items():
        (out / "Negative.kn").write_text("Unit Tests.Negative;\n" + source, encoding="utf-8")
        manifest(project, source="Negative.kn", entry="")
        output = out / f"rejected-{name}.ll"
        invoke([str(compiler), "build", "--project", str(project), "--emit", "ir", "-o", str(output)],
               root, error="Return Type" if name == "implicit-string" else "Freestanding Core")
        assert not output.exists(), output

    for entry, error in (("Missing", "freestanding entry not found"),
                         ("Sum", "freestanding entry must take")):
        manifest(project, entry=entry)
        invoke([str(compiler), "build", "--project", str(project), "--emit", "ir", "-o", str(out / "bad-entry.ll")],
               root, error=error)
    manifest(project)
    invoke([str(compiler), "build", "--project", str(project), "-o", str(out / "unsupported-bin")],
           root, error="Freestanding linking is not supported")
    assert not (out / "unsupported-bin").exists()
    assert not (out / "unsupported-bin.obj").exists()
    print("[OK] freestanding host consumer, entry/Panic contracts and rejection gates", flush=True)
    return {"name": "freestanding_core", "ok": True, "targets": len(targets),
            "negative_programs": len(negatives), "host_consumer": True,
            "stage0_runtime_reference": stage0_reference, "hosted_lvalues": True}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--stage0-role", choices=("reference", "bootstrap"), default="reference")
    args = parser.parse_args()
    result = check_freestanding(args.compiler.resolve(), args.stage0.resolve(), args.root.resolve(), args.out_dir.resolve(),
                                stage0_reference=args.stage0_role == "reference")
    print(json.dumps(result))
