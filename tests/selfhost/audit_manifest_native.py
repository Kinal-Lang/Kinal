from __future__ import annotations

import argparse
import json
import platform
import re
import subprocess
import sys
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


EXPECTED_HOST_CASES = {"windows": 192, "linux": 190, "macos": 186}
UNIT_PATTERN = re.compile(r"^\s*Unit\s+([A-Za-z_][A-Za-z0-9_.]*)\s*;", re.MULTILINE)
GET_PATTERN = re.compile(r"^\s*Get\s+([^;\r\n]+)\s*;", re.MULTILINE)
MAIN_PATTERN = re.compile(r"\bFunction\b[^;{}]*\bMain\s*\(", re.MULTILINE)


def source_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def unit_name(path: Path) -> str | None:
    match = UNIT_PATTERN.search(source_text(path))
    return match.group(1) if match else None


def imported_targets(path: Path) -> list[str]:
    targets: list[str] = []
    for match in GET_PATTERN.finditer(source_text(path)):
        body = match.group(1).strip()
        alias = re.split(r"\s+By\s+", body, flags=re.IGNORECASE)
        target = alias[-1].strip()
        if target:
            targets.append(target)
    return targets


def related_sources(entry: Path) -> list[Path]:
    candidates = sorted(entry.parent.rglob("*.kn"))
    units: dict[str, list[Path]] = {}
    for candidate in candidates:
        name = unit_name(candidate)
        if name:
            units.setdefault(name, []).append(candidate)

    selected: set[Path] = {entry.resolve()}
    pending: list[Path] = [entry.resolve()]
    while pending:
        current = pending.pop()
        current_unit = unit_name(current)
        if current_unit:
            for sibling in units.get(current_unit, []):
                resolved = sibling.resolve()
                if resolved not in selected:
                    selected.add(resolved)
                    pending.append(resolved)
        for target in imported_targets(current):
            if target == "IO" or target.startswith("IO."):
                continue
            matches = [
                name for name in units
                if target == name or target.startswith(name + ".")
            ]
            if not matches:
                continue
            imported_unit = max(matches, key=len)
            for dependency in units[imported_unit]:
                resolved = dependency.resolve()
                if resolved not in selected:
                    selected.add(resolved)
                    pending.append(resolved)
    return sorted(selected)


def entry_source(sources: list[Path]) -> Path:
    for source in sources:
        if MAIN_PATTERN.search(source_text(source)):
            return source
    return sources[0]


def quote(value: str) -> str:
    return value.replace("\\", "/").replace('"', '\\"')


def write_project(path: Path, name: str, entry: Path, sources: list[Path]) -> None:
    files = ", ".join(f'"{quote(str(source))}"' for source in sources)
    project_name = re.sub(r"[^A-Za-z0-9_]", "", name) or "ManifestCase"
    content = (
        f"Project Audit{project_name}\n"
        "{\n"
        f"    SourceSet \"app\" {{ Files = [{files}]; RequireUnit = false; }}\n"
        "    Profile \"native\"\n"
        "    {\n"
        "        Source\n"
        "        {\n"
        f"            Entry = \"{quote(str(entry))}\";\n"
        "            Sets = [\"app\"];\n"
        "            Mode = AllSources;\n"
        "        }\n"
        "        Build { Backend = Native; Environment = Hosted; }\n"
        "    }\n"
        "}\n"
    )
    path.write_text(content, encoding="utf-8")


def host_platform() -> str:
    return normalize_platform_name(platform.system())


def normalize_platform_name(value: object) -> str:
    name = str(value).strip().lower()
    if name in {"win", "win32", "windows"}:
        return "windows"
    if name in {"darwin", "osx", "mac", "macos"}:
        return "macos"
    if name in {"gnu/linux", "linux"}:
        return "linux"
    return name


def supports_host(case: dict[str, object], host: str) -> bool:
    platforms = case.get("platforms")
    if platforms is not None and host not in {normalize_platform_name(p) for p in platforms}:
        return False
    return host not in {normalize_platform_name(p) for p in (case.get("skip_platforms") or [])}


