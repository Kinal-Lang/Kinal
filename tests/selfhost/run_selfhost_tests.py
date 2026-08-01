from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def run(command: list[str], *, cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, text=True, capture_output=True, check=False)


def require(condition: bool, message: str, proc: subprocess.CompletedProcess[str] | None = None) -> None:
    if condition:
        return
    if proc is not None:
        message += "\n" + (proc.stdout or "") + (proc.stderr or "")
    raise SystemExit(message)


def parser_diagnostic_titles(proc: subprocess.CompletedProcess[str]) -> list[str]:
    output = (proc.stdout or "") + (proc.stderr or "")
    titles: list[str] = []
    for line in output.splitlines():
        if not line.startswith("[Parser]") or "[warning]" in line:
            continue
        message = line[line.rfind("] ") + 2 :]
        titles.append(message.split(":", 1)[0])
    return titles


def compiler_diagnostic_titles(proc: subprocess.CompletedProcess[str]) -> list[str]:
    output = (proc.stdout or "") + (proc.stderr or "")
    titles: list[str] = []
    for line in output.splitlines():
        if not line.startswith("[") or "[warning]" in line:
            continue
        stage_end = line.find("]")
        if stage_end < 0:
            continue
        stage = line[1:stage_end]
        if stage not in {"Lexer", "Parser", "Sema", "Project", "Driver", "Link", "KNC", "Native"}:
            continue
        message = line[line.rfind("] ") + 2 :]
        titles.append(f"{stage}:{message.split(':', 1)[0]}")
    return titles


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

    unsafe_alias_fixtures = [
        root / "tests" / "common" / "unsafe_alias_keyword.kn",
        root / "tests" / "common" / "unsafe_alias_get_keyword.kn",
        root / "tests" / "common" / "unsafe_alias_unicode_keyword.kn",
        root / "tests" / "common" / "localized_aliases.kn",
        root / "tests" / "common" / "unsafe_unsafe_unsafe_alias.kn",
        root / "tests" / "common" / "unsafe_unsafe_unsafe_alias_sequence.kn",
        root / "tests" / "common" / "unsafe_alias_latest_wins.kn",
        root / "tests" / "common" / "unsafe_unsafe_unsafe_alias_semicolon_target.kn",
    ]
    for index, unsafe_alias_fixture in enumerate(unsafe_alias_fixtures):
        unsafe_stage0_ast_path = out_dir / f"unsafe-alias-{index}-stage0.kast"
        unsafe_stage0_ast = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "ast",
                str(unsafe_alias_fixture),
                "-o",
                str(unsafe_stage0_ast_path),
            ],
            cwd=root,
        )
        require(unsafe_stage0_ast.returncode == 0, "stage0 unsafe-alias AST failed", unsafe_stage0_ast)
        unsafe_stage1_ast = run([str(compiler), "ast", str(unsafe_alias_fixture)], cwd=root)
        require(unsafe_stage1_ast.returncode == 0, "stage1 unsafe-alias AST failed", unsafe_stage1_ast)
        require(
            unsafe_stage0_ast_path.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
            == unsafe_stage1_ast.stdout.replace("\r\n", "\n").strip(),
            f"stage0/stage1 unsafe-alias syntax mismatch: {unsafe_alias_fixture}",
        )
    results.append({"name": "unsafe_alias_syntax", "ok": True, "files": len(unsafe_alias_fixtures)})

    unsafe_alias_unit_fixture = root / "tests" / "common" / "error_unsafe_alias_unit.kn"
    unsafe_alias_unit_stage0 = run(
        [
            str(stage0),
            "build",
            "--no-module-discovery",
            "--color",
            "never",
            "--emit",
            "check",
            str(unsafe_alias_unit_fixture),
            "-o",
            str(out_dir / "unsafe-alias-unit-stage0.kcheck"),
        ],
        cwd=root,
    )
    unsafe_alias_unit_stage1 = run(
        [str(compiler), "check-source", str(unsafe_alias_unit_fixture)],
        cwd=root,
    )
    unsafe_alias_unit_title = "Invalid Unit With Unsafe Alias"
    unsafe_alias_unit_stage0_output = (
        (unsafe_alias_unit_stage0.stdout or "") + (unsafe_alias_unit_stage0.stderr or "")
    )
    unsafe_alias_unit_stage1_output = (
        (unsafe_alias_unit_stage1.stdout or "") + (unsafe_alias_unit_stage1.stderr or "")
    )
    require(
        unsafe_alias_unit_stage0.returncode != 0
        and "[Parser]" in unsafe_alias_unit_stage0_output
        and unsafe_alias_unit_title in unsafe_alias_unit_stage0_output,
        "stage0 did not reject Unit after Unsafe Alias",
        unsafe_alias_unit_stage0,
    )
    require(
        unsafe_alias_unit_stage1.returncode != 0
        and "[Parser]" in unsafe_alias_unit_stage1_output
        and unsafe_alias_unit_title in unsafe_alias_unit_stage1_output,
        "stage1 did not match the stage0 Unsafe Alias + Unit diagnostic",
        unsafe_alias_unit_stage1,
    )
    results.append({"name": "unsafe_alias_unit_diagnostic", "ok": True})

    unsafe_alias_category_fixtures = [
        root / "tests" / "common" / "error_unsafe_alias_operator_source.kn",
        root / "tests" / "common" / "error_unsafe_alias_identifier_target.kn",
        root / "tests" / "common" / "error_unsafe_alias_triple_identifier_target.kn",
    ]
    for unsafe_alias_category_fixture in unsafe_alias_category_fixtures:
        stage0_category = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "check",
                str(unsafe_alias_category_fixture),
                "-o",
                str(out_dir / f"{unsafe_alias_category_fixture.stem}-stage0.kcheck"),
            ],
            cwd=root,
        )
        stage1_category = run(
            [str(compiler), "check-source", str(unsafe_alias_category_fixture)],
            cwd=root,
        )
        stage0_category_output = (stage0_category.stdout or "") + (stage0_category.stderr or "")
        stage1_category_output = (stage1_category.stdout or "") + (stage1_category.stderr or "")
        require(
            stage0_category.returncode != 0
            and "[Parser]" in stage0_category_output
            and "Invalid Unsafe Alias" in stage0_category_output,
            f"stage0 accepted invalid Unsafe Alias token category: {unsafe_alias_category_fixture}",
            stage0_category,
        )
        require(
            stage1_category.returncode != 0
            and "[Parser]" in stage1_category_output
            and "Invalid Unsafe Alias" in stage1_category_output,
            f"stage1 differs on Unsafe Alias token category: {unsafe_alias_category_fixture}",
            stage1_category,
        )
        stage0_parser_diagnostics = sum(
            1 for line in stage0_category_output.splitlines() if line.startswith("[Parser]")
        )
        stage1_parser_diagnostics = sum(
            1 for line in stage1_category_output.splitlines() if line.startswith("[Parser]")
        )
        require(
            stage0_parser_diagnostics == stage1_parser_diagnostics,
            f"Unsafe Alias diagnostic count differs: {unsafe_alias_category_fixture}",
            stage1_category,
        )
    results.append(
        {
            "name": "unsafe_alias_category_diagnostics",
            "ok": True,
            "files": len(unsafe_alias_category_fixtures),
        }
    )

    unknown_token_fixtures = [
        (
            root / "tests" / "common" / "error_unknown_statement_token.kn",
            "Unexpected Token",
        ),
        (
            root / "tests" / "common" / "error_unknown_type_member.kn",
            "Expected Identifier",
        ),
    ]
    for unknown_token_fixture, expected_title in unknown_token_fixtures:
        stage0_unknown = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "check",
                str(unknown_token_fixture),
                "-o",
                str(out_dir / f"{unknown_token_fixture.stem}-stage0.kcheck"),
            ],
            cwd=root,
        )
        stage1_unknown = run(
            [str(compiler), "check-source", str(unknown_token_fixture)],
            cwd=root,
        )
        stage0_unknown_output = (stage0_unknown.stdout or "") + (stage0_unknown.stderr or "")
        stage1_unknown_output = (stage1_unknown.stdout or "") + (stage1_unknown.stderr or "")
        require(
            stage0_unknown.returncode != 0
            and expected_title in stage0_unknown_output,
            f"stage0 unknown-token diagnostic changed: {unknown_token_fixture}",
            stage0_unknown,
        )
        require(
            stage1_unknown.returncode != 0
            and expected_title in stage1_unknown_output,
            f"stage1 silently accepted an unknown token: {unknown_token_fixture}",
            stage1_unknown,
        )
        stage0_unknown_count = sum(
            1 for line in stage0_unknown_output.splitlines() if line.startswith("[Parser]")
        )
        stage1_unknown_count = sum(
            1 for line in stage1_unknown_output.splitlines() if line.startswith("[Parser]")
        )
        require(
            stage0_unknown_count == stage1_unknown_count,
            f"unknown-token diagnostic count differs: {unknown_token_fixture}",
            stage1_unknown,
        )
    results.append(
        {"name": "unknown_token_diagnostics", "ok": True, "files": len(unknown_token_fixtures)}
    )

    builder_diagnostic_fixtures = [
        (
            root / "tests" / "common" / "error_builder_field_semicolon.kn",
            "Expected Semicolon",
        ),
        (
            root / "tests" / "common" / "error_builder_method_right_paren.kn",
            "Expected ')'",
        ),
    ]
    for builder_fixture, expected_title in builder_diagnostic_fixtures:
        stage0_builder = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "check",
                str(builder_fixture),
                "-o",
                str(out_dir / f"{builder_fixture.stem}-stage0.kcheck"),
            ],
            cwd=root,
        )
        stage1_builder = run(
            [str(compiler), "check-source", str(builder_fixture)],
            cwd=root,
        )
        stage0_builder_output = (stage0_builder.stdout or "") + (stage0_builder.stderr or "")
        stage1_builder_output = (stage1_builder.stdout or "") + (stage1_builder.stderr or "")
        require(
            stage0_builder.returncode != 0 and expected_title in stage0_builder_output,
            f"stage0 builder-diagnostic fixture changed: {builder_fixture}",
            stage0_builder,
        )
        require(
            stage1_builder.returncode != 0 and expected_title in stage1_builder_output,
            f"stage1 AST builder failed without a diagnostic: {builder_fixture}",
            stage1_builder,
        )
        stage0_builder_count = sum(
            1 for line in stage0_builder_output.splitlines() if line.startswith("[Parser]")
        )
        stage1_builder_count = sum(
            1 for line in stage1_builder_output.splitlines() if line.startswith("[Parser]")
        )
        require(
            stage0_builder_count == stage1_builder_count,
            f"builder diagnostic count differs: {builder_fixture}",
            stage1_builder,
        )
    results.append(
        {
            "name": "builder_failure_diagnostics",
            "ok": True,
            "files": len(builder_diagnostic_fixtures),
        }
    )

    single_pass_fixtures = [
        root / "tests" / "common" / "error_builder_field_semicolon.kn",
        root / "tests" / "common" / "error_builder_method_right_paren.kn",
        root / "tests" / "common" / "error_parser_recovery_multiple.kn",
        root / "tests" / "common" / "error_top_level_get_after_decl.kn",
        root / "tests" / "common" / "error_top_level_alias_after_decl.kn",
        root / "tests" / "common" / "error_meta_lowercase_on.kn",
        root / "tests" / "common" / "error_incomplete_block_body.kn",
    ]
    for single_pass_fixture in single_pass_fixtures:
        stage0_single_pass = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "check",
                str(single_pass_fixture),
                "-o",
                str(out_dir / f"{single_pass_fixture.stem}-single-pass-stage0.kcheck"),
            ],
            cwd=root,
        )
        stage1_parse = run([str(compiler), "parse", str(single_pass_fixture)], cwd=root)
        stage1_ast = run([str(compiler), "ast", str(single_pass_fixture)], cwd=root)
        stage1_check = run([str(compiler), "check-source", str(single_pass_fixture)], cwd=root)
        expected_titles = parser_diagnostic_titles(stage0_single_pass)
        require(expected_titles, f"stage0 emitted no Parser diagnostic: {single_pass_fixture}")
        for command_name, stage1_result in (
            ("parse", stage1_parse),
            ("ast", stage1_ast),
            ("check-source", stage1_check),
        ):
            require(
                stage1_result.returncode != 0,
                f"stage1 {command_name} accepted invalid source: {single_pass_fixture}",
                stage1_result,
            )
            require(
                parser_diagnostic_titles(stage1_result) == expected_titles,
                f"stage1 {command_name} Parser diagnostics differ from stage0: "
                f"{single_pass_fixture}; expected={expected_titles}, "
                f"actual={parser_diagnostic_titles(stage1_result)}",
                stage1_result,
            )
    results.append(
        {
            "name": "single_pass_parser_consistency",
            "ok": True,
            "files": len(single_pass_fixtures),
            "commands": 3,
        }
    )

    recovery_fixtures = [
        (
            root / "tests" / "common" / "error_parser_recovery_multiple.kn",
            "Expected Semicolon",
        ),
        (
            root / "tests" / "common" / "error_top_level_recovery_multiple.kn",
            "Unexpected Top-Level Token",
        ),
    ]
    for recovery_fixture, expected_title in recovery_fixtures:
        stage0_recovery = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "check",
                str(recovery_fixture),
                "-o",
                str(out_dir / f"{recovery_fixture.stem}-stage0.kcheck"),
            ],
            cwd=root,
        )
        stage1_recovery = run(
            [str(compiler), "check-source", str(recovery_fixture)],
            cwd=root,
        )
        stage0_recovery_output = (
            (stage0_recovery.stdout or "") + (stage0_recovery.stderr or "")
        )
        stage1_recovery_output = (
            (stage1_recovery.stdout or "") + (stage1_recovery.stderr or "")
        )
        require(
            stage0_recovery.returncode != 0
            and expected_title in stage0_recovery_output,
            f"stage0 panic-recovery fixture changed: {recovery_fixture}",
            stage0_recovery,
        )
        require(
            stage1_recovery.returncode != 0
            and expected_title in stage1_recovery_output,
            f"stage1 panic recovery failed: {recovery_fixture}",
            stage1_recovery,
        )
        stage0_recovery_count = sum(
            1
            for line in stage0_recovery_output.splitlines()
            if line.startswith("[Parser]")
        )
        stage1_recovery_count = sum(
            1
            for line in stage1_recovery_output.splitlines()
            if line.startswith("[Parser]")
        )
        require(
            stage0_recovery_count == 2,
            f"stage0 panic-recovery diagnostic count changed: {recovery_fixture}",
            stage0_recovery,
        )
        require(
            stage1_recovery_count == stage0_recovery_count,
            f"panic-recovery diagnostic count differs: {recovery_fixture}",
            stage1_recovery,
        )
    results.append(
        {
            "name": "parser_panic_recovery",
            "ok": True,
            "files": len(recovery_fixtures),
        }
    )

    nested_statements = "\n".join("        If (true)" for _ in range(193))
    statement_depth_fixtures = [
        out_dir / "error-statement-depth-validator.kn",
        out_dir / "error-statement-depth-builder.kn",
    ]
    statement_depth_fixtures[0].write_text(
        "Unit Tests.ErrorStatementDepthValidator;\n\n"
        "Static Function int Main()\n"
        "{\n"
        f"{nested_statements}\n"
        "        Return 0;\n"
        "}\n",
        encoding="utf-8",
    )
    statement_depth_fixtures[1].write_text(
        "Unit Tests.ErrorStatementDepthBuilder;\n\n"
        "Class Deep\n"
        "{\n"
        "    Function int Run()\n"
        "    {\n"
        f"{nested_statements}\n"
        "        Return 0;\n"
        "    }\n"
        "}\n\n"
        "Static Function int Main()\n"
        "{\n"
        "    Return 0;\n"
        "}\n",
        encoding="utf-8",
    )
    for statement_depth_fixture in statement_depth_fixtures:
        stage0_depth = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "check",
                str(statement_depth_fixture),
                "-o",
                str(out_dir / f"{statement_depth_fixture.stem}-stage0.kcheck"),
            ],
            cwd=root,
        )
        stage1_depth = run(
            [str(compiler), "check-source", str(statement_depth_fixture)],
            cwd=root,
        )
        stage0_depth_output = (stage0_depth.stdout or "") + (stage0_depth.stderr or "")
        stage1_depth_output = (stage1_depth.stdout or "") + (stage1_depth.stderr or "")
        require(
            stage0_depth.returncode != 0
            and "Statement Too Deep" in stage0_depth_output,
            f"stage0 statement-depth fixture changed: {statement_depth_fixture}",
            stage0_depth,
        )
        require(
            stage1_depth.returncode != 0
            and "Statement Too Deep" in stage1_depth_output,
            f"stage1 did not enforce statement depth: {statement_depth_fixture}",
            stage1_depth,
        )
        stage0_depth_count = sum(
            1 for line in stage0_depth_output.splitlines() if line.startswith("[Parser]")
        )
        stage1_depth_count = sum(
            1 for line in stage1_depth_output.splitlines() if line.startswith("[Parser]")
        )
        require(
            stage0_depth_count == 1,
            f"stage0 statement-depth diagnostic count changed: {statement_depth_fixture}",
            stage0_depth,
        )
        require(
            stage1_depth_count == stage0_depth_count,
            f"statement-depth diagnostic count differs: {statement_depth_fixture}",
            stage1_depth,
        )
    results.append(
        {
            "name": "statement_depth_limit",
            "ok": True,
            "limit": 192,
            "files": len(statement_depth_fixtures),
        }
    )

    phase5_diagnostic_fixtures = [
        ("error_meta_missing_keep.kn", "Parser", ("Missing Meta Clause",)),
        ("error_meta_lowercase_on.kn", "Parser", ("Invalid Meta Clause",)),
        ("error_meta_lowercase_keep.kn", "Parser", ("Invalid Meta Clause",)),
        ("error_meta_lowercase_repeatable.kn", "Parser", ("Invalid Meta Clause",)),
        ("error_meta_old_target_syntax.kn", "Parser", ("Invalid Meta Target",)),
        ("error_meta_old_keep_syntax.kn", "Parser", ("Invalid Meta Keep",)),
        ("error_attribute_on_meta.kn", "Parser", ("Invalid Attribute",)),
        ("error_attribute_on_global.kn", "Parser", ("Invalid Attribute",)),
        ("error_block_context_record.kn", "Parser", ("Unexpected Token", "Expected Semicolon")),
        ("error_block_context_jump.kn", "Parser", ("Unexpected Token", "Expected Semicolon")),
        ("error_meta_unknown_attribute.kn", "Sema", ("Unknown Attribute",)),
        ("error_meta_repeatable.kn", "Sema", ("Invalid Attribute",)),
        ("error_meta_wrong_target.kn", "Sema", ("Invalid Attribute",)),
        ("error_invalid_cast_method.kn", "Sema", ("Invalid Cast Method",)),
        ("error_cast_duplicate.kn", "Sema", ("Ambiguous Cast", "Invalid Cast")),
    ]
    for source_name, stage, expected_titles in phase5_diagnostic_fixtures:
        source = root / "tests" / "common" / source_name
        stage0_diagnostic = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "check",
                str(source),
                "-o",
                str(out_dir / f"{source.stem}-stage0.kcheck"),
            ],
            cwd=root,
        )
        stage1_diagnostic = run(
            [str(compiler), "check-source", str(source)],
            cwd=root,
        )
        stage0_output = (stage0_diagnostic.stdout or "") + (stage0_diagnostic.stderr or "")
        stage1_output = (stage1_diagnostic.stdout or "") + (stage1_diagnostic.stderr or "")
        require(
            stage0_diagnostic.returncode != 0,
            f"stage0 unexpectedly accepted phase-5 diagnostic fixture: {source}",
            stage0_diagnostic,
        )
        require(
            stage1_diagnostic.returncode != 0,
            f"stage1 unexpectedly accepted phase-5 diagnostic fixture: {source}",
            stage1_diagnostic,
        )
        for expected_title in expected_titles:
            require(
                expected_title in stage0_output and expected_title in stage1_output,
                f"phase-5 diagnostic title differs: {source}: {expected_title}",
                stage1_diagnostic,
            )
        stage0_count = sum(
            1 for line in stage0_output.splitlines() if line.startswith(f"[{stage}]")
        )
        stage1_count = sum(
            1 for line in stage1_output.splitlines() if line.startswith(f"[{stage}]")
        )
        require(
            stage0_count == stage1_count,
            f"phase-5 diagnostic count differs: {source}",
            stage1_diagnostic,
        )
    results.append(
        {
            "name": "meta_attribute_cast_scope_diagnostics",
            "ok": True,
            "files": len(phase5_diagnostic_fixtures),
        }
    )

    phase5_syntax_fixtures = [
        root / "tests" / "common" / "custom_cast_simple.kn",
        root / "tests" / "common" / "package_expression_depth.kn",
        root / "tests" / "common" / "nested_types_in_class.kn",
    ]
    for index, source in enumerate(phase5_syntax_fixtures):
        stage0_ast_path = out_dir / f"phase5-{index}-stage0.kast"
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
                str(stage0_ast_path),
            ],
            cwd=root,
        )
        stage1_ast = run([str(compiler), "ast", str(source)], cwd=root)
        require(stage0_ast.returncode == 0, f"stage0 phase-5 AST failed: {source}", stage0_ast)
        require(stage1_ast.returncode == 0, f"stage1 phase-5 AST failed: {source}", stage1_ast)
        require(
            stage0_ast_path.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
            == stage1_ast.stdout.replace("\r\n", "\n").strip(),
            f"stage0/stage1 phase-5 syntax mismatch: {source}",
        )
    results.append(
        {
            "name": "meta_attribute_cast_scope_syntax",
            "ok": True,
            "files": len(phase5_syntax_fixtures),
        }
    )

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
    token_source = source_root / "IO" / "Kinal" / "Compiler" / "Lex" / "Token.kn"
    token_text = token_source.read_text(encoding="utf-8")
    for declaration in (
        "Class Token",
        "Public TokenKind Kind;",
        "Public string Text;",
        "Public int Line;",
        "Public int Column;",
    ):
        require(declaration in token_text, f"structured Token declaration is missing {declaration!r}")
    require("Safe Function string Encode(" not in token_text,
            "serialized string Token.Encode ABI returned")
    require("Safe Function string Part(" not in token_text,
            "serialized string Token.Part ABI returned")
    require("Separator()" not in token_text,
            "serialized token separator returned")
    for source_file in source_files:
        require(
            "Token.Encode(" not in source_file.read_text(encoding="utf-8"),
            f"serialized Token.Encode call returned: {source_file}",
        )
    results.append({"name": "structured_token_boundary", "ok": True})

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

    stdlib_project = root / "tests" / "selfhost" / "fixtures" / "stdlib_core" / "kinal.knproj"
    stdlib_executable = out_dir / f"stdlib-core{executable_suffix}"
    stdlib_check = run([str(compiler), "check", str(stdlib_project), "test"], cwd=root)
    require(stdlib_check.returncode == 0, "stage1 standard-library source check failed", stdlib_check)
    stdlib_build = run(
        [str(compiler), "build", str(stdlib_project), str(stdlib_executable), "test"],
        cwd=root,
    )
    require(stdlib_build.returncode == 0, "stage1 standard-library fixture build failed", stdlib_build)
    stdlib_run = run([str(stdlib_executable)], cwd=root)
    require(stdlib_run.returncode == 0, "stage1 standard-library fixture execution failed", stdlib_run)
    require(
        stdlib_run.stdout.replace("\r\n", "\n").strip() == "R\n7\nkinal\ntrue",
        "stage1 standard-library fixture output differs",
        stdlib_run,
    )
    results.append({"name": "stdlib_source_package", "ok": True})

    oop_project = root / "tests" / "selfhost" / "fixtures" / "oop_inheritance" / "kinal.knproj"
    oop_executable = out_dir / f"oop-inheritance{executable_suffix}"
    oop_build = run(
        [str(compiler), "build", str(oop_project), str(oop_executable), "test"],
        cwd=root,
    )
    require(oop_build.returncode == 0, "stage1 inheritance fixture build failed", oop_build)
    oop_run = run([str(oop_executable)], cwd=root)
    require(oop_run.returncode == 0, "stage1 inheritance fixture execution failed", oop_run)
    require(
        oop_run.stdout.replace("\r\n", "\n").strip() == "5\nbase:derived",
        "stage1 inheritance fixture output differs",
        oop_run,
    )
    results.append({"name": "oop_inheritance", "ok": True})

    runtime_type_project = (
        root / "tests" / "selfhost" / "fixtures" / "runtime_type_checks" / "kinal.knproj"
    )
    runtime_type_executable = out_dir / f"runtime-type-checks{executable_suffix}"
    runtime_type_build = run(
        [
            str(compiler),
            "build",
            str(runtime_type_project),
            str(runtime_type_executable),
            "test",
        ],
        cwd=root,
    )
    require(
        runtime_type_build.returncode == 0,
        "stage1 runtime-type fixture build failed",
        runtime_type_build,
    )
    runtime_type_run = run([str(runtime_type_executable)], cwd=root)
    require(
        runtime_type_run.returncode == 0,
        "stage1 runtime-type fixture execution failed",
        runtime_type_run,
    )
    require(
        runtime_type_run.stdout.replace("\r\n", "\n").strip()
        == "true\ntrue\nfalse\ntrue\ntrue\ntrue\ntrue\nfalse\n"
           "false\nfalse\nfalse\ntrue\ntrue\nfalse\ntrue\nfalse\n"
           "true\ntrue\nfalse\n"
           "dog\ndog\nmiss\ndog\ndog\ntrue\nInvalid Cast\nInvalid Cast\n"
           "true\nfalse\ntrue\n1\n1\nInvalid Cast",
        "stage1 runtime-type fixture output differs",
        runtime_type_run,
    )
    results.append({"name": "runtime_type_checks", "ok": True})

    callable_identity_executable = out_dir / f"callable-identity{executable_suffix}"
    callable_identity_stage0_executable = (
        out_dir / f"callable-identity-stage0{executable_suffix}"
    )
    callable_identity_build = run(
        [
            str(compiler),
            "build",
            str(runtime_type_project),
            str(callable_identity_executable),
            "callable",
        ],
        cwd=root,
    )
    require(
        callable_identity_build.returncode == 0,
        "stage1 callable-identity fixture build failed",
        callable_identity_build,
    )
    callable_identity_stage0_build = run(
        [
            str(stage0),
            "build",
            "--project",
            str(runtime_type_project.parent),
            "--profile",
            "callable",
            "-o",
            str(callable_identity_stage0_executable),
        ],
        cwd=root,
    )
    require(
        callable_identity_stage0_build.returncode == 0,
        "stage0 callable-identity fixture build failed",
        callable_identity_stage0_build,
    )
    callable_identity_run = run([str(callable_identity_executable)], cwd=root)
    callable_identity_stage0_run = run(
        [str(callable_identity_stage0_executable)], cwd=root
    )
    callable_identity_expected = (
        "true\nfalse\ntrue\nfalse\ntrue\nfalse\ntrue\nfalse\n"
        "true\nfalse\ntrue\nfalse\ntrue\nfalse\ntrue\ntrue\n"
        "false\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\n"
        "Invalid Cast\nInvalid Cast"
    )
    require(
        callable_identity_run.returncode == 0
        and callable_identity_stage0_run.returncode == 0
        and callable_identity_run.stdout.replace("\r\n", "\n").strip()
        == callable_identity_expected
        and callable_identity_stage0_run.stdout.replace("\r\n", "\n").strip()
        == callable_identity_expected,
        "stage0/stage1 callable runtime identity differs",
        callable_identity_run,
    )
    results.append({"name": "callable_runtime_identity", "ok": True})

    multiple_interfaces_executable = out_dir / f"multiple-interfaces{executable_suffix}"
    multiple_interfaces_stage0_executable = (
        out_dir / f"multiple-interfaces-stage0{executable_suffix}"
    )
    multiple_interfaces_build = run(
        [
            str(compiler),
            "build",
            str(runtime_type_project),
            str(multiple_interfaces_executable),
            "interfaces",
        ],
        cwd=root,
    )
    multiple_interfaces_stage0_build = run(
        [
            str(stage0),
            "build",
            "--project",
            str(runtime_type_project.parent),
            "--profile",
            "interfaces",
            "-o",
            str(multiple_interfaces_stage0_executable),
        ],
        cwd=root,
    )
    require(
        multiple_interfaces_build.returncode == 0,
        "stage1 multiple-interface fixture build failed",
        multiple_interfaces_build,
    )
    require(
        multiple_interfaces_stage0_build.returncode == 0,
        "stage0 multiple-interface fixture build failed",
        multiple_interfaces_stage0_build,
    )
    multiple_interfaces_run = run([str(multiple_interfaces_executable)], cwd=root)
    multiple_interfaces_stage0_run = run(
        [str(multiple_interfaces_stage0_executable)], cwd=root
    )
    multiple_interfaces_expected = (
        "dog\n5\nwoof\n7\n10\ndog\nwoof\n9\nwoof\n"
        "true\ntrue\ntrue\ntrue\ntrue\ntrue\nInvalid Cast"
    )
    require(
        multiple_interfaces_run.returncode == 0
        and multiple_interfaces_stage0_run.returncode == 0
        and multiple_interfaces_run.stdout.replace("\r\n", "\n").strip()
        == multiple_interfaces_expected
        and multiple_interfaces_stage0_run.stdout.replace("\r\n", "\n").strip()
        == multiple_interfaces_expected,
        "stage0/stage1 multiple-interface behavior differs",
        multiple_interfaces_run,
    )
    results.append({"name": "multiple_interface_runtime", "ok": True})

    interface_diagnostic_cases = (
        ("ErrorMissingInterfaceMethod.kn", ["Sema:Interface Method"]),
        ("ErrorPrivateInterfaceMethod.kn", ["Sema:Interface Method"]),
        ("ErrorDuplicateInterface.kn", ["Sema:Duplicate Interface"]),
        ("ErrorClassInInterfaceList.kn", ["Sema:Unknown Interface"]),
        ("ErrorInvalidFirstBase.kn", ["Sema:Unknown Base"]),
        ("ErrorInheritedInterfaceMethod.kn", ["Sema:Interface Method"]),
    )
    interface_source_dir = runtime_type_project.parent / "src"
    for source_name, expected_titles in interface_diagnostic_cases:
        source = interface_source_dir / source_name
        stage1_diagnostic = run(
            [str(compiler), "check-source", str(source)],
            cwd=root,
        )
        stage0_diagnostic = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "check",
                str(source),
                "-o",
                str(out_dir / f"{source.stem}-stage0.kcheck"),
            ],
            cwd=root,
        )
        require(
            stage1_diagnostic.returncode != 0,
            f"stage1 accepted {source_name}",
            stage1_diagnostic,
        )
        require(
            stage0_diagnostic.returncode != 0,
            f"stage0 accepted {source_name}",
            stage0_diagnostic,
        )
        require(
            compiler_diagnostic_titles(stage1_diagnostic) == expected_titles
            and compiler_diagnostic_titles(stage0_diagnostic) == expected_titles,
            f"stage0/stage1 interface diagnostics differ for {source_name}: "
            f"stage0={compiler_diagnostic_titles(stage0_diagnostic)} "
            f"stage1={compiler_diagnostic_titles(stage1_diagnostic)}",
            stage1_diagnostic,
        )
    results.append(
        {
            "name": "multiple_interface_diagnostics",
            "ok": True,
            "files": len(interface_diagnostic_cases),
        }
    )

    exceptions_project = (
        root / "tests" / "selfhost" / "fixtures" / "exceptions" / "kinal.knproj"
    )
    exceptions_executable = out_dir / f"exceptions{executable_suffix}"
    exceptions_build = run(
        [
            str(compiler),
            "build",
            str(exceptions_project),
            str(exceptions_executable),
            "test",
        ],
        cwd=root,
    )
    require(
        exceptions_build.returncode == 0,
        "stage1 exception fixture build failed",
        exceptions_build,
    )
    exceptions_run = run([str(exceptions_executable)], cwd=root)
    require(
        exceptions_run.returncode == 0,
        "stage1 exception fixture execution failed",
        exceptions_run,
    )
    require(
        exceptions_run.stdout.replace("\r\n", "\n").strip()
        == "boom\nTitle\nMessage\nTitle\nMessage\nInnerT\nOuterT\nOuterM\n"
           "InnerT\nInnerM\nfalse\ntrue\ntrue",
        "stage1 exception fixture output differs",
        exceptions_run,
    )
    results.append({"name": "exceptions", "ok": True})

    unhandled_executable = out_dir / f"unhandled-exception{executable_suffix}"
    unhandled_build = run(
        [
            str(compiler),
            "build",
            str(exceptions_project),
            str(unhandled_executable),
            "unhandled",
        ],
        cwd=root,
    )
    require(
        unhandled_build.returncode == 0,
        "stage1 unhandled-exception fixture build failed",
        unhandled_build,
    )
    unhandled_run = run([str(unhandled_executable)], cwd=root)
    require(
        unhandled_run.returncode == 1,
        "stage1 unhandled exception must return exit code 1",
        unhandled_run,
    )
    require(
        unhandled_run.stdout.replace("\r\n", "\n").strip()
        == "bad\nTests.Selfhost.Exceptions.Unhandled.Fail -> "
           "Tests.Selfhost.Exceptions.Unhandled.Pick -> "
           "Tests.Selfhost.Exceptions.Unhandled.Main",
        "stage1 unhandled-exception output differs",
        unhandled_run,
    )
    results.append({"name": "unhandled_exception", "ok": True})

    nested_project = root / "tests" / "selfhost" / "fixtures" / "nested_types" / "kinal.knproj"
    nested_executable = out_dir / f"nested-types{executable_suffix}"
    nested_build = run(
        [str(compiler), "build", str(nested_project), str(nested_executable), "test"],
        cwd=root,
    )
    require(nested_build.returncode == 0, "stage1 nested-type fixture build failed", nested_build)
    nested_run = run([str(nested_executable)], cwd=root)
    require(nested_run.returncode == 0, "stage1 nested-type fixture execution failed", nested_run)
    require(
        nested_run.stdout.replace("\r\n", "\n").strip() == "7",
        "stage1 nested-type fixture output differs",
        nested_run,
    )
    results.append({"name": "nested_types", "ok": True})

    closure_project = root / "tests" / "selfhost" / "fixtures" / "function_closures" / "kinal.knproj"
    closure_executable = out_dir / f"function-closures{executable_suffix}"
    closure_build = run(
        [str(compiler), "build", str(closure_project), str(closure_executable), "test"],
        cwd=root,
    )
    require(closure_build.returncode == 0, "stage1 closure fixture build failed", closure_build)
    closure_run = run([str(closure_executable)], cwd=root)
    require(closure_run.returncode == 0, "stage1 closure fixture execution failed", closure_run)
    require(
        closure_run.stdout.replace("\r\n", "\n").strip() == "5\n25\n15\n21\n11\n13",
        "stage1 closure fixture output differs",
        closure_run,
    )
    results.append({"name": "function_closures", "ok": True})

    package_project = root / "tests" / "selfhost" / "fixtures" / "package_values" / "kinal.knproj"
    package_executable = out_dir / f"package-values{executable_suffix}"
    package_build = run(
        [str(compiler), "build", str(package_project), str(package_executable), "test"],
        cwd=root,
    )
    require(package_build.returncode == 0, "stage1 package-value fixture build failed", package_build)
    package_run = run([str(package_executable)], cwd=root)
    require(package_run.returncode == 0, "stage1 package-value fixture execution failed", package_run)
    require(
        package_run.stdout.replace("\r\n", "\n").strip() ==
        "200\n200\nOK\nOK\n3\n1\n200\n2\nb\n2\n200\n999\n3\n3\n201",
        "stage1 package-value fixture output differs",
        package_run,
    )
    results.append({"name": "package_values", "ok": True})

    package_errors = (
        ("error_package_index_runtime.kn", "[Sema] Invalid Package Index"),
        ("error_package_index_bool_constant.kn", "[Sema] Type Mismatch"),
        ("error_package_index_large_constant.kn", "[Sema] Invalid Package Index"),
        ("error_local_const_assignment.kn", "[Sema] Const Assignment"),
        ("error_local_const_missing_init.kn", "[Sema] Const Requires Init"),
        ("error_package_field_count.kn", "[Sema] Package Field Count"),
        ("error_package_array_cast.kn", "[Sema] Invalid Cast"),
    )
    for source_name, expected_error in package_errors:
        source = root / "tests" / "common" / source_name
        package_error = run(
            [str(compiler), "check-source", str(source)],
            cwd=root,
        )
        require(package_error.returncode != 0, f"stage1 accepted {source_name}", package_error)
        require(
            expected_error in (package_error.stdout + package_error.stderr),
            f"stage1 diagnostic differs for {source_name}",
            package_error,
        )
        stage0_package_error = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "check",
                str(source),
                "-o",
                str(out_dir / f"{source.stem}-stage0.kcheck"),
            ],
            cwd=root,
        )
        require(
            stage0_package_error.returncode != 0,
            f"stage0 accepted {source_name}",
            stage0_package_error,
        )
        require(
            compiler_diagnostic_titles(package_error)
            == compiler_diagnostic_titles(stage0_package_error),
            f"stage0/stage1 diagnostic sequence differs for {source_name}: "
            f"stage0={compiler_diagnostic_titles(stage0_package_error)} "
            f"stage1={compiler_diagnostic_titles(package_error)}",
            package_error,
        )
    results.append({"name": "package_diagnostics", "ok": True})

    block_project = root / "tests" / "selfhost" / "fixtures" / "block_features" / "kinal.knproj"
    block_executable = out_dir / f"block-features{executable_suffix}"
    block_build = run(
        [str(compiler), "build", str(block_project), str(block_executable), "test"],
        cwd=root,
    )
    require(block_build.returncode == 0, "stage1 Block fixture build failed", block_build)
    block_run = run([str(block_executable)], cwd=root)
    require(block_run.returncode == 0, "stage1 Block fixture execution failed", block_run)
    require(
        block_run.stdout.replace("\r\n", "\n").strip() ==
        "jump-3\nflow-1\nflow-2\nflow-3\nflow-2\n2\nflow-2\nafter\n7",
        "stage1 Block fixture output differs",
        block_run,
    )
    block_stage0_executable = out_dir / f"block-features-stage0{executable_suffix}"
    block_stage0_build = run(
        [
            str(stage0),
            "build",
            "--project",
            str(block_project.parent),
            "--profile",
            "test",
            "-o",
            str(block_stage0_executable),
        ],
        cwd=root,
    )
    require(block_stage0_build.returncode == 0, "stage0 Block fixture build failed", block_stage0_build)
    block_stage0_run = run([str(block_stage0_executable)], cwd=root)
    require(
        block_stage0_run.returncode == 0
        and block_stage0_run.stdout.replace("\r\n", "\n")
        == block_run.stdout.replace("\r\n", "\n"),
        "stage0/stage1 Block/local-declaration behavior differs",
        block_stage0_run,
    )
    results.append({"name": "block_features", "ok": True})

    struct_project = root / "tests" / "selfhost" / "fixtures" / "struct_values" / "kinal.knproj"
    struct_executable = out_dir / f"struct-values{executable_suffix}"
    struct_stage0_executable = out_dir / f"struct-values-stage0{executable_suffix}"
    struct_build = run(
        [str(compiler), "build", str(struct_project), str(struct_executable), "test"],
        cwd=root,
    )
    require(struct_build.returncode == 0, "stage1 Struct value fixture build failed", struct_build)
    struct_stage0_build = run(
        [
            str(stage0),
            "build",
            "--project",
            str(struct_project.parent),
            "--profile",
            "test",
            "-o",
            str(struct_stage0_executable),
        ],
        cwd=root,
    )
    require(
        struct_stage0_build.returncode == 0,
        "stage0 Struct value fixture build failed",
        struct_stage0_build,
    )
    struct_run = run([str(struct_executable)], cwd=root)
    struct_stage0_run = run([str(struct_stage0_executable)], cwd=root)
    struct_expected = (
        "0\n0\n0\n10\n99\n20\n77\n87\n10\n55\nnested\n20\n"
        "3\n4\n11\n10\n44\n10\n7\ntrue\n7\n2\n31\n41\n3\n32\n"
        "88\n20\n10"
    )
    require(
        struct_run.returncode == 0
        and struct_stage0_run.returncode == 0
        and struct_run.stdout.replace("\r\n", "\n").strip() == struct_expected
        and struct_stage0_run.stdout.replace("\r\n", "\n").strip() == struct_expected,
        "stage0/stage1 Struct value behavior differs",
        struct_run,
    )
    results.append({"name": "struct_value_runtime", "ok": True})

    struct_stage1_ir = out_dir / "struct-values-stage1.ll"
    struct_stage0_ir = out_dir / "struct-values-stage0.ll"
    struct_ir_build = run(
        [str(compiler), "build-ir", str(struct_project), str(struct_stage1_ir), "test"],
        cwd=root,
    )
    struct_stage0_ir_build = run(
        [
            str(stage0),
            "build",
            "--project",
            str(struct_project.parent),
            "--profile",
            "test",
            "--emit",
            "ir",
            "-o",
            str(struct_stage0_ir),
        ],
        cwd=root,
    )
    require(struct_ir_build.returncode == 0, "stage1 Struct IR build failed", struct_ir_build)
    require(
        struct_stage0_ir_build.returncode == 0,
        "stage0 Struct IR build failed",
        struct_stage0_ir_build,
    )
    stage1_struct_ir = struct_stage1_ir.read_text(encoding="utf-8")
    stage0_struct_ir = struct_stage0_ir.read_text(encoding="utf-8")
    struct_layout_markers = (
        "%Tests.Selfhost.StructValues.Pair = type { i64, i64 }",
        "%Tests.Selfhost.StructValues.PackedPair = type <{ i8, i64 }>",
        "%Tests.Selfhost.StructValues.AlignedPair = type { i8, i64 }",
        "alloca %Tests.Selfhost.StructValues.AlignedPair, align 32",
    )
    require(
        all(marker in stage1_struct_ir and marker in stage0_struct_ir
            for marker in struct_layout_markers),
        "stage0/stage1 Struct LLVM layouts differ",
    )
    results.append({"name": "struct_layout_ir", "ok": True})

    struct_diagnostic_cases = (
        ("ErrorInvalidAlign.kn", ["Sema:Invalid Align"]),
        ("ErrorInvalidEnumUnderlying.kn", ["Sema:Invalid Enum"]),
        ("ErrorUnknownStructSpecifier.kn", ["Parser:Unknown Specifier"]),
        ("ErrorNewStruct.kn", ["Sema:Unknown Type", "Sema:Type Mismatch"]),
        ("ErrorNullStruct.kn", ["Sema:Type Mismatch"]),
    )
    struct_source_dir = struct_project.parent / "src"
    for source_name, expected_titles in struct_diagnostic_cases:
        source = struct_source_dir / source_name
        stage1_diagnostic = run(
            [str(compiler), "check-source", str(source)],
            cwd=root,
        )
        stage0_diagnostic = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "check",
                str(source),
                "-o",
                str(out_dir / f"{source.stem}-stage0.kcheck"),
            ],
            cwd=root,
        )
        require(stage1_diagnostic.returncode != 0,
                f"stage1 accepted {source_name}", stage1_diagnostic)
        require(stage0_diagnostic.returncode != 0,
                f"stage0 accepted {source_name}", stage0_diagnostic)
        require(
            compiler_diagnostic_titles(stage1_diagnostic) == expected_titles
            and compiler_diagnostic_titles(stage0_diagnostic) == expected_titles,
            f"stage0/stage1 Struct diagnostics differ for {source_name}: "
            f"stage0={compiler_diagnostic_titles(stage0_diagnostic)} "
            f"stage1={compiler_diagnostic_titles(stage1_diagnostic)}",
            stage1_diagnostic,
        )
    results.append(
        {
            "name": "struct_diagnostics",
            "ok": True,
            "files": len(struct_diagnostic_cases),
        }
    )

    collection_project = (
        root / "tests" / "selfhost" / "fixtures" / "collection_runtime" / "kinal.knproj"
    )
    collection_executable = out_dir / f"collection-runtime{executable_suffix}"
    collection_stage0_executable = out_dir / f"collection-runtime-stage0{executable_suffix}"
    collection_build = run(
        [str(compiler), "build", str(collection_project), str(collection_executable), "test"],
        cwd=root,
    )
    require(collection_build.returncode == 0,
            "stage1 collection-runtime fixture build failed", collection_build)
    collection_stage0_build = run(
        [
            str(stage0),
            "build",
            "--project",
            str(collection_project.parent),
            "--profile",
            "test",
            "-o",
            str(collection_stage0_executable),
        ],
        cwd=root,
    )
    require(collection_stage0_build.returncode == 0,
            "stage0 collection-runtime fixture build failed", collection_stage0_build)
    collection_run = run([str(collection_executable)], cwd=root)
    collection_stage0_run = run([str(collection_stage0_executable)], cwd=root)
    collection_expected = (
        "true\n4\n4\nRED\nblue\nGREEN\nfallback\ntrue\nfalse\n2\n-1\ntail\n"
        "blue\ntail\n2\ntrue\n3\n3\nkinal\n8\nfallback\n8\ntrue\nfalse\n3\n"
        "true\n3\ntrue\ntrue\nfalse\ntrue\n2\n2\ntrue\nfalse\n2\ntrue\ntrue\n"
        "false\ntrue"
    )
    require(
        collection_run.returncode == 0
        and collection_stage0_run.returncode == 0
        and collection_run.stdout.replace("\r\n", "\n").strip() == collection_expected
        and collection_stage0_run.stdout.replace("\r\n", "\n").strip()
        == collection_expected,
        "stage0/stage1 collection-runtime behavior differs",
        collection_run,
    )
    results.append({"name": "collection_runtime", "ok": True})

    ffi_arrays_project = (
        root / "tests" / "selfhost" / "fixtures" / "ffi_arrays" / "kinal.knproj"
    )
    ffi_arrays_executable = out_dir / f"ffi-arrays{executable_suffix}"
    ffi_arrays_stage0_executable = out_dir / f"ffi-arrays-stage0{executable_suffix}"
    ffi_arrays_build = run(
        [str(compiler), "build", str(ffi_arrays_project), str(ffi_arrays_executable), "test"],
        cwd=root,
    )
    require(ffi_arrays_build.returncode == 0,
            "stage1 FFI-array fixture build failed", ffi_arrays_build)
    ffi_arrays_stage0_build = run(
        [
            str(stage0),
            "build",
            "--project",
            str(ffi_arrays_project.parent),
            "--profile",
            "test",
            "-o",
            str(ffi_arrays_stage0_executable),
        ],
        cwd=root,
    )
    require(ffi_arrays_stage0_build.returncode == 0,
            "stage0 FFI-array fixture build failed", ffi_arrays_stage0_build)
    ffi_arrays_run = run([str(ffi_arrays_executable)], cwd=root)
    ffi_arrays_stage0_run = run([str(ffi_arrays_stage0_executable)], cwd=root)
    ffi_arrays_expected = "3\n5\n3\n5\n7\n8\n9\n0\n22\n3"
    require(
        ffi_arrays_run.returncode == 0
        and ffi_arrays_stage0_run.returncode == 0
        and ffi_arrays_run.stdout.replace("\r\n", "\n").strip() == ffi_arrays_expected
        and ffi_arrays_stage0_run.stdout.replace("\r\n", "\n").strip()
        == ffi_arrays_expected,
        "stage0/stage1 FFI-array behavior differs",
        ffi_arrays_run,
    )
    results.append({"name": "ffi_array_abi", "ok": True})

    async_runtime_cases = [
        ("async_runtime", "42\nasync-error"),
        ("async_main", "async-main"),
    ]
    for async_name, async_expected in async_runtime_cases:
        async_project = (
            root / "tests" / "selfhost" / "fixtures" / async_name / "kinal.knproj"
        )
        async_executable = out_dir / f"{async_name}{executable_suffix}"
        async_stage0_executable = out_dir / f"{async_name}-stage0{executable_suffix}"
        async_build = run(
            [str(compiler), "build", str(async_project), str(async_executable), "test"],
            cwd=root,
        )
        require(async_build.returncode == 0,
                f"stage1 {async_name} fixture build failed", async_build)
        async_stage0_build = run(
            [
                str(stage0),
                "build",
                "--project",
                str(async_project.parent),
                "--profile",
                "test",
                "-o",
                str(async_stage0_executable),
            ],
            cwd=root,
        )
        require(async_stage0_build.returncode == 0,
                f"stage0 {async_name} fixture build failed", async_stage0_build)
        async_run = run([str(async_executable)], cwd=root)
        async_stage0_run = run([str(async_stage0_executable)], cwd=root)
        require(
            async_run.returncode == 0
            and async_stage0_run.returncode == 0
            and async_run.stdout.replace("\r\n", "\n").strip() == async_expected
            and async_stage0_run.stdout.replace("\r\n", "\n").strip()
            == async_expected,
            f"stage0/stage1 {async_name} behavior differs",
            async_run,
        )

    async_diagnostic_cases = [
        "async_await_outside.kn",
        "async_await_in_try.kn",
        "async_await_non_task.kn",
    ]
    for async_source_name in async_diagnostic_cases:
        async_source = (
            root / "tests" / "selfhost" / "fixtures" / async_source_name
        )
        async_stage0_diagnostic = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "check",
                str(async_source),
                "-o",
                str(out_dir / f"{async_source.stem}-stage0.kcheck"),
            ],
            cwd=root,
        )
        async_stage1_diagnostic = run(
            [str(compiler), "check-source", str(async_source)], cwd=root
        )
        require(
            async_stage0_diagnostic.returncode != 0
            and async_stage1_diagnostic.returncode != 0
            and compiler_diagnostic_titles(async_stage0_diagnostic)
            == compiler_diagnostic_titles(async_stage1_diagnostic),
            f"stage0/stage1 async diagnostics differ for {async_source_name}: "
            f"stage0={compiler_diagnostic_titles(async_stage0_diagnostic)} "
            f"stage1={compiler_diagnostic_titles(async_stage1_diagnostic)}",
            async_stage1_diagnostic,
        )
    results.append(
        {
            "name": "async_runtime",
            "ok": True,
            "runtime_cases": len(async_runtime_cases),
            "diagnostic_cases": len(async_diagnostic_cases),
        }
    )

    global_constants_project = (
        root / "tests" / "selfhost" / "fixtures" / "global_constants" / "kinal.knproj"
    )
    global_constants_executable = out_dir / f"global-constants{executable_suffix}"
    global_constants_stage0_executable = out_dir / f"global-constants-stage0{executable_suffix}"
    global_constants_build = run(
        [str(compiler), "build", str(global_constants_project),
         str(global_constants_executable), "native"],
        cwd=root,
    )
    require(global_constants_build.returncode == 0,
            "stage1 global-constants fixture build failed", global_constants_build)
    global_constants_stage0_build = run(
        [
            str(stage0),
            "build",
            "--project",
            str(global_constants_project.parent),
            "--profile",
            "native",
            "-o",
            str(global_constants_stage0_executable),
        ],
        cwd=root,
    )
    require(global_constants_stage0_build.returncode == 0,
            "stage0 global-constants fixture build failed", global_constants_stage0_build)
    global_constants_run = run([str(global_constants_executable)], cwd=root)
    global_constants_stage0_run = run([str(global_constants_stage0_executable)], cwd=root)
    global_constants_expected = "16384\nalpha\nbeta\nfixed 4 42 127\n41\n85"
    require(
        global_constants_run.returncode == 0
        and global_constants_stage0_run.returncode == 0
        and global_constants_run.stdout.replace("\r\n", "\n").strip()
        == global_constants_expected
        and global_constants_stage0_run.stdout.replace("\r\n", "\n").strip()
        == global_constants_expected,
        "stage0/stage1 global constants and fixed arrays behavior differs",
        global_constants_run,
    )
    results.append({"name": "global_constants_fixed_arrays", "ok": True})

    phase5_project = (
        root / "tests" / "selfhost" / "fixtures" / "phase5_semantics" / "kinal.knproj"
    )
    phase5_profiles = [
        ("custom_cast", "custom-cast-simple", "42\n"),
        ("package_expression", "package-expression-depth", "true\nfalse\n14\n"),
        (
            "package_contextual_default",
            "package-contextual-default",
            "0\ntrue\nfalse\n0\ntrue\nfalse\n",
        ),
        (
            "package_const_index",
            "package-const-index",
            "10\n20\n30\n40\n20\n40\n30\n30\n30\n20\n20\n30\n30\n",
        ),
        ("literal_ids", "literal-ids", "42\n"),
    ]
    for profile, output_name, expected_output in phase5_profiles:
        stage1_executable = out_dir / f"{output_name}{executable_suffix}"
        stage0_executable = out_dir / f"{output_name}-stage0{executable_suffix}"
        stage1_build = run(
            [str(compiler), "build", str(phase5_project), str(stage1_executable), profile],
            cwd=root,
        )
        stage0_build = run(
            [
                str(stage0),
                "build",
                "--project",
                str(phase5_project.parent),
                "--profile",
                profile,
                "-o",
                str(stage0_executable),
            ],
            cwd=root,
        )
        require(stage1_build.returncode == 0, f"stage1 {profile} build failed", stage1_build)
        require(stage0_build.returncode == 0, f"stage0 {profile} build failed", stage0_build)
        stage1_run = run([str(stage1_executable)], cwd=root)
        stage0_run = run([str(stage0_executable)], cwd=root)
        require(stage1_run.returncode == 0, f"stage1 {profile} execution failed", stage1_run)
        require(stage0_run.returncode == 0, f"stage0 {profile} execution failed", stage0_run)
        require(
            stage1_run.stdout.replace("\r\n", "\n") == expected_output
            and stage0_run.stdout.replace("\r\n", "\n")
            == stage1_run.stdout.replace("\r\n", "\n"),
            f"stage0/stage1 {profile} behavior differs",
            stage1_run,
        )
    results.append(
        {
            "name": "custom_cast_and_package_expression_runtime",
            "ok": True,
            "profiles": len(phase5_profiles),
        }
    )

    globals_project = root / "tests" / "selfhost" / "fixtures" / "global_variables" / "kinal.knproj"
    globals_executable = out_dir / f"global-variables{executable_suffix}"
    globals_build = run(
        [str(compiler), "build", str(globals_project), str(globals_executable), "test"],
        cwd=root,
    )
    require(globals_build.returncode == 0, "stage1 global-variable fixture build failed", globals_build)
    require(globals_executable.is_file(), "stage1 global-variable fixture executable is missing")
    globals_run = run([str(globals_executable)], cwd=root)
    require(globals_run.returncode == 0, "stage1 global-variable fixture execution failed", globals_run)
    require(
        globals_run.stdout.replace("\r\n", "\n").strip() == "9",
        "stage1 global-variable fixture output differs",
        globals_run,
    )
    globals_stage0_ast_path = out_dir / "globals-stage0.kast"
    globals_stage0_ast = run(
        [
            str(stage0),
            "build",
            "--no-module-discovery",
            "--color",
            "never",
            "--emit",
            "ast",
            str(globals_project.parent / "src" / "Main.kn"),
            "-o",
            str(globals_stage0_ast_path),
        ],
        cwd=root,
    )
    require(globals_stage0_ast.returncode == 0, "stage0 global-variable syntax emit failed", globals_stage0_ast)
    globals_stage1_ast = run(
        [str(compiler), "ast", str(globals_project.parent / "src" / "Main.kn")],
        cwd=root,
    )
    require(globals_stage1_ast.returncode == 0, "stage1 global-variable syntax build failed", globals_stage1_ast)
    require(
        globals_stage0_ast_path.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
        == globals_stage1_ast.stdout.replace("\r\n", "\n").strip(),
        "stage0/stage1 global-variable syntax mismatch",
    )
    results.append({"name": "global_variables", "ok": True})

    imports_project = root / "tests" / "selfhost" / "fixtures" / "selective_imports" / "kinal.knproj"
    imports_executable = out_dir / f"selective-imports{executable_suffix}"
    imports_build = run(
        [str(compiler), "build", str(imports_project), str(imports_executable), "test"],
        cwd=root,
    )
    require(imports_build.returncode == 0, "stage1 selective-import fixture build failed", imports_build)
    require(imports_executable.is_file(), "stage1 selective-import fixture executable is missing")
    imports_run = run([str(imports_executable)], cwd=root)
    require(imports_run.returncode == 0, "stage1 selective-import fixture execution failed", imports_run)
    require(
        imports_run.stdout.replace("\r\n", "\n").strip() == "selective-ok",
        "stage1 selective-import fixture output differs",
        imports_run,
    )
    results.append({"name": "selective_imports", "ok": True})

    static_class_project = root / "tests" / "selfhost" / "fixtures" / "static_class" / "kinal.knproj"
    static_class_executable = out_dir / f"static-class{executable_suffix}"
    static_class_build = run(
        [str(compiler), "build", str(static_class_project), str(static_class_executable), "test"],
        cwd=root,
    )
    require(static_class_build.returncode == 0, "stage1 static-class fixture build failed", static_class_build)
    require(static_class_executable.is_file(), "stage1 static-class fixture executable is missing")
    static_class_run = run([str(static_class_executable)], cwd=root)
    require(static_class_run.returncode == 0, "stage1 static-class fixture execution failed", static_class_run)
    require(
        static_class_run.stdout.replace("\r\n", "\n").strip() == "12",
        "stage1 static-class fixture output differs",
        static_class_run,
    )
    results.append({"name": "static_class", "ok": True})

    switch_statement_project = root / "tests" / "selfhost" / "fixtures" / "switch_statement" / "kinal.knproj"
    switch_statement_executable = out_dir / f"switch-statement{executable_suffix}"
    switch_statement_build = run(
        [str(compiler), "build", str(switch_statement_project), str(switch_statement_executable), "test"],
        cwd=root,
    )
    require(switch_statement_build.returncode == 0, "stage1 switch-statement fixture build failed", switch_statement_build)
    require(switch_statement_executable.is_file(), "stage1 switch-statement fixture executable is missing")
    switch_statement_run = run([str(switch_statement_executable)], cwd=root)
    require(switch_statement_run.returncode == 0, "stage1 switch-statement fixture execution failed", switch_statement_run)
    require(
        switch_statement_run.stdout.replace("\r\n", "\n").strip() == "two",
        "stage1 switch-statement fixture output differs",
        switch_statement_run,
    )
    results.append({"name": "switch_statement", "ok": True})

    language_foundations_project = (
        root / "tests" / "selfhost" / "fixtures" / "language_foundations" / "kinal.knproj"
    )
    language_foundations_executable = out_dir / f"language-foundations{executable_suffix}"
    language_foundations_build = run(
        [
            str(compiler),
            "build",
            str(language_foundations_project),
            str(language_foundations_executable),
            "test",
        ],
        cwd=root,
    )
    require(
        language_foundations_build.returncode == 0,
        "stage1 language-foundations fixture build failed",
        language_foundations_build,
    )
    language_foundations_run = run([str(language_foundations_executable)], cwd=root)
    require(
        language_foundations_run.returncode == 0 and
        language_foundations_run.stdout.replace("\r\n", "\n") == "ok\nok\n336\n",
        "stage1 language-foundations fixture output differs",
        language_foundations_run,
    )
    results.append({"name": "language_foundations", "ok": True})

    implicit_unit_project = root / "tests" / "selfhost" / "fixtures" / "implicit_unit" / "kinal.knproj"
    implicit_unit_executable = out_dir / f"implicit-unit{executable_suffix}"
    implicit_unit_build = run(
        [str(compiler), "build", str(implicit_unit_project), str(implicit_unit_executable), "test"],
        cwd=root,
    )
    require(implicit_unit_build.returncode == 0, "stage1 implicit-unit fixture build failed", implicit_unit_build)
    require(implicit_unit_executable.is_file(), "stage1 implicit-unit fixture executable is missing")
    implicit_unit_run = run([str(implicit_unit_executable)], cwd=root)
    require(implicit_unit_run.returncode == 0, "stage1 implicit-unit fixture execution failed", implicit_unit_run)
    require(
        implicit_unit_run.stdout.replace("\r\n", "\n").strip() == "42",
        "stage1 implicit-unit fixture output differs",
        implicit_unit_run,
    )
    results.append({"name": "implicit_unit", "ok": True})

    same_unit_project = root / "tests" / "selfhost" / "fixtures" / "same_unit" / "kinal.knproj"
    same_unit_executable = out_dir / f"same-unit{executable_suffix}"
    same_unit_build = run(
        [str(compiler), "build", str(same_unit_project), str(same_unit_executable), "test"],
        cwd=root,
    )
    require(same_unit_build.returncode == 0, "stage1 same-unit fixture build failed", same_unit_build)
    require(same_unit_executable.is_file(), "stage1 same-unit fixture executable is missing")
    same_unit_run = run([str(same_unit_executable)], cwd=root)
    require(same_unit_run.returncode == 0, "stage1 same-unit fixture execution failed", same_unit_run)
    require(
        same_unit_run.stdout.replace("\r\n", "\n").strip() == "same-unit",
        "stage1 same-unit fixture output differs",
        same_unit_run,
    )
    results.append({"name": "same_unit", "ok": True})

    aliases_project = root / "tests" / "selfhost" / "fixtures" / "symbol_aliases" / "kinal.knproj"
    aliases_executable = out_dir / f"symbol-aliases{executable_suffix}"
    aliases_build = run(
        [str(compiler), "build", str(aliases_project), str(aliases_executable), "test"],
        cwd=root,
    )
    require(aliases_build.returncode == 0, "stage1 symbol-alias fixture build failed", aliases_build)
    require(aliases_executable.is_file(), "stage1 symbol-alias fixture executable is missing")
    aliases_run = run([str(aliases_executable)], cwd=root)
    require(aliases_run.returncode == 0, "stage1 symbol-alias fixture execution failed", aliases_run)
    require(
        aliases_run.stdout.replace("\r\n", "\n").strip() == "method\n7",
        "stage1 symbol-alias fixture output differs",
        aliases_run,
    )
    results.append({"name": "symbol_aliases", "ok": True})

    delegates_project = root / "tests" / "selfhost" / "fixtures" / "delegates" / "kinal.knproj"
    delegates_executable = out_dir / f"delegates{executable_suffix}"
    delegates_build = run(
        [str(compiler), "build", str(delegates_project), str(delegates_executable), "test"],
        cwd=root,
    )
    require(delegates_build.returncode == 0, "stage1 delegate fixture build failed", delegates_build)
    require(delegates_executable.is_file(), "stage1 delegate fixture executable is missing")
    delegates_run = run([str(delegates_executable)], cwd=root)
    require(delegates_run.returncode == 0, "stage1 delegate fixture execution failed", delegates_run)
    require(
        delegates_run.stdout.replace("\r\n", "\n").strip() == "42\n42\n42\n42",
        "stage1 delegate fixture output differs",
        delegates_run,
    )
    delegate_syntax_fixtures = [
        root / "tests" / "common" / "ptr_to_function_cast.kn",
        root / "tests" / "common" / "dynlib_loader.kn",
        root / "tests" / "windows" / "ffi_abi.kn",
    ]
    for index, delegate_fixture in enumerate(delegate_syntax_fixtures):
        delegate_stage0_path = out_dir / f"delegate-{index}-stage0.kast"
        delegate_stage0 = run(
            [
                str(stage0),
                "build",
                "--no-module-discovery",
                "--color",
                "never",
                "--emit",
                "ast",
                str(delegate_fixture),
                "-o",
                str(delegate_stage0_path),
            ],
            cwd=root,
        )
        require(delegate_stage0.returncode == 0, "stage0 delegate syntax emit failed", delegate_stage0)
        delegate_stage1 = run([str(compiler), "ast", str(delegate_fixture)], cwd=root)
        require(delegate_stage1.returncode == 0, "stage1 delegate syntax build failed", delegate_stage1)
        require(
            delegate_stage0_path.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
            == delegate_stage1.stdout.replace("\r\n", "\n").strip(),
            f"stage0/stage1 delegate syntax mismatch: {delegate_fixture}",
        )
    results.append({"name": "delegates", "ok": True, "files": len(delegate_syntax_fixtures)})

    global_literal_fixture = root / "tests" / "common" / "global_null_sugar.kn"
    global_literal_stage0_path = out_dir / "global-literals-stage0.kast"
    global_literal_stage0 = run(
        [
            str(stage0),
            "build",
            "--no-module-discovery",
            "--color",
            "never",
            "--emit",
            "ast",
            str(global_literal_fixture),
            "-o",
            str(global_literal_stage0_path),
        ],
        cwd=root,
    )
    require(global_literal_stage0.returncode == 0, "stage0 global-literal syntax emit failed", global_literal_stage0)
    global_literal_stage1 = run([str(compiler), "ast", str(global_literal_fixture)], cwd=root)
    require(global_literal_stage1.returncode == 0, "stage1 global-literal syntax build failed", global_literal_stage1)
    require(
        global_literal_stage0_path.read_text(encoding="utf-8").replace("\r\n", "\n").strip()
        == global_literal_stage1.stdout.replace("\r\n", "\n").strip(),
        "stage0/stage1 global-literal syntax mismatch",
    )
    results.append({"name": "global_literal_syntax", "ok": True})

    global_literals_project = root / "tests" / "selfhost" / "fixtures" / "global_literals" / "kinal.knproj"
    global_literals_executable = out_dir / f"global-literals{executable_suffix}"
    global_literals_build = run(
        [str(compiler), "build", str(global_literals_project), str(global_literals_executable), "test"],
        cwd=root,
    )
    require(global_literals_build.returncode == 0, "stage1 global-literal fixture build failed", global_literals_build)
    require(global_literals_executable.is_file(), "stage1 global-literal fixture executable is missing")
    global_literals_run = run([str(global_literals_executable)], cwd=root)
    require(global_literals_run.returncode == 0, "stage1 global-literal fixture execution failed", global_literals_run)
    require(
        global_literals_run.stdout.replace("\r\n", "\n").strip() ==
        "0\nnull\nblock\nIO.Type.Object.Class\nlocal-block\nIO.Type.Object.Class",
        "stage1 global-literal fixture output differs",
        global_literals_run,
    )
    results.append({"name": "global_literals", "ok": True})

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

    expression_ops_project = root / "tests" / "selfhost" / "fixtures" / "expression_ops" / "kinal.knproj"
    expression_ops_executable = out_dir / f"expression-ops{executable_suffix}"
    expression_ops_build = run(
        [str(compiler), "build", str(expression_ops_project), str(expression_ops_executable), "test"],
        cwd=root,
    )
    require(expression_ops_build.returncode == 0, "stage1 expression-ops fixture build failed", expression_ops_build)
    require(expression_ops_executable.is_file(), "stage1 expression-ops fixture executable is missing")
    expression_ops_run = run([str(expression_ops_executable)], cwd=root)
    require(expression_ops_run.returncode == 0, "stage1 expression-ops fixture execution failed", expression_ops_run)
    require(
        expression_ops_run.stdout.replace("\r\n", "\n").strip() ==
        "1\n3\n9\n3\n1\n3\n10\n12\n3\n6\n6\nwindows",
        "stage1 expression-ops fixture output differs",
        expression_ops_run,
    )
    results.append({"name": "expression_ops", "ok": True})

    string_prefixes_project = root / "tests" / "selfhost" / "fixtures" / "string_prefixes" / "kinal.knproj"
    string_prefixes_executable = out_dir / f"string-prefixes{executable_suffix}"
    string_prefixes_build = run(
        [str(compiler), "build", str(string_prefixes_project), str(string_prefixes_executable), "test"],
        cwd=root,
    )
    require(string_prefixes_build.returncode == 0, "stage1 string-prefix fixture build failed", string_prefixes_build)
    require(string_prefixes_executable.is_file(), "stage1 string-prefix fixture executable is missing")
    string_prefixes_run = run([str(string_prefixes_executable)], cwd=root)
    require(string_prefixes_run.returncode == 0, "stage1 string-prefix fixture execution failed", string_prefixes_run)
    require(
        string_prefixes_run.stdout.replace("\r\n", "\n") ==
        "\\n \\\\ / abc\n11\nv=7\nsum=12\n{7}\n3\na\nc\n3\nz\n7\n2\n\\\nn\n",
        "stage1 string-prefix fixture output differs",
        string_prefixes_run,
    )
    results.append({"name": "string_prefixes", "ok": True})

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

    parameter_commas_project = (
        root / "tests" / "selfhost" / "fixtures" / "parameter_nested_commas" / "kinal.knproj"
    )
    parameter_commas_executable = out_dir / f"parameter-nested-commas{executable_suffix}"
    parameter_commas_build = run(
        [
            str(compiler),
            "build",
            str(parameter_commas_project),
            str(parameter_commas_executable),
            "test",
        ],
        cwd=root,
    )
    require(
        parameter_commas_build.returncode == 0,
        "stage1 nested-parameter-comma fixture build failed",
        parameter_commas_build,
    )
    parameter_commas_run = run([str(parameter_commas_executable)], cwd=root)
    require(
        parameter_commas_run.returncode == 0
        and parameter_commas_run.stdout.replace("\r\n", "\n") == "7\n5\n",
        "stage1 nested-parameter-comma fixture output differs",
        parameter_commas_run,
    )
    results.append({"name": "parameter_nested_commas", "ok": True})

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

    manifest_parser_report = out_dir / "manifest-parser.json"
    manifest_parser = run(
        [
            sys.executable,
            str(root / "tests" / "selfhost" / "audit_manifest_parser.py"),
            "--compiler",
            str(compiler),
            "--stage0",
            str(stage0),
            "--root",
            str(root),
            "--baseline",
            str(root / "tests" / "selfhost" / "manifest_parser_baseline.json"),
            "--output",
            str(manifest_parser_report),
        ],
        cwd=root,
    )
    require(manifest_parser.returncode == 0, "stage1 manifest parser audit failed", manifest_parser)
    manifest_parser_data = json.loads(manifest_parser_report.read_text(encoding="utf-8"))
    results.append({
        "name": "manifest_parser_coverage",
        "ok": True,
        "sources": manifest_parser_data["sources"],
        "passed": manifest_parser_data["passed"],
        "coverage": manifest_parser_data["coverage"],
    })

    manifest_sema_report = out_dir / "manifest-sema.json"
    manifest_sema = run(
        [
            sys.executable,
            str(root / "tests" / "selfhost" / "audit_manifest_sema.py"),
            "--compiler",
            str(compiler),
            "--root",
            str(root),
            "--baseline",
            str(root / "tests" / "selfhost" / "manifest_sema_baseline.json"),
            "--output",
            str(manifest_sema_report),
        ],
        cwd=root,
    )
    require(manifest_sema.returncode == 0, "stage1 manifest semantic audit failed", manifest_sema)
    manifest_sema_data = json.loads(manifest_sema_report.read_text(encoding="utf-8"))
    results.append({
        "name": "manifest_sema_coverage",
        "ok": True,
        "cases": manifest_sema_data["cases"],
        "passed": manifest_sema_data["passed"],
        "coverage": manifest_sema_data["coverage"],
    })

    manifest_diagnostics_report = out_dir / "manifest-diagnostics.json"
    manifest_diagnostics = run(
        [
            sys.executable,
            str(root / "tests" / "selfhost" / "audit_manifest_diagnostics.py"),
            "--compiler",
            str(compiler),
            "--stage0",
            str(stage0),
            "--root",
            str(root),
            "--baseline",
            str(root / "tests" / "selfhost" / "manifest_diagnostics_baseline.json"),
            "--output",
            str(manifest_diagnostics_report),
            "--all-stages",
        ],
        cwd=root,
    )
    require(manifest_diagnostics.returncode == 0,
            "stage1 manifest diagnostic audit failed", manifest_diagnostics)
    manifest_diagnostics_data = json.loads(
        manifest_diagnostics_report.read_text(encoding="utf-8")
    )
    results.append({
        "name": "manifest_diagnostic_coverage",
        "ok": True,
        "cases": manifest_diagnostics_data["cases"],
        "passed": manifest_diagnostics_data["passed"],
        "coverage": manifest_diagnostics_data["coverage"],
        "all_stage_differential_cases":
            manifest_diagnostics_data["all_stage_differential_cases"],
    })

    manifest_native = run(
        [
            sys.executable,
            str(root / "tests" / "selfhost" / "audit_manifest_native.py"),
            "--compiler",
            str(compiler),
            "--root",
            str(root),
            "--out-dir",
            str(out_dir / "manifest-native-audit"),
        ],
        cwd=root,
    )
    require(manifest_native.returncode == 0,
            "stage1 manifest native audit failed", manifest_native)
    manifest_native_data = json.loads(manifest_native.stdout.strip())
    results.append({
        "name": "manifest_native_coverage",
        "ok": True,
        "positive_cases": manifest_native_data.get("positive_cases", 0),
        "unsupported_cases": manifest_native_data.get("unsupported_cases", []),
        "skipped": manifest_native_data.get("skipped", False),
    })

    manifest_runtime = run(
        [
            sys.executable,
            str(root / "tests" / "selfhost" / "audit_manifest_runtime.py"),
            "--compiler",
            str(compiler),
            "--root",
            str(root),
            "--out-dir",
            str(out_dir / "manifest-runtime-audit"),
        ],
        cwd=root,
    )
    require(manifest_runtime.returncode == 0,
            "stage1 manifest runtime audit failed", manifest_runtime)
    manifest_runtime_data = json.loads(manifest_runtime.stdout.strip())
    results.append({
        "name": "manifest_runtime_coverage",
        "ok": True,
        "runtime_cases": manifest_runtime_data.get("runtime_cases", 0),
        "unsupported_cases": manifest_runtime_data.get("unsupported_cases", []),
        "skipped": manifest_runtime_data.get("skipped", False),
    })

    (out_dir / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print(f"[OK] selfhost tests: {len(results)} groups")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
