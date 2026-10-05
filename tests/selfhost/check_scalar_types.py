"""Native scalar coercion/remainder contracts, compared with the C compiler."""
from __future__ import annotations

import argparse
import json
import os
import subprocess
from pathlib import Path


CASES = {
    "float-remainder": '''
Static Function float Remainder(float left, float right) { Return left % right; }
Static Function f32 Remainder32(f32 left, f32 right) { Return left % right; }
Static Function int Main() {
    If (Remainder(7.5, 2.0) != 1.5) Return 1;
    If (Remainder(-7.5, 2.0) != -1.5) Return 2;
    If (Remainder(7.5, -2.0) != 1.5) Return 3;
    If (Remainder32([f32](7.5), [f32](2.0)) != [f32](1.5)) Return 4;
    Return 0;
}
''',
    "float-compound-remainder": '''
Class Box {
    Public float Field;
    Private float Stored;
    Public Property float Value { Get { Return Stored; } Set { Stored = value; } }
}
Static Function int Main() {
    float value = 7.5; value %= 2.0;
    If (value != 1.5) Return 1;
    f32 narrow = [f32](-7.5); narrow %= [f32](2.0);
    If (narrow != [f32](-1.5)) Return 2;
    float[] values = {7.5}; values[0] %= 2.0;
    If (values[0] != 1.5) Return 3;
    Box box = New Box(); box.Field = 7.5; box.Field %= 2.0;
    If (box.Field != 1.5) Return 4;
    box.Value = 7.5; box.Value %= 2.0;
    If (box.Value != 1.5) Return 5;
    Return 0;
}
''',
    "mixed-float-compound-remainder": '''
Class Box
{
    Public f32 Field;
    Public f32 Guard;
    Private f32 Stored;
    Public Property f32 Value { Get { Return Stored; } Set { Stored = value; } }
}
Static Function int Main()
{
    float divisor = 16777215.5;
    f32 value = 16777216.0;
    f32 result = value %= divisor;
    If ([float](value) != 0.5 || [float](result) != 0.5) Return 1;
    f32[] values = {16777216.0, 9.0};
    result = values[0] %= divisor;
    If ([float](values[0]) != 0.5 || [float](values[1]) != 9.0 || [float](result) != 0.5) Return 2;
    Box box = New Box();
    box.Field = 16777216.0; box.Guard = 9.0;
    result = box.Field %= divisor;
    If ([float](box.Field) != 0.5 || [float](box.Guard) != 9.0 || [float](result) != 0.5) Return 3;
    box.Value = 16777216.0;
    result = box.Value %= divisor;
    If ([float](box.Value) != 0.5 || [float](result) != 0.5) Return 4;
    Return 0;
}
''',
    "scalar-bool-casts": '''
Static Function bool Truth(float value) { Return [bool](value); }
Static Function bool Truth32(f32 value) { Return [bool](value); }
Static Function float Number(bool value) { Return [float](value); }
Static Function f32 Number32(bool value) { Return [f32](value); }
Static Function float Divide(float left, float right) { Return left / right; }
Static Function int Main() {
    If (Truth(0.0) || Truth(-0.0) || !Truth(0.5) || !Truth(-2.0)) Return 1;
    If (Truth32([f32](0.0)) || !Truth32([f32](0.5))) Return 2;
    If (Number(false) != 0.0 || Number(true) != 1.0) Return 3;
    If (Number32(false) != [f32](0.0) || Number32(true) != [f32](1.0)) Return 4;
    // Match stage0's ordered truth conversion: NaN and signed zero are false.
    If (Truth(Divide(0.0, 0.0))) Return 5;
    Return 0;
}
''',
    "mixed-float-arithmetic": '''
Static Function int Main() {
    int whole = 2; float fraction = 0.5; f32 narrow = [f32](0.25);
    Var sum = whole + fraction;
    If (sum != 2.5 || fraction + whole != 2.5) Return 1;
    If (whole - fraction != 1.5 || fraction - whole != -1.5) Return 2;
    If (whole * fraction != 1.0 || whole / fraction != 4.0) Return 3;
    If (whole % [float](1.5) != 0.5) Return 4;
    Var widened = narrow + fraction;
    If (widened != 0.75 || fraction + narrow != 0.75) Return 5;
    If (!(whole > fraction) || whole == fraction || !(fraction < whole)) Return 6;
    If (sum.TypeOf() != "float" || widened.TypeOf() != "float") Return 7;
    Return 0;
}
''',
    "mixed-integer-comparison": '''
Static Function int Main() {
    i8 small = [i8](1); i64 large = [i64](257);
    If (small == large || !(small < large) || !(large > small)) Return 1;
    i64 negative = [i64](-1); u8 positive = [u8](1);
    If (!(negative > positive) || negative < positive) Return 2;
    u8 bits = [u8](1); u32 high = [u32](256);
    Var merged = bits | high;
    If (merged != [u32](257) || merged.TypeOf() != "u32") Return 3;
    If ((bits << [u32](8)) != [u32](256)) Return 4;
    If ((high >> [u8](8)) != [u32](1)) Return 5;
    // Arithmetic intentionally preserves the established left-integer rule.
    Var truncated = small + large;
    If (truncated.TypeOf() != "i8" || truncated != [i8](2)) Return 6;
    If (([i8](-1) >> [u32](1)) != [u32](2147483647)) Return 7;
    Return 0;
}
''',
}


