"""Ordinary source/project CLI contracts for the pure-Kinal compiler.

The command shape, runtime behavior and target metadata are shared with stage0.
Selfhost deliberately keeps its own typed-HIR summary schema and counters.
"""
from __future__ import annotations

import argparse
import json
import os
import platform
import subprocess
from pathlib import Path


def check_cli_workflows(compiler: Path, stage0: Path, root: Path, out: Path,
                        *, stage0_reference: bool = True) -> dict[str, object]:
    out.mkdir(parents=True, exist_ok=True)
    logs = out / "logs"
    logs.mkdir(exist_ok=True)
    temporary = out / "temporary outputs"
    temporary.mkdir(exist_ok=True)
    environment = dict(os.environ, TMP=str(temporary), TEMP=str(temporary), TMPDIR=str(temporary))
    sequence = 0
    cases = 0
    suffix = ".exe" if os.name == "nt" else ""
    object_suffix = ".obj" if os.name == "nt" else ".o"

    def invoke(label: str, command: list[str | Path], *, code: int = 0,
               text: str | None = None, cwd: Path = out) -> subprocess.CompletedProcess[str]:
        nonlocal sequence
        sequence += 1
        arguments = [str(part) for part in command]
        proc = subprocess.run(arguments, cwd=cwd, env=environment, text=True,
                              capture_output=True, timeout=180)
        (logs / f"{sequence:03d}-{label}.log").write_text(
            "command=" + repr(arguments) + "\nexit=" + str(proc.returncode) + "\n" +
            proc.stdout + proc.stderr, encoding="utf-8")
        assert proc.returncode == code, (label, proc.returncode, proc.stdout, proc.stderr)
        if text is not None:
            assert text in proc.stdout + proc.stderr, (label, text, proc.stdout, proc.stderr)
        return proc

    def summary(path: Path) -> dict[str, str]:
        return dict(line.split("=", 1) for line in path.read_text(encoding="utf-8").splitlines())

    # Existing application fixtures exercise direct sources and ordinary stdlib
    # imports without bringing unrelated sibling test sources into the session.
    for name, expected in (("hello", "hello\n"), ("functions", "ok\n"), ("control", None)):
        source = root / "tests" / "common" / f"{name}.kn"
        tools = [("stage0", stage0)] if stage0_reference else []
        tools.append(("selfhost", compiler))
        executions = []
        for label, tool in tools:
            executable = out / f"{label}-{name}{suffix}"
            invoke(label + "-source-build", [tool, "build", source, "--no-module-discovery", "-o", executable])
            run = invoke(label + "-source-execute", [executable])
            if expected is not None:
                assert run.stdout.replace("\r\n", "\n") == expected, (name, label, run.stdout)
            executions.append(run.stdout.replace("\r\n", "\n"))
            if label == "selfhost":
                assert not Path(str(executable) + object_suffix).exists()
        assert len(set(executions)) == 1, (name, executions)
        cases += 1

    source_dir = out / "source files"
    source_dir.mkdir(exist_ok=True)
    source = source_dir / "Main.kn"
    source.write_text('''Unit Tests.CliWorkflow;
Get IO.Console;
Static Function int Main(string[] args)
{
    If (args.Length() == 0) { IO.Console.PrintLine("cli-ok"); Return 0; }
    If (args.Length() == 1 && args[0] == "simple") { IO.Console.PrintLine("simple-argument-ok"); Return 3; }
    If (args.Length() != 5) Return 9;
    If (args[0] != "two words" || args[1] != "" || args[2] != "quote'and\\\"double" ||
        args[3] != "--flag" || args[4] != "雪") Return 10;
    IO.Console.PrintLine("arguments-ok");
    Return 7;
}
''', encoding="utf-8")
    arguments = ["two words", "", "quote'and\"double", "--flag", "雪"]
    # With no selected sibling sources, default discovery remains usable too.
    for label, tool in (("stage0", stage0), ("selfhost", compiler)):
        if label == "stage0" and not stage0_reference:
            continue
        before = set(temporary.iterdir())
        # Stage0's POSIX `run` uses a shell command and does not preserve mixed
        # quotes. Its directly launched executable remains the argv reference.
        if label == "stage0":
            reference = out / ("stage0-arguments" + suffix)
            invoke("stage0-arguments-build", [tool, "build", source, "--no-module-discovery", "-o", reference])
            direct = invoke("stage0-arguments-execute", [reference, *arguments], code=7)
            assert direct.stdout.replace("\r\n", "\n") == "arguments-ok\n"
            result = invoke(label + "-run", [tool, "run", source, "--", "simple"], code=3)
            assert result.stdout.replace("\r\n", "\n") == "simple-argument-ok\n", result.stdout
        else:
            result = invoke(label + "-run", [tool, "run", source, "--", *arguments], code=7)
            assert result.stdout.replace("\r\n", "\n") == "arguments-ok\n", result.stdout
        assert set(temporary.iterdir()) == before, (label, "run did not clean temporary outputs")
        cases += 1

    kept = invoke("run-keep", [compiler, "run", "--keep-temps", source])
    retained = set(temporary.iterdir()) - before
    assert len(retained) == 1 and kept.stdout == "cli-ok\n", (retained, kept.stdout)
    kept_path = next(iter(retained)) / ("program" + suffix)
    assert kept_path.is_file() and Path(str(kept_path) + object_suffix).is_file()
    invoke("run-kept-executable", [kept_path], text="cli-ok")
    # Tests own this retained output; remove exactly the files they created.
    Path(str(kept_path) + object_suffix).unlink()
    kept_path.unlink()
    kept_path.parent.rmdir()
    cases += 1

    for extension in ("kinal", "fx"):
        renamed = out / f"Alternate.{extension}"
        renamed.write_text(source.read_text(encoding="utf-8"), encoding="utf-8")
        invoke("source-extension-" + extension, [compiler, "run", "--no-module-discovery", renamed], text="cli-ok")
        cases += 1

    # Multiple direct input files are one compilation, and duplicate paths do
    # not register declarations twice. This tests explicit input, not discovery.
    main = out / "Multi.kn"
    helper = out / "Helper.kn"
    main.write_text("Unit Tests.CliMulti; Static Function int Main() { Return Helper() - 42; }\n", encoding="utf-8")
    helper.write_text("Unit Tests.CliMulti; Function int Helper() { Return 42; }\n", encoding="utf-8")
    multi = out / ("multi" + suffix)
    invoke("multi-source", [compiler, "build", "--no-module-discovery", main, helper, main, "-o", multi])
    invoke("multi-source-execute", [multi])
    cases += 1

    # Each emit mode gets an appropriate default file name in the invoking
    # directory, not next to an input that may be read-only.
    for mode, extension in (("bin", suffix), ("obj", object_suffix),
                            ("asm", ".s"), ("ir", ".ll"), ("check", ".kcheck")):
        output = out / ("Main" + extension)
        invoke("default-" + mode, [compiler, "build", "--emit", mode, "--no-module-discovery", source])
        assert output.is_file() and output.stat().st_size > 0, (mode, output)
        if mode == "bin":
            invoke("default-executable", [output], text="cli-ok")
        elif mode == "asm":
            assert "main" in output.read_text(encoding="utf-8")
        elif mode == "ir":
            assert "target triple" in output.read_text(encoding="utf-8")
        elif mode == "check":
            first = output.read_bytes()
            invoke("check-repeat", [compiler, "build", source, "--emit", "sema-summary", "--no-module-discovery"])
            assert first == output.read_bytes(), "semantic summaries must be deterministic"
            fields = summary(output)
            assert fields["format"] == "kinal-selfhost-sema-v1"
            assert fields["pointer_bits"] == "64" and fields["environment"] == "hosted"
            assert fields["unresolved_types"] == fields["unresolved_expressions"] == "0"
        cases += 1

    # A frontend-only check accepts a source without an executable entry point.
    library = out / "Library.kn"
    library.write_text("Unit Tests.CliLibrary; Function int Add(int a, int b) { Return a + b; }\n", encoding="utf-8")
    invoke("frontend-only", [compiler, "build", library, "--no-module-discovery", "--emit", "check"])
    assert not (out / "Library.o").exists() and not (out / ("Library" + suffix)).exists()
    cases += 1
    # The default executable artifact contract matches stage0: intermediate
    # native outputs retain an entry wrapper and therefore still require Main.
    for mode, extension in (("obj", object_suffix), ("ir", ".ll"), ("asm", ".s")):
        rejected = out / ("entry-free" + extension)
        invoke("entry-free-" + mode, [compiler, "build", library, "--no-module-discovery",
                                     "--emit", mode, "-o", rejected], code=1,
               text="entry unit does not declare Main")
        assert not rejected.exists()
        cases += 1

    foreign = "win64" if platform.system() != "Windows" else "linux64"
    foreign_asm = out / "foreign.s"
    invoke("foreign-assembly", [compiler, "build", source, "--no-module-discovery", "--target", foreign,
                               "--emit", "assembly", "-o", foreign_asm])
    assert foreign_asm.stat().st_size > 0
    self_check = out / "foreign.kcheck"
    invoke("foreign-check", [compiler, "build", source, "--no-module-discovery", "--target", foreign,
                            "--emit", "check", "-o", self_check])
    if stage0_reference:
        reference = out / "stage0.kcheck"
        invoke("stage0-check", [stage0, "build", source, "--no-module-discovery", "--target", foreign,
                               "--emit", "check", "-o", reference])
        expected, actual = summary(reference), summary(self_check)
        assert expected["format"] == "kinal-sema-v1"
        for key in ("target", "pointer_bits", "environment"):
            assert actual[key] == expected[key], (key, actual[key], expected[key])
    cases += 1

    project = source_dir / "kinal.knproj"
    project.write_text('''Project CliProject {
    DefaultProfile = "native";
    SourceSet "main" { Files = ["Main.kn"]; RequireUnit = true; }
    Profile "native" {
        Source { Entry = "Main.kn"; Sets = ["main"]; Mode = FileOnly; }
        Build { Output = "configured"; }
    }
    Profile "alternate" {
        Source { Entry = "Main.kn"; Sets = ["main"]; Mode = FileOnly; }
    }
}
''', encoding="utf-8")
    for mode, extension in (("check", ".kcheck"), ("asm", ".s")):
        invoke("project-" + mode, [compiler, "build", "--project", source_dir, "--emit", mode])
        assert (source_dir / ("configured" + extension)).is_file()
        cases += 1
    for command, extension in (("build", suffix), ("build-object", object_suffix), ("build-ir", ".ll")):
        artifact = out / ("legacy-" + command + extension)
        invoke("legacy-" + command, [compiler, command, project, artifact, "alternate"])
        assert artifact.stat().st_size > 0
        cases += 1
    before = set(temporary.iterdir())
    run = invoke("project-run", [compiler, "run", "--project", project, "--profile", "alternate", "--", *arguments], code=7)
    assert run.stdout.replace("\r\n", "\n") == "arguments-ok\n"
    assert set(temporary.iterdir()) == before
    cases += 1

    # Preserve the original CLI's status and error wording: unknown/missing
    # options return 2; invalid values, files and diagnostics return 1.
    bad = out / "Invalid.kn"
    bad.write_text('Unit Tests.Invalid; Static Function int Main() { Return Missing(); }\n', encoding="utf-8")
    before = set(temporary.iterdir())
    invoke("run-invalid", [compiler, "run", "--no-module-discovery", bad], code=1)
    assert set(temporary.iterdir()) == before
    invoke("run-backend-failure", [compiler, "run", "--no-module-discovery", library], code=1,
           text="entry unit does not declare Main")
    assert set(temporary.iterdir()) == before
    invalid_output = out / "invalid.kcheck"
    invoke("check-invalid", [compiler, "build", "--no-module-discovery", bad, "--emit", "check", "-o", invalid_output], code=1)
    assert not invalid_output.exists()
    unresolved = out / "Unlinked.kn"
    unresolved.write_text('Unit Tests.Unlinked; [LinkName("kinal_cli_missing_dependency")] '
                          'Extern Function int MissingDependency() By C; '
                          'Trusted Static Function int Main() { Return MissingDependency(); }\n',
                          encoding="utf-8")
    invoke("run-link-failure", [compiler, "run", "--no-module-discovery", unresolved], code=1,
           text="native link failed")
    assert set(temporary.iterdir()) == before
    invoke("missing-source", [compiler, "build", out / "Missing.kn", "--emit", "check"], code=1,
           text="failed to read input")
    invoke("missing-project", [compiler, "build", "--project", out / "missing.knproj"], code=1,
           text="invalid manifest path")
    invoke("missing-option-value", [compiler, "build", source, "--emit"], code=2,
           text="unknown option: --emit")
    invoke("unsupported-emit", [compiler, "build", source, "--emit", "wrong"], code=1,
           text="unknown emit mode: wrong")
    invoke("extra-run-input", [compiler, "run", source, helper], code=2,
           text="exactly one input")
    invoke("unsupported-run-option", [compiler, "run", source, "--emit", "obj"], code=2,
           text="unknown option: --emit")
    invoke("profile-without-project", [compiler, "build", source, "--profile", "native", "--emit", "check"])
    invoke("no-input", [compiler, "build"], code=1, text="Usage: kinal build")
    invoke("unknown-profile", [compiler, "build", "--project", project, "--profile", "missing"], code=1,
           text="requested profile was not found")
    invoke("unwritable-summary", [compiler, "build", source, "--no-module-discovery", "--emit", "check",
                                  "-o", source_dir], code=1,
           text="failed to write semantic summary")
    blocked_parent = out / "not-a-directory"
    blocked_parent.write_text("regular file\n", encoding="utf-8")
    invoke("invalid-output-parent", [compiler, "build", source, "--no-module-discovery", "--emit", "check",
                                    "-o", blocked_parent / "summary.kcheck"], code=1,
           text="failed to create output directory")
    for mode, extension in (("bin", suffix), ("check", ".kcheck"), ("asm", ".s")):
        nested = out / ("new-output-" + mode) / "nested" / ("application" + extension)
        invoke("create-output-parent-" + mode, [compiler, "build", source, "--no-module-discovery",
                                               "--emit", mode, "-o", nested])
        assert nested.is_file()
        if mode == "bin":
            invoke("nested-output-execute", [nested], text="cli-ok")
        cases += 1
    invoke("help", [compiler, "run", "--help"], text="Usage: kinal run [options] <file.kn>")
    cases += 16
    result = {"name": "source_project_cli_workflows", "ok": True, "cases": cases,
              "commands": sequence, "stage0_reference": stage0_reference,
              "check_schema": "kinal-selfhost-sema-v1"}
    (out / "results.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"[OK] source/project CLI: {cases} cases, {sequence} commands", flush=True)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(check_cli_workflows(args.compiler.resolve(), args.stage0.resolve(),
                                        Path(__file__).resolve().parents[2], args.out_dir.resolve())))
