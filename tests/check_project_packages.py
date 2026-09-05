"""Shared C stage0/selfhost package-resolution contract tests."""
from __future__ import annotations

import argparse
import json
import struct
import subprocess
import tempfile
from pathlib import Path


def run(command: list[str], root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=root, capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=120, check=False)


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def source(unit: str, value: str) -> str:
    return (f"Unit {unit};\nSafe Function string Value()\n"
            f"{{ Return {json.dumps(value)}; }}\n")


def package(root: Path, folder: str, name: str, version: str,
            units: dict[str, str], **fields: object) -> Path:
    directory = root / folder
    manifest = {"kind": "library", "name": name, "version": version, "source_root": "src"}
    manifest.update(fields)
    write(directory / "package.knpkg.json", json.dumps(manifest, ensure_ascii=False))
    for unit, value in units.items():
        write(directory / "src" / f"{unit}.kn", source(unit, value))
    return directory


def archive(path: Path, name: str, files: dict[str, str]) -> None:
    """Use the documented KNKLIB1 wire format, including an embedded manifest."""
    manifest = json.dumps({"kind": "library", "name": name, "version": "1.0",
                           "source_root": "src"}).encode()
    producer = b"project-package-regression"
    data = bytearray(b"KNKLIB1\0")
    data.extend(struct.pack("<IIII", 1, len(producer), len(manifest), len(files)))
    data.extend(producer)
    data.extend(manifest)
    for name, text in files.items():
        name_bytes, contents = name.encode(), text.encode()
        data.extend(struct.pack("<IQ", len(name_bytes), len(contents)))
        data.extend(name_bytes)
        data.extend(contents)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


