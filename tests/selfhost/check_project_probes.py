"""SourceSet validation and compilation-local probe reuse contracts."""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


def check_project_probes(compiler: Path, stage0: Path, root: Path, out: Path,
                         *, stage0_reference: bool = True) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    # Shared C/selfhost source path and nested RequireUnit regression.
    sys.path.insert(0, str(root / "tests"))
    from run_stage_tests import check_project_source_paths
    check_project_source_paths(compiler, out)
    samples = out / "samples"
    samples.mkdir(exist_ok=True)
    first = samples / "First.kn"
    second = samples / "Second.kn"
    first.write_text("Unit Tests.Probes.First; Static Function int Main() { Return 0; }\n", encoding="utf-8")
    second.write_text("Unit Tests.Probes.Second; Static Function void Broken( {\n", encoding="utf-8")
    sample_project = samples / "kinal.knproj"
    sample_project.write_text('Project Samples { DefaultProfile = "test"; '
                              'SourceSet "main" { Roots = ["."]; Include = ["*.kn"]; RequireUnit = true; } '
                              'Profile "test" { Source { Entry = "First.kn"; Sets = ["main"]; Mode = FileOnly; } } }',
                              encoding="utf-8")
    alternate = first.parent / "unused" / ".." / first.name
    source = out / "Main.kn"
    source.write_text('''Unit Tests.Selfhost.ProjectProbes;
Get IO.Kinal.Compiler.Core.CompileSession;
Static Function int Main() {
    CompileSession session = New CompileSession();
    ProjectSourceProbe first = session.ProbeProjectSource(FIRST);
    ProjectSourceProbe same = session.ProbeProjectSource(FIRST);
    ProjectSourceProbe normalized = session.ProbeProjectSource(ALTERNATE);
    If (first == null || first != same || first != normalized) Return 1;
    If (session.ProjectProbeCache.Count() != 1) Return 2;
    ProjectSourceProbe second = session.ProbeProjectSource(SECOND);
    If (second == null || second == first || session.ProjectProbeCache.Count() != 2) Return 3;
    If (session.HasErrors()) Return 4;
    CompileSession other = New CompileSession();
    If (!other.LoadProject(PROJECT, "test")) Return 5;
    If (other.ProbeProjectSource(FIRST) == first || other.ProjectProbeCache.Count() != 2) Return 6;
    If (!other.ParseProject() || other.UnitCount() != 1 || other.HasErrors()) Return 7;
    Return 0;
}
'''.replace("FIRST", json.dumps(str(first))).replace("ALTERNATE", json.dumps(str(alternate)))
       .replace("SECOND", json.dumps(str(second))).replace("PROJECT", json.dumps(str(sample_project))),
       encoding="utf-8")
    project = out / "kinal.knproj"
    project.write_text('Project ProbeContract { DefaultProfile = "test"; '
                       'SourceSet "main" { Files = ["Main.kn"]; RequireUnit = true; } '
                       'SourceSet "compiler" { Roots = [' + json.dumps(str(root / "apps/kinal-selfhost/src")) + ']; '
                       'Include = ["**/*.kn"]; RequireUnit = true; } '
                       'Profile "test" { Source { Entry = "Main.kn"; Sets = ["main", "compiler"]; '
                       'Mode = ReachableUnits; } } }\n', encoding="utf-8")
    compilers = [("stage0", stage0)] if stage0_reference else []
    compilers.append(("selfhost", compiler))
    for label, tool in compilers:
        output = out / (label + (".exe" if os.name == "nt" else ""))
        built = subprocess.run([str(tool), "build", "--project", str(project), "-o", str(output)],
                               cwd=root, text=True, capture_output=True, timeout=240)
        (out / f"{label}.build.log").write_text(built.stdout + built.stderr, encoding="utf-8")
        assert built.returncode == 0, (label, built.stdout, built.stderr)
        execution = subprocess.run([str(output)], cwd=root, text=True, capture_output=True, timeout=30)
        assert execution.returncode == 0 and not execution.stdout and not execution.stderr, (
            label, execution.returncode, execution.stdout, execution.stderr)
        print(f"[OK] {label} project probe canonical reuse/isolation", flush=True)
    return {"name": "project_probe_contract", "ok": True, "runtime_compilers": len(compilers),
            "unique_cached_sources": 2, "repeated_probe_requests": 3,
            "unreachable_body_diagnostics_isolated": True}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--stage0", required=True, type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    args = parser.parse_args()
    print(json.dumps(check_project_probes(args.compiler.resolve(), args.stage0.resolve(),
                                          Path(__file__).resolve().parents[2], args.out_dir.resolve())))
