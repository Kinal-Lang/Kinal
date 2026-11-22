"""Release promotion must use the six tested SDKs from the requested commit."""
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from infra.scripts import selfhost_release as release


class ReleasePromotionTests(unittest.TestCase):
    def test_only_successful_trusted_run_on_exact_commit_is_accepted(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "output"
            env = {"GITHUB_SHA": "abc", "BRANCH": "main", "GITHUB_REPOSITORY": "org/project",
                   "GITHUB_REF_TYPE": "branch", "RUN_ID": "17", "GITHUB_OUTPUT": str(output)}
            run = {"id": 17, "head_sha": "abc", "head_branch": "main", "workflow_id": 3,
                   "head_repository": {"full_name": "org/project"}, "conclusion": "success", "event": "push"}
            jobs = [{"name": host + " selfhost", "conclusion": "success"} for host in release.HOSTS]

            def api(command):
                data = {"actions/workflows/ci.yml": {"id": 3}, "actions/runs/17": run,
                        "actions/runs/17/jobs?per_page=100": {"jobs": jobs}}
                return json.dumps(data[command[-1].split("org/project/")[1]]).encode()

            with patch.dict(os.environ, env), patch.object(release.subprocess, "check_output", side_effect=api):
                release.resolve_run()
                self.assertIn("run_id=17", output.read_text())
                for key, value in (("head_sha", "older"), ("event", "pull_request"), ("workflow_id", 4),
                                   ("head_branch", "other"), ("head_repository", {"full_name": "fork/project"})):
                    with self.subTest(key=key), patch.dict(run, {key: value}), self.assertRaises(SystemExit):
                        release.resolve_run()
                jobs[-1]["conclusion"] = "skipped"
                with self.assertRaises(SystemExit):
                    release.resolve_run()

    def test_every_archive_digest_matches_its_bytes_before_promotion(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for host in release.HOSTS:
                lines = []
                for ext in (("zip",) if host.startswith("win-") else ("zip", "tar.gz", "tar.xz")):
                    path = root / f"Kinal-{release.read_version()}-{host}.{ext}"
                    path.write_bytes(host.encode())
                    lines.append(f"{release.sha256_file(path)}  {path.name}")
                (root / f"SHA256SUMS-{host}.txt").write_text("\n".join(lines))
            release.verify_assets(root)
            self.assertEqual(14, len((root / "SHA256SUMS.txt").read_text().splitlines()))
            (root / "SHA256SUMS.txt").unlink()
            path.write_bytes(b"wrong archive")
            with self.assertRaisesRegex(SystemExit, "Checksum mismatch"):
                release.verify_assets(root)
