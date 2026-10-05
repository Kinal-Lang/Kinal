"""Source/project `vm build` integration and execution-backend constants.

Runs emitted KNC with the supplied existing KinalVM executable. It never routes
selfhost compilation through stage0. The stage0 executable is only a reference.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import struct
import subprocess


RUNTIME_CONSTANTS = '''Unit Tests.RuntimeConstants;
Get IO.Runtime;
Get IO.Console;
int GlobalBackend = IO.Runtime.Current;
bool GlobalVm = IO.Runtime.IsVM;
string GlobalName = IO.Runtime.Name;
int Calls = 0;
[LinkName("abs")]
Extern Function i32 NativeAbs(i32 value) By C;
Function bool Touch() { Calls += 1; Return true; }
Trusted Static Function int Main()
{
    If (IO.Runtime.Unknown != 0 || IO.Runtime.Kind.Unknown != 0 ||
        IO.Runtime.Native != 1 || IO.Runtime.Kind.Native != 1 ||
        IO.Runtime.VM != 2 || IO.Runtime.Kind.VM != 2) Return 1;
    If (IO.Runtime != IO.Runtime.Current || IO.Runtime != IO.Runtime.Kind ||
        IO.Runtime != IO.Runtime.Kind.Current) Return 2;
    If ((IO.Runtime.IsVM).TypeOf() != "bool" || (IO.Runtime.IsNative).TypeOf() != "bool") Return 3;
    If (GlobalBackend != IO.Runtime || GlobalVm != IO.Runtime.IsVM || GlobalName != IO.Runtime.Name) Return 4;
    If (IO.Runtime.IsVM)
    {
        If (IO.Runtime != IO.Runtime.VM || IO.Runtime.Name != "Kinal.VM" || IO.Runtime.IsNative) Return 5;
    }
    Else
    {
        If (IO.Runtime != IO.Runtime.Native || IO.Runtime.Name != "Kinal.Native" || !IO.Runtime.IsNative) Return 6;
    }
    // Folding a known right operand must not remove effects on the left.
    If (Touch() && IO.Runtime.IsVM) { }
    If (Touch() || IO.Runtime.IsVM) { }
    If (Calls != 2) Return 7;
    If (IO.Runtime.IsNative)
    {
        If (NativeAbs([i32](-7)) != 7) Return 8;
    }
    If (!IO.Runtime.IsVM && IO.Runtime.IsNative) IO.Console.PrintLine("native-branch");
    Else IO.Console.PrintLine("vm-branch");
    IO.Console.PrintLine(GlobalBackend);
    IO.Console.PrintLine(GlobalVm);
    IO.Console.PrintLine(GlobalName);
    Return 0;
}
'''


def check_vm_cli(compiler: Path, stage0: Path, vm: Path, root: Path, out: Path,
                 *, stage0_reference: bool = True) -> dict[str, object]:
    out.mkdir(parents=True, exist_ok=True)
    logs = out / "logs"
    logs.mkdir(exist_ok=True)
    sequence = 0
    cases = 0

    def invoke(label: str, command: list[str | Path], *, code: int = 0,
               text: str | None = None) -> subprocess.CompletedProcess[str]:
        nonlocal sequence
        sequence += 1
        values = [str(value) for value in command]
        proc = subprocess.run(values, cwd=out, text=True, capture_output=True, timeout=180)
        (logs / f"{sequence:03d}-{label}.log").write_text(
            "command=" + repr(values) + "\nexit=" + str(proc.returncode) + "\n" +
            proc.stdout + proc.stderr, encoding="utf-8")
        assert proc.returncode == code, (label, proc.returncode, proc.stdout, proc.stderr)
        if text is not None:
            assert text in proc.stdout + proc.stderr, (label, text, proc.stdout, proc.stderr)
        return proc

    def execute(label: str, output: Path, expected: str) -> None:
        assert output.read_bytes()[:8] == b"KNC2" + struct.pack("<HH", 3, 0), output
        run = invoke(label, [vm, output])
        assert run.stdout.replace("\r\n", "\n") == expected, (label, run.stdout, expected)
        assert not run.stderr, (label, run.stderr)

    tools = [("stage0", stage0)] if stage0_reference else []
    tools.append(("selfhost", compiler))
    for name, expected in (("hello", "hello\n"), ("functions", "ok\n"), ("control", "ok\n")):
        source = root / "tests" / "common" / f"{name}.kn"
        for label, tool in tools:
            output = out / f"{label}-{name}.knc"
            invoke(label + "-source", [tool, "vm", "build", source, "--no-module-discovery", "-o", output])
            execute(label + "-execute", output, expected)
        cases += 1

    source = out / "Runtime.kn"
    source.write_text(RUNTIME_CONSTANTS, encoding="utf-8")
    expected_vm = "vm-branch\n2\ntrue\nKinal.VM\n"
    expected_native = "native-branch\n1\nfalse\nKinal.Native\n"
    for label, tool in tools:
        output = out / f"{label}-runtime.knc"
        invoke(label + "-runtime-build", [tool, "vm", "build", "--no-module-discovery", source, "-o", output])
        execute(label + "-runtime-execute", output, expected_vm)
        native = out / (label + "-runtime" + (".exe" if os.name == "nt" else ""))
        invoke(label + "-native-build", [tool, "build", "--no-module-discovery", source, "-o", native])
        run = invoke(label + "-native-execute", [native])
        assert run.stdout.replace("\r\n", "\n") == expected_native, (label, run.stdout)
        cases += 2

    # No -o uses the first source's base name in the invoking directory.
    invoke("default-output", [compiler, "vm", "build", "--no-module-discovery", source])
    default_output = out / "Runtime.knc"
    execute("default-execute", default_output, expected_vm)
    first = default_output.read_bytes()
    invoke("deterministic-output", [compiler, "vm", "build", source, "--no-module-discovery"])
    assert default_output.read_bytes() == first
    nested = out / "new" / "nested" / "runtime.knc"
    invoke("nested-output", [compiler, "vm", "build", "--no-module-discovery", source, "--output", nested])
    execute("nested-execute", nested, expected_vm)
    cases += 2

    project = out / "kinal.knproj"
    project.write_text('''Project VmCli {
    DefaultProfile = "vm";
    SourceSet "app" { Files = ["Runtime.kn"]; RequireUnit = true; }
    Profile "vm" {
        Source { Entry = "Runtime.kn"; Sets = ["app"]; Mode = FileOnly; }
        Build { Backend = VM; Output = "configured.knc"; }
    }
    Profile "implicit" {
        Source { Entry = "Runtime.kn"; Sets = ["app"]; Mode = FileOnly; }
    }
    Profile "native" {
        Source { Entry = "Runtime.kn"; Sets = ["app"]; Mode = FileOnly; }
        Build { Backend = Native; }
    }
}
''', encoding="utf-8")
    for label, tool in tools:
        for profile in ("vm", "implicit"):
            artifact = out / f"{label}-{profile}.knc"
            invoke(label + "-project-" + profile, [tool, "vm", "build", "--project", project,
                                                 "--profile", profile, "-o", artifact])
            execute(label + "-project-execute-" + profile, artifact, expected_vm)
            cases += 1
    invoke("project-default-output", [compiler, "vm", "build", "--project", out])
    execute("project-default-execute", out / "configured.knc", expected_vm)
    cases += 1

    # Backend selection applies independently per compilation, not to Host or
    # to the compiler process that happens to emit the artifact.
    native = out / ("implicit-native" + (".exe" if os.name == "nt" else ""))
    invoke("implicit-native-profile", [compiler, "build", "--project", project,
                                       "--profile", "implicit", "-o", native])
    run = invoke("implicit-native-execute", [native])
    assert run.stdout.replace("\r\n", "\n") == expected_native
    invoke("native-profile-rejected-for-vm", [compiler, "vm", "build", "--project", project,
                                             "--profile", "native"], code=1, text="Build.Backend=Native")
    invoke("vm-profile-rejected-for-native", [compiler, "build", "--project", project], code=1,
           text="Build.Backend=VM")
    cases += 3

    args = out / "Arguments.kn"
    args.write_text('Unit Tests.VmArguments; Static Function int Main(string[] args) { Return args.Length(); }\n',
                    encoding="utf-8")
    args_output = out / "args.knc"
    invoke("empty-entry-args", [compiler, "vm", "build", "--no-module-discovery", args, "-o", args_output])
    execute("empty-entry-args-execute", args_output, "")
    cases += 1

    invalid = out / "Invalid.kn"
    invalid.write_text('Unit Tests.InvalidVm; Static Function int Main() { Return Missing(); }\n', encoding="utf-8")
    invalid_output = out / "invalid.knc"
    invoke("semantic-failure", [compiler, "vm", "build", "--no-module-discovery", invalid, "-o", invalid_output], code=1)
    assert not invalid_output.exists()
    unsupported = out / "External.kn"
    unsupported.write_text('Unit Tests.ExternalVm; Extern Function int ExternalCall() By C; '
                           'Trusted Static Function int Main() { Return ExternalCall(); }\n', encoding="utf-8")
    unsupported_output = out / "unsupported.knc"
    invoke("unsupported-operation", [compiler, "vm", "build", "--no-module-discovery", unsupported,
                                     "-o", unsupported_output], code=1, text="bytecode backend failed: KNC:")
    assert not unsupported_output.exists()
    for label, arguments in (
        ("emit", ["vm", "build", source, "--emit", "obj"]),
        ("keep-temps", ["vm", "build", source, "--keep-temps"]),
        ("target", ["vm", "build", source, "--target", "host"]),
        ("link", ["vm", "build", source, "--lib", "example"]),
        ("multiple-inputs", ["vm", "build", source, args]),
        ("runtime-arguments", ["vm", "build", source, "--", "program-argument"]),
    ):
        invoke("rejected-" + label, [compiler, *arguments], code=2)
    invoke("vm-help", [compiler, "vm", "build", "--help"], text="vm build <source>")
    invoke("vm-group-help", [compiler, "vm", "--help"], text="vm build <source>")
    cases += 10
    result = {"name": "selfhost_vm_cli", "ok": True, "cases": cases, "commands": sequence,
              "stage0_reference": stage0_reference, "backend": "pure-Kinal scalar KNC v3"}
    (out / "results.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"[OK] selfhost VM CLI: {cases} cases, {sequence} commands", flush=True)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--vm", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(check_vm_cli(args.compiler.resolve(), args.stage0.resolve(), args.vm.resolve(),
                                 Path(__file__).resolve().parents[2], args.out_dir.resolve())))
