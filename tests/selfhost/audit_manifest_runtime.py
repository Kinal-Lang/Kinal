from __future__ import annotations

import argparse
import contextlib
import json
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

from audit_manifest_native import (
    entry_source,
    quote,
    related_sources,
    supports_windows,
)


EXPECTED_WINDOWS_RUNTIME_CASES = 163
ANSI_ESCAPE_RE = re.compile(r"\x1b\[[0-9;]*m")


def runtime_cases(manifest: list[dict[str, object]]) -> list[dict[str, object]]:
    cases: list[dict[str, object]] = []
    for case in manifest:
        if "expect_error" in case or not supports_windows(case):
            continue
        if case.get("compile_only") or "expected" not in case:
            continue
        cases.append(case)
    return cases


def case_sources(root: Path, case: dict[str, object]) -> tuple[Path, list[Path]]:
    raw_sources = case.get("files") or [case["file"]]
    sources = [(root / str(source)).resolve() for source in raw_sources]
    entry = entry_source(sources)
    if case.get("auto_link"):
        sources = related_sources(entry)
    return entry, sources


def runtime_link_options(
    root: Path, asset_dir: Path, case: dict[str, object]
) -> tuple[list[Path], list[Path], list[str]]:
    link_files: list[Path] = []
    lib_dirs: list[Path] = []
    libs: list[str] = []
    arguments = [str(value) for value in case.get("compiler_args", [])]
    index = 0
    while index < len(arguments):
        argument = arguments[index]
        value = arguments[index + 1] if index + 1 < len(arguments) else ""
        if argument == "--link-file":
            link_files.append(
                asset_dir / Path(value).name
                if "native_ffi" in value
                else (root / value).resolve()
            )
            index += 2
            continue
        if argument in ("-L", "--lib-dir"):
            lib_dirs.append(
                asset_dir if "out/test" in value.replace("\\", "/")
                else (root / value).resolve()
            )
            index += 2
            continue
        if argument in ("-l", "--lib"):
            libs.append(value)
            index += 2
            continue
        index += 1
    return link_files, lib_dirs, libs


def write_runtime_project(
    path: Path,
    name: str,
    entry: Path,
    sources: list[Path],
    link_files: list[Path],
    lib_dirs: list[Path],
    libs: list[str],
) -> None:
    files = ", ".join(f'"{quote(str(source))}"' for source in sources)
    project_name = re.sub(r"[^A-Za-z0-9_]", "", name) or "ManifestCase"
    link_lines: list[str] = []
    if link_files:
        values = ", ".join(f'"{quote(str(value))}"' for value in link_files)
        link_lines.append(f"            LinkFiles = [{values}];")
    if lib_dirs:
        values = ", ".join(f'"{quote(str(value))}"' for value in lib_dirs)
        link_lines.append(f"            LibDirs = [{values}];")
    if libs:
        values = ", ".join(f'"{quote(value)}"' for value in libs)
        link_lines.append(f"            Libs = [{values}];")
    link_block = ""
    if link_lines:
        link_block = "        Link\n        {\n" + "\n".join(link_lines) + "\n        }\n"
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
        f"{link_block}"
        "    }\n"
        "}\n"
    )
    path.write_text(content, encoding="utf-8")


def build_case(
    compiler: Path, root: Path, out_dir: Path, asset_dir: Path,
    case: dict[str, object]
) -> tuple[str, Path | None, str]:
    name = str(case["name"])
    entry, sources = case_sources(root, case)
    case_dir = out_dir / name
    case_dir.mkdir(parents=True, exist_ok=True)
    project = case_dir / "kinal.knproj"
    link_files, lib_dirs, libs = runtime_link_options(root, asset_dir, case)
    write_runtime_project(project, name, entry, sources, link_files, lib_dirs, libs)
    executable = case_dir / f"{name}.exe"
    proc = subprocess.run(
        [str(compiler), "build", str(project), str(executable), "native"],
        cwd=root,
        text=True,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        check=False,
        timeout=120,
    )
    detail = (proc.stdout or "") + (proc.stderr or "")
    if proc.returncode != 0 or not executable.is_file():
        return name, None, detail
    return name, executable, detail


def normalize_unhandled_runtime_output(output: str) -> str:
    text = ANSI_ESCAPE_RE.sub("", output).replace("\r\n", "\n")
    if "Unhandled Error:" not in text or "Stack Trace" not in text:
        return text

    message = ""
    frames: list[str] = []
    for line in text.split("\n"):
        stripped = line.strip()
        if stripped.startswith("Unhandled Error:"):
            message = stripped[len("Unhandled Error:") :].strip()
            continue
        if not stripped.startswith("at "):
            continue
        frame = stripped[3:]
        if "  " in frame:
            frame = frame.split("  ", 1)[0].rstrip()
        if frame.endswith("()"):
            frame = frame[:-2]
        if frame:
            frames.append(frame)
    if not message or not frames:
        return text
    return f"{message}\n{' -> '.join(frames)}\n"


