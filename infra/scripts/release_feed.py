from __future__ import annotations

import argparse
import json
import os
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from urllib import error, parse, request


API_ROOT = "https://api.github.com"
USER_AGENT = "kinal-release-feed/1.0"
DEFAULT_LIMIT = 16
DEFAULT_ASSET_EXTENSIONS = (".tar.xz", ".tar.gz", ".zip")


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def resolve_token(token: str | None, token_env: str | None) -> str | None:
    if token:
        return token
    if token_env:
        value = os.environ.get(token_env, "").strip()
        if value:
            return value
    return None


def github_headers(token: str | None) -> dict[str, str]:
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": USER_AGENT,
    }
    if token:
        headers["Authorization"] = f"Bearer {token}"
    return headers


def github_request(path: str, token: str | None) -> Any:
    req = request.Request(f"{API_ROOT}{path}", headers=github_headers(token))
    try:
        with request.urlopen(req) as response:
            return json.loads(response.read().decode("utf-8"))
    except error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise SystemExit(f"GitHub API request failed: {exc.code} {exc.reason}: {detail}") from exc


def normalize_asset(asset: dict[str, Any]) -> dict[str, Any]:
    return {
        "name": asset.get("name", ""),
        "label": asset.get("label") or "",
        "download_url": asset.get("browser_download_url", ""),
        "size_bytes": int(asset.get("size") or 0),
        "content_type": asset.get("content_type") or "",
    }


def release_sort_key(release: dict[str, Any]) -> str:
    return str(release.get("published_at") or release.get("created_at") or "")


def asset_sort_key(asset: dict[str, Any]) -> tuple[int, str]:
    name = str(asset.get("name") or "")
    checksum_rank = 1 if "sha256" in name.lower() else 0
    return (checksum_rank, name.lower())


def fetch_releases(repo: str, limit: int, token: str | None) -> list[dict[str, Any]]:
    payload = github_request(f"/repos/{repo}/releases?per_page={limit}", token)
    if not isinstance(payload, list):
        raise SystemExit("unexpected GitHub releases payload")
    return payload


def fetch_release_by_tag(repo: str, tag: str, token: str | None) -> dict[str, Any]:
    payload = github_request(f"/repos/{repo}/releases/tags/{parse.quote(tag, safe='')}", token)
    if not isinstance(payload, dict):
        raise SystemExit("unexpected GitHub release payload")
    return payload


def build_release_feed(repo: str, releases: list[dict[str, Any]], include_drafts: bool) -> dict[str, Any]:
    normalized_releases: list[dict[str, Any]] = []
    for release in sorted(releases, key=release_sort_key, reverse=True):
        if release.get("draft") and not include_drafts:
            continue
        assets = [normalize_asset(asset) for asset in release.get("assets") or []]
        assets.sort(key=asset_sort_key)
        normalized_releases.append(
            {
                "tag_name": release.get("tag_name", ""),
                "name": release.get("name") or release.get("tag_name") or "",
                "html_url": release.get("html_url", ""),
                "draft": bool(release.get("draft")),
                "prerelease": bool(release.get("prerelease")),
                "published_at": release.get("published_at") or release.get("created_at") or "",
                "body_markdown": release.get("body") or "",
                "assets": assets,
            }
        )
    return {
        "schema_version": 1,
        "generated_at": utc_now_iso(),
        "source_repo": repo,
        "releases": normalized_releases,
    }


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def preferred_extensions(args: argparse.Namespace) -> tuple[str, ...]:
    values = tuple(args.prefer_ext) if args.prefer_ext else DEFAULT_ASSET_EXTENSIONS
    return values


def asset_rank(name: str, host_tag: str, extensions: tuple[str, ...]) -> tuple[int, str]:
    lower = name.lower()
    if f"-{host_tag}" not in lower:
        return (len(extensions) + 1, lower)
    for index, ext in enumerate(extensions):
        if lower.endswith(ext.lower()):
            return (index, lower)
    return (len(extensions), lower)


def select_release_asset(release: dict[str, Any], host_tag: str, extensions: tuple[str, ...]) -> dict[str, Any]:
    assets = [asset for asset in release.get("assets") or [] if isinstance(asset, dict)]
    ranked = sorted(assets, key=lambda asset: asset_rank(str(asset.get("name") or ""), host_tag, extensions))
    if not ranked:
        raise SystemExit(f"no assets found on release {release.get('tag_name') or release.get('name')}")

    best = ranked[0]
    best_rank = asset_rank(str(best.get("name") or ""), host_tag, extensions)
    if best_rank[0] >= len(extensions):
        raise SystemExit(f"no asset matched host tag '{host_tag}' on release {release.get('tag_name') or release.get('name')}")
    return best


def write_github_output(path: Path, values: dict[str, str]) -> None:
    with path.open("a", encoding="utf-8") as handle:
        for key, value in values.items():
            handle.write(f"{key}={value}\n")


def cmd_generate(args: argparse.Namespace) -> int:
    token = resolve_token(args.token, args.token_env)
    releases = fetch_releases(args.repo, args.limit, token)
    payload = build_release_feed(args.repo, releases, args.include_drafts)
    write_json(args.output.resolve(), payload)
    print(f"[OK] wrote release feed: {args.output.resolve()}")
    print(f"[OK] releases in feed: {len(payload['releases'])}")
    return 0


def cmd_select_asset(args: argparse.Namespace) -> int:
    token = resolve_token(args.token, args.token_env)
    release = fetch_release_by_tag(args.repo, args.tag, token)
    asset = select_release_asset(release, args.host_tag, preferred_extensions(args))
    payload = {
        "asset_tag": release.get("tag_name", args.tag),
        "asset_name": str(asset.get("name") or ""),
        "asset_url": str(asset.get("browser_download_url") or ""),
    }
    if args.github_output:
        write_github_output(args.github_output.resolve(), payload)
    print(json.dumps(payload, ensure_ascii=False))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate Kinal website release feed data from GitHub releases")
    sub = parser.add_subparsers(dest="cmd", required=True)

    generate = sub.add_parser("generate", help="fetch releases and write releases.json for kinal.org")
    generate.add_argument("--repo", required=True, help="GitHub repository in owner/name form")
    generate.add_argument("--output", type=Path, required=True, help="path to write releases.json")
    generate.add_argument("--limit", type=int, default=DEFAULT_LIMIT, help="maximum number of releases to fetch")
    generate.add_argument("--include-drafts", action="store_true", help="include draft releases in the output feed")
    generate.add_argument("--token", help="explicit GitHub token")
    generate.add_argument("--token-env", help="environment variable containing a GitHub token")
    generate.set_defaults(func=cmd_generate)

    select_asset = sub.add_parser("select-asset", help="resolve a published release asset URL for deployment")
    select_asset.add_argument("--repo", required=True, help="GitHub repository in owner/name form")
    select_asset.add_argument("--tag", required=True, help="release tag name, for example v1.2.3")
    select_asset.add_argument("--host-tag", required=True, help="target host tag, for example linux-x64")
    select_asset.add_argument("--prefer-ext", action="append", help="preferred archive extension order")
    select_asset.add_argument("--github-output", type=Path, help="optional path to append GitHub Actions outputs")
    select_asset.add_argument("--token", help="explicit GitHub token")
    select_asset.add_argument("--token-env", help="environment variable containing a GitHub token")
    select_asset.set_defaults(func=cmd_select_asset)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
