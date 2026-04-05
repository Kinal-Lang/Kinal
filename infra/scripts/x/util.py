from __future__ import annotations

import json
import shutil
import subprocess
import urllib.request
from pathlib import Path

from .context import ROOT


def resolve_tool(name: str) -> str:
    path = shutil.which(name)
    if not path:
        raise SystemExit(f"required tool not found in PATH: {name}")
    return path


def run(cmd: list[str | Path], *, cwd: Path = ROOT, env: dict[str, str] | None = None) -> None:
    cmd_str = [str(x) for x in cmd]
    print("+", " ".join(cmd_str), flush=True)
    subprocess.check_call(cmd_str, cwd=str(cwd), env=env)


def find_artifact(root: Path, name: str) -> Path:
    matches = [p for p in root.rglob(name) if p.is_file()]
    if not matches:
        raise SystemExit(f"artifact not found: {name} under {root}")
    matches.sort(key=lambda p: (len(p.parts), len(str(p))))
    return matches[0]


def copy_tree(src: Path, dst: Path) -> None:
    if not src.exists():
        return
    shutil.copytree(src, dst, dirs_exist_ok=True)


def download_file(url: str, dest: Path) -> None:
    print(f"+ download {url}", flush=True)
    dest.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url) as resp, dest.open("wb") as fh:
        shutil.copyfileobj(resp, fh)


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def read_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)
