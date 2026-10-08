#!/usr/bin/env python3
"""Fast parser/sema and backend-boundary checks."""

from __future__ import annotations

import argparse
import re
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def run(compiler: Path, source: Path, output: Path, *extra: str) -> subprocess.CompletedProcess[str]:
    if output.exists():
        output.unlink()
    command = [
        str(compiler),
        "build",
        "--no-module-discovery",
        "--color",
        "never",
        "--emit",
        "check",
        *extra,
        str(source),
        "-o",
        str(output),
    ]
    return subprocess.run(command, cwd=ROOT, text=True, capture_output=True, check=False)


def run_vm_build(compiler: Path, source: Path, output: Path) -> subprocess.CompletedProcess[str]:
    if output.exists():
        output.unlink()
    command = [
        str(compiler),
        "vm",
        "build",
        "--no-module-discovery",
        "--color",
        "never",
        str(source),
        "-o",
        str(output),
    ]
    return subprocess.run(command, cwd=ROOT, text=True, capture_output=True, check=False)


def run_tokens(compiler: Path, source: Path, output: Path) -> subprocess.CompletedProcess[str]:
    if output.exists():
        output.unlink()
    command = [
        str(compiler),
        "build",
        "--no-module-discovery",
        "--color",
        "never",
        "--emit",
        "tokens",
        str(source),
        "-o",
        str(output),
    ]
    return subprocess.run(command, cwd=ROOT, text=True, capture_output=True, check=False)


def run_syntax(compiler: Path, source: Path, output: Path) -> subprocess.CompletedProcess[str]:
    if output.exists():
        output.unlink()
    command = [
        str(compiler),
        "build",
        "--no-module-discovery",
        "--color",
        "never",
        "--emit",
        "ast",
        str(source),
        "-o",
        str(output),
    ]
    return subprocess.run(command, cwd=ROOT, text=True, capture_output=True, check=False)


