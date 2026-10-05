"""Switch cases preserve the innermost loop's Break and Continue targets."""
from __future__ import annotations

import argparse
import json
import os
import subprocess
from pathlib import Path


CASES = {
    "for-case-break": '''
Static Function int Main() {
    int sum = 0;
    For (int i = 0; i < 4; i++) {
        Switch (i) { Case (1) { Break; } Case (default) { sum += i; } }
        sum += 10;
    }
    If (sum != 10) Return 1;
    Return 0;
}
''',
    "while-default-break": '''
Static Function int Main() {
    int i = 0; int sum = 0;
    While (i < 4) {
        i++;
        Switch (i) { Case (1) { sum += 1; } Case (default) { Break; } }
        sum += 10;
    }
    If (i != 2 || sum != 11) Return 1;
    Return 0;
}
''',
    "nested-switch-break": '''
Static Function int Main() {
    int sum = 0;
    For (int i = 0; i < 4; i++) {
        Switch (i) {
            Case (0) { sum += 1; }
            Case (default) {
                Switch (i) { Case (1) { Break; } Case (default) { sum += 100; } }
                sum += 1000;
            }
        }
        sum += 10;
    }
    If (sum != 11) Return 1;
    Return 0;
}
''',
    "innermost-loop-break": '''
Static Function int Main() {
    int outer = 0; int sum = 0;
    While (outer < 3) {
        outer++;
        For (int inner = 0; inner < 4; inner++) {
            Switch (inner) { Case (1) { Break; } Case (default) { sum += 1; } }
            sum += 10;
        }
        sum += 100;
    }
    If (outer != 3 || sum != 333) Return 1;
    Return 0;
}
''',
    "loop-inside-switch": '''
Static Function int Main() {
    int sum = 0;
    For (int outer = 0; outer < 3; outer++) {
        Switch (outer) {
            Case (0) {
                int inner = 0;
                While (inner < 3) {
                    inner++;
                    Switch (inner) { Case (1) { Break; } Case (default) { sum += 100; } }
                    sum += 1000;
                }
                If (inner != 1) Return 1;
                sum += 1;
                Break;
            }
            Case (default) { sum += 10000; }
        }
        sum += 10;
    }
    If (sum != 1) Return 2;
    Return 0;
}
''',
    "for-nested-switch-continue": '''
Static Function int Main() {
    int sum = 0;
    For (int i = 0; i < 4; i++) {
        Switch (i) {
            Case (1) { Switch (i) { Case (default) { Continue; } } }
            Case (default) { sum += i; }
        }
        sum += 10;
    }
    If (sum != 35) Return 1;
    Return 0;
}
''',
    "while-default-continue": '''
Static Function int Main() {
    int i = 0; int sum = 0;
    While (i < 4) {
        i++;
        Switch (i) { Case (2) { sum += 2; } Case (default) { Continue; } }
        sum += 10;
    }
    If (i != 4 || sum != 12) Return 1;
    Return 0;
}
''',
    "standalone-no-fallthrough": '''
Static Function int Main() {
    int sum = 0;
    Switch (1) { Case (1) { sum += 1; } Case (2) { sum += 10; } Case (default) { sum += 100; } }
    Switch (3) { Case (1) { sum += 10000; } Case (default) { sum += 2; } }
    If (sum != 3) Return 1;
    Return 0;
}
''',
}


def write_project(directory: Path, body: str) -> Path:
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "Main.kn").write_text("Unit Tests.Selfhost.SwitchLoopControl;\n" + body,
                                        encoding="utf-8")
    project = directory / "kinal.knproj"
    project.write_text('Project SwitchLoopControl { DefaultProfile = "native"; '
                       'SourceSet "main" { Files = ["Main.kn"]; RequireUnit = true; } '
                       'Profile "native" { Source { Entry = "Main.kn"; '
                       'Sets = ["main"]; Mode = FileOnly; } } }\n', encoding="utf-8")
    return project


def check_switch_loop_control(compiler: Path, stage0: Path, root: Path, out: Path,
                              *, stage0_reference: bool = True) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    compilers = [("stage0", stage0)] if stage0_reference else []
    compilers.append(("selfhost", compiler))
    results = []
    for name, body in CASES.items():
        directory = out / name
        project = write_project(directory, body)
        for label, tool in compilers:
            output = directory / (label + (".exe" if os.name == "nt" else ""))
            build = subprocess.run([str(tool), "build", "--project", str(project), "-o", str(output)],
                                   cwd=root, text=True, capture_output=True, timeout=180)
            (directory / (label + ".build.log")).write_text(build.stdout + build.stderr, encoding="utf-8")
            if build.returncode != 0:
                results.append({"name": name, "compiler": label, "ok": False, "phase": "build",
                                "returncode": build.returncode})
                print(f"[FAIL] {label} switch {name}: build exit {build.returncode}", flush=True)
                continue
            run = subprocess.run([str(output)], cwd=root, text=True, capture_output=True, timeout=30)
            (directory / (label + ".run.log")).write_text(run.stdout + run.stderr, encoding="utf-8")
            ok = run.returncode == 0 and not run.stdout and not run.stderr
            results.append({"name": name, "compiler": label, "ok": ok, "phase": "run",
                            "returncode": run.returncode})
            print(f"[{'OK' if ok else 'FAIL'}] {label} switch {name}: exit {run.returncode}", flush=True)

    # A standalone Switch must not make loop-only control statements legal.
    for control in ("Break", "Continue"):
        for case in ("1", "default"):
            name = f"outside-loop-{control.lower()}-{case}"
            directory = out / name
            project = write_project(directory, "Static Function int Main() { "
                                    f"Switch (1) {{ Case ({case}) {{ {control}; }} }} Return 0; }}\n")
            for label, tool in compilers:
                rejected = subprocess.run([str(tool), "build", "--project", str(project),
                                           "--emit", "ir", "-o", str(directory / (label + ".ll"))],
                                          cwd=root, text=True, capture_output=True, timeout=180)
                diagnostic = rejected.stdout + rejected.stderr
                (directory / (label + ".build.log")).write_text(diagnostic, encoding="utf-8")
                expected = (f"{control.lower()} used outside loop" if label == "selfhost"
                            else "break/continue not in loop")
                ok = rejected.returncode == 1 and expected in diagnostic
                results.append({"name": name, "compiler": label, "ok": ok, "phase": "diagnostic",
                                "returncode": rejected.returncode})
                print(f"[{'OK' if ok else 'FAIL'}] {label} switch {name}: diagnostic", flush=True)

    report = {"name": "switch_loop_control", "ok": all(result["ok"] for result in results),
              "runtime_cases": len(CASES), "diagnostic_cases": 4, "compilers": len(compilers),
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
    print(json.dumps(check_switch_loop_control(args.compiler.resolve(), args.stage0.resolve(),
                                              Path(__file__).resolve().parents[2], args.out_dir.resolve(),
                                              stage0_reference=args.stage0_role == "reference")))
