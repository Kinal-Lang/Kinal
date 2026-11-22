"""The C maintenance line is available on demand."""
from pathlib import Path
import unittest


class CiWorkflowTriggerTests(unittest.TestCase):
    def test_c_maintenance_ci_is_manual(self):
        text = (Path(__file__).resolve().parents[1] / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        triggers = text.split("\non:\n", 1)[1].split("\nconcurrency:", 1)[0]
        self.assertIn("  workflow_dispatch:", triggers)
        self.assertNotIn("  push:", triggers)
        self.assertNotIn("  pull_request:", triggers)
