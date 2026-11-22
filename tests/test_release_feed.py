from __future__ import annotations

import unittest
from unittest import mock

from infra.scripts.release_feed import fetch_releases


class ReleaseFeedTests(unittest.TestCase):
    def test_small_limit_uses_one_request(self) -> None:
        releases = [{"id": index} for index in range(16)]
        with mock.patch("infra.scripts.release_feed.github_request", return_value=releases) as request:
            self.assertEqual(fetch_releases("owner/repo", 16, "token"), releases)
        request.assert_called_once_with("/repos/owner/repo/releases?per_page=16&page=1", "token")

    def test_limit_above_page_size_fetches_and_trims_second_page(self) -> None:
        releases = [{"id": index} for index in range(200)]
        with mock.patch("infra.scripts.release_feed.github_request", side_effect=[releases[:100], releases[100:]]) as request:
            result = fetch_releases("owner/repo", 150, "token")
        self.assertEqual(result, releases[:150])
        self.assertEqual(request.call_args_list, [
            mock.call("/repos/owner/repo/releases?per_page=100&page=1", "token"),
            mock.call("/repos/owner/repo/releases?per_page=100&page=2", "token"),
        ])

    def test_exact_page_multiple_does_not_fetch_extra_page(self) -> None:
        releases = [{"id": index} for index in range(200)]
        with mock.patch("infra.scripts.release_feed.github_request", side_effect=[releases[:100], releases[100:]]) as request:
            self.assertEqual(fetch_releases("owner/repo", 200, None), releases)
        self.assertEqual(request.call_count, 2)

    def test_short_page_stops_before_limit(self) -> None:
        releases = [{"id": index} for index in range(125)]
        with mock.patch("infra.scripts.release_feed.github_request", side_effect=[releases[:100], releases[100:]]) as request:
            self.assertEqual(fetch_releases("owner/repo", 250, None), releases)
        self.assertEqual(request.call_count, 2)

    def test_empty_page_stops_pagination(self) -> None:
        releases = [{"id": index} for index in range(100)]
        with mock.patch("infra.scripts.release_feed.github_request", side_effect=[releases, []]) as request:
            self.assertEqual(fetch_releases("owner/repo", 250, None), releases)
        self.assertEqual(request.call_count, 2)

    def test_invalid_payload_on_later_page_is_rejected(self) -> None:
        releases = [{"id": index} for index in range(100)]
        with mock.patch("infra.scripts.release_feed.github_request", side_effect=[releases, {}]):
            with self.assertRaisesRegex(SystemExit, "unexpected GitHub releases payload"):
                fetch_releases("owner/repo", 150, None)

    def test_nonpositive_limit_is_rejected_without_request(self) -> None:
        for limit in (0, -1):
            with self.subTest(limit=limit):
                with mock.patch("infra.scripts.release_feed.github_request") as request:
                    with self.assertRaisesRegex(SystemExit, "release limit must be greater than zero"):
                        fetch_releases("owner/repo", limit, None)
                request.assert_not_called()


if __name__ == "__main__":
    unittest.main()
