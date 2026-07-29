from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path


def collect_cases(root: Path, platform: str) -> list[tuple[str, list[Path], str]]:
    manifest = json.loads((root / "tests" / "manifest.json").read_text(encoding="utf-8"))
    cases: list[tuple[str, list[Path], str]] = []
    for case in manifest:
        expected = case.get("expect_error")
        if not expected:
            continue
        if "platforms" in case and platform not in case["platforms"]:
            continue
        sources: list[Path] = []
        for key in ("file", "files"):
            values = case.get(key, [])
            if isinstance(values, str):
                values = [values]
            sources.extend((root / value).resolve() for value in values if value.endswith(".kn"))
        if sources:
            cases.append((case["name"], sources, expected))
    return cases


def contains_expected_diagnostic(output: str, expected: str) -> bool:
    if expected in output:
        return True
    if not expected.startswith("[") or "] " not in expected:
        return False
    stage, message = expected.split("] ", 1)
    # C stage0 inserts the registry code/severity and diagnostic title between
    # the stage and detail. The selfhost formatter omits the registry fields.
    # Preserve both assertions without depending on either presentation.
    return f"{stage}]" in output and message in output


def parser_diagnostic_titles(output: str) -> list[str]:
    titles: list[str] = []
    for line in output.splitlines():
        if not line.startswith("[Parser]") or "[warning]" in line:
            continue
        message = line[line.rfind("] ") + 2 :]
        titles.append(message.split(":", 1)[0])
    return titles


def parser_warning_titles(output: str) -> list[str]:
    titles: list[str] = []
    for line in output.splitlines():
        if not line.startswith("[Parser]") or "[warning]" not in line:
            continue
        message = line[line.rfind("] ") + 2 :]
        titles.append(message.split(":", 1)[0])
    return titles


def compiler_diagnostic_titles(output: str) -> list[str]:
    titles: list[str] = []
    known_stages = {"Lexer", "Parser", "Sema", "Project", "Driver", "Link", "KNC", "Native"}
    for line in output.splitlines():
        if not line.startswith("[") or "[warning]" in line:
            continue
        stage_end = line.find("]")
        if stage_end < 0:
            continue
        stage = line[1:stage_end]
        if stage not in known_stages:
            continue
        message = line[line.rfind("] ") + 2 :]
        titles.append(f"{stage}:{message.split(':', 1)[0]}")
    return titles


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--all-stages", action="store_true")
    args = parser.parse_args()

    root = args.root.resolve()
    compiler = args.compiler.resolve()
    stage0 = args.stage0.resolve() if args.stage0 else None
    baseline = json.loads(args.baseline.resolve().read_text(encoding="utf-8"))
    cases = collect_cases(root, baseline["platform"])
    failures: list[str] = []
    diagnostics: dict[str, str] = {}
    parser_mismatches: dict[str, dict[str, list[str]]] = {}
    warning_mismatches: dict[str, dict[str, list[str]]] = {}
    stage_mismatches: dict[str, dict[str, list[str]]] = {}
    stage0_out = (
        (args.output.resolve().parent if args.output else root / "out" / "selfhost")
        / "manifest-diagnostics-stage0"
    )
    if stage0:
        stage0_out.mkdir(parents=True, exist_ok=True)
    for name, sources, expected in cases:
        result = subprocess.run(
            [str(compiler), "check-source", *(str(source) for source in sources)],
            cwd=root,
            text=True,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        output = (result.stdout or "") + (result.stderr or "")
        if result.returncode == 0 or not contains_expected_diagnostic(output, expected):
            failures.append(name)
            diagnostics[name] = next(
                (line for line in output.splitlines() if line.startswith("[")),
                output.splitlines()[0] if output else "no diagnostic",
            )
        if stage0:
            stage0_result = subprocess.run(
                [
                    str(stage0),
                    "build",
                    "--no-module-discovery",
                    "--color",
                    "never",
                    "--emit",
                    "check",
                    *(str(source) for source in sources),
                    "-o",
                    str(stage0_out / f"{name}.kcheck"),
                ],
                cwd=root,
                text=True,
                capture_output=True,
                encoding="utf-8",
                errors="replace",
                check=False,
            )
            stage0_output = (stage0_result.stdout or "") + (stage0_result.stderr or "")
            expected_parser = parser_diagnostic_titles(stage0_output)
            actual_parser = parser_diagnostic_titles(output)
            if actual_parser != expected_parser:
                parser_mismatches[name] = {
                    "stage0": expected_parser,
                    "stage1": actual_parser,
                }
            expected_warnings = parser_warning_titles(stage0_output)
            actual_warnings = parser_warning_titles(output)
            if actual_warnings != expected_warnings:
                warning_mismatches[name] = {
                    "stage0": expected_warnings,
                    "stage1": actual_warnings,
                }
            if args.all_stages:
                expected_stages = compiler_diagnostic_titles(stage0_output)
                actual_stages = compiler_diagnostic_titles(output)
                if actual_stages != expected_stages:
                    stage_mismatches[name] = {
                        "stage0": expected_stages,
                        "stage1": actual_stages,
                    }

    report = {
        "format": "kinal-selfhost-manifest-diagnostics-v1",
        "platform": baseline["platform"],
        "cases": len(cases),
        "passed": len(cases) - len(failures),
        "failed": len(failures),
        "coverage": (len(cases) - len(failures)) / len(cases),
        "unsupported_cases": sorted(failures),
        "diagnostics": diagnostics,
        "parser_differential_cases": len(cases) if stage0 else 0,
        "parser_mismatches": parser_mismatches,
        "warning_mismatches": warning_mismatches,
        "all_stage_differential_cases": len(cases) if stage0 and args.all_stages else 0,
        "stage_mismatches": stage_mismatches,
    }
    if args.output:
        args.output.resolve().write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if len(cases) != baseline["negative_cases"]:
        print(f"manifest negative case count changed: expected {baseline['negative_cases']}, got {len(cases)}")
        return 1
    expected_failures = sorted(baseline["unsupported_cases"])
    if sorted(failures) != expected_failures:
        print("manifest diagnostic baseline changed")
        print("new failures:", sorted(set(failures) - set(expected_failures)))
        print("newly supported:", sorted(set(expected_failures) - set(failures)))
        return 1
    if parser_mismatches:
        print("manifest Parser diagnostics differ from stage0")
        for name, mismatch in sorted(parser_mismatches.items()):
            print(f"{name}: stage0={mismatch['stage0']} stage1={mismatch['stage1']}")
        return 1
    if warning_mismatches:
        print("manifest Parser warnings differ from stage0")
        for name, mismatch in sorted(warning_mismatches.items()):
            print(f"{name}: stage0={mismatch['stage0']} stage1={mismatch['stage1']}")
        return 1
    if stage_mismatches:
        print("manifest compiler diagnostics differ from stage0")
        for name, mismatch in sorted(stage_mismatches.items()):
            print(f"{name}: stage0={mismatch['stage0']} stage1={mismatch['stage1']}")
        return 1
    print(f"[OK] manifest diagnostics: {report['passed']}/{report['cases']} ({report['coverage']:.1%})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
