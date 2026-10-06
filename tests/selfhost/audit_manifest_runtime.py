from __future__ import annotations

import argparse
import contextlib
import json
import os
import re
import shutil
import subprocess
import sys
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

from audit_manifest_native import (
    entry_source,
    quote,
    related_sources,
    exclusion_reason,
    manifest_exclusions,
    host_platform,
)


EXPECTED_HOST_RUNTIME_CASES = {"windows": 188, "linux": 186, "macos": 184}
ANSI_ESCAPE_RE = re.compile(r"\x1b\[[0-9;]*m")


def runtime_cases(manifest: list[dict[str, object]], host: str | None = None) -> list[dict[str, object]]:
    host = host or host_platform()
    return [case for case in manifest if exclusion_reason(case, host, runtime=True) is None]


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


def native_ffi_commands(root: Path, asset_dir: Path, host: str, llvm: Path) -> list[list[str]]:
    """Preserve manifest fixture names while building actual host-format assets.

    The .obj/.dll names are literals in existing source-level LinkFile and
    LoadLibrary tests. They do not select COFF/PE on POSIX: clang selects the
    object format, and the native loader accepts the shared library by content.
    Keep a distinct import-library alias so ffi_lib still tests a static archive
    and ffi_dll/ffi_abi still test real shared-library linking/loading.
    """
    if host not in {"linux", "macos"}:
        raise ValueError(f"Unsupported POSIX FFI fixture host: {host}")
    obj = asset_dir / "native_ffi.obj"
    shared = asset_dir / "native_ffi.dll"
    shared_options = (["-dynamiclib", "-Wl,-install_name,@loader_path/native_ffi.dll"]
                      if host == "macos" else ["-shared", "-Wl,-soname,native_ffi.dll"])
    return [
        [str(llvm / "clang"), "-c", str(root / "tests" / "ffi_native" / "native_ffi.c"),
         "-o", str(obj), "-fPIC", "-ffreestanding", "-fno-builtin", "-fno-stack-protector"],
        [str(llvm / "llvm-ar"), *(["--format=darwin"] if host == "macos" else []),
         "rcs", str(asset_dir / "libnative_ffi.a"), str(obj)],
        [str(llvm / "clang"), *shared_options, str(obj), "-o", str(shared)],
    ]


def build_native_ffi_assets(root: Path, asset_dir: Path, legacy_tests: object) -> None:
    asset_dir.mkdir(parents=True, exist_ok=True)
    host = host_platform()
    if host == "windows":
        legacy_tests.build_native_ffi_assets(asset_dir)
        return
    # Follow selfhost's selected LLVM toolchain instead of assuming system clang.
    sys.path.insert(0, str(root))
    from infra.scripts.x.llvm import detect_llvm_dir, llvm_bin_dir

    llvm = llvm_bin_dir(detect_llvm_dir())
    for command in native_ffi_commands(root, asset_dir, host, llvm):
        proc = subprocess.run(command, cwd=root, text=True, capture_output=True,
                              encoding="utf-8", errors="replace", timeout=120, check=False)
        if proc.returncode:
            raise RuntimeError(f"FFI fixture command failed: {command!r}\n"
                               + (proc.stdout or "") + (proc.stderr or ""))
    suffix = ".dylib" if host == "macos" else ".so"
    shutil.copy2(asset_dir / "native_ffi.dll", asset_dir / ("libnative_ffi_import" + suffix))


