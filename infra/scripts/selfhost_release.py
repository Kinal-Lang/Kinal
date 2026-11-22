"""Verify the pinned bootstrap asset and package the converged public SDK."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from .release_assets import prepare_assets
from .x.context import LOCALES_DIR, ROOT, exe_name, host_tag, read_version
from .x.release_ops import sha256_file
from .x.util import extract_zip_safely, find_artifact


def extract_seed(download: Path, tag: str, host: str, destination: Path) -> None:
    name = f"Kinal-{tag.removeprefix('v')}-{host}"
    archive = download / f"{name}.zip"
    manifest = (download / f"SHA256SUMS-{host}.txt").read_text(encoding="utf-8")
    hashes = [digest for digest, filename in (line.split() for line in manifest.splitlines())
              if filename == archive.name]
    if hashes != [sha256_file(archive)]:
        raise SystemExit(f"Bootstrap checksum mismatch for {archive.name}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent) as tmp:
        extract_zip_safely(archive, Path(tmp))
        bundle = Path(tmp) / name
        if not (bundle / exe_name("kinal")).is_file():
            raise SystemExit(f"Missing bootstrap compiler in {archive.name}")
        bundle.rename(destination)


def link_or_copy(source, target):
    try:
        os.link(source, target)
    except OSError:
        shutil.copy2(source, target)
    return target


def smoke(sdk: Path, work: Path) -> None:
    compiler = sdk / exe_name("kinal")
    vm = sdk / exe_name("kinalvm")
    for tool, expected in ((compiler, read_version()), (vm, read_version("kinalvm"))):
        output = subprocess.check_output([tool, "--version"], cwd=work, text=True, stderr=subprocess.STDOUT)
        if expected not in output.split():
            raise SystemExit(f"Unexpected {tool.name} version {output!r}")
    source = work / "Hello.kn"
    source.write_text('Get IO.Console; Static Function int Main() { IO.Console.PrintLine("Kinal SDK"); Return 0; }', encoding="utf-8")
    for options, output, runner in (([], work / exe_name("hello"), []), (["vm"], work / "hello.knc", [vm])):
        subprocess.run([compiler, *options, "build", source, "--no-module-discovery", "-o", output],
                       cwd=work, check=True)
        if subprocess.check_output([*runner, output], cwd=work, text=True).strip() != "Kinal SDK":
            raise SystemExit(f"SDK execution failed for {output.name}")


def package() -> None:
    host = host_tag()
    if os.getenv("HOST_TAG", host) != host:
        raise SystemExit(f"Expected native runner for {os.environ['HOST_TAG']}, got {host}")
    version = read_version()
    stages = ROOT / "out/selfhost"
    stage = stages / "stage3"
    compiler = stage / exe_name("kinal-selfhost")
    report = json.loads((stages / "bootstrap-report.json").read_text(encoding="utf-8"))
    digest = sha256_file(compiler)
    if digest != report["compiler_sha256"]["3"] or digest != report["compiler_sha256"]["2"]:
        raise SystemExit("Stage3 compiler does not match the verified bootstrap")
    with tempfile.TemporaryDirectory(dir=ROOT / "out") as tmp:
        sdk = Path(tmp) / f"Kinal-{version}-{host}"
        sdk.mkdir()
        for name in ("bridge", "linker", "llvm", "runtime", "stdpkg"):
            shutil.copytree(stage / name, sdk / name, copy_function=link_or_copy)
        for source in (compiler, stage / exe_name("kinalvm"), stage / "VERSION", *stage.glob("*.dll")):
            link_or_copy(source, sdk / source.name)
        link_or_copy(compiler, sdk / exe_name("kinal"))
        shutil.copy2(find_artifact(ROOT / "out/lsp", exe_name("kinal-lsp-server")), sdk)
        for name in ("LICENSE.txt", "README.md", "README.zh-CN.md"):
            shutil.copy2(ROOT / name, sdk)
        shutil.copytree(ROOT / "docs", sdk / "docs")
        shutil.copytree(LOCALES_DIR, sdk / "locales")
        output = ROOT / "out/release-assets"
        archives = prepare_assets(version, host, sdk, output)
        with tempfile.TemporaryDirectory(dir=ROOT / "out") as verify:
            work = Path(verify)
            extract_zip_safely(archives[0], work)
            installed = work / sdk.name
            if sha256_file(installed / exe_name("kinal")) != digest:
                raise SystemExit("Archived public compiler does not match stage3")
            smoke(installed, work)


HOSTS = ("win-x64", "win-arm64", "linux-x64", "linux-arm64", "macos-x64", "macos-arm64")


def resolve_run() -> None:
    sha, branch, repo = (os.environ[name] for name in ("GITHUB_SHA", "BRANCH", "GITHUB_REPOSITORY"))
    tag = "v" + read_version()
    if os.environ["GITHUB_REF_TYPE"] == "tag" and os.environ["GITHUB_REF_NAME"] != tag:
        raise SystemExit(f"Release tag must match {tag}")
    if not (ROOT / "dev/releases" / (read_version() + ".md")).is_file():
        raise SystemExit(f"Missing release notes for {tag}")

    def api(path):
        return json.loads(subprocess.check_output(["gh", "api", f"repos/{repo}/{path}"]))

    workflow_id = api("actions/workflows/ci.yml")["id"]
    run_id = os.getenv("RUN_ID")
    if run_id:
        if not run_id.isdecimal():
            raise SystemExit("Expected a numeric run ID")
        candidates = [api(f"actions/runs/{run_id}")]
    else:
        candidates = api(f"actions/workflows/{workflow_id}/runs?head_sha={sha}&status=success&per_page=100")["workflow_runs"]
    for run in candidates:
        if (run["head_sha"] == sha and run["head_branch"] == branch and run["workflow_id"] == workflow_id
                and run["head_repository"]["full_name"] == repo and run["conclusion"] == "success"
                and run["event"] in ("push", "workflow_dispatch")):
            run_id = run["id"]
            jobs = api(f"actions/runs/{run_id}/jobs?per_page=100")["jobs"]
            passed = {job["name"] for job in jobs if job["conclusion"] == "success"}
            if all(host + " selfhost" in passed for host in HOSTS):
                with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as out:
                    print(f"run_id={run_id}", f"tag={tag}", sep="\n", file=out)
                return
    raise SystemExit(f"No successful six-host Selfhost CI run for {branch} at {sha}")


def verify_assets(directory: Path) -> None:
    all_lines = []
    expected_files = set()
    for host in HOSTS:
        manifest = directory / f"SHA256SUMS-{host}.txt"
        names = {f"Kinal-{read_version()}-{host}.{ext}" for ext in
                 (("zip",) if host.startswith("win-") else ("zip", "tar.gz", "tar.xz"))}
        lines = manifest.read_text(encoding="utf-8").splitlines()
        entries = [line.split() for line in lines]
        if len(entries) != len(names) or {name for _, name in entries} != names:
            raise SystemExit(f"Incomplete archive list in {manifest.name}")
        for digest, name in entries:
            if digest != sha256_file(directory / name):
                raise SystemExit(f"Checksum mismatch for {name}")
        expected_files.update(names | {manifest.name})
        all_lines.extend(lines)
    if {p.name for p in directory.iterdir()} != expected_files:
        raise SystemExit("Unexpected files in release artifacts")
    (directory / "SHA256SUMS.txt").write_text("\n".join(sorted(all_lines, key=lambda line: line.split()[1])) + "\n",
                                             encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    seed = commands.add_parser("extract-seed")
    seed.add_argument("download", type=Path)
    seed.add_argument("tag")
    seed.add_argument("host")
    seed.add_argument("destination", type=Path)
    commands.add_parser("package")
    commands.add_parser("resolve-run")
    verify = commands.add_parser("verify-assets")
    verify.add_argument("directory", type=Path)
    args = parser.parse_args()
    if args.command == "package":
        package()
    elif args.command == "resolve-run":
        resolve_run()
    elif args.command == "verify-assets":
        verify_assets(args.directory)
    else:
        extract_seed(args.download, args.tag, args.host, args.destination)


if __name__ == "__main__":
    main()