def exclusion_reason(case: dict[str, object], host: str, *, runtime: bool = False) -> str | None:
    if "expect_error" in case:
        return "negative diagnostic case"
    if not supports_host(case, host):
        return "not enabled for " + host
    if not has_kinal_source(case):
        return "no Kinal source"
    if runtime and case.get("compile_only"):
        return "compile-only case"
    if runtime and "expected" not in case:
        return "no runtime expectation"
    return None


def manifest_exclusions(manifest: list[dict[str, object]], host: str,
                        *, runtime: bool = False) -> list[dict[str, str]]:
    return [{"name": str(case["name"]), "reason": reason} for case in manifest
            if (reason := exclusion_reason(case, host, runtime=runtime)) is not None]


def positive_cases(manifest: list[dict[str, object]], host: str) -> list[dict[str, object]]:
    return [case for case in manifest if exclusion_reason(case, host) is None]


def has_kinal_source(case: dict[str, object]) -> bool:
    for key in ("file", "files"):
        values = case.get(key, [])
        if isinstance(values, str):
            values = [values]
        if any(str(value).endswith(".kn") for value in values):
            return True
    return False


def audit_case(
    compiler: Path, root: Path, out_dir: Path, case: dict[str, object]
) -> tuple[str, bool, str]:
    name = str(case["name"])
    raw_sources = case.get("files") or [case["file"]]
    sources = [(root / str(source)).resolve() for source in raw_sources]
    entry = entry_source(sources)
    if case.get("auto_link"):
        sources = related_sources(entry)
    case_dir = out_dir / name
    case_dir.mkdir(parents=True, exist_ok=True)
    project = case_dir / "kinal.knproj"
    write_project(project, name, entry, sources)
    output = case_dir / (name + (".obj" if sys.platform == "win32" else ".o"))
    output.unlink(missing_ok=True)
    try:
        proc = subprocess.run(
            [str(compiler), "build-object", str(project), str(output), "native"],
            cwd=root,
            text=True,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            check=False,
            timeout=120,
        )
        detail = (proc.stdout or "") + (proc.stderr or "")
        ok = proc.returncode == 0 and output.is_file()
        if not ok:
            detail = f"exit={proc.returncode}; object_exists={output.is_file()}\n" + detail
    except (subprocess.TimeoutExpired, OSError) as error:
        ok, detail = False, str(error)
    (case_dir / "build.log").write_text(detail, encoding="utf-8")
    return name, ok, detail


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--output", type=Path, help="write a JSON report, including failures")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")

    compiler = args.compiler.resolve()
    root = args.root.resolve()
    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    host = host_platform()
    if host not in EXPECTED_HOST_CASES:
        raise SystemExit(f"Unsupported manifest audit host: {host}")
    if not compiler.is_file():
        raise SystemExit(f"Compiler not found: {compiler}")
    manifest = json.loads((root / "tests" / "manifest.json").read_text(encoding="utf-8"))
    cases = positive_cases(manifest, host)
    expected = EXPECTED_HOST_CASES[host]
    if len(cases) != expected:
        raise SystemExit(
            f"{host} positive manifest baseline changed: expected {expected}, found {len(cases)}"
        )

    failures: list[tuple[str, str]] = []
    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        futures = {
            executor.submit(audit_case, compiler, root, out_dir, case): str(case["name"])
            for case in cases
        }
        for future in as_completed(futures):
            name, ok, detail = future.result()
            if not ok:
                failures.append((name, detail))

    for name, detail in sorted(failures):
        print(f"[{name}]\n{detail}", file=sys.stderr)
    exclusions = manifest_exclusions(manifest, host)
    report = {
        "format": "kinal-selfhost-manifest-native-v1",
        "host": host,
        "positive_cases": len(cases),
        "passed": len(cases) - len(failures),
        "unsupported_cases": [],
        "excluded_cases": exclusions,
        "excluded_case_counts": dict(Counter(case["reason"] for case in exclusions)),
        "failures": [{"name": name, "phase": "build", "detail": detail}
                     for name, detail in sorted(failures)],
    }
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main())
