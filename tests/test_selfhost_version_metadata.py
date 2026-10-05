"""Selfhost banners embed the canonical compiler component version."""
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from infra.scripts.x.compiler_metadata import BUILD_INFO, generate_selfhost_build_info


class SelfhostVersionMetadataTests(unittest.TestCase):
    def test_checked_in_version_is_current(self):
        self.assertEqual(generate_selfhost_build_info(check=True), ROOT / BUILD_INFO)

    def test_component_versions_remain_distinct(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'VERSION').write_text('kinal=9.2.3\nkinalvm=4.5.6\n')
            result = generate_selfhost_build_info(root)
            self.assertIn('Return "9.2.3";', result.read_text())
            self.assertIn('Safe Function string VMVersion()', result.read_text())
            self.assertIn('Return "4.5.6";', result.read_text())
            before = result.stat().st_mtime_ns
            generate_selfhost_build_info(root)
            self.assertEqual(result.stat().st_mtime_ns, before)

    def test_stale_version_is_rejected_then_refreshed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            version = root / 'VERSION'
            version.write_text('kinal=9.2.3\nkinalvm=4.5.6\n')
            result = generate_selfhost_build_info(root)
            version.write_text('kinal=9.2.4\nkinalvm=4.5.6\n')
            with self.assertRaisesRegex(ValueError, 'is stale'):
                generate_selfhost_build_info(root, check=True)
            generate_selfhost_build_info(root)
            self.assertIn('Return "9.2.4";', result.read_text())

    def test_vm_version_change_also_requires_regeneration(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            version = root / 'VERSION'
            version.write_text('kinal=9.2.3\nkinalvm=4.5.6\n')
            result = generate_selfhost_build_info(root)
            version.write_text('kinal=9.2.3\nkinalvm=4.5.7\n')
            with self.assertRaisesRegex(ValueError, 'is stale'):
                generate_selfhost_build_info(root, check=True)
            generate_selfhost_build_info(root)
            self.assertIn('Return "4.5.7";', result.read_text())

    def test_invalid_version_cannot_generate_code(self):
        for data in ('kinalvm=4.5.6\n', 'kinal=9.2\n', 'kinal=9.2.3";\n', 'kinal=9.2.3\nkinal=9.2.4\n'):
            with self.subTest(data=data), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'VERSION').write_text(data)
                with self.assertRaisesRegex(ValueError, 'exactly one'):
                    generate_selfhost_build_info(root)
                self.assertFalse((root / BUILD_INFO).exists())


if __name__ == '__main__':
    unittest.main()