def runtime_environment(executable: Path, case: dict[str, object]) -> dict[str, str]:
    env = os.environ.copy()
    # Only dynamic FFI fixtures need loader search-path changes. Preserve any
    # user-provided entries, and leave unrelated runtimes/fixtures untouched.
    dynamic_ffi = any(Path(str(value)).name == "native_ffi.dll"
                      for value in case.get("runtime_files", []))
    if sys.platform != "win32" and dynamic_ffi:
        key = "DYLD_LIBRARY_PATH" if sys.platform == "darwin" else "LD_LIBRARY_PATH"
        previous = env.get(key, "")
        env[key] = str(executable.parent) + (os.pathsep + previous if previous else "")
    return env


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
    executable = case_dir / (name + (".exe" if sys.platform == "win32" else ""))
    executable.unlink(missing_ok=True)
    try:
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
        ok = proc.returncode == 0 and executable.is_file()
        if not ok:
            detail = f"exit={proc.returncode}; executable_exists={executable.is_file()}\n" + detail
    except (subprocess.TimeoutExpired, OSError) as error:
        ok, detail = False, str(error)
    (case_dir / "build.log").write_text(detail, encoding="utf-8")
    return name, executable if ok else None, detail


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
    stdin_text = case.get("stdin")
    try:
        for runtime_file in case.get("runtime_files", []):
            source = asset_dir / Path(str(runtime_file)).name
            shutil.copy2(source, executable.parent / source.name)
        if sys.platform == "win32" and case.get("needs_openssl_runtime"):
            legacy_tests.copy_windows_openssl_runtime(executable.parent)
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
                    env=runtime_environment(executable, case),
                    text=True,
                    input=None if stdin_text is None else str(stdin_text),
                    capture_output=True,
                    encoding="utf-8",
                    errors="replace",
                    check=False,
                    timeout=30,
                )
    except (subprocess.TimeoutExpired, OSError) as error:
        return False, f"runtime fixture/execution failed: {error}"

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
    return True, (f"exit={proc.returncode}\nstdout={(proc.stdout or '')!r}\n"
                  f"stderr={(proc.stderr or '')!r}\n")


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
    if host not in EXPECTED_HOST_RUNTIME_CASES:
        raise SystemExit(f"Unsupported manifest audit host: {host}")
    if not compiler.is_file():
        raise SystemExit(f"Compiler not found: {compiler}")
    sys.path.insert(0, str(root / "tests"))
    import run_tests as legacy_tests

    # Source-relative LinkFile attributes and filesystem fixtures intentionally
    # use out/test. Preserve the canonical runner's layout on every host.
    asset_dir = root / "out" / "test"
    asset_dir.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((root / "tests" / "manifest.json").read_text(encoding="utf-8"))
    candidates = runtime_cases(manifest, host)
    expected = EXPECTED_HOST_RUNTIME_CASES[host]
    if len(candidates) != expected:
        raise SystemExit(
            f"{host} runtime manifest baseline changed: expected {expected}, found {len(candidates)}"
        )
    cases = candidates
    ffi_names = {str(case["name"]) for case in cases if legacy_tests.case_needs_native_ffi(case)}
    fixture_error = ""
    if ffi_names:
        try:
            build_native_ffi_assets(root, asset_dir, legacy_tests)
        except (subprocess.SubprocessError, OSError, RuntimeError, SystemExit) as error:
            fixture_error = str(error)

    case_by_name = {str(case["name"]): case for case in cases}
    executables: dict[str, Path] = {}
    failures: list[tuple[str, str, str]] = [
        (name, "fixture", fixture_error) for name in sorted(ffi_names) if fixture_error
    ]
    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        futures = {
            executor.submit(
                build_case, compiler, root, out_dir, asset_dir, case
            ): str(case["name"])
            for case in cases if not (fixture_error and case["name"] in ffi_names)
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
        (executable.parent / "runtime.log").write_text(detail, encoding="utf-8")
        if not ok:
            failures.append((name, "runtime", detail))

    for name, phase, detail in sorted(failures):
        print(f"[{name}:{phase}]\n{detail}", file=sys.stderr)
    exclusions = manifest_exclusions(manifest, host, runtime=True)
    report = {
        "format": "kinal-selfhost-manifest-runtime-v1",
        "host": host,
        "runtime_candidates": len(candidates),
        "runtime_cases": len(cases),
        "built": len(executables),
        "passed": len(cases) - len(failures),
        "unsupported_cases": [],
        "excluded_cases": exclusions,
        "excluded_case_counts": dict(Counter(case["reason"] for case in exclusions)),
        "failures": [{"name": name, "phase": phase, "detail": detail}
                     for name, phase, detail in sorted(failures)],
    }
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main())
