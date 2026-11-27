from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from infra.scripts.normalize_markdown_kinal_blocks import process_file


class MarkdownNormalizerTests(unittest.TestCase):
    def test_preserves_line_endings_and_trailing_newlines(self) -> None:
        for eol in (b"\n", b"\r\n"):
            for ending in (b"", eol, eol * 3):
                with self.subTest(eol=eol, ending=ending), tempfile.TemporaryDirectory() as directory:
                    path = Path(directory) / "example.md"
                    original = eol.join([b"# Example", b"```kinal", b"Function Main()", b"", b"{", b"}", b"```", b"Tail"]) + ending
                    expected = original.replace(b"Function Main()" + eol * 2, b"Function Main()" + eol)
                    path.write_bytes(original)
                    changed, stats = process_file(path, check_only=False)
                    self.assertTrue(changed)
                    self.assertEqual(stats.normalized_runs, 1)
                    self.assertEqual(path.read_bytes(), expected)
                    changed, stats = process_file(path, check_only=False)
                    self.assertFalse(changed)
                    self.assertEqual(stats.suspicious_runs, 0)
                    self.assertEqual(path.read_bytes(), expected)

    def test_check_only_reports_change_without_rewriting(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "example.md"
            original = b"```kinal\r\nMain()\r\n\r\n{\r\n}\r\n```\r\n"
            path.write_bytes(original)
            changed, stats = process_file(path, check_only=True)
            self.assertTrue(changed)
            self.assertEqual(stats.suspicious_runs, 1)
            self.assertEqual(path.read_bytes(), original)

    def test_unterminated_block_is_left_unmodified_and_not_reported(self) -> None:
        for check_only in (False, True):
            with self.subTest(check_only=check_only), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "example.md"
                original = b"Intro\n```kinal\nMain()\n\n{\n}\n"
                path.write_bytes(original)
                changed, stats = process_file(path, check_only=check_only)
                self.assertFalse(changed)
                self.assertEqual(stats.suspicious_runs, 0)
                self.assertEqual(stats.normalized_runs, 0)
                self.assertEqual(path.read_bytes(), original)

    def test_closed_block_before_unterminated_block_is_normalized(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "example.md"
            first = b"```kinal\nMain()\n\n{\n}\n```\n"
            unfinished = b"```kinal\nOther()\n\n{\n}\n"
            path.write_bytes(first + unfinished)
            changed, stats = process_file(path, check_only=False)
            self.assertTrue(changed)
            self.assertEqual(stats.normalized_runs, 1)
            self.assertEqual(path.read_bytes(), first.replace(b"Main()\n\n", b"Main()\n") + unfinished)


if __name__ == "__main__":
    unittest.main()