class ProjectPackageChecks:
    def __init__(self, compiler: Path, root: Path, work: Path) -> None:
        self.compiler, self.root, self.work = compiler, root, work
        self.count = 0

    def project(self, name: str, units: list[str], *, top: str = "",
                profile: str = "", source_options: str = "Mode = ReachableUnits;") -> Path:
        directory = self.work / name
        imports = "".join(f"Get {unit};\n" for unit in units)
        calls = "".join(f"    IO.Console.PrintLine({unit}.Value());\n" for unit in units)
        write(directory / "src" / "Main.kn",
              f"Unit Tests.PackageEntry;\nGet IO.Console;\n{imports}"
              f"Safe Static Function int Main()\n{{\n{calls}    Return 0;\n}}\n")
        write(directory / "kinal.knproj",
              "Project PackageContract\n{\n"
              f"    {top}\n"
              '    SourceSet "main" { Roots = ["src"]; Include = ["**/*.kn"]; }\n'
              '    DefaultProfile = "test";\n'
              '    Profile "test"\n    {\n'
              '        Source { Entry = "src/Main.kn"; Sets = ["main"]; '
              f"{source_options} }}\n"
              "        Build { Backend = Native; Environment = Hosted; }\n"
              f"        {profile}\n    }}\n}}\n")
        return directory

    def check(self, project: Path, label: str, expected: str | None,
              diagnostic: str = "") -> None:
        suffix = ".exe" if self.compiler.suffix.lower() == ".exe" else ""
        output = self.work / f"result-{self.count}{suffix}"
        command = [str(self.compiler), "build", "--project", str(project),
                   "--profile", "test", "-o", str(output)]
        if expected is None:
            command.extend(["--emit", "ir"])
        process = run(command, self.root)
        detail = (process.stdout or "") + (process.stderr or "")
        if expected is None:
            if process.returncode == 0 or (diagnostic and diagnostic not in detail):
                raise SystemExit(f"[FAIL] {label}: expected rejection\n{detail}")
        else:
            if process.returncode != 0:
                raise SystemExit(f"[FAIL] {label}: build\n{detail}")
            execution = run([str(output)], self.root)
            if execution.returncode != 0 or execution.stdout.replace("\r\n", "\n") != expected:
                raise SystemExit(f"[FAIL] {label}: runtime\n{execution}")
        self.count += 1
        print(f"[OK] {label}", flush=True)

    def resolution(self) -> None:
        project = self.project("priority", ["Probe.Shared", "Probe.OfficialOnly", "Probe.Profile"],
                               top='Packages { Roots = ["ordinary"]; OfficialRoots = ["official"]; }',
                               profile='Packages { OfficialRoots = ["profile-official"]; }')
        # Package identity and Unit identity differ. Only the overlapping Unit
        # should be shadowed; the other Unit from the same official package survives.
        package(project / "ordinary", "normal", "Probe.Package", "1.0",
                {"Probe.Shared": "ordinary"})
        package(project / "official", "official", "Probe.Package", "2.0",
                {"Probe.Shared": "official", "Probe.OfficialOnly": "official-only"})
        package(project / "profile-official", "extra", "IO.PackageProbe", "1.0",
                {"Probe.Profile": "profile"})
        self.check(project, "package_unit_precedence", "ordinary\nofficial-only\nprofile\n")
        write(project / "src" / "Shadow.kn", source("Probe.Shared", "project"))
        self.check(project, "project_unit_precedence", "project\nofficial-only\nprofile\n")

        project = self.project("source-files", ["Probe.Explicit"],
                               top='Packages { Roots = ["packages"]; }')
        directory = package(project / "packages", "explicit", "Probe.Explicit", "1",
                            {"Probe.Explicit": "explicit"},
                            source_files=["src/Probe.Explicit.kn"])
        write(directory / "src" / "Ignored.kn", "Unit Probe.Explicit;\nINVALID SOURCE\n")
        self.check(project, "package_source_files_precedence", "explicit\n")

        project = self.project("automatic", ["Probe.Automatic"])
        package(project / "kpkg", "auto", "Probe.Auto", "1", {"Probe.Automatic": "automatic"})
        self.check(project, "project_automatic_kpkg", "automatic\n")

        project = self.project("equal-version", ["Probe.Equal"],
                               top='Packages { Roots = ["first", "second"]; }')
        package(project / "first", "one", "Probe.Equal", "1.0", {"Probe.Equal": "first"})
        package(project / "second", "two", "Probe.Equal", "1.0.0", {"Probe.Equal": "second"})
        write(project / "first" / "out" / "bad" / "package.knpkg.json", "{invalid}")
        self.check(project, "equal_version_root_order_and_ignored_tree", "first\n")

        project = self.project("no-discovery", ["Probe.Official"],
                               top='Packages { OfficialRoots = ["official"]; }',
                               source_options="AutoDiscovery = false;")
        package(project / "official", "one", "IO.Probe", "1", {"Probe.Official": "official"})
        self.check(project, "official_import_without_local_discovery", "official\n")

        project = self.project("transitive", ["Probe.Dependency"],
                               top='Packages { Roots = ["packages"]; }')
        directory = package(project / "packages", "one", "Probe.Dep", "1", {})
        write(directory / "src" / "Dependency.kn",
              "Unit Probe.Dependency;\nGet Probe.Local;\n"
              "Safe Function string Value() { Return Probe.Local.Value(); }\n")
        write(project / "src" / "Local.kn", source("Probe.Local", "transitive-local"))
        self.check(project, "package_import_reaches_local_unit", "transitive-local\n")

    def archives(self) -> None:
        project = self.project("archives", ["Probe.Archive"],
                               top='Packages { Roots = ["packages"]; }')
        directory = package(project / "packages", "one", "Probe.Archive", "1",
                            {"Probe.Archive": "fallback"}, klib="lib/probe.klib")
        self.check(project, "missing_klib_source_fallback", "fallback\n")
        packed = directory / "lib" / "probe.klib"
        archive(packed, "Probe.Archive", {"src/Old.kn": source("Probe.Archive", "old")})
        old_size = packed.stat().st_size
        self.check(project, "klib_preferred_over_loose_sources", "old\n")
        archive(packed, "Probe.Archive", {"src/New.kn": source("Probe.Archive", "new")})
        assert packed.stat().st_size == old_size
        self.check(project, "same_size_klib_replacement_has_no_stale_units", "new\n")
        archive(packed, "IO.Reserved", {"src/New.kn": source("Probe.Archive", "new")})
        self.check(project, "embedded_reserved_namespace", None, "reserved package namespace")

        project = self.project("isolated-archives", ["Probe.Shared", "Probe.OfficialOnly"],
                               top='Packages { Roots = ["ordinary"]; OfficialRoots = ["official"]; }')
        ordinary = package(project / "ordinary", "one", "Probe.Same", "1", {}, klib="lib/a.klib")
        official = package(project / "official", "one", "Probe.Same", "1", {}, klib="lib/b.klib")
        archive(ordinary / "lib/a.klib", "Probe.Same", {"src/Shared.kn": source("Probe.Shared", "normal")})
        archive(official / "lib/b.klib", "Probe.Same",
                {"src/Shared.kn": source("Probe.Shared", "shadow"),
                 "src/Other.kn": source("Probe.OfficialOnly", "other")})
        self.check(project, "equal_identity_archive_cache_isolation", "normal\nother\n")

    def manifests(self) -> None:
        project = self.project("manifest-errors", ["Probe.Manifest"],
                               top='Packages { Roots = ["packages"]; }')
        directory = package(project / "packages", "one", "IO.NotAllowed", "1",
                            {"Probe.Manifest": "valid"})
        self.check(project, "ordinary_reserved_namespace", None, "reserved package namespace")
        path = directory / "package.knpkg.json"
        base = '{"name":"Probe.Manifest","source_root":"src"'
        invalid = {
            "missing_close": base,
            "trailing_comma": base + ",}",
            "trailing_data": base + "} garbage",
            "missing_comma": base + ' "version":"1"}',
            "unclosed_string_array": base + ',"source_files":["file"}',
            "wrong_field_type": base + ',"version":2}',
            "invalid_escape": base + r',"summary":"\q"}',
            "incomplete_unicode": base + r',"summary":"\u123"}',
            "lone_surrogate": base + r',"summary":"\ud800"}',
            "low_surrogate": base + r',"summary":"\udc00"}',
            "nul_escape": base + r',"summary":"\u0000"}',
            "raw_control": base + ',"summary":"bad\tcontrol"}',
            "invalid_literal": base + ',"extra":truth}',
            "leading_zero": base + ',"extra":01}',
            "missing_fraction": base + ',"extra":1.}',
            "missing_exponent": base + ',"extra":1e+}',
            "invalid_nested_object": base + ',"extra":{"a" 1}}',
            "excessive_depth": base + ',"extra":' + "[" * 70 + "0" + "]" * 70 + "}",
        }
        for name, contents in invalid.items():
            write(path, contents)
            self.check(project, f"manifest_{name}", None, "invalid package manifest")

        unicode_project = self.project("unicode", ["Probe.Unicode"],
                                       top='Packages { Roots = ["first", "second"]; }')
        package(unicode_project / "first", "one", "Probe.包📦", "1",
                {"Probe.Unicode": "old"})
        second = package(unicode_project / "second", "two", "Probe.包📦", "2",
                         {"Probe.Unicode": "unicode"})
        manifest = {"name": "Probe.包📦", "version": "2", "source_root": "src",
                    "summary": "escaped\n\t\b\f\r\"\\/",
                    "extra": {"valid": [True, False, None, -1.2e3, {"nested": "ok"}]}}
        write(second / "package.knpkg.json", json.dumps(manifest, ensure_ascii=True)
              .replace('"src"', r'"\u0073rc"'))
        self.check(unicode_project, "manifest_utf8_and_surrogate_pair_identity", "unicode\n")

        valid = {"name": "Probe.Manifest", "source_root": "src",
                 "summary": "escaped\n\t\b\f\r\"\\/",
                 "extra": {"valid": [True, False, None, -1.2e3, {"nested": "ok"}]}}
        write(path, json.dumps(valid))
        self.check(project, "manifest_escapes_and_unknown_metadata", "valid\n")

    def execute(self) -> None:
        self.resolution()
        self.archives()
        self.manifests()
        print(f"[OK] project package contract: {self.count} cases", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    args.out_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="packages-", dir=args.out_dir.resolve()) as temporary:
        ProjectPackageChecks(args.compiler.resolve(), root, Path(temporary)).execute()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
