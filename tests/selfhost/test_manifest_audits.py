from __future__ import annotations

import contextlib
import io
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

import audit_manifest_native as native
import audit_manifest_runtime as runtime


ROOT = Path(__file__).resolve().parents[2]


class ManifestAuditTests(unittest.TestCase):
    def test_host_baselines(self) -> None:
        manifest = json.loads((ROOT / "tests" / "manifest.json").read_text(encoding="utf-8"))
        for host, expected in native.EXPECTED_HOST_CASES.items():
            with self.subTest(host=host, kind="native"):
                self.assertEqual(len(native.positive_cases(manifest, host)), expected)
        for host, expected in runtime.EXPECTED_HOST_RUNTIME_CASES.items():
            with self.subTest(host=host, kind="runtime"):
                self.assertEqual(len(runtime.runtime_cases(manifest, host)), expected)

    def test_platform_aliases_and_exclusions(self) -> None:
        for alias in ("darwin", "mac", "osx", "macos"):
            self.assertTrue(native.supports_host({"platforms": [alias]}, "macos"))
        self.assertTrue(native.supports_host({"platforms": ["gnu/linux"]}, "linux"))
        self.assertTrue(native.supports_host({"platforms": ["Win32"]}, "windows"))
        self.assertFalse(native.supports_host({"skip_platforms": ["gnu/linux"]}, "linux"))
        self.assertFalse(native.supports_host({"platforms": ["windows"]}, "linux"))
        self.assertFalse(native.supports_host({"platforms": []}, "linux"))
        self.assertTrue(native.supports_host({"skip_platforms": None}, "linux"))

    def test_runtime_filters_do_not_change_expectations(self) -> None:
        case = {"name": "example", "file": "example.kn", "expected": "exact\n"}
        excluded = [{**case, "expect_error": "bad"}, {**case, "compile_only": True},
                    {"name": "no_output", "file": "example.kn"},
                    {**case, "file": "example.knc"}, {**case, "platforms": ["windows"]}]
        self.assertEqual(runtime.runtime_cases([case, *excluded], "linux"), [case])
        self.assertEqual(case["expected"], "exact\n")

    def test_exclusions_account_for_every_manifest_case(self) -> None:
        manifest = json.loads((ROOT / "tests" / "manifest.json").read_text(encoding="utf-8"))
        for host in native.EXPECTED_HOST_CASES:
            self.assertEqual(len(native.positive_cases(manifest, host))
                             + len(native.manifest_exclusions(manifest, host)), len(manifest))
            self.assertEqual(len(runtime.runtime_cases(manifest, host))
                             + len(native.manifest_exclusions(manifest, host, runtime=True)), len(manifest))
        exclusions = native.manifest_exclusions(manifest, "linux", runtime=True)
        reasons = {case["name"]: case["reason"] for case in exclusions}
        self.assertEqual(reasons["stdlib_web_compile"], "compile-only case")
        self.assertEqual(reasons["const_if"], "not enabled for linux")
        for name in ("ffi_obj", "ffi_lib", "ffi_dll", "ffi_abi", "ffi_attr_lib", "ffi_attr_file"):
            self.assertNotIn(name, reasons)

    def test_ffi_commands_keep_host_formats_and_distinct_link_modes(self) -> None:
        root, assets, llvm = Path("/repo"), Path("/repo/out/test"), Path("/llvm/bin")
        linux = runtime.native_ffi_commands(root, assets, "linux", llvm)
        self.assertIn("-fPIC", linux[0])
        self.assertIn("/repo/out/test/native_ffi.obj", linux[0])
        self.assertIn("/repo/out/test/libnative_ffi.a", linux[1])
        self.assertIn("-shared", linux[2])
        self.assertIn("-Wl,-soname,native_ffi.dll", linux[2])
        macos = runtime.native_ffi_commands(root, assets, "macos", llvm)
        self.assertIn("--format=darwin", macos[1])
        self.assertIn("-dynamiclib", macos[2])
        self.assertIn("-Wl,-install_name,@loader_path/native_ffi.dll", macos[2])
        with self.assertRaises(ValueError):
            runtime.native_ffi_commands(root, assets, "windows", llvm)

    def test_dynamic_ffi_loader_environment_is_scoped(self) -> None:
        with patch.object(runtime.sys, "platform", "linux"), patch.dict(os.environ, {"LD_LIBRARY_PATH": "/existing"}):
            self.assertEqual(runtime.runtime_environment(Path("/audit/case/program"), {})["LD_LIBRARY_PATH"], "/existing")
            env = runtime.runtime_environment(Path("/audit/case/program"), {"runtime_files": ["native_ffi.dll"]})
            self.assertEqual(env["LD_LIBRARY_PATH"], "/audit/case:/existing")
            self.assertEqual(os.environ["LD_LIBRARY_PATH"], "/existing")
            unrelated = runtime.runtime_environment(Path("/audit/program"), {"runtime_files": ["fixture.txt"]})
            self.assertEqual(unrelated["LD_LIBRARY_PATH"], "/existing")

    def test_stale_object_cannot_make_failed_compilation_pass(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "main.kn").write_text("Function int Main() { Return 0; }", encoding="utf-8")
            case_dir = root / "out" / "example"
            case_dir.mkdir(parents=True)
            output = case_dir / ("example.obj" if os.name == "nt" else "example.o")
            output.write_bytes(b"stale")
            with patch.object(native.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "", "")):
                name, ok, detail = native.audit_case(root / "compiler", root, root / "out", {"name": "example", "file": "main.kn"})
            self.assertFalse(ok)
            self.assertFalse(output.exists())
            self.assertIn("object_exists=False", detail)

    def test_build_timeout_is_a_case_failure(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "main.kn").write_text("Function int Main() { Return 0; }", encoding="utf-8")
            with patch.object(native.subprocess, "run", side_effect=subprocess.TimeoutExpired("compiler", 120)):
                _, ok, detail = native.audit_case(root / "compiler", root, root / "out", {"name": "example", "file": "main.kn"})
            self.assertFalse(ok)
            self.assertIn("120", detail)
            self.assertEqual((root / "out/example/build.log").read_text(encoding="utf-8"), detail)

    def test_missing_runtime_fixture_is_a_case_failure(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            ok, detail = runtime.run_case(root, root / "assets", root / "program",
                                          {"runtime_files": ["native_ffi.dll"], "expected": ""}, object())
            self.assertFalse(ok)
            self.assertIn("native_ffi.dll", detail)

    def test_windows_ffi_keeps_the_canonical_builder(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            legacy = Mock()
            with patch.object(runtime, "host_platform", return_value="windows"):
                runtime.build_native_ffi_assets(root, root / "assets", legacy)
            legacy.build_native_ffi_assets.assert_called_once_with(root / "assets")

    def test_failed_native_audit_still_writes_a_complete_json_report(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "tests").mkdir()
            (root / "tests/manifest.json").write_text(json.dumps([
                {"name": "first", "file": "first.kn"},
                {"name": "second", "file": "second.kn"},
                {"name": "foreign", "file": "foreign.kn", "platforms": ["windows"]},
            ]), encoding="utf-8")
            compiler = root / "compiler"
            compiler.touch()
            output = root / "report.json"
            argv = ["audit", "--compiler", str(compiler), "--root", str(root),
                    "--out-dir", str(root / "out"), "--output", str(output), "--jobs", "1"]
            def audit(_compiler: Path, _root: Path, _out: Path, case: dict[str, object]):
                return str(case["name"]), case["name"] == "first", "failed" if case["name"] == "second" else ""
            with patch.object(native.sys, "argv", argv), patch.object(native, "host_platform", return_value="linux"), \
                 patch.dict(native.EXPECTED_HOST_CASES, {"linux": 2}), \
                 patch.object(native, "audit_case", side_effect=audit), \
                 contextlib.redirect_stdout(io.StringIO()) as stdout, contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(native.main(), 1)
            report = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(json.loads(stdout.getvalue()), report)
            self.assertEqual(report["positive_cases"], 2)
            self.assertEqual(report["passed"], 1)
            self.assertEqual(report["failures"][0]["name"], "second")
            self.assertEqual(report["excluded_case_counts"], {"not enabled for linux": 1})
            self.assertEqual(report["unsupported_cases"], [])

    def test_output_comparison_remains_exact(self) -> None:
        self.assertTrue(runtime.output_matches("hello\r\n", "hello\n"))
        self.assertFalse(runtime.output_matches("hello \n", "hello\n"))
        self.assertFalse(runtime.output_matches("hello\nextra\n", "hello\n"))


if __name__ == "__main__":
    unittest.main()
