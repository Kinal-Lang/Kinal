"""VERSION remains the single authority for C/selfhost KinalVM build metadata."""
from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from infra.scripts.x import runtime_build, vm_metadata
from check_vm_version import check_vm_version


class VmVersionMetadataTests(unittest.TestCase):
    def test_checked_in_unit_matches_canonical_version(self):
        generated = vm_metadata.generate_kinalvm_build_info(check=True)
        self.assertEqual(generated, ROOT / vm_metadata.BUILD_INFO)

    def test_component_version_is_not_compiler_version(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "VERSION").write_text("kinal=91.2.3\nkinalvm=4.5.6\n")
            output = vm_metadata.generate_kinalvm_build_info(root)
            self.assertIn('Return "4.5.6";', output.read_text())
            self.assertNotIn("91.2.3", output.read_text())
            self.assertIn("Generated from VERSION:kinalvm", output.read_text())
            self.assertEqual(vm_metadata.generate_kinalvm_build_info(root, check=True), output)

    def test_stale_unit_fails_check_then_regenerates(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            version = root / "VERSION"
            version.write_text("kinal=91.2.3\nkinalvm=4.5.6\n")
            output = vm_metadata.generate_kinalvm_build_info(root)
            old = output.read_bytes()
            version.write_text("kinal=91.2.3\nkinalvm=4.5.7\n")
            with self.assertRaisesRegex(ValueError, "is stale"):
                vm_metadata.generate_kinalvm_build_info(root, check=True)
            self.assertEqual(output.read_bytes(), old)
            vm_metadata.generate_kinalvm_build_info(root)
            self.assertIn('Return "4.5.7";', output.read_text())

    def test_missing_unit_fails_check_without_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "VERSION").write_text("kinalvm=4.5.6\n")
            with self.assertRaisesRegex(ValueError, "is stale"):
                vm_metadata.generate_kinalvm_build_info(root, check=True)
            self.assertFalse((root / vm_metadata.BUILD_INFO).exists())

    def test_current_generation_does_not_rewrite_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "VERSION").write_text("kinalvm=4.5.6\n")
            output = vm_metadata.generate_kinalvm_build_info(root)
            before = output.stat().st_mtime_ns
            vm_metadata.generate_kinalvm_build_info(root)
            self.assertEqual(output.stat().st_mtime_ns, before)

    def test_invalid_or_duplicate_version_cannot_generate_source(self):
        for text in ("kinal=91.2.3\n", "kinalvm=4.5\n", "kinalvm=\n",
                     'kinalvm=4.5.6";\n', "kinalvm=4.5.6\nkinalvm=4.5.7\n"):
            with self.subTest(text=text), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / "VERSION").write_text(text)
                with self.assertRaisesRegex(ValueError, "exactly one"):
                    vm_metadata.generate_kinalvm_build_info(root)
                self.assertFalse((root / vm_metadata.BUILD_INFO).exists())

    def test_c_vm_build_refreshes_metadata_before_compiling(self):
        events = []
        compiler = Path("/isolated/c/kinal")
        bundle = compiler.parent
        with patch.object(runtime_build, "generate_kinalvm_build_info",
                          side_effect=lambda: events.append("generate")), \
             patch.object(runtime_build, "host_tag", return_value="macos-x64"), \
             patch.object(runtime_build, "run", side_effect=lambda *a, **kw: events.append("build")) as run:
            output = runtime_build.build_kinal_vm_binary(compiler, bundle)
        self.assertEqual(events, ["generate", "build"])
        self.assertEqual(run.call_args.args[0][0], compiler)
        self.assertEqual(output.parent, bundle)


    def test_banner_checks_both_aliases_from_an_unrelated_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "VERSION").write_text("kinal=91.2.3\nkinalvm=4.5.6\n")
            vm_metadata.generate_kinalvm_build_info(root)
            vm = root / "bundle/kinalvm"
            vm.parent.mkdir()
            (vm.parent / "VERSION").write_bytes((root / "VERSION").read_bytes())
            result = subprocess.CompletedProcess([], 0, "KinalVM 4.5.6\n", "")
            with patch("check_vm_version.subprocess.run", return_value=result) as run:
                report = check_vm_version(vm, root, require_packaged_version=True)
            self.assertTrue(report["ok"])
            self.assertEqual([call.args[0][-1] for call in run.call_args_list], ["--version", "-V"])
            for call in run.call_args_list:
                self.assertNotEqual(Path(call.kwargs["cwd"]), root)
                self.assertNotEqual(Path(call.kwargs["cwd"]), vm.parent)

    def test_banner_rejects_compiler_version(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "VERSION").write_text("kinal=91.2.3\nkinalvm=4.5.6\n")
            vm_metadata.generate_kinalvm_build_info(root)
            result = subprocess.CompletedProcess([], 0, "KinalVM 91.2.3\n", "")
            with patch("check_vm_version.subprocess.run", return_value=result):
                with self.assertRaises(AssertionError):
                    check_vm_version(root / "bundle/kinalvm", root)

    def test_banner_rejects_missing_or_stale_required_package_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "VERSION").write_text("kinal=91.2.3\nkinalvm=4.5.6\n")
            vm_metadata.generate_kinalvm_build_info(root)
            vm = root / "bundle/kinalvm"
            vm.parent.mkdir()
            result = subprocess.CompletedProcess([], 0, "KinalVM 4.5.6\n", "")
            with patch("check_vm_version.subprocess.run", return_value=result):
                with self.assertRaisesRegex(AssertionError, "missing packaged VERSION"):
                    check_vm_version(vm, root, require_packaged_version=True)
                (vm.parent / "VERSION").write_text("kinal=91.2.2\nkinalvm=4.5.6\n")
                with self.assertRaisesRegex(AssertionError, "differs from canonical VERSION"):
                    check_vm_version(vm, root, require_packaged_version=True)


if __name__ == "__main__":
    unittest.main()
