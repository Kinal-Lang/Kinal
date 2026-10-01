"""Native callable identity and escaping-storage differential contracts."""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
from pathlib import Path

from check_targets import TARGETS, function_body


def check_capture_ir(body: str, *, loop: str | None = None, aligned: bool = False) -> None:
    """Heap cells are values owned by stack slots, never frame scan regions."""
    stack_slots = set(re.findall(r"(%[\w.]+) = alloca ", body))
    captures = set(re.findall(r"(%captured_root\d+) = alloca ptr", body))
    assert captures, "fixture no longer exercises captured cell storage"
    registrations: dict[str, int] = {}
    block = ""
    loop_allocation = False
    for line in body.splitlines():
        label = re.match(r"([\w.]+):", line)
        if label:
            block = label[1]
        region = re.search(r"@__kn_sh_IO_Kinal_Runtime_GarbageCollector_AddRoot_3"
                           r"\(ptr [^,]+, ptr (%[\w.]+),", line)
        if region:
            assert region[1] in stack_slots, f"heap scan region: {line}"
            if region[1] in captures:
                assert block == "entry", f"capture root registered in {block}"
                registrations[region[1]] = registrations.get(region[1], 0) + 1
        if loop and block.startswith(loop) and re.search(r"%captured_storage\d+ = call ptr", line):
            loop_allocation = True
    assert registrations == dict.fromkeys(captures, 1), "capture roots must register exactly once"
    first_allocation = body.index(" = call ptr @__kn_sh_IO_Kinal_Runtime_GarbageCollector_Allocate_1")
    entry_setup = body[:first_allocation]
    for slot in captures:
        assert f"store ptr null, ptr {slot}," in entry_setup, f"uninitialized capture root: {slot}"
    if loop:
        assert loop_allocation, f"capture allocation must execute inside {loop}"
    if aligned:
        assert re.search(r"%captured_aligned\d+ = and i64 %captured_padded\d+, -32", body), \
            "Align(32) capture lost its alignment"


def check_capture_targets(compiler: Path, projects: dict[str, Path], root: Path,
                          out: Path, env: dict[str, str]) -> int:
    for target, _, _, _ in TARGETS:
        for name, functions in (
            ("escaping_capture_storage", (("RepeatedManagedCapture", "while_body", False),
                                           ("AggregateCapture", None, True))),
            ("foreach", (("MakeCaptures", "foreach_body", False),)),
        ):
            output = out / f"capture-{name}-{target}.ll"
            proc = subprocess.run(
                [str(compiler), "build", "--project", str(projects[name]), "--profile", "native",
                 "--target", target, "--emit", "ir", "-o", str(output)],
                cwd=root, env=env, text=True, capture_output=True,
            )
            assert proc.returncode == 0, (target, name, proc.stdout, proc.stderr)
            ir = output.read_text(encoding="utf-8")
            for function, loop, aligned in functions:
                check_capture_ir(function_body(ir, function), loop=loop, aligned=aligned)
        print(f"[OK] callable capture roots/alignment {target}", flush=True)
    return len(TARGETS)


def check_callables(compiler: Path, stage0: Path, root: Path, out: Path,
                    *, stage0_reference: bool = True) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    cases = [
        ("identity", root / "tests/pkg/callable_identity/kinal.knproj",
         "11\n22\nfirst\nsecond\n7\n9\n33\n"),
        ("same-unit", root / "tests/selfhost/fixtures/callable_same_unit/kinal.knproj",
         "11\n22\nfirst\nsecond\n"),
        ("alignment", root / "tests/selfhost/fixtures/capture_alignment/kinal.knproj", ""),
        ("foreach", root / "tests/selfhost/fixtures/capture_foreach/kinal.knproj", "10\n20\n30\n"),
        ("control-flow", root / "tests/selfhost/fixtures/capture_control_flow/kinal.knproj",
         "8\n-8\n5\n6\n2\n10\n11\n12\n20\n21\n22\n3\n2\n31\ncaught\n41\n42\n"),
        ("increment-types", root / "tests/selfhost/fixtures/capture_increment_types/kinal.knproj",
         "1.5\n2.5\na\nb\n20\n"),
    ]
    for name, expected in (("escaping_capture_contract", "7\n9\n11\n21\n13\n"),
                           ("escaping_capture_storage", "")):
        project_dir = out / name
        project_dir.mkdir(parents=True, exist_ok=True)
        source = (root / "tests/common" / (name + ".kn")).as_posix()
        project = project_dir / "kinal.knproj"
        project.write_text(
            'Project CallableStorage { DefaultProfile = "native"; '
            'SourceSet "source" { Files = [' + json.dumps(source) + ']; RequireUnit = true; } '
            'Profile "native" { Source { Entry = ' + json.dumps(source) + '; '
            'Sets = ["source"]; Mode = FileOnly; } Build { Backend = Native; } } }\n',
            encoding="utf-8",
        )
        cases.append((name, project, expected))
    compilers = [("stage0", stage0)] if stage0_reference else []
    compilers.append(("selfhost", compiler))
    for label, tool in compilers:
        env = dict(os.environ)
        env["PATH"] = str(tool.parent) + os.pathsep + env.get("PATH", "")
        for name, project, expected in cases:
            output = out / (label + "-" + name + (".exe" if os.name == "nt" else ""))
            proc = subprocess.run(
                [str(tool), "build", "--project", str(project), "--profile", "native", "-o", str(output)],
                cwd=root, env=env, text=True, capture_output=True,
            )
            assert proc.returncode == 0, (label, name, "build", proc.returncode, proc.stdout, proc.stderr)
            proc = subprocess.run([str(output)], cwd=root, env=env, text=True, capture_output=True)
            assert proc.returncode == 0 and proc.stdout.replace("\r\n", "\n") == expected and not proc.stderr, (
                label, name, "runtime", proc.returncode, proc.stdout, proc.stderr)
            print(f"[OK] {label} callable {name}", flush=True)
    targets = check_capture_targets(compiler, {name: project for name, project, _ in cases},
                                    root, out, env)
    return {"name": "callable_identity_and_storage", "ok": True,
            "cases": len(cases), "runtime_compilers": len(compilers), "ir_targets": targets,
            "stage0_reference": stage0_reference}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--stage0-role", choices=("reference", "bootstrap"), default="reference")
    args = parser.parse_args()
    print(json.dumps(check_callables(args.compiler.resolve(), args.stage0.resolve(),
                                    Path(__file__).resolve().parents[2], args.out_dir.resolve(),
                                    stage0_reference=args.stage0_role == "reference")))
