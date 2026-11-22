"""File-local symbol binding in both selfhost backends."""

from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory


def check_symbol_aliases(compiler: Path, root: Path, out: Path) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    fixture = root / "tests/selfhost/fixtures/symbol_aliases"
    checks = 0

    def execute(args, cwd):
        return subprocess.run([compiler, *args], cwd=cwd, capture_output=True,
                              encoding="utf-8", errors="replace", timeout=90)

    modes = [(["run"], "test"), (["vm", "run"], "vm")]
    for command, profile in modes:
        result = execute([*command, "--project", fixture, "--profile", profile], out)
        assert result.returncode == 0 and result.stdout.splitlines() == ["method", "7"], result
        checks += 1

    with TemporaryDirectory(dir=out) as temp:
        project = Path(temp)
        (project / "kinal.knproj").write_text(
            'Project AliasChecks { SourceSet "src" { Files=["Main.kn","Lib.kn","Other.kn"]; } '
            'Profile "all" { Source { Entry="Main.kn"; Sets=["src"]; Mode=AllSources; } } }',
            encoding="utf-8")
        (project / "Lib.kn").write_text("""Unit Models;
Class Box {
    Public int Count;
    Public Static int Stored;
    Public Class Inner { Public int Code; }
    Public Static Property int Total { Get { Return Stored; } Set { Stored=value; } }
    Private Static Function int Secret() { Return 5; }
}
Function int Next(int x) { Return x + 1; }
""", encoding="utf-8")
        main = project / "Main.kn"
        other = project / "Other.kn"
        other.write_text("Unit App;", encoding="utf-8")
        # A chain longer than 32 also exercises alias expansion in a type argument.
        chain = ["Alias A0 By M.Box;"] + [f"Alias A{i} By A{i-1};" for i in range(1, 45)]
        main.write_text("Unit App; Get M By Models; Get Gateway By Models; Get Inc By Models.Next;\n"
                        + "\n".join(chain) + """
Alias Total By A44.Total;
Alias Field By M.Box.Stored;
Alias Step By Inc;
Class Gateway { Public Class Box { Public Static int Stored = 17; } }
Function T Identity<T>(T x) { Return x; }
Static Function int Main() {
    A44 box = Identity<A44>(New A44());
    box.Count = 39;
    Field = box.Count;
    Total = Step(Total);
    A44.Inner inner = New A44.Inner();
    inner.Code = Gateway.Box.Stored;
    Return Field == 40 && Total == 40 && inner.Code == 40 ? 0 : 2;
}""", encoding="utf-8")
        for command, _ in modes:
            result = execute([*command, "--project", project, "--profile", "all"], out)
            assert result.returncode == 0, result
            checks += 1

        for directives, diagnostic in [
            ("Alias Missing By Models.Box;", "E-SEM-00051"),
            ("Get M By Models; Alias Missing By Models.Next;", "E-SEM-00051"),
            ("Get Models; Alias Missing By Models.Unknown;", "E-SEM-00051"),
            ("Get IO.Volatile; Alias Missing By IO.Volatile.ReadMissing;", "E-SEM-00051"),
            ("Get Models; Alias M By Models;", "E-SEM-00051"),
            ("Get Models; Alias Missing By Next;", "E-SEM-00051"),
            ("Get Models; Alias Secret By Models.Box.Secret;", "E-SEM-00051"),
            ("Get Models; Alias Count By Models.Box.Count;", "E-SEM-00051"),
            ("Alias First By Second; Alias Second By First;", "E-SEM-00051"),
            ("Get M By Models; Alias Selected By M.Box;", "Unknown Type"),
        ]:
            main.write_text(f"Unit App;\n{directives}\nStatic Function int Main() {{ Return 0; }}",
                            encoding="utf-8")
            other.write_text("Unit App;" + (" Function Selected Leak() { Return New Selected(); }"
                                             if diagnostic == "Unknown Type" else ""), encoding="utf-8")
            for command, ext in [(["build"], ".exe"), (["vm", "build"], ".knc")]:
                artifact = project / ("rejected" + ext)
                result = execute([*command, "--project", project, "--profile", "all", "-o", artifact], out)
                output = result.stdout + result.stderr
                assert result.returncode == 1 and diagnostic in output and not artifact.exists(), result
                checks += 1
    return {"name": "symbol_aliases", "ok": True, "checks": checks}