def output_matches(actual: str, expected: str) -> bool:
    actual = actual.replace("\r\n", "\n")
    expected = expected.replace("\r\n", "\n")
    return actual == expected or normalize_unhandled_runtime_output(actual) == expected


def run_case(
    root: Path, asset_dir: Path, executable: Path,
    case: dict[str, object], legacy_tests: object
) -> tuple[bool, str]:
    for runtime_file in case.get("runtime_files", []):
        source = asset_dir / Path(str(runtime_file)).name
        shutil.copy2(source, executable.parent / source.name)
    if case.get("needs_openssl_runtime"):
        legacy_tests.copy_windows_openssl_runtime(executable.parent)

    stdin_text = case.get("stdin")
    try:
        fixture_responses: list[tuple[int, str]] | None = None
        if case.get("runtime_fixture") == "web_gc_roots":
            proc, fixture_responses = legacy_tests.run_web_gc_roots_fixture(executable)
        else:
            fixture = contextlib.nullcontext()
            if case.get("https_fixture") == "request_echo":
                fixture = legacy_tests.request_https_fixture()
            with fixture:
                proc = subprocess.run(
                    [str(executable)],
                    cwd=root,
                    text=True,
                    input=None if stdin_text is None else str(stdin_text),
                    capture_output=True,
                    encoding="utf-8",
                    errors="replace",
                    check=False,
                    timeout=30,
                )
    except subprocess.TimeoutExpired as error:
        return False, f"runtime timed out: {error}"

    if fixture_responses is not None and fixture_responses != [(200, "ok"), (200, "ok")]:
        return False, f"unexpected web fixture responses: {fixture_responses!r}"

    expected_output = str(case["expected"])
    expected_exit = int(case.get("expected_exit_code", 0))
    if proc.returncode != expected_exit or not output_matches(
        proc.stdout or "", expected_output
    ):
        return (
            False,
            f"expected exit={expected_exit}, stdout={expected_output!r}\n"
            f"actual exit={proc.returncode}, stdout={(proc.stdout or '')!r}\n"
            f"stderr={(proc.stderr or '')!r}",
        )
    return True, ""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()

    if sys.platform != "win32":
        print(json.dumps({"format": "kinal-selfhost-manifest-runtime-v1", "skipped": True}))
        return 0

    compiler = args.compiler.resolve()
    root = args.root.resolve()
    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(root / "tests"))
    import run_tests as legacy_tests

    # The manifest intentionally contains source-relative LinkFile attributes
    # and filesystem fixtures rooted under out/test.  Mirror the canonical
    # test-runner layout instead of placing native assets in an audit-only
    # directory, otherwise those paths fail before runtime behavior is tested.
    asset_dir = root / "out" / "test"
    asset_dir.mkdir(parents=True, exist_ok=True)
    legacy_tests.build_native_ffi_assets(asset_dir)
    manifest = json.loads((root / "tests" / "manifest.json").read_text(encoding="utf-8"))
    cases = runtime_cases(manifest)
    if len(cases) != EXPECTED_WINDOWS_RUNTIME_CASES:
        raise SystemExit(
            "Windows runtime manifest baseline changed: "
            f"expected {EXPECTED_WINDOWS_RUNTIME_CASES}, found {len(cases)}"
        )

    case_by_name = {str(case["name"]): case for case in cases}
    executables: dict[str, Path] = {}
    failures: list[tuple[str, str, str]] = []
    with ThreadPoolExecutor(max_workers=4) as executor:
        futures = {
            executor.submit(
                build_case, compiler, root, out_dir, asset_dir, case
            ): str(case["name"])
            for case in cases
        }
        for future in as_completed(futures):
            name, executable, detail = future.result()
            if executable is None:
                failures.append((name, "build", detail))
            else:
                executables[name] = executable

    for case in cases:
        name = str(case["name"])
        executable = executables.get(name)
        if executable is None:
            continue
        ok, detail = run_case(
            root, asset_dir, executable, case_by_name[name], legacy_tests
        )
        if not ok:
            failures.append((name, "runtime", detail))

    if failures:
        for name, phase, detail in sorted(failures):
            print(f"[{name}:{phase}]\n{detail}", file=sys.stderr)
        return 1

    print(
        json.dumps(
            {
                "format": "kinal-selfhost-manifest-runtime-v1",
                "runtime_cases": len(cases),
                "unsupported_cases": [],
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
