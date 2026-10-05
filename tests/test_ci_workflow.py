"""Keep ordinary and stacked pull requests eligible for the same CI workflow."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CiWorkflowTriggerTests(unittest.TestCase):
    def test_stacked_pull_requests_have_no_base_branch_filter(self):
        text = (ROOT / '.github/workflows/ci.yml').read_text(encoding='utf-8')
        block = re.search(r'^  pull_request:\n(?P<body>(?:^    .*\n|^\s*\n)*)', text, re.M)
        self.assertIsNotNone(block, 'pull_request trigger is missing')
        body = block.group('body')
        self.assertNotRegex(body, re.compile(r'^    branches(?:-ignore)?:', re.M), 'stacked PR bases must remain eligible')
        self.assertIn('    paths:\n', body)
        for path in ('apps/**', 'libs/**', 'tests/**', '.github/workflows/ci.yml'):
            self.assertIn('"'+path+'"', body)

    def test_push_filter_and_dispatch_are_retained(self):
        text = (ROOT / '.github/workflows/ci.yml').read_text(encoding='utf-8')
        self.assertIn('  push:\n    branches: [ main, dev, "release/**" ]\n', text)
        self.assertIn('  workflow_dispatch:\n', text)


if __name__ == '__main__':
    unittest.main()
