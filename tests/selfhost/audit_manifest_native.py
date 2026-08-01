from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


EXPECTED_WINDOWS_CASES = 166
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


def supports_windows(case: dict[str, object]) -> bool:
    platforms = case.get("platforms")
    return platforms is None or "windows" in platforms


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
    output = case_dir / f"{name}.obj"
    proc = subprocess.run(
        [str(compiler), "build-object", str(project), str(output), "native"],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    detail = (proc.stdout or "") + (proc.stderr or "")
    return name, proc.returncode == 0 and output.is_file(), detail


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()

    if sys.platform != "win32":
        print(json.dumps({"format": "kinal-selfhost-manifest-native-v1", "skipped": True}))
        return 0

    compiler = args.compiler.resolve()
    root = args.root.resolve()
    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((root / "tests" / "manifest.json").read_text(encoding="utf-8"))
    cases = [
        case for case in manifest
        if "expect_error" not in case
        and supports_windows(case)
        and has_kinal_source(case)
    ]
    if len(cases) != EXPECTED_WINDOWS_CASES:
        raise SystemExit(
            f"Windows positive manifest baseline changed: "
            f"expected {EXPECTED_WINDOWS_CASES}, found {len(cases)}"
        )

    failures: list[tuple[str, str]] = []
    with ThreadPoolExecutor(max_workers=4) as executor:
        futures = {
            executor.submit(audit_case, compiler, root, out_dir, case): str(case["name"])
            for case in cases
        }
        for future in as_completed(futures):
            name, ok, detail = future.result()
            if not ok:
                failures.append((name, detail))

    if failures:
        for name, detail in sorted(failures):
            print(f"[{name}]\n{detail}", file=sys.stderr)
        return 1
    print(
        json.dumps(
            {
                "format": "kinal-selfhost-manifest-native-v1",
                "positive_cases": len(cases),
                "unsupported_cases": [],
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
