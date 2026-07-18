from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path


def collect_positive_sources(root: Path, platform: str) -> list[Path]:
    manifest = json.loads((root / "tests" / "manifest.json").read_text(encoding="utf-8"))
    sources: list[Path] = []
    for case in manifest:
        if "expect_error" in case:
            continue
        if "platforms" in case and platform not in case["platforms"]:
            continue
        for key in ("file", "files"):
            values = case.get(key, [])
            if isinstance(values, str):
                values = [values]
            for value in values:
                source = (root / value).resolve()
                if source.suffix == ".kn" and source not in sources:
                    sources.append(source)
    return sources


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
    sources = collect_positive_sources(root, baseline["platform"])
    failures: list[str] = []
    diagnostics: dict[str, str] = {}
    for source in sources:
        result = subprocess.run(
            [str(compiler), "parse", str(source)],
            cwd=root,
            text=True,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        if result.returncode != 0:
            relative = source.relative_to(root).as_posix()
            failures.append(relative)
            output = (result.stdout or "") + (result.stderr or "")
            diagnostics[relative] = output.splitlines()[0] if output else ""

    expected = sorted(baseline["unsupported_sources"])
    actual = sorted(failures)
    report = {
        "format": "kinal-selfhost-manifest-parser-v1",
        "platform": baseline["platform"],
        "sources": len(sources),
        "passed": len(sources) - len(failures),
        "failed": len(failures),
        "coverage": (len(sources) - len(failures)) / len(sources),
        "unsupported_sources": actual,
        "diagnostics": diagnostics,
    }
    if args.output:
        args.output.resolve().write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if len(sources) != baseline["positive_sources"]:
        print(f"manifest source count changed: expected {baseline['positive_sources']}, got {len(sources)}")
        return 1
    if actual != expected:
        print("manifest parser baseline changed")
        print("new failures:", sorted(set(actual) - set(expected)))
        print("newly supported:", sorted(set(expected) - set(actual)))
        return 1
    print(
        f"[OK] manifest parser: {report['passed']}/{report['sources']} "
        f"({report['coverage']:.1%})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
