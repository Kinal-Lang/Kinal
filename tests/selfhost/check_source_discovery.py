"""BOM, open Unit, reachability and source snapshot reuse contracts."""
from __future__ import annotations

import os
import subprocess
from pathlib import Path


def check_source_discovery(compiler: Path, stage0: Path, root: Path, out: Path,
                           *, stage0_reference: bool = True) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    source = out / "src"
    source.mkdir(exist_ok=True)
    (source / "nested").mkdir(exist_ok=True)
    (source / "Main.kn").write_text(
        "Unit Tests.Discovery; Get Tests.Dependency; "
        "Static Function Response Make() { Return New Response(); } "
        "Static Function int Main() { Response value = Make(); "
        "Return value.Value + Tests.Dependency.DependencyValue(); }\n", encoding="utf-8-sig")
    (source / "Response.kn").write_text(
        "Unit Tests.Discovery; Class Response { Public int Value; "
        "Public Constructor() { Value = 3; } }\n", encoding="utf-8-sig")
    (source / "nested/Dependency.kn").write_text(
        "Unit Tests.Dependency; Static Function int DependencyValue() { Return 4; }\n",
        encoding="utf-8-sig")
    # A broken unrelated body cannot poison discovery or hide the reachable Units.
    (source / "Unused.kn").write_text(
        "Unit Tests.Unused; Static Function void Broken( {\n", encoding="utf-8-sig")
    manifest = out / "kinal.knproj"

    def project(explicit: bool = False, mode: str = "ReachableUnits") -> None:
        sets = 'SourceSet "main" { Roots = ["src"]; RequireUnit = true; } ' if explicit else ""
        selection = f'Mode = {mode}; ' if explicit else ""
        manifest.write_text(
            'Project Discovery { DefaultProfile = "test"; ' + sets +
            'Profile "test" { Source { Entry = "src/Main.kn"; AutoDiscovery = true; ' +
            selection + '} } }\n', encoding="utf-8")

    tools = [("stage0", stage0)] if stage0_reference else []
    tools.append(("selfhost", compiler))
    checks = 0
    for label, tool in tools:
        def invoke(name: str, arguments: list[str], success: bool = True) -> subprocess.CompletedProcess:
            nonlocal checks
            result = subprocess.run([str(tool), "build", *arguments], cwd=root,
                                    capture_output=True, text=True, encoding="utf-8", timeout=180)
            (out / f"{label}-{name}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
            assert (result.returncode == 0) == success, (label, name, result.stdout, result.stderr)
            checks += 1
            return result

        for explicit in (False, True):
            project(explicit)
            invoke("explicit" if explicit else "implicit",
                   ["--project", str(manifest), "--emit", "check", "-o", str(out / f"{label}.kcheck")])
        invoke("direct", [str(source / "Main.kn"), "--emit", "check", "-o", str(out / f"{label}.kcheck")])
        project()
        binary = out / (label + (".exe" if os.name == "nt" else ""))
        invoke("build", ["--project", str(manifest), "-o", str(binary)])
        execution = subprocess.run([str(binary)], cwd=out, capture_output=True, timeout=30)
        assert execution.returncode == 7, (label, execution.returncode, execution.stdout, execution.stderr)
        checks += 1
        for mode in ("FileOnly", "EntryUnit"):
            project(True, mode)
            invoke(mode, ["--project", str(manifest), "--emit", "check", "-o", str(out / f"{label}.kcheck")], False)
        project(True)
        broken = source / "Broken.kn"
        broken.write_text("Unit Tests.Discovery; Static Function void Broken( {\n", encoding="utf-8-sig")
        failure = invoke("reachable-error", ["--project", str(manifest), "--emit", "check",
                                             "-o", str(out / f"{label}.kcheck")], False)
        assert "[Parser]" in failure.stdout + failure.stderr, (label, failure.stdout, failure.stderr)
        broken.unlink()
    return {"name": "source_discovery", "ok": True, "checks": checks,
            "bom": True, "same_unit_class": True, "transitive_import": True,
            "unreachable_error_isolated": True, "reachable_error_reported": True}
