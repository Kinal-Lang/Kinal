#!/usr/bin/env python3
"""Fast parser/sema and backend-boundary checks."""

from __future__ import annotations

import argparse
import subprocess
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
    if summary.get("format") != "kinal-sema-v1" or summary.get("sources") != "1":
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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
