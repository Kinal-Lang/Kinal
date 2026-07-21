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
    cases = collect_cases(root, baseline["platform"])
    failures: list[str] = []
    diagnostics: dict[str, str] = {}
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
        if result.returncode == 0 or expected not in output:
            failures.append(name)
            diagnostics[name] = next(
                (line for line in output.splitlines() if line.startswith("[")),
                output.splitlines()[0] if output else "no diagnostic",
            )

    report = {
        "format": "kinal-selfhost-manifest-diagnostics-v1",
        "platform": baseline["platform"],
        "cases": len(cases),
        "passed": len(cases) - len(failures),
        "failed": len(failures),
        "coverage": (len(cases) - len(failures)) / len(cases),
        "unsupported_cases": sorted(failures),
        "diagnostics": diagnostics,
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
    print(f"[OK] manifest diagnostics: {report['passed']}/{report['cases']} ({report['coverage']:.1%})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
