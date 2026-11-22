"""Normal VM run/disasm/pack lifecycle, temporary ownership and runner packaging."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
from pathlib import Path


def check_vm_lifecycle(compiler: Path, vm: Path, root: Path, out: Path) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    logs = out / "logs"; logs.mkdir(exist_ok=True)
    temporary = out / "temporary files"; temporary.mkdir(exist_ok=True)
    env = dict(os.environ, TMP=str(temporary), TEMP=str(temporary), TMPDIR=str(temporary))
    sequence = 0
    suffix = ".exe" if os.name == "nt" else ""
    vm_hash = hashlib.sha256(vm.read_bytes()).hexdigest()

    def invoke(label: str, arguments: list[str | Path], code: int = 0, text: str | None = None):
        nonlocal sequence
        sequence += 1
        command = [str(part) for part in arguments]
        proc = subprocess.run(command, cwd=out, env=env, encoding="utf-8", capture_output=True, timeout=180)
        (logs / f"{sequence:03d}-{label}.log").write_text(
            "command=" + repr(command) + "\nexit=" + str(proc.returncode) + "\n" + proc.stdout + proc.stderr, encoding="utf-8")
        assert proc.returncode == code, (label, proc.returncode, proc.stdout, proc.stderr)
        if text is not None: assert text in proc.stdout + proc.stderr, (label, proc.stdout, proc.stderr)
        return proc

    source = out / "Main.kn"
    source.write_text('Unit Tests.VmLifecycle; Get IO.Console; Static Function int Main() { '
                      'IO.Console.PrintLine("lifecycle-ok"); Return 7; }\n')
    project = out / "kinal.knproj"
    project.write_text('Project Lifecycle { DefaultProfile="vm"; SourceSet "app" { Files=["Main.kn"]; '
                       'RequireUnit=true; } Profile "vm" { Source { Entry="Main.kn"; Sets=["app"]; '
                       'Mode=FileOnly; } Build { Backend=VM; } } }\n')
    bytecode = out / "program.knc"
    invoke("bytecode-build", [compiler, "vm", "build", "--project", project, "-o", bytecode])
    assert bytecode.read_bytes()[:4] == b"KNC2"
    before = set(temporary.iterdir())
    for label, inputs in (("source", ["--no-module-discovery", source]),
                          ("project", ["--project", project]), ("bytecode", [bytecode])):
        executed = invoke("run-" + label, [compiler, "vm", "run", "--vm-path", vm, *inputs], 7)
        assert executed.stdout == "lifecycle-ok\n" and not executed.stderr, (label, executed.stdout)
        assert set(temporary.iterdir()) == before
    # A packaged runner must support the default sibling lookup as well.
    assert (compiler.parent / ("kinalvm" + suffix)).is_file()
    default = invoke("default-runner", [compiler, "vm", "run", bytecode], 7)
    assert default.stdout == "lifecycle-ok\n"
    listing = invoke("disasm", [compiler, "vm", "disasm", "--vm-path", vm, bytecode])
    assert "Tests.VmLifecycle.Main" in listing.stdout

    kept = invoke("run-keep", [compiler, "vm", "run", "--keep-temps", "--vm-path", vm,
                                "--no-module-discovery", source], 7)
    retained_dirs = set(temporary.iterdir()) - before
    assert len(retained_dirs) == 1 and kept.stdout == "lifecycle-ok\n"
    retained = next(iter(retained_dirs)) / "program.knc"
    assert retained.is_file()
    invoke("retained-bytecode", [vm, retained], 7, "lifecycle-ok")
    retained.unlink(); retained.parent.rmdir()
    assert set(temporary.iterdir()) == before

    for label, inputs in (("source", ["--no-module-discovery", source]),
                          ("project", ["--project", project]), ("bytecode", [bytecode])):
        packed = out / "packed output" / (label + suffix)
        invoke("pack-" + label, [compiler, "vm", "pack", "--vm-path", vm, *inputs, "-o", packed])
        data = packed.read_bytes(); length = struct.unpack("<I", data[-8:-4])[0]
        assert data[-4:] == b"KNCE" and data[-8-length:-8][:4] == b"KNC2"
        assert data[:len(vm.read_bytes())] == vm.read_bytes()
        if label == "bytecode": assert data[-8-length:-8] == bytecode.read_bytes()
        run = invoke("packed-run-" + label, [packed], 7)
        assert run.stdout == "lifecycle-ok\n" and not run.stderr
        assert set(temporary.iterdir()) == before
    # Use a copied bytecode stem, so default pack output cannot overlap source.
    default_knc = out / "LifecycleStandalone.knc"; shutil.copy2(bytecode, default_knc)
    invoke("default-pack", [compiler, "vm", "pack", "--vm-path", vm, default_knc])
    invoke("default-packed-run", [default_knc.with_suffix(suffix)], 7, "lifecycle-ok")

    kept_exe = out / ("keep-packed" + suffix)
    kept = invoke("pack-keep", [compiler, "vm", "pack", "--keep-temps", "--vm-path", vm,
                                 "--no-module-discovery", source, "-o", kept_exe])
    retained_dirs = set(temporary.iterdir()) - before
    assert len(retained_dirs) == 1 and kept.stdout == ""
    retained = next(iter(retained_dirs)) / "program.knc"
    assert retained.is_file() and kept_exe.is_file()
    invoke("keep-packed-run", [kept_exe], 7, "lifecycle-ok")
    retained.unlink(); retained.parent.rmdir()
    assert set(temporary.iterdir()) == before

    bad = out / "Bad.kn"
    bad.write_text('Unit Tests.BadVm; Static Function int Main() { Return Missing(); }\n')
    invoke("source-error", [compiler, "vm", "run", "--vm-path", vm, "--no-module-discovery", bad], 1)
    assert set(temporary.iterdir()) == before
    invoke("missing-runner", [compiler, "vm", "run", "--vm-path", out / "missing-vm", bytecode], 1,
           "failed to locate kinalvm")
    invoke("missing-input", [compiler, "vm", "run", "--vm-path", vm, out / "missing.knc"], 1,
           "bytecode file not found")
    invoke("non-bytecode-disasm", [compiler, "vm", "disasm", "--vm-path", vm, source], 1,
           "requires a .knc file")
    ignored = invoke("zero-parameter-entry", [compiler, "vm", "run", "--vm-path", vm,
                                              bytecode, "--", "ignored"], 7)
    assert ignored.stdout == "lifecycle-ok\n" and not ignored.stderr
    argument_source = out / "Arguments.kn"
    argument_source.write_text('Unit Tests.PassArgs; Get IO.Console; Static Function int Main(string[] args) { '
                               'For (int i = 0; i < args.Length(); i++) IO.Console.PrintLine("[" + args[i] + "]"); '
                               'Return 7; }\n', encoding="utf-8")
    argument_project = out / "arguments.knproj"
    argument_project.write_text(project.read_text(encoding="utf-8").replace(source.name, argument_source.name),
                                encoding="utf-8")
    packed_args = out / ("with-args" + suffix)
    invoke("pack-arguments", [compiler, "vm", "pack", "--vm-path", vm, "--no-module-discovery",
                              argument_source, "-o", packed_args])
    for first in ("--disasm", "--version", "-V"):
        values = [first, "", "two words", 'a"b', "尾📂\\"]
        expected = "".join(f"[{value}]\n" for value in values)
        for mode, command in (("packed", [packed_args]),
                              ("project", [compiler, "vm", "run", "--vm-path", vm, "--project", argument_project, "--"]),
                              ("source", [compiler, "vm", "run", "--vm-path", vm, "--no-module-discovery",
                                          argument_source, "--"])):
            got = invoke(mode + "-arguments", [*command, *values], 7)
            assert got.stdout == expected and not got.stderr, (mode, got.stdout, got.stderr)
    assert set(temporary.iterdir()) == before
    original = bytecode.read_bytes()
    invoke("overlapping-pack-output", [compiler, "vm", "pack", "--vm-path", vm, bytecode, "-o", bytecode], 1,
           "must differ from its inputs")
    assert bytecode.read_bytes() == original
    same_output = out / "same-output.knc"
    same_output.write_bytes(b"preserve this existing artifact")
    invoke("overlapping-listing-output", [compiler, "vm", "build", "--no-module-discovery", source,
                                         "-o", same_output, "--listing", same_output], 1,
           "bytecode and listing output paths must differ")
    assert same_output.read_bytes() == b"preserve this existing artifact"
    invoke("foreign-pack-target", [compiler, "vm", "pack", "--vm-path", vm, bytecode,
                                     "--target", "linux64" if os.name == "nt" else "win64"], 1,
           "requires the host target")
    assert set(temporary.iterdir()) == before
    assert hashlib.sha256(vm.read_bytes()).hexdigest() == vm_hash
    result = {"name":"selfhost_vm_lifecycle", "ok":True, "commands":sequence,
              "runner_sha256":vm_hash, "runtime_arguments":"forwarded"}
    (out / "results.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"[OK] VM lifecycle: {sequence} commands", flush=True)
    return result


if __name__ == "__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--compiler",type=Path,required=True)
    parser.add_argument("--vm",type=Path,required=True)
    parser.add_argument("--out-dir",type=Path,required=True)
    args=parser.parse_args()
    print(json.dumps(check_vm_lifecycle(args.compiler.resolve(),args.vm.resolve(),
                                       Path(__file__).resolve().parents[2],args.out_dir.resolve())))
