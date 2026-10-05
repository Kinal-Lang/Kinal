"""Hosted startup must initialize native process arguments for every Main shape."""
from __future__ import annotations

import argparse
import json
import os
import subprocess
from pathlib import Path


def check_startup_arguments(compiler: Path, stage0: Path, root: Path, out: Path,
                            *, stage0_reference: bool = True) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    tools = [("stage0", stage0)] if stage0_reference else []
    tools.append(("selfhost", compiler))
    for name, signature, extra in (
        ("no-parameters", "", ""),
        ("array-parameter", "string[] args",
         'If (args.Length() != 2 || args[0] != "startup-marker" || args[1] != "two words") Return 3;'),
    ):
        directory = out / name
        directory.mkdir(exist_ok=True)
        source = directory / "Main.kn"
        source.write_text('Unit Tests.Selfhost.StartupArguments;\nGet IO.System;\nGet IO.Text;\n'
                          f'Trusted Static Function int Main({signature}) {{\n'
                          'string command = IO.System.CommandLine();\n'
                          'If (IO.Text.IsEmpty(command)) Return 1;\n'
                          'If (!IO.Text.Contains(command, "startup-marker") || '
                          '!IO.Text.Contains(command, "two words")) Return 2;\n'
                          + extra + '\nReturn 0;\n}\n', encoding="utf-8")
        project = directory / "kinal.knproj"
        project.write_text('Project Startup { DefaultProfile = "test"; '
                           'SourceSet "main" { Files = ["Main.kn"]; RequireUnit = true; } '
                           'Profile "test" { Source { Entry = "Main.kn"; Sets = ["main"]; Mode = FileOnly; } } }',
                           encoding="utf-8")
        for label, tool in tools:
            output = directory / (label + (".exe" if os.name == "nt" else ""))
            build = subprocess.run([str(tool), "build", "--project", str(project), "-o", str(output)],
                                   cwd=root, text=True, capture_output=True, timeout=120)
            (directory / f"{label}.build.log").write_text(build.stdout + build.stderr, encoding="utf-8")
            assert build.returncode == 0, (label, name, build.stdout, build.stderr)
            execution = subprocess.run([str(output), "startup-marker", "two words"], cwd=root,
                                       text=True, capture_output=True, timeout=30)
            assert execution.returncode == 0 and not execution.stdout and not execution.stderr, (
                label, name, execution.returncode, execution.stdout, execution.stderr)
            print(f"[OK] {label} process arguments: {name}", flush=True)
    return {"name": "hosted_startup_arguments", "ok": True, "cases": 2, "runtime_compilers": len(tools)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(check_startup_arguments(args.compiler.resolve(), args.stage0.resolve(),
                                            Path(__file__).resolve().parents[2], args.out_dir.resolve())))
