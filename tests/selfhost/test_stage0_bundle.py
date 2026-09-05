from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from infra.scripts.x import selfhost_ops  # noqa: E402


class Stage0BundleTests(unittest.TestCase):
    def make_bundle(self, path: Path) -> Path:
        for directory in ("llvm/lib", "runtime", "stdpkg", "linker"):
            (path / directory).mkdir(parents=True, exist_ok=True)
        compiler = path / selfhost_ops.exe_name("kinal")
        compiler.write_bytes(b"compiler-test-input")
        return compiler

    def test_same_resolved_bundle_does_not_delete_or_copy_itself(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            bundle = Path(directory) / "frozen"
            compiler = self.make_bundle(bundle)
            with (patch.object(selfhost_ops, "selfhost_stage0_dir", return_value=bundle),
                  patch.object(selfhost_ops, "sync_selfhost_stdlib_native_assets") as sync,
                  patch.object(selfhost_ops.shutil, "rmtree") as remove,
                  patch.object(selfhost_ops.shutil, "copytree") as copy):
                actual = selfhost_ops.freeze_selfhost_stage0_bundle(bundle / "unused" / "..")
                self.assertEqual(actual, compiler.resolve())
                self.assertEqual(compiler.read_bytes(), b"compiler-test-input")
                sync.assert_called_once_with(compiler.resolve())
                remove.assert_not_called()
                copy.assert_not_called()

    def test_overlapping_directories_are_rejected_before_deletion(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            parent = Path(directory) / "parent"
            child = parent / "child"
            self.make_bundle(parent)
            self.make_bundle(child)
            for source, destination in ((parent, child), (child, parent)):
                with (self.subTest(source=source),
                      patch.object(selfhost_ops, "selfhost_stage0_dir", return_value=destination),
                      patch.object(selfhost_ops.shutil, "rmtree") as remove,
                      patch.object(selfhost_ops.shutil, "copytree") as copy):
                    with self.assertRaisesRegex(SystemExit, "overlap"):
                        selfhost_ops.freeze_selfhost_stage0_bundle(source)
                    remove.assert_not_called()
                    copy.assert_not_called()

    def test_distinct_bundle_is_frozen(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source, destination = Path(directory) / "release", Path(directory) / "frozen"
            compiler = self.make_bundle(source)
            frozen = destination / compiler.name
            with (patch.object(selfhost_ops, "selfhost_stage0_dir", return_value=destination),
                  patch.object(selfhost_ops, "selfhost_stage0_exe", return_value=frozen),
                  patch.object(selfhost_ops, "sync_selfhost_stdlib_native_assets") as sync):
                self.assertEqual(selfhost_ops.freeze_selfhost_stage0_bundle(source), frozen)
                self.assertEqual(frozen.read_bytes(), compiler.read_bytes())
                sync.assert_called_once_with(frozen)

    def test_incomplete_same_bundle_is_not_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            bundle = Path(directory) / "frozen"
            bundle.mkdir()
            with patch.object(selfhost_ops, "selfhost_stage0_dir", return_value=bundle):
                with self.assertRaisesRegex(SystemExit, "incomplete"):
                    selfhost_ops.freeze_selfhost_stage0_bundle(bundle)


if __name__ == "__main__":
    unittest.main()
