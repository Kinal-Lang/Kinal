#!/usr/bin/env python3
"""Enforce explicit test ownership for every builtin and official std package."""

from __future__ import annotations

import json
import re
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ALLOWED_MODES = {"runtime", "compile_only", "freestanding_compile", "internal"}


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def load_cases(path: str) -> dict[str, dict[str, object]]:
    values = json.loads(read(path))
    return {str(case["name"]): case for case in values}


def builtin_names() -> list[str]:
    header = read("apps/kinal/include/kn/std.h")
    match = re.search(r"typedef\s+enum\s*\{(?P<body>.*?)\}\s*KnBuiltinKind\s*;", header, re.S)
    if not match:
        raise AssertionError("missing KnBuiltinKind")
    names = list(dict.fromkeys(re.findall(r"\bKN_BUILTIN_[A-Z0-9_]+\b", match.group("body"))))
    return [name for name in names if name not in {"KN_BUILTIN_NONE", "KN_BUILTIN_COUNT"}]


def official_packages() -> set[str]:
    return {path.name for path in (ROOT / "libs" / "std").iterdir() if path.is_dir()}


def main() -> int:
    coverage = json.loads(read("tests/stdlib_coverage.json"))
    manifest_cases = load_cases("tests/manifest.json")
    smoke_cases = load_cases("tests/smoke.json")
    freestanding_cases = load_cases("tests/freestanding.json")
    # Prefer the full-manifest classification when smoke carries a cheaper
    # compile-only variant of the same named case.
    all_cases = {**smoke_cases, **freestanding_cases, **manifest_cases}

    owned: list[str] = []
    referenced_cases: set[str] = set()
    for group in coverage.get("builtin_groups", []):
        mode = str(group.get("mode", ""))
        if mode not in ALLOWED_MODES:
            raise AssertionError(f"invalid stdlib coverage mode: {mode!r}")
        cases = [str(name) for name in group.get("cases", [])]
        builtins = [str(name) for name in group.get("builtins", [])]
        if not cases or not builtins:
            raise AssertionError(f"coverage group must own cases and builtins: {group.get('name')}")
        missing_cases = sorted(name for name in cases if name not in all_cases)
        if missing_cases:
            raise AssertionError(f"unknown test cases in {group.get('name')}: {missing_cases}")
        if mode == "runtime" and not any(
            not bool(all_cases[name].get("compile_only", False)) and "expect_error" not in all_cases[name]
            for name in cases
        ):
            raise AssertionError(f"runtime group has no runtime case: {group.get('name')}")
        if mode == "compile_only" and not any(bool(all_cases[name].get("compile_only", False)) for name in cases):
            raise AssertionError(f"compile-only group has no compile-only case: {group.get('name')}")
        if mode == "freestanding_compile" and not any(name in freestanding_cases for name in cases):
            raise AssertionError(f"freestanding group has no freestanding case: {group.get('name')}")
        referenced_cases.update(cases)
        owned.extend(builtins)

    expected = builtin_names()
    counts = Counter(owned)
    duplicates = sorted(name for name, count in counts.items() if count != 1)
    missing = sorted(set(expected) - set(owned))
    unknown = sorted(set(owned) - set(expected))
    if duplicates:
        raise AssertionError(f"builtin coverage must have one owner: {duplicates}")
    if missing:
        raise AssertionError(f"builtins without test ownership: {missing}")
    if unknown:
        raise AssertionError(f"coverage references unknown builtins: {unknown}")

    package_map = coverage.get("packages", {})
    expected_packages = official_packages()
    actual_packages = set(package_map)
    if expected_packages != actual_packages:
        raise AssertionError(
            f"official package coverage mismatch: missing={sorted(expected_packages - actual_packages)} "
            f"unknown={sorted(actual_packages - expected_packages)}"
        )
    for package, cases_value in package_map.items():
        cases = [str(name) for name in cases_value]
        if not cases:
            raise AssertionError(f"official package has no tests: {package}")
        missing_cases = sorted(name for name in cases if name not in all_cases)
        if missing_cases:
            raise AssertionError(f"unknown tests for {package}: {missing_cases}")
        if not any(
            not bool(all_cases[name].get("compile_only", False)) and "expect_error" not in all_cases[name]
            for name in cases
        ):
            raise AssertionError(f"official package has no behavior test: {package}")
        referenced_cases.update(cases)

    for name in sorted(referenced_cases):
        case = all_cases[name]
        paths = case.get("files") or [case.get("file")]
        for value in paths:
            if value and not (ROOT / str(value)).exists():
                raise AssertionError(f"coverage case source is missing: {name}: {value}")

    print(
        f"stdlib coverage ok: {len(expected)} builtins, "
        f"{len(expected_packages)} official packages, {len(referenced_cases)} owned test cases"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
