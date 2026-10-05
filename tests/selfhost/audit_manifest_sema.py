from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def collect_positive_cases(root: Path, platform: str) -> list[tuple[str, list[Path], bool]]:
    manifest = json.loads((root / "tests" / "manifest.json").read_text(encoding="utf-8"))
    cases: list[tuple[str, list[Path], bool]] = []
    for case in manifest:
        if "expect_error" in case:
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
            cases.append((case["name"], sources, bool(case.get("auto_link"))))
    return cases


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    root = args.root.resolve()
    compiler = args.compiler.resolve()
    baseline = json.loads(args.baseline.resolve().read_text(encoding="utf-8"))
    # Target-dependent branches are bound for the compiler's host here.
    # Never feed Windows-only semantic fixtures to a Linux/macOS compiler.
    platform = {"win32": "windows", "linux": "linux", "darwin": "macos"}[sys.platform]
    expected_count = baseline.get("positive_cases_by_platform", {}).get(platform)
    if expected_count is None:
        if platform != baseline["platform"]:
            raise SystemExit(f"No semantic baseline for host platform: {platform}")
        expected_count = baseline["positive_cases"]
    cases = collect_positive_cases(root, platform)
    reference_cases = collect_positive_cases(root, baseline["platform"])
    reference_names = {name for name, _, _ in reference_cases}
    selected_names = {name for name, _, _ in cases}
    excluded_platform_cases = sorted(reference_names - selected_names)
    failures: list[str] = []
    diagnostics: dict[str, str] = {}
    for name, sources, auto_link in cases:
        command = "check-source-auto" if auto_link else "check-source"
        result = subprocess.run(
            [str(compiler), command, *(str(source) for source in sources)],
            cwd=root,
            text=True,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        if result.returncode != 0:
            failures.append(name)
            output = (result.stdout or "") + (result.stderr or "")
            diagnostics[name] = next(
                (line for line in output.splitlines() if line.startswith("[")),
                output.splitlines()[0] if output else "",
            )

    expected = sorted(baseline["unsupported_cases"])
    actual = sorted(failures)
    report = {
        "format": "kinal-selfhost-manifest-sema-v1",
        "platform": platform,
        "reference_platform": baseline["platform"],
        "excluded_platform_cases": excluded_platform_cases,
        "excluded_platform_case_count": len(excluded_platform_cases),
        "platform_filter_reason": "manifest platform restrictions must match the semantic target host",
        "added_platform_cases": sorted(selected_names - reference_names),
        "cases": len(cases),
        "passed": len(cases) - len(failures),
        "failed": len(failures),
        "coverage": (len(cases) - len(failures)) / len(cases),
        "unsupported_cases": actual,
        "diagnostics": diagnostics,
    }
    if args.output:
        args.output.resolve().write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if len(cases) != expected_count:
        print(f"manifest case count changed: expected {expected_count}, got {len(cases)}")
        return 1
    if actual != expected:
        print("manifest semantic baseline changed")
        print("new failures:", sorted(set(actual) - set(expected)))
        print("newly supported:", sorted(set(expected) - set(actual)))
        return 1
    print(f"[OK] manifest sema: {report['passed']}/{report['cases']} ({report['coverage']:.1%})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
