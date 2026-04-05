#!/usr/bin/env python3
"""Minimal SSH/SCP helper for syncing to the Linux build server."""
from __future__ import annotations

import os
import posixpath
import stat
import sys
from pathlib import Path

import paramiko

HOST = "192.168.1.12"
PORT = 22
USER = "yiyi"
PASSWORD = "12345678"

ROOT = Path(__file__).resolve().parent


def connect() -> paramiko.SSHClient:
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(HOST, port=PORT, username=USER, password=PASSWORD, timeout=10)
    return client


def run_cmd(client: paramiko.SSHClient, cmd: str, *, timeout: int = 60, print_output: bool = True) -> tuple[int, str, str]:
    print(f"[remote] {cmd}")
    _, stdout, stderr = client.exec_command(cmd, timeout=timeout)
    out = stdout.read().decode("utf-8", errors="replace")
    err = stderr.read().decode("utf-8", errors="replace")
    rc = stdout.channel.recv_exit_status()
    if print_output:
        if out.strip():
            print(out.rstrip())
        if err.strip():
            print(err.rstrip(), file=sys.stderr)
    return rc, out, err


def upload_file(sftp: paramiko.SFTPClient, local: Path, remote: str) -> None:
    remote_dir = posixpath.dirname(remote)
    _mkdirs(sftp, remote_dir)
    print(f"  {local} -> {remote}")
    sftp.put(str(local), remote)


def _mkdirs(sftp: paramiko.SFTPClient, path: str) -> None:
    if not path or path == "/" or path == ".":
        return
    try:
        sftp.stat(path)
        return
    except FileNotFoundError:
        pass
    _mkdirs(sftp, posixpath.dirname(path))
    try:
        sftp.mkdir(path)
    except OSError:
        pass


def upload_tree(sftp: paramiko.SFTPClient, local_dir: Path, remote_dir: str, *, exclude: set[str] | None = None) -> int:
    count = 0
    exclude = exclude or set()
    for root, dirs, files in os.walk(local_dir):
        dirs[:] = [d for d in dirs if d not in exclude]
        for f in files:
            lp = Path(root) / f
            rel = lp.relative_to(local_dir).as_posix()
            rp = posixpath.join(remote_dir, rel)
            upload_file(sftp, lp, rp)
            count += 1
    return count


def sync_changed_sources(client: paramiko.SSHClient, remote_root: str) -> None:
    """Upload the three changed source files + x.py to the remote repo."""
    sftp = client.open_sftp()
    files = [
        ("kinal-compiler/src/link/kn_link_support.inc", "kinal-compiler/src/link/kn_link_support.inc"),
        ("kinal-compiler/src/driver/kn_driver_support.inc", "kinal-compiler/src/driver/kn_driver_support.inc"),
        ("kinal-compiler/src/kn_link.c", "kinal-compiler/src/kn_link.c"),
        ("kinal-compiler/src/kn_driver.c", "kinal-compiler/src/kn_driver.c"),
        ("x.py", "x.py"),
    ]
    for local_rel, remote_rel in files:
        local = ROOT / local_rel
        remote = posixpath.join(remote_root, remote_rel)
        upload_file(sftp, local, remote)
    sftp.close()
    print(f"[OK] synced {len(files)} changed files")


def sync_release_bundle(client: paramiko.SSHClient, local_bundle: Path, remote_bundle: str) -> None:
    """Upload an entire release bundle directory."""
    sftp = client.open_sftp()
    n = upload_tree(sftp, local_bundle, remote_bundle, exclude={".git", "__pycache__", "node_modules"})
    sftp.close()
    print(f"[OK] uploaded {n} files to {remote_bundle}")


