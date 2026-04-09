from __future__ import annotations

from pathlib import Path

from .context import THIRD_PARTY_REPO_URL, THIRD_PARTY_ROOT
from .util import resolve_tool, run


def third_party_present() -> bool:
    return THIRD_PARTY_ROOT.exists()


def sparse_checkout_patterns() -> list[str]:
    return [
        ".gitignore",
        "README.md",
        "README.zh-CN.md",
        "cjson/**",
        "civetweb/**",
        "llvm/README.md",
        "llvm/README.zh-CN.md",
        "llvm/manifests/**",
    ]


def ensure_third_party_repo(*, update: bool = False) -> Path:
    git = resolve_tool("git")
    if not THIRD_PARTY_ROOT.exists():
        THIRD_PARTY_ROOT.parent.mkdir(parents=True, exist_ok=True)
        run(
            [
                git,
                "-c",
                "core.longpaths=true",
                "clone",
                "--filter=blob:none",
                "--no-checkout",
                THIRD_PARTY_REPO_URL,
                THIRD_PARTY_ROOT,
            ],
            cwd=THIRD_PARTY_ROOT.parent,
        )
        run([git, "sparse-checkout", "init", "--no-cone"], cwd=THIRD_PARTY_ROOT)
        run([git, "sparse-checkout", "set", *sparse_checkout_patterns()], cwd=THIRD_PARTY_ROOT)
        run([git, "checkout", "main"], cwd=THIRD_PARTY_ROOT)
        return THIRD_PARTY_ROOT

    if not (THIRD_PARTY_ROOT / ".git").exists():
        raise SystemExit(f"Kinal-ThirdParty exists but is not a git repository: {THIRD_PARTY_ROOT}")

    if update:
        run([git, "pull", "--ff-only"], cwd=THIRD_PARTY_ROOT)
    return THIRD_PARTY_ROOT