def read_summary(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        key, separator, value = line.partition("=")
        if not separator or not key:
            raise AssertionError(f"invalid semantic summary line: {line!r}")
        values[key] = value
    return values


def require_failure(
    compiler: Path,
    out_dir: Path,
    name: str,
    source_name: str,
    stage: str,
    detail: str,
) -> None:
    output = out_dir / f"{name}.kcheck"
    first = run(compiler, ROOT / "tests" / "common" / source_name, output)
    second = run(compiler, ROOT / "tests" / "common" / source_name, output)
    combined_first = (first.stdout + first.stderr).replace("\r\n", "\n")
    combined_second = (second.stdout + second.stderr).replace("\r\n", "\n")
    if first.returncode == 0 or second.returncode == 0:
        raise AssertionError(f"{name}: expected failure")
    if f"[{stage}]" not in combined_first or detail not in combined_first:
        raise AssertionError(f"{name}: unexpected diagnostic:\n{combined_first}")
    if combined_first != combined_second:
        raise AssertionError(f"{name}: diagnostics are not deterministic")
    if output.exists():
        raise AssertionError(f"{name}: failed check left a stale artifact")
    print(f"[OK] stage_{name}")


def check_capture_storage(compiler: Path, out_dir: Path) -> None:
    source = ROOT / "tests" / "common" / "escaping_capture_storage.kn"
    for target in ("win64", "win86", "win-arm64", "linux64", "linux-arm64", "mac64", "macos-arm64"):
        output = out_dir / f"capture-storage-{target}.ll"
        result = subprocess.run(
            [str(compiler), "build", "--no-module-discovery", "--emit", "ir",
             "--target", target, str(source), "-o", str(output)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        if result.returncode != 0:
            raise AssertionError(f"{target}: capture IR failed:\n{result.stdout}{result.stderr}")
        ir = output.read_text(encoding="utf-8")

        def body(name: str) -> str:
            match = re.search(
                rf"^define [^\n]*@Tests\.EscapingCaptureStorage\.{name}\([^\n]*\) \{{\n(.*?)^\}}",
                ir, re.MULTILINE | re.DOTALL,
            )
            if match is None:
                raise AssertionError(f"{target}: missing {name}")
            return match[1]

        repeated = body("RepeatedManagedCapture")
        stack_slots = set(re.findall(r"(%[\w.]+) = alloca ", repeated))
        regions = re.findall(r"@__kn_gc_add_root\(ptr [^,]+, ptr (%[\w.]+),", repeated)
        if not regions or any(region not in stack_slots for region in regions):
            raise AssertionError(f"{target}: captured heap cell registered as a stack root region")
        if not re.search(r"^while\.body:[^\n]*\n\s+%capture\.cell\.memory", repeated, re.MULTILINE):
            raise AssertionError(f"{target}: loop capture allocation lost declaration placement")
        aggregate = body("AggregateCapture")
        first_allocation = aggregate.index("call ptr @__kn_gc_alloc")
        argument_root = re.search(r"@__kn_gc_add_root\([^\n]*%capture\.argument\.root", aggregate)
        if argument_root is None or argument_root.start() > first_allocation:
            raise AssertionError(f"{target}: incoming managed parameter was not rooted before allocation")
        if not re.search(r"and i\d+ %capture\.cell\.rounded\d*, -32", aggregate):
            raise AssertionError(f"{target}: aligned struct capture lost its alignment")
        print(f"[OK] stage_capture_storage_{target}")


def check_collection_loop_storage(compiler: Path, out_dir: Path) -> None:
    source = ROOT / "tests" / "common" / "collection_loop_storage.kn"
    for target in ("win64", "win86", "win-arm64", "linux64", "linux-arm64", "mac64", "macos-arm64"):
        output = out_dir / f"collection-loop-storage-{target}.ll"
        result = subprocess.run(
            [str(compiler), "build", "--no-module-discovery", "--emit", "ir",
             "--target", target, str(source), "-o", str(output)],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        if result.returncode != 0:
            raise AssertionError(f"{target}: collection IR failed:\n{result.stdout}{result.stderr}")
        ir = output.read_text(encoding="utf-8")
        match = re.search(
            r"^define [^\n]*@Tests\.CollectionLoopStorage\.RepeatCollections\([^\n]*\) \{\n(.*?)^\}",
            ir, re.MULTILINE | re.DOTALL,
        )
        if match is None:
            raise AssertionError(f"{target}: missing RepeatCollections")
        body = match[1]
        if "while.body:" not in body:
            raise AssertionError(f"{target}: missing collection loop")
        block = None
        allocations = 0
        for line in body.splitlines():
            label = re.match(r"^([\w.$-]+):", line)
            if label:
                block = label[1]
            if " = alloca " in line:
                allocations += 1
                if block != "entry":
                    raise AssertionError(f"{target}: repeated stack allocation in {block}: {line.strip()}")
        if not allocations:
            raise AssertionError(f"{target}: missing collection stack storage")
        print(f"[OK] stage_collection_loop_storage_{target}")


def check_project_source_paths(compiler: Path, out_dir: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="project-paths-", dir=out_dir) as directory:
        project = Path(directory)
        (project / "src/placeholder").mkdir(parents=True)
        (project / "src/Main.kn").write_text(
            "Unit Tests.ProjectPaths;\nStatic Function int Main() { Return 0; }\n", encoding="utf-8")
        manifest = project / "kinal.knproj"
        for index, (root, entry) in enumerate([
            ("src", "src/placeholder/../Main.kn"),
            ("src/placeholder/../", "src/Main.kn"),
            ("src/placeholder/../", "src/placeholder/../Main.kn"),
        ]):
            manifest.write_text(
                'Project PathChecks {\n'
                f' SourceSet "main" {{ Roots = ["{root}"]; Include = ["**/*.kn"]; RequireUnit = true; }}\n'
                ' DefaultProfile = "test";\n'
                ' Profile "test" {\n'
                f'  Source {{ Entry = "{entry}"; Sets = ["main"]; Mode = ReachableUnits; }}\n'
                '  Build { Backend = Native; Environment = Hosted; }\n'
                ' }\n}\n', encoding="utf-8")
            output = project / f"case-{index}.ll"
            command = [str(compiler), "build", "--project", str(project), "--profile", "test",
                       "--emit", "ir", "-o", str(output)]
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
            if result.returncode != 0 or not output.is_file():
                raise AssertionError(f"normalized project paths {index}:\n{result.stdout}{result.stderr}")
        print("[OK] stage_project_normalized_source_paths")
        # Recursive failure must propagate instead of silently dropping invalid sources.
        (project / "src/placeholder/Bad.kn").write_text(
            "Static Function int MissingUnit() { Return 1; }\n", encoding="utf-8")
        output.unlink()
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
        if result.returncode == 0 or output.exists() or "must declare Unit" not in result.stdout + result.stderr:
            raise AssertionError(f"nested RequireUnit validation was swallowed:\n{result.stdout}{result.stderr}")
        print("[OK] stage_project_nested_source_validation")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run Kinal parser/sema stage checks")
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--out-dir", default=str(ROOT / "out" / "test" / "stages"))
    args = parser.parse_args()

    compiler = Path(args.compiler).resolve()
    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    source = ROOT / "tests" / "common" / "hello.kn"
    first_output = out_dir / "hello-first.kcheck"
    second_output = out_dir / "hello-second.kcheck"
    first = run(compiler, source, first_output)
    second = run(compiler, source, second_output)
    if first.returncode != 0 or second.returncode != 0:
        raise AssertionError(f"valid stage check failed:\n{first.stdout}{first.stderr}{second.stdout}{second.stderr}")
    if first_output.read_bytes() != second_output.read_bytes():
        raise AssertionError("semantic summaries are not deterministic")
    summary = read_summary(first_output)
    if summary.get("format") != "kinal-sema-v1" or int(summary.get("sources", "0")) < 1:
        raise AssertionError(f"unexpected semantic summary: {summary}")
    if summary.get("hir_format") != "kinal-call-hir-v1":
        raise AssertionError(f"missing typed call HIR summary: {summary}")
    if summary.get("binary_hir_format") != "kinal-binary-hir-v1":
        raise AssertionError(f"missing typed binary HIR summary: {summary}")
    if summary.get("hir_unresolved_calls") != "0" or int(summary.get("hir_builtin_calls", "0")) < 1:
        raise AssertionError(f"unexpected typed call resolution: {summary}")
    if summary.get("hir_unresolved_binaries") != "0":
        raise AssertionError(f"unresolved binary plan escaped sema: {summary}")
    print("[OK] stage_sema_summary")

    dead_branch = ROOT / "tests" / "common" / "hir_const_branches.kn"
    dead_output = out_dir / "hir-const-branches.kcheck"
    dead_result = run(compiler, dead_branch, dead_output)
    if dead_result.returncode != 0:
        raise AssertionError(f"inactive branch was bound:\n{dead_result.stdout}{dead_result.stderr}")
    dead_summary = read_summary(dead_output)
    if dead_summary.get("hir_unresolved_calls") != "0" or \
            dead_summary.get("hir_unresolved_binaries") != "0":
        raise AssertionError(f"inactive syntax escaped into Typed HIR: {dead_summary}")
    print("[OK] stage_hir_const_branches")

    token_source = ROOT / "tests" / "selfhost" / "fixtures" / "lex_basic.kn"
    first_tokens = out_dir / "lex-basic-first.ktokens"
    second_tokens = out_dir / "lex-basic-second.ktokens"
    first_token_result = run_tokens(compiler, token_source, first_tokens)
    second_token_result = run_tokens(compiler, token_source, second_tokens)
    if first_token_result.returncode != 0 or second_token_result.returncode != 0:
        raise AssertionError(
            "token emit failed:\n"
            f"{first_token_result.stdout}{first_token_result.stderr}"
            f"{second_token_result.stdout}{second_token_result.stderr}"
        )
    if first_tokens.read_bytes() != second_tokens.read_bytes():
        raise AssertionError("token summaries are not deterministic")
    token_lines = first_tokens.read_text(encoding="utf-8").splitlines()
    if token_lines[:3] != ["format=kinal-tokens-v1", "sources=1", "source=0"]:
        raise AssertionError(f"unexpected token summary header: {token_lines[:3]}")
    if not token_lines[-1].startswith("0\t"):
        raise AssertionError(f"token summary is missing EOF: {token_lines[-1:]}")
    print("[OK] stage_token_summary")

    syntax_output = out_dir / "lex-basic.kast"
    syntax_result = run_syntax(compiler, token_source, syntax_output)
    if syntax_result.returncode != 0:
        raise AssertionError(f"syntax emit failed:\n{syntax_result.stdout}{syntax_result.stderr}")
    syntax_summary = read_summary(syntax_output)
    if syntax_summary != {
        "format": "kinal-syntax-v1",
        "sources": "1",
        "unit": "SelfhostFixture.LexBasic",
        "imports": "1",
        "functions": "1",
        "types": "0",
        "fields": "0",
        "methods": "0",
        "globals": "0",
    }:
        raise AssertionError(f"unexpected syntax summary: {syntax_summary}")
    print("[OK] stage_syntax_summary")

    # Link fields are read from a short-lived project config. The command is
    # expected to fail on intentionally missing inputs, but every resolved item
    # must survive until linker command construction.
    project_link_output = out_dir / "project-link-settings"
    project_link = subprocess.run(
        [
            str(compiler),
            "build",
            "--project",
            str(ROOT / "tests" / "pkg" / "project_link_settings"),
            "--show-link",
            "-o",
            str(project_link_output),
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    project_link_text = (project_link.stdout + project_link.stderr).replace("\\", "/")
    if project_link.returncode == 0:
        raise AssertionError("project link-settings fixture unexpectedly linked")
    for fragment in (
        "project_link_settings/native",
        "project_settings_probe",
        "project_link_settings/native/probe-object.o",
    ):
        if fragment not in project_link_text:
            raise AssertionError(
                f"project link item did not survive config teardown ({fragment}):\n{project_link_text}"
            )
    print("[OK] stage_project_link_settings_lifetime")

    function_output = out_dir / "functions.kcheck"
    function_result = run(compiler, ROOT / "tests" / "common" / "functions.kn", function_output)
    if function_result.returncode != 0:
        raise AssertionError(f"typed call HIR check failed:\n{function_result.stdout}{function_result.stderr}")
    function_summary = read_summary(function_output)
    if function_summary.get("hir_unresolved_calls") != "0":
        raise AssertionError(f"unresolved calls escaped sema: {function_summary}")
    if int(function_summary.get("hir_function_calls", "0")) < 1:
        raise AssertionError(f"direct function target missing from typed call HIR: {function_summary}")
    if int(function_summary.get("hir_builtin_calls", "0")) < 2:
        raise AssertionError(f"builtin targets missing from typed call HIR: {function_summary}")
    print("[OK] stage_typed_call_hir")

    binary_source = ROOT / "tests" / "common" / "typed_binary_hir.kn"
    binary_first_output = out_dir / "typed-binary-first.kcheck"
    binary_second_output = out_dir / "typed-binary-second.kcheck"
    binary_first = run(compiler, binary_source, binary_first_output)
    binary_second = run(compiler, binary_source, binary_second_output)
    if binary_first.returncode != 0 or binary_second.returncode != 0:
        raise AssertionError(
            "typed binary HIR check failed:\n"
            f"{binary_first.stdout}{binary_first.stderr}{binary_second.stdout}{binary_second.stderr}"
        )
    if binary_first_output.read_bytes() != binary_second_output.read_bytes():
        raise AssertionError("typed binary HIR summaries are not deterministic")
    binary_summary = read_summary(binary_first_output)
    if binary_summary.get("binary_hir_format") != "kinal-binary-hir-v1":
        raise AssertionError(f"unexpected binary HIR format: {binary_summary}")
    if binary_summary.get("hir_unresolved_binaries") != "0":
        raise AssertionError(f"unresolved binary plans escaped sema: {binary_summary}")
    binary_categories = (
        "hir_binary_numeric_arithmetic",
        "hir_binary_string_concat",
        "hir_binary_pointer_arithmetic",
        "hir_binary_bitwise",
        "hir_binary_string_equality",
        "hir_binary_reference_equality",
        "hir_binary_scalar_equality",
        "hir_binary_numeric_comparison",
        "hir_binary_logical_short_circuit",
    )
    missing_categories = [
        name for name in binary_categories if int(binary_summary.get(name, "0")) < 1
    ]
    if missing_categories:
        raise AssertionError(
            f"typed binary HIR categories missing {missing_categories}: {binary_summary}"
        )
    if int(binary_summary.get("hir_binaries", "0")) != sum(
        int(binary_summary.get(name, "0")) for name in binary_categories
    ):
        raise AssertionError(f"typed binary HIR classification is not exhaustive: {binary_summary}")
    print("[OK] stage_typed_binary_hir")

    for target, expected_bits in (("win86", "32"), ("linux64", "64")):
        target_output = out_dir / f"hello-{target}.kcheck"
        result = run(compiler, source, target_output, "--target", target)
        if result.returncode != 0:
            raise AssertionError(f"{target} stage check failed:\n{result.stdout}{result.stderr}")
        target_summary = read_summary(target_output)
        if target_summary.get("pointer_bits") != expected_bits:
            raise AssertionError(f"{target}: expected pointer_bits={expected_bits}, got {target_summary}")
        print(f"[OK] stage_target_{target}")

    require_failure(compiler, out_dir, "parser", "error_parser.kn", "Parser", "Missing ';' after return")
    require_failure(compiler, out_dir, "sema", "error_missing_return.kn", "Sema", "must return a value")
    require_failure(compiler, out_dir, "ffi_abi", "error_ffi_unsupported_type.kn", "Sema", "C ABI")
    require_failure(compiler, out_dir, "delegate_signature", "error_delegate_signature.kn", "Sema", "signature")
    require_failure(compiler, out_dir, "aggregate_equality", "error_aggregate_equality.kn", "Sema", "Equality is not defined")

    unsupported_output = out_dir / "knc-time-unsupported.knc"
    unsupported_source = ROOT / "tests" / "common" / "time.kn"
    first_unsupported = run_vm_build(compiler, unsupported_source, unsupported_output)
    second_unsupported = run_vm_build(compiler, unsupported_source, unsupported_output)
    first_text = (first_unsupported.stdout + first_unsupported.stderr).replace("\r\n", "\n")
    second_text = (second_unsupported.stdout + second_unsupported.stderr).replace("\r\n", "\n")
    if first_unsupported.returncode == 0 or second_unsupported.returncode == 0:
        raise AssertionError("KNC Time.Now unexpectedly compiled without a VM handler")
    if "not mapped in the bootstrap KNC emitter yet" not in first_text:
        raise AssertionError(f"unexpected KNC unsupported diagnostic:\n{first_text}")
    if first_text != second_text:
        raise AssertionError("KNC unsupported diagnostics are not deterministic")
    if unsupported_output.exists():
        raise AssertionError("unsupported KNC builtin left a stale artifact")
    print("[OK] stage_knc_unregistered_builtin")
    check_capture_storage(compiler, out_dir)
    check_collection_loop_storage(compiler, out_dir)
    check_project_source_paths(compiler, out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
