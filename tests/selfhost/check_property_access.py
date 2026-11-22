"""Check accessor selection before lowering and run the same positive case on both backends."""

from pathlib import Path
import shutil
import subprocess
from tempfile import TemporaryDirectory


def check_property_access(compiler: Path, root: Path, out: Path) -> dict:
    fixture = root / "tests/selfhost/fixtures/property_access"
    source = (fixture / "Main.kn").read_text(encoding="utf-8")
    out.mkdir(parents=True, exist_ok=True)
    checks = 0
    with TemporaryDirectory(dir=out) as temp:
        project = Path(temp)
        shutil.copy2(fixture / "kinal.knproj", project)
        main = project / "Main.kn"

        def execute(*arguments):
            return subprocess.run([compiler, *arguments], cwd=project, capture_output=True,
                                  encoding="utf-8", errors="replace", timeout=90)

        insertion = "    Other other = New Other();"
        before, marker, after = source.partition(insertion)
        assert marker, "missing insertion point in property_access/Main.kn"
        mixed = "\nClass Mixed By Meter { Public Property string Reading { Set { } } }\n"
        for backend, suffix in [([], ".exe"), (["vm"], ".knc")]:
            main.write_text(source, encoding="utf-8")
            result = execute(*backend, "run", "--project", project)
            assert result.returncode == 0 and result.stdout.splitlines() == ["42"], result
            checks += 1
            for statement, diagnostic in [
                ("meter.Reading = 42;", "Read-only Property"),
                ("meter.Reading++;", "Read-only Property"),
                ("int x = sink.Value;", "Write-only Property"),
                ("sink.Value += 1;", "Write-only Property"),
                ("++sink.Value;", "Write-only Property"),
                ("meter.Caption++;", "Increment/decrement requires numeric or pointer type"),
                ("int x = SetTotal;", "Write-only Property"),
                ('sink.Value = "text";', "Type Mismatch"),
                ("Mixed mixed = New Mixed(); mixed.Reading += 1;", "different value types"),
            ]:
                main.write_text(before + "    " + statement + "\n" + marker + after + mixed, encoding="utf-8")
                artifact = project / ("invalid" + suffix)
                result = execute(*backend, "build", "--project", project, "-o", artifact)
                assert result.returncode == 1 and diagnostic in result.stdout + result.stderr, result
                assert not artifact.exists(), artifact
                checks += 1
    return {"name": "property_access", "ok": True, "checks": checks}
