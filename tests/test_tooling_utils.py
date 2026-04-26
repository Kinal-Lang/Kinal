from __future__ import annotations

import io
import tarfile
import tempfile
import unittest
import urllib.error
import zipfile
from pathlib import Path
from unittest import mock

from infra.scripts.x.util import download_file, extract_tar_safely, extract_zip_safely


class ToolingUtilsTests(unittest.TestCase):
    def test_download_file_removes_partial_file_on_failure(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            dest = Path(temp_dir) / "artifact.bin"
            with mock.patch("urllib.request.urlopen", side_effect=urllib.error.URLError("dns down")):
                with self.assertRaises(SystemExit):
                    download_file("https://example.invalid/file", dest)
            self.assertFalse(dest.exists())
            self.assertFalse(dest.with_name("artifact.bin.tmp").exists())

    def test_extract_zip_safely_rejects_path_traversal(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            archive = root / "payload.zip"
            dest = root / "extract"
            with zipfile.ZipFile(archive, "w") as zf:
                zf.writestr("../escape.txt", "owned")
            with self.assertRaises(SystemExit):
                extract_zip_safely(archive, dest)
            self.assertFalse((root / "escape.txt").exists())

    def test_extract_tar_safely_rejects_path_traversal(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            archive = root / "payload.tar.xz"
            dest = root / "extract"
            with tarfile.open(archive, "w:xz") as tf:
                data = b"owned"
                info = tarfile.TarInfo("../escape.txt")
                info.size = len(data)
                tf.addfile(info, io.BytesIO(data))
            with self.assertRaises(SystemExit):
                extract_tar_safely(archive, dest, "r:xz")
            self.assertFalse((root / "escape.txt").exists())

    def test_safe_extract_preserves_regular_files(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            zip_archive = root / "good.zip"
            tar_archive = root / "good.tar.xz"
            zip_dest = root / "zip_extract"
            tar_dest = root / "tar_extract"

            with zipfile.ZipFile(zip_archive, "w") as zf:
                zf.writestr("nested/file.txt", "zip-ok")
            extract_zip_safely(zip_archive, zip_dest)
            self.assertEqual((zip_dest / "nested" / "file.txt").read_text(encoding="utf-8"), "zip-ok")

            with tarfile.open(tar_archive, "w:xz") as tf:
                data = b"tar-ok"
                info = tarfile.TarInfo("nested/file.txt")
                info.size = len(data)
                tf.addfile(info, io.BytesIO(data))
            extract_tar_safely(tar_archive, tar_dest, "r:xz")
            self.assertEqual((tar_dest / "nested" / "file.txt").read_text(encoding="utf-8"), "tar-ok")


if __name__ == "__main__":
    unittest.main()