def check_scalar_types(compiler: Path, stage0: Path, root: Path, out: Path,
                       *, stage0_reference: bool = True) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    compilers = [("stage0", stage0)] if stage0_reference else []
    compilers.append(("selfhost", compiler))
    results = []
    for name, body in CASES.items():
        directory = out / name
        directory.mkdir(parents=True, exist_ok=True)
        source = directory / "Main.kn"
        source.write_text("Unit Tests.Selfhost.ScalarTypes;\n" + body, encoding="utf-8")
        project = directory / "kinal.knproj"
        project.write_text('Project ScalarTypes { DefaultProfile = "native"; '
                           'SourceSet "main" { Files = ["Main.kn"]; RequireUnit = true; } '
                           'Profile "native" { Source { Entry = "Main.kn"; '
                           'Sets = ["main"]; Mode = FileOnly; } } }\n', encoding="utf-8")
        for label, tool in compilers:
            output = directory / (label + (".exe" if os.name == "nt" else ""))
            build = subprocess.run([str(tool), "build", "--project", str(project), "-o", str(output)],
                                   cwd=root, text=True, capture_output=True, timeout=180)
            (directory / (label + ".build.log")).write_text(build.stdout + build.stderr, encoding="utf-8")
            if build.returncode != 0:
                results.append({"name": name, "compiler": label, "ok": False, "phase": "build",
                                "returncode": build.returncode})
                print(f"[FAIL] {label} scalar {name}: build exit {build.returncode}", flush=True)
                continue
            run = subprocess.run([str(output)], cwd=root, text=True, capture_output=True, timeout=30)
            (directory / (label + ".run.log")).write_text(run.stdout + run.stderr, encoding="utf-8")
            ok = run.returncode == 0 and not run.stdout and not run.stderr
            results.append({"name": name, "compiler": label, "ok": ok, "phase": "run",
                            "returncode": run.returncode})
            print(f"[{'OK' if ok else 'FAIL'}] {label} scalar {name}: exit {run.returncode}", flush=True)
    # Extended precision outside the registered subset must never silently
    # masquerade as the default 64-bit float type.
    unsupported = out / "unsupported-f80.kn"
    unsupported.write_text("Unit Tests.Selfhost.UnsupportedFloat;\n"
                           "Static Function int Main() { f80 value; Return 0; }\n", encoding="utf-8")
    rejected = subprocess.run([str(compiler), "check-source", str(unsupported)], cwd=root,
                              text=True, capture_output=True, timeout=30)
    (out / "unsupported-f80.log").write_text(rejected.stdout + rejected.stderr, encoding="utf-8")
    assert rejected.returncode == 1 and "[Sema] Unknown Type: f80" in rejected.stdout + rejected.stderr, rejected
    report = {"name": "scalar_types", "ok": all(result["ok"] for result in results),
              "cases": len(CASES), "runtime_compilers": len(compilers), "unsupported_type_checks": 1,
              "results": results}
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    assert report["ok"], report
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--stage0-role", choices=("reference", "bootstrap"), default="reference")
    args = parser.parse_args()
    print(json.dumps(check_scalar_types(args.compiler.resolve(), args.stage0.resolve(),
                                       Path(__file__).resolve().parents[2], args.out_dir.resolve(),
                                       stage0_reference=args.stage0_role == "reference")))
