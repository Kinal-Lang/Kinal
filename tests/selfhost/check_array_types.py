from __future__ import annotations

import argparse
import json
import os
import subprocess
from pathlib import Path


NEGATIVE = {
    "dynamic-local": "Static Function int Main() { int[] a = {1,2}; int[2] b = a; Return 0; }",
    "dynamic-assign": "Static Function int Main() { int[] a = {1,2}; int[2] b; b = a; Return 0; }",
    "too-large-local": "Static Function int Main() { int[3] a; int[2] b = a; Return 0; }",
    "add-too-large": "Static Function int Main() { int[2] a = {1,2}; int[2] b = a.Add(3); Return 0; }",
    "dynamic-parameter": "Static Function void Use(int[2] a) {} Static Function int Main() { int[] a = {1,2}; Use(a); Return 0; }",
    "large-parameter": "Static Function void Use(int[2] a) {} Static Function int Main() { int[3] a; Use(a); Return 0; }",
    "dynamic-return": "Static Function int[2] Make() { int[] a = {1,2}; Return a; } Static Function int Main() { Return 0; }",
    "large-return": "Static Function int[2] Make() { int[3] a; Return a; } Static Function int Main() { Return 0; }",
    "dynamic-field": "Class Holder { Public int[2] Values; } Static Function int Main() { Holder h = New Holder(); int[] a = {1,2}; h.Values = a; Return 0; }",
    "dynamic-global": "int[] a = {1,2}; int[2] b = a; Static Function int Main() { Return 0; }",
    "dynamic-static": "Static Class Holder { Public int[2] Values; } Static Function int Main() { int[] a = {1,2}; Holder.Values = a; Return 0; }",
    "nested-dynamic": "Static Function int Main() { int[][2] a; int[2][2] b = a; Return 0; }",
    "nested-large": "Static Function int Main() { int[3][2] a; int[2][2] b = a; Return 0; }",
    "generic-bound": "Static Function T Identity<T>(T value) { Return value; } Static Function int Main() { int[3] a; Var b = Identity<int[2]>(a); Return 0; }",
    "package-bound": "Static Function int Main() { <int[3]> a = <{1,2,3}>; <int[2]> b = a; Return 0; }",
    "inner-nonconstant": "Static Function int Main() { int n = 2; int[n][] a; Return 0; }",
}


def invoke(command: list[str], root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=root, text=True, capture_output=True)


def check_array_types(compiler: Path, stage0: Path, root: Path, out: Path,
                      *, stage0_reference: bool = True) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    source = out / "Main.kn"
    source.write_text((root / "tests/common/array_type_contract.kn").read_text(encoding="utf-8"), encoding="utf-8")
    project = out / "kinal.knproj"
    project.write_text('''Project ArrayTypes {
    DefaultProfile = "native";
    SourceSet "source" { Files = ["Main.kn"]; RequireUnit = true; }
    Profile "native" { Source { Entry = "Main.kn"; Sets = ["source"]; Mode = FileOnly; } }
}
''', encoding="utf-8")
    compilers = [("stage0", stage0)] if stage0_reference else []
    compilers.append(("selfhost", compiler))
    for label, tool in compilers:
        output = out / (label + (".exe" if os.name == "nt" else ""))
        proc = invoke([str(tool), "build", "--project", str(project), "-o", str(output)], root)
        assert proc.returncode == 0, (label, proc.stdout, proc.stderr)
        proc = invoke([str(output)], root)
        assert proc.returncode == 0, (label, proc.returncode, proc.stdout, proc.stderr)
    symbols = invoke([str(compiler), "symbols", str(project), "native"], root)
    assert symbols.returncode == 0, (symbols.stdout, symbols.stderr)
    for identity in ("int[2]", "int[3]", "<int[2]>"):
        assert f"function=Tests.ArrayTypes.Identity${identity}|" in symbols.stdout, symbols.stdout
    for name, body in NEGATIVE.items():
        source = out / (name + ".kn")
        source.write_text("Unit Tests.ArrayTypes;\n" + body, encoding="utf-8")
        if stage0_reference:
            reference = invoke([str(stage0), "build", "--no-module-discovery", str(source),
                                "--emit", "ir", "-o", str(out / (name + ".ll"))], root)
            assert reference.returncode == 1 and "[Sema]" in reference.stdout + reference.stderr, (
                name, "stage0 must reject semantically", reference.returncode, reference.stdout, reference.stderr)
            assert "Entry Signature" not in reference.stdout + reference.stderr, (name, reference.stderr)
        actual = invoke([str(compiler), "check-source", str(source)], root)
        assert actual.returncode == 1 and "[Sema]" in actual.stdout + actual.stderr, (
            name, "selfhost must reject semantically", actual.returncode, actual.stdout, actual.stderr)
        print(f"[OK] array type rejection: {name}", flush=True)
    return {"name": "array_type_contract", "ok": True, "runtime_compilers": len(compilers),
            "negative_cases": len(NEGATIVE), "stage0_reference": stage0_reference,
            "distinct_generic_instances": 3}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--stage0-role", choices=("reference", "bootstrap"), default="reference")
    args = parser.parse_args()
    print(json.dumps(check_array_types(args.compiler.resolve(), args.stage0.resolve(),
                                      Path(__file__).resolve().parents[2], args.out_dir.resolve(),
                                      stage0_reference=args.stage0_role == "reference")))
