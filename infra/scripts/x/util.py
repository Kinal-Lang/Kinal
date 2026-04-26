from __future__ import annotations

import json
import inspect
import os
import shutil
import subprocess
import tarfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path
from pathlib import PurePosixPath
from pathlib import PureWindowsPath

from .context import ROOT


_TAR_EXTRACT_SUPPORTS_FILTER = "filter" in inspect.signature(tarfile.TarFile.extract).parameters


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
    temp = dest.with_name(f"{dest.name}.tmp")
    temp.unlink(missing_ok=True)
    try:
        with urllib.request.urlopen(url, timeout=30) as resp, temp.open("wb") as fh:
            shutil.copyfileobj(resp, fh)
        temp.replace(dest)
    except (OSError, urllib.error.URLError) as exc:
        temp.unlink(missing_ok=True)
        raise SystemExit(f"download failed: {url}: {exc}") from exc


def _extract_target(dest: Path, member_name: str) -> Path:
    normalized_name = member_name.replace("\\", "/")
    member = PurePosixPath(normalized_name)
    if PureWindowsPath(member_name).drive:
        raise SystemExit(f"archive entry uses an invalid drive-qualified path: {member_name}")
    if member.is_absolute():
        raise SystemExit(f"archive entry uses an absolute path: {member_name}")
    target = (dest / Path(*member.parts)).resolve()
    base = dest.resolve()
    try:
        common = os.path.commonpath([str(base), str(target)])
    except ValueError as exc:
        raise SystemExit(f"archive entry escapes destination: {member_name}") from exc
    if common != str(base):
        raise SystemExit(f"archive entry escapes destination: {member_name}")
    return target


def _validate_tar_link(dest: Path, member: tarfile.TarInfo) -> None:
    link = member.linkname.replace("\\", "/")
    link_path = PurePosixPath(link)
    if link_path.is_absolute():
        raise SystemExit(f"archive link escapes destination: {member.name} -> {member.linkname}")
    target = _extract_target(dest, member.name)
    resolved = (target.parent / Path(*link_path.parts)).resolve()
    base = dest.resolve()
    try:
        common = os.path.commonpath([str(base), str(resolved)])
    except ValueError as exc:
        raise SystemExit(f"archive link escapes destination: {member.name} -> {member.linkname}") from exc
    if common != str(base):
        raise SystemExit(f"archive link escapes destination: {member.name} -> {member.linkname}")


def extract_tar_safely(archive: Path, dest: Path, mode: str = "r:*") -> None:
    dest.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive, mode) as tf:
        members = tf.getmembers()
        for member in members:
            _extract_target(dest, member.name)
            if member.issym() or member.islnk():
                _validate_tar_link(dest, member)
            elif not (member.isdir() or member.isfile()):
                raise SystemExit(f"archive entry type is not allowed: {member.name} (type={member.type!r})")
        for member in members:
            if _TAR_EXTRACT_SUPPORTS_FILTER:
                tf.extract(member, dest, filter="fully_trusted")
            else:
                tf.extract(member, dest)


def extract_zip_safely(archive: Path, dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as zf:
        for member in zf.infolist():
            target = _extract_target(dest, member.filename)
            if member.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with zf.open(member) as src, target.open("wb") as fh:
                shutil.copyfileobj(src, fh)
            # ZIP stores Unix mode bits in the upper 16 bits of external_attr.
            mode = (member.external_attr >> 16) & 0o777
            if mode:
                target.chmod(mode)


def extract_archive_safely(archive: Path, dest: Path) -> None:
    if archive.name.endswith(".tar.xz"):
        extract_tar_safely(archive, dest, "r:xz")
        return
    if archive.suffix == ".zip":
        extract_zip_safely(archive, dest)
        return
    raise SystemExit(f"unsupported archive format: {archive.name}")


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def read_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)