def sync_project_tar(client: paramiko.SSHClient, remote_root: str) -> None:
    """Create a local tar.gz excluding heavy dirs, upload it, and extract remotely."""
    import subprocess as sp
    import tempfile

    tar_name = "kinal-sync.tar.gz"
    local_tar = ROOT / tar_name

    # Build tar with git archive + untracked, or just use tar with exclusions
    print("[local] creating tar.gz (excluding .git, artifacts, Kinal-ThirdParty caches, node_modules) ...")
    # Use tar via Python's tarfile to be cross-platform
    import tarfile
    if local_tar.exists():
        local_tar.unlink()
    with tarfile.open(str(local_tar), "w:gz", compresslevel=6) as tf:
        skip_prefixes = (
            ".git", "artifacts", "out", "release",
            "..\\Kinal-ThirdParty\\.cache", "../Kinal-ThirdParty/.cache",
            "node_modules", "__pycache__",
        )
        skip_suffixes = (".obj", ".exe", ".dll", ".lib", ".pdb")
        for path in sorted(ROOT.rglob("*")):
            try:
                if not path.is_file():
                    continue
            except (PermissionError, OSError):
                continue
            rel = path.relative_to(ROOT).as_posix()
            skip = False
            for prefix in skip_prefixes:
                normed = prefix.replace("\\", "/")
                if rel == normed or rel.startswith(normed + "/"):
                    skip = True
                    break
            if skip:
                continue
            if any(rel.lower().endswith(s) for s in skip_suffixes):
                continue
            tf.add(str(path), arcname=rel)
    size_mb = local_tar.stat().st_size / (1024 * 1024)
    print(f"[local] tar created: {local_tar} ({size_mb:.1f} MB)")

    # Upload
    remote_tar = posixpath.join("/tmp", tar_name)
    sftp = client.open_sftp()
    print(f"[upload] {local_tar} -> {remote_tar} ...")
    sftp.put(str(local_tar), remote_tar, callback=lambda sent, total: print(f"\r  {sent*100//total}%", end="", flush=True) if total > 0 else None)
    sftp.close()
    print("\n[upload] done")

    # Extract on remote
    run_cmd(client, f"mkdir -p {remote_root}")
    run_cmd(client, f"cd {remote_root} && tar xzf {remote_tar}")
    run_cmd(client, f"rm -f {remote_tar}")

    # Clean up local tar
    local_tar.unlink(missing_ok=True)
    print(f"[OK] project synced to {remote_root}")


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage:")
        print("  remote.py cmd <shell-command>        Run a command on the remote server")
        print("  remote.py sync-src [remote-root]     Upload changed source files")
        print("  remote.py sync-project [remote-root] Sync entire project (tar+upload+extract)")
        print("  remote.py sync-bundle <local> <remote>  Upload a release bundle tree")
        print("  remote.py upload <local> <remote>    Upload a single file")
        print("  remote.py test                       Test connection")
        return 1

    action = sys.argv[1]
    client = connect()

    try:
        if action == "test":
            rc, out, _ = run_cmd(client, "uname -a && whoami && which zig && zig version")
            return rc

        elif action == "cmd":
            cmd = " ".join(sys.argv[2:])
            rc, _, _ = run_cmd(client, cmd)
            return rc

        elif action == "sync-project":
            remote_root = sys.argv[2] if len(sys.argv) > 2 else "/home/yiyi/build-kinal/Kinal-Lang"
            sync_project_tar(client, remote_root)
            return 0

        elif action == "sync-src":
            remote_root = sys.argv[2] if len(sys.argv) > 2 else "/home/yiyi/build-kinal/Kinal-Lang"
            sync_changed_sources(client, remote_root)
            return 0

        elif action == "sync-bundle":
            if len(sys.argv) < 4:
                print("Usage: remote.py sync-bundle <local-dir> <remote-dir>")
                return 1
            local = Path(sys.argv[2])
            remote = sys.argv[3]
            sync_release_bundle(client, local, remote)
            return 0

        elif action == "upload":
            if len(sys.argv) < 4:
                print("Usage: remote.py upload <local-file> <remote-path>")
                return 1
            sftp = client.open_sftp()
            upload_file(sftp, Path(sys.argv[2]), sys.argv[3])
            sftp.close()
            return 0

        else:
            print(f"unknown action: {action}")
            return 1
    finally:
        client.close()


if __name__ == "__main__":
    raise SystemExit(main())
