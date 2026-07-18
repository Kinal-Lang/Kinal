from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path


def run(command: list[str], *, cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, text=True, capture_output=True, check=False)


def require(condition: bool, message: str, proc: subprocess.CompletedProcess[str] | None = None) -> None:
    if condition:
        return
    if proc is not None:
        message += "\n" + (proc.stdout or "") + (proc.stderr or "")
    raise SystemExit(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()

    stage0 = args.stage0.resolve()
    compiler = args.compiler.resolve()
    root = args.root.resolve()
    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    results: list[dict[str, object]] = []
    version = run([str(compiler), "version"], cwd=root)
    require(version.returncode == 0 and "stage1" in version.stdout, "stage1 version smoke failed", version)
    results.append({"name": "version", "ok": True})

    probe_object = out_dir / ("llvm-probe.obj" if compiler.suffix.lower() == ".exe" else "llvm-probe.o")
    llvm_probe = run([str(compiler), "llvm-probe", str(probe_object)], cwd=root)
    require(llvm_probe.returncode == 0, "stage1 LLVM bridge probe failed", llvm_probe)
    require(probe_object.is_file() and probe_object.stat().st_size > 0,
            "stage1 LLVM bridge did not emit an object", llvm_probe)
    probe_rows = llvm_probe.stdout.replace("\r\n", "\n").splitlines()
    require("format=kinal-selfhost-llvm-v1" in probe_rows,
            "stage1 LLVM bridge summary is missing", llvm_probe)
    results.append({"name": "llvm_bridge", "ok": True, "bytes": probe_object.stat().st_size})

    fixture = root / "tests" / "selfhost" / "fixtures" / "lex_basic.kn"
    lex = run([str(compiler), "lex", str(fixture)], cwd=root)
    require(lex.returncode == 0, "stage1 lexer fixture failed", lex)
    normalized = lex.stdout.replace("\r\n", "\n")
    for expected in ("KwUnit @1:1", "Number", "String", "Character", "Eof"):
        require(expected in normalized, f"stage1 lexer output missing {expected!r}", lex)
    results.append({"name": "lex_fixture", "ok": True, "tokens": len(normalized.splitlines())})

    contextual_fixture = (
        root / "tests" / "selfhost" / "fixtures" / "contextual_qualified_names.kn"
    )
    contextual_parse = run([str(compiler), "parse", str(contextual_fixture)], cwd=root)
    require(
        contextual_parse.returncode == 0,
        "stage1 rejected a contextual keyword in a qualified name",
        contextual_parse,
    )
    contextual_stage0_path = out_dir / "contextual-stage0.kast"
    contextual_stage0 = run(
        [
            str(stage0),
            "build",
            "--no-module-discovery",
            "--color",
            "never",
            "--emit",
            "ast",
            str(contextual_fixture),
            "-o",
            str(contextual_stage0_path),
        ],
        cwd=root,
    )
    require(contextual_stage0.returncode == 0, "stage0 contextual-name fixture failed", contextual_stage0)
    contextual_stage1 = run([str(compiler), "ast", str(contextual_fixture)], cwd=root)
    require(contextual_stage1.returncode == 0, "stage1 contextual-name AST failed", contextual_stage1)
    require(
        contextual_stage0_path.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
        == contextual_stage1.stdout.replace("\r\n", "\n").strip(),
        "stage0/stage1 contextual-name syntax mismatch",
    )
    results.append({"name": "contextual_qualified_names", "ok": True})

    switch_fixture = root / "tests" / "selfhost" / "fixtures" / "switch_expression.kn"
    switch_stage0_path = out_dir / "switch-expression-stage0.kast"
    switch_stage0 = run(
        [
            str(stage0),
            "build",
            "--no-module-discovery",
            "--color",
            "never",
            "--emit",
            "ast",
            str(switch_fixture),
            "-o",
            str(switch_stage0_path),
        ],
        cwd=root,
    )
    require(switch_stage0.returncode == 0, "stage0 switch-expression fixture failed", switch_stage0)
    switch_stage1 = run([str(compiler), "ast", str(switch_fixture)], cwd=root)
    require(switch_stage1.returncode == 0, "stage1 switch-expression AST failed", switch_stage1)
    require(
        switch_stage0_path.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
        == switch_stage1.stdout.replace("\r\n", "\n").strip(),
        "stage0/stage1 switch-expression syntax mismatch",
    )
    results.append({"name": "switch_expression", "ok": True})

    source_root = root / "apps" / "kinal-selfhost" / "src"
    source_files = sorted(source_root.rglob("*.kn"))
    project_file = root / "apps" / "kinal-selfhost" / "kinal.knproj"
    project = run([str(compiler), "project", str(project_file)], cwd=root)
    require(project.returncode == 0, "stage1 project loader failed", project)
    project_rows = project.stdout.replace("\r\n", "\n").splitlines()
    for expected in (
        "format=kinal-project-v1",
        "name=KinalSelfhost",
        "profile=stage1",
        f"sources={len(source_files)}",
    ):
        require(expected in project_rows, f"stage1 project output missing {expected!r}", project)
    results.append({"name": "project_self", "ok": True, "files": len(source_files)})

    stage0_project_ast_path = out_dir / "stage0-project.kast"
    stage0_project_ast = run(
        [
            str(stage0),
            "build",
            "--project",
            str(project_file.parent),
            "--profile",
            "stage1",
            "--emit",
            "ast",
            "-o",
            str(stage0_project_ast_path),
        ],
        cwd=root,
    )
    require(stage0_project_ast.returncode == 0, "stage0 project syntax emit failed", stage0_project_ast)
    stage1_project_ast = run([str(compiler), "project-ast", str(project_file), "stage1"], cwd=root)
    require(stage1_project_ast.returncode == 0, "stage1 project syntax build failed", stage1_project_ast)
    require(
        stage0_project_ast_path.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
        == stage1_project_ast.stdout.replace("\r\n", "\n").strip(),
        "stage0/stage1 project syntax mismatch",
    )
    results.append({"name": "project_syntax_differential", "ok": True, "files": len(source_files)})

    semantic = run([str(compiler), "check", str(project_file), "stage1"], cwd=root)
    require(semantic.returncode == 0, "stage1 semantic declaration check failed", semantic)
    semantic_rows = semantic.stdout.replace("\r\n", "\n").splitlines()
    require("format=kinal-selfhost-sema-v1" in semantic_rows, "stage1 semantic summary is missing", semantic)
    require("unresolved_imports=0" in semantic_rows, "stage1 left unresolved imports", semantic)
    require("unresolved_types=0" in semantic_rows, "stage1 left unresolved declaration types", semantic)
    require("unresolved_expressions=0" in semantic_rows, "stage1 left unresolved expressions", semantic)
    results.append({"name": "semantic_declarations", "ok": True, "files": len(source_files)})

    executable_suffix = ".exe" if compiler.suffix.lower() == ".exe" else ""
    backend_project = root / "tests" / "selfhost" / "fixtures" / "backend_core" / "kinal.knproj"
    backend_executable = out_dir / f"backend-core{executable_suffix}"
    backend_build = run(
        [str(compiler), "build", str(backend_project), str(backend_executable), "test"],
        cwd=root,
    )
    require(backend_build.returncode == 0, "stage1 backend fixture build failed", backend_build)
    require(backend_executable.is_file(), "stage1 backend fixture executable is missing")
    backend_run = run([str(backend_executable)], cwd=root)
    require(backend_run.returncode == 0, "stage1 backend fixture execution failed", backend_run)
    require(
        backend_run.stdout.replace("\r\n", "\n").strip() == "string[]\nchar[]",
        "stage1 backend fixture output differs",
        backend_run,
    )
    results.append({"name": "native_backend_fixture", "ok": True})

    array_project = root / "tests" / "selfhost" / "fixtures" / "array_types" / "kinal.knproj"
    array_executable = out_dir / f"array-types{executable_suffix}"
    array_build = run(
        [str(compiler), "build", str(array_project), str(array_executable), "test"],
        cwd=root,
    )
    require(array_build.returncode == 0, "stage1 array fixture build failed", array_build)
    require(array_executable.is_file(), "stage1 array fixture executable is missing")
    array_run = run([str(array_executable), "alpha", "beta"], cwd=root)
    require(array_run.returncode == 0, "stage1 array fixture execution failed", array_run)
    results.append({"name": "array_types_fixture", "ok": True})

    generic_project = root / "tests" / "selfhost" / "fixtures" / "generic_functions" / "kinal.knproj"
    generic_executable = out_dir / f"generic-functions{executable_suffix}"
    generic_build = run(
        [str(compiler), "build", str(generic_project), str(generic_executable), "test"],
        cwd=root,
    )
    require(generic_build.returncode == 0, "stage1 generic fixture build failed", generic_build)
    require(generic_executable.is_file(), "stage1 generic fixture executable is missing")
    generic_run = run([str(generic_executable)], cwd=root)
    require(generic_run.returncode == 0, "stage1 generic fixture execution failed", generic_run)
    require(
        generic_run.stdout.replace("\r\n", "\n").strip() == "42\nnest\n3",
        "stage1 generic fixture output differs",
        generic_run,
    )
    generic_stage0_ast_path = out_dir / "generic-stage0.kast"
    generic_stage0_ast = run(
        [
            str(stage0),
            "build",
            "--project",
            str(generic_project.parent),
            "--profile",
            "test",
            "--emit",
            "ast",
            "-o",
            str(generic_stage0_ast_path),
        ],
        cwd=root,
    )
    require(generic_stage0_ast.returncode == 0, "stage0 generic syntax emit failed", generic_stage0_ast)
    generic_stage1_ast = run([str(compiler), "project-ast", str(generic_project), "test"], cwd=root)
    require(generic_stage1_ast.returncode == 0, "stage1 generic syntax build failed", generic_stage1_ast)
    require(
        generic_stage0_ast_path.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
        == generic_stage1_ast.stdout.replace("\r\n", "\n").strip(),
        "stage0/stage1 generic syntax mismatch",
    )
    results.append({"name": "generic_function_instances", "ok": True})

    missing_generic_project = (
        root / "tests" / "selfhost" / "fixtures" / "generic_missing_args" / "kinal.knproj"
    )
    missing_generic = run(
        [str(compiler), "check", str(missing_generic_project), "test"],
        cwd=root,
    )
    missing_generic_text = (missing_generic.stdout + missing_generic.stderr).lower()
    require(
        missing_generic.returncode != 0 and "generic function" in missing_generic_text,
        "stage1 accepted a generic call without type arguments",
        missing_generic,
    )
    results.append({"name": "generic_missing_arguments", "ok": True})

    kinalvm_project = root / "apps" / "kinalvm" / "kinal.knproj"
    kinalvm_source_files = sorted((kinalvm_project.parent / "src").rglob("*.kn"))
    kinalvm_stage0_ast_path = out_dir / "kinalvm-stage0.kast"
    kinalvm_stage0_ast = run(
        [
            str(stage0),
            "build",
            "--project",
            str(kinalvm_project.parent),
            "--profile",
            "release",
            "--emit",
            "ast",
            "-o",
            str(kinalvm_stage0_ast_path),
        ],
        cwd=root,
    )
    require(kinalvm_stage0_ast.returncode == 0, "stage0 KinalVM syntax emit failed", kinalvm_stage0_ast)
    kinalvm_stage1_ast = run([str(compiler), "project-ast", str(kinalvm_project), "release"], cwd=root)
    require(kinalvm_stage1_ast.returncode == 0, "stage1 KinalVM syntax build failed", kinalvm_stage1_ast)
    require(
        kinalvm_stage0_ast_path.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
        == kinalvm_stage1_ast.stdout.replace("\r\n", "\n").strip(),
        "stage0/stage1 KinalVM syntax mismatch",
    )
    kinalvm_check = run([str(compiler), "check", str(kinalvm_project), "release"], cwd=root)
    require(kinalvm_check.returncode == 0, "stage1 KinalVM semantic check failed", kinalvm_check)
    require(
        "unresolved_expressions=0" in kinalvm_check.stdout.replace("\r\n", "\n").splitlines(),
        "stage1 KinalVM semantic check left unresolved expressions",
        kinalvm_check,
    )
    kinalvm_executable = out_dir / f"kinalvm-selfhost{executable_suffix}"
    kinalvm_build = run(
        [str(compiler), "build", str(kinalvm_project), str(kinalvm_executable), "release"],
        cwd=root,
    )
    require(kinalvm_build.returncode == 0, "stage1 KinalVM build failed", kinalvm_build)
    require(kinalvm_executable.is_file(), "stage1 KinalVM executable is missing")

    kinalvm_smoke_source = root / "tests" / "common" / "knc_superloop.kn"
    kinalvm_smoke_knc = out_dir / "kinalvm-superloop.knc"
    kinalvm_smoke_build = run(
        [
            str(stage0),
            "vm",
            "build",
            "--no-module-discovery",
            str(kinalvm_smoke_source),
            "-o",
            str(kinalvm_smoke_knc),
        ],
        cwd=root,
    )
    require(kinalvm_smoke_build.returncode == 0, "stage0 KinalVM smoke bytecode build failed", kinalvm_smoke_build)
    kinalvm_smoke = run([str(kinalvm_executable), str(kinalvm_smoke_knc)], cwd=root)
    require(kinalvm_smoke.returncode == 0, "self-built KinalVM smoke execution failed", kinalvm_smoke)
    require(
        kinalvm_smoke.stdout.replace("\r\n", "\n").strip() == "5\n0\n1000",
        "self-built KinalVM smoke output differs",
        kinalvm_smoke,
    )
    kinalvm_disasm = run([str(kinalvm_executable), "--disasm", str(kinalvm_smoke_knc)], cwd=root)
    require(kinalvm_disasm.returncode == 0, "self-built KinalVM disassembly failed", kinalvm_disasm)
    require(
        "LoopIntLtInc" in kinalvm_disasm.stdout and "IntToString" in kinalvm_disasm.stdout,
        "self-built KinalVM disassembly is incomplete",
        kinalvm_disasm,
    )
    results.append({"name": "kinalvm_selfhost", "ok": True, "sources": len(kinalvm_source_files)})

    for source in source_files:
        proc = run([str(compiler), "lex", str(source)], cwd=root)
        require(proc.returncode == 0, f"stage1 cannot lex its own source: {source}", proc)
    results.append({"name": "lex_self", "ok": True, "files": len(source_files)})

    differential_sources = [fixture, *source_files]
    for index, source in enumerate(differential_sources):
        stage0_output = out_dir / f"stage0-{index}.ktokens"
        stage0_lex = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "tokens",
                str(source),
                "-o",
                str(stage0_output),
            ],
            cwd=root,
        )
        require(stage0_lex.returncode == 0, f"stage0 token emit failed: {source}", stage0_lex)
        stage1_lex = run([str(compiler), "tokens", str(source)], cwd=root)
        require(stage1_lex.returncode == 0, f"stage1 token dump failed: {source}", stage1_lex)
        stage0_rows = [
            line for line in stage0_output.read_text(encoding="utf-8").splitlines()
            if line and line[0].isdigit()
        ]
        stage1_rows = stage1_lex.stdout.replace("\r\n", "\n").splitlines()
        require(
            stage0_rows == stage1_rows,
            f"stage0/stage1 token mismatch: {source}\nstage0={stage0_rows}\nstage1={stage1_rows}",
        )
    results.append({"name": "lex_differential", "ok": True, "files": len(differential_sources)})

    for source in source_files:
        proc = run([str(compiler), "parse", str(source)], cwd=root)
        require(proc.returncode == 0, f"stage1 cannot parse its own source: {source}", proc)
    results.append({"name": "parse_self", "ok": True, "files": len(source_files)})

    for index, source in enumerate(source_files):
        stage0_output = out_dir / f"stage0-ast-{index}.kast"
        stage0_ast = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "ast",
                str(source),
                "-o",
                str(stage0_output),
            ],
            cwd=root,
        )
        require(stage0_ast.returncode == 0, f"stage0 syntax emit failed: {source}", stage0_ast)
        stage1_ast = run([str(compiler), "ast", str(source)], cwd=root)
        require(stage1_ast.returncode == 0, f"stage1 syntax build failed: {source}", stage1_ast)
        stage0_summary = stage0_output.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
        stage1_summary = stage1_ast.stdout.replace("\r\n", "\n").strip()
        require(
            stage0_summary == stage1_summary,
            f"stage0/stage1 syntax mismatch: {source}\nstage0={stage0_summary}\nstage1={stage1_summary}",
        )
    results.append({"name": "syntax_differential", "ok": True, "files": len(source_files)})

    (out_dir / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print(f"[OK] selfhost tests: {len(results)} groups")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
