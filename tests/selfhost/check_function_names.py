"""Check source-level overload identity and executable entry selection."""
from pathlib import Path
import subprocess
import sys


def check_function_names(compiler: Path, stage0: Path, out: Path, *, stage0_reference: bool = True) -> dict:
    suffix = ".exe" if sys.platform == "win32" else ""
    compilers = [("selfhost", compiler)]
    if stage0_reference:
        compilers.append(("stage0", stage0))
    checks = 0

    def run(args, code=0, *, error=None, file=None):
        nonlocal checks
        result = subprocess.run(args, cwd=out, capture_output=True, text=True, encoding="utf-8",
                                errors="replace", timeout=60)
        output = result.stderr + result.stdout
        assert result.returncode == code, (args, result.returncode, output)
        if error:
            errors = [line for line in output.splitlines() if "[error]" in line]
            assert len(errors) == 1 and error in errors[0], (args, output)
            assert f"{file}>" in output, (args, output)
        checks += 1

    def named(symbol, param):
        return f'[Overload]\n[SymbolName("{symbol}")]\nFunction int Read({param} v) {{ Return 2; }}\n'

    def project(directory, entry=""):
        manifest = directory / "kinal.knproj"
        override = f'EntrySymbol = "{entry}";' if entry else ""
        profiles = "".join(f'Profile "{profile}" {{ Source {{ Entry = "First.kn"; Sets = ["s"]; '
                           f'Mode = AllSources; }} Build {{ Backend = {backend}; {override} }} }}\n'
                           for profile, backend in [("native", "Native"), ("vm", "VM")])
        manifest.write_text('Project Names { SourceSet "s" { Files = ["First.kn", "Second.kn"]; '
                            'RequireUnit = true; }\n' + profiles + '}\n', encoding="utf-8")
        return manifest

    first_main = "Static Function int Main() { Return 17; }\n"
    plain = "Function int Read(int v) { Return 1; }\n"
    for label, tool in compilers:
        work = out / label
        work.mkdir(parents=True, exist_ok=True)
        first, second = work / "First.kn", work / "Second.kn"
        manifest = project(work)
        for name, a, b, error in [
            ("duplicate-main", first_main, first_main, "Function already defined"),
            ("no-overload", plain, plain.replace("int v", "string v"), "Function already defined"),
            ("mixed", named("ReadInt", "int"), plain, "Function already defined"),
            ("return-only", named("ReadOne", "int"), named("ReadTwo", "int").replace(
                "int Read", "bool Read").replace("Return 2", "Return true"), "same parameter types"),
            ("qualified-type", "Class Box {}\n" + named("ReadOne", "Box"), named("ReadTwo", "Names.Box"),
             "same parameter types"),
            ("same-symbol", named("ReadImpl", "int"), named("ReadImpl", "string"), "unique SymbolName"),
            ("other-name", named("ReadImpl", "int"), "Function int ReadImpl() { Return 1; }", "unique SymbolName"),
        ]:
            first.write_text("Unit Names;\n" + a + ("" if name == "duplicate-main" else first_main), encoding="utf-8")
            second.write_text("Unit Names;\n\n" + b, encoding="utf-8")
            for mode, profile, ext in [(["build", "--emit", "check"], "native", ".kcheck"),
                                       (["build"], "native", suffix), (["vm", "build"], "vm", ".knc")]:
                artifact = work / (name + ext)
                artifact.unlink(missing_ok=True)
                run([tool, *mode, "--project", manifest, "--profile", profile, "-o", artifact],
                    1, error=error, file=second.name)
                assert not artifact.exists(), artifact

        first.write_text("Unit Names;\nStatic Function int OtherMain() { Return 91; }\n"
                         + named("ReadInt", "int") + "Static Function int Main() { Return Read(1); }",
                         encoding="utf-8")
        second.write_text('Unit Names;\n' + named("ReadText", "string").replace("Return 2", "Return 9"),
                          encoding="utf-8")
        for mode, profile in [(["run"], "native"), (["vm", "run"], "vm")]:
            run([tool, *mode, "--project", manifest, "--profile", profile], 2)

        # Both files can contribute a Main and a same-named helper to distinct Units.
        # Only executable emission needs a unique entry.
        helper = "Function int Value() { Return 23; }\n"
        first.write_text("Unit Names.First;\n" + first_main + helper, encoding="utf-8")
        second.write_text("Unit Names.Second;\n" + helper +
                          "Static Function int Main() { Return Value(); }\n"
                          "Static Function int Other() { Return 29; }\n", encoding="utf-8")
        run([tool, "build", "--project", manifest, "--profile", "native", "--emit", "check",
             "-o", work / "distinct.kcheck"])
        for mode, profile in [(["build"], "native"), (["vm", "build"], "vm")]:
            run([tool, *mode, "--project", manifest, "--profile", profile, "-o", work / "ambiguous"],
                1, error="Entry Ambiguous", file=second.name)
        for entry, expected in [("Names.Second.Main", 23), ("Other", 29)]:
            project(work, entry)
            native, bytecode, packed = work / ("app" + suffix), work / "app.knc", work / ("packed" + suffix)
            run([tool, "build", "--project", manifest, "--profile", "native", "-o", native])
            run([native], expected)
            run([tool, "run", "--project", manifest, "--profile", "native"], expected)
            run([tool, "vm", "build", "--project", manifest, "--profile", "vm", "-o", bytecode])
            runner = tool.with_name("kinalvm" + suffix)
            run([runner, bytecode], expected)
            run([tool, "vm", "run", "--project", manifest, "--profile", "vm"], expected)
            run([tool, "vm", "pack", "--vm-path", runner, "--project", manifest, "--profile", "vm", "-o", packed])
            run([packed], expected)

        # Checking Main registers outer specializations. Their bodies create a
        # second wave after the function table has grown several times.
        project(work)
        types = [f"Tag{i}" for i in range(37)]
        first.write_text("Unit Names;\n" + named("ReadInt", "int") +
                         "Function int Inner<T>(T value) { Return Read(1); }\n"
                         "Function int Outer<T>(T value) { Return Inner<T>(value); }\n" +
                         "".join(f"Class {name} {{}}\n" for name in types) +
                         'Static Function int Main() { int total = Read("tag");\n' +
                         "".join(f"total += Outer<{name}>(New {name}());\n" for name in types) +
                         "Return total; }\n", encoding="utf-8")
        second.write_text("Unit Names;\n" + named("ReadText", "string").replace("Return 2", "Return 9"),
                          encoding="utf-8")
        for mode, profile in [(["run"], "native"), (["vm", "run"], "vm")]:
            run([tool, *mode, "--project", manifest, "--profile", profile], 9 + 2 * len(types))
    return {"name": "function_names_and_entry", "ok": True, "checks": checks}
