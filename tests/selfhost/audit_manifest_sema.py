from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path


def collect_positive_cases(root: Path, platform: str) -> list[tuple[str, list[Path]]]:
    manifest = json.loads((root / "tests" / "manifest.json").read_text(encoding="utf-8"))
    cases: list[tuple[str, list[Path]]] = []
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
            cases.append((case["name"], sources))
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
    cases = collect_positive_cases(root, baseline["platform"])
    failures: list[str] = []
    diagnostics: dict[str, str] = {}
    for name, sources in cases:
        result = subprocess.run(
            [str(compiler), "check-source", *(str(source) for source in sources)],
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
        "platform": baseline["platform"],
        "cases": len(cases),
        "passed": len(cases) - len(failures),
        "failed": len(failures),
        "coverage": (len(cases) - len(failures)) / len(cases),
        "unsupported_cases": actual,
        "diagnostics": diagnostics,
    }
    if args.output:
        args.output.resolve().write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if len(cases) != baseline["positive_cases"]:
        print(f"manifest case count changed: expected {baseline['positive_cases']}, got {len(cases)}")
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
