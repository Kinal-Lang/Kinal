from __future__ import annotations

import argparse
import concurrent.futures
import os
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_project_packages import archive
from check_knc_backend import stage_source_packages


def check_archive_cache(compiler: Path, stage0: Path, root: Path, out: Path,
                        *, stage0_reference: bool = True) -> dict[str, object]:
    out.mkdir(parents=True, exist_ok=True)
    packages = stage_source_packages(root, out / "packages")
    tools = [("stage0", stage0)] if stage0_reference else []
    tools.append(("selfhost", compiler))
    project = root / "tests/selfhost/fixtures/archive_cache/kinal.knproj"
    payloads = {f"src/part-{index:03d}.kn": "// " + str(index) + "\n" + "x" * 8192
                for index in range(128)}
    package = out / "input.klib"
    archive(package, "Tests.Cache", payloads)

    def verify(directory: Path) -> None:
        assert (directory / ".klib-ready").is_file(), "cache was not committed"
        assert (directory / "package.knpkg.json").is_file()
        for name, contents in payloads.items():
            assert (directory / name).read_text(encoding="utf-8") == contents, name

    def extract(executable: Path, directory: Path) -> subprocess.CompletedProcess[str]:
        result = subprocess.run([str(executable), str(package), str(directory)], cwd=root,
                                capture_output=True, text=True, timeout=90)
        assert result.returncode == 0 and result.stdout == "cache-ok\n" and not result.stderr, \
            (result.returncode, result.stdout, result.stderr)
        return result

    for label, tool in tools:
        executable = out / (label + (".exe" if os.name == "nt" else ""))
        build = subprocess.run([str(tool), "build", "--project", str(project), "--profile", "test",
                                "--stdpkg-root", str(packages), "-o", str(executable)],
                               cwd=root, capture_output=True, text=True, timeout=900)
        assert build.returncode == 0, (label, build.returncode, build.stdout, build.stderr)
        with tempfile.TemporaryDirectory(prefix=label + "-", dir=out) as temporary:
            directory = Path(temporary) / "cold"
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as workers:
                futures = [workers.submit(extract, executable, directory) for _ in range(4)]
                for future in futures:
                    future.result()
            verify(directory)
            extract(executable, directory)

            # A failed extraction must neither publish readiness nor retain
            # the process's lock. Repair the owned fixture and retry directly.
            failed_directory = Path(temporary) / "failed"
            failed_directory.mkdir()
            obstacle = failed_directory / "src"
            obstacle.write_text("not a directory", encoding="utf-8")
            failure = subprocess.run([str(executable), str(package), str(failed_directory)],
                                     cwd=root, capture_output=True, text=True, timeout=90)
            assert failure.returncode == 1 and "failed to extract" in failure.stdout, \
                (failure.returncode, failure.stdout, failure.stderr)
            assert not (failed_directory / ".klib-ready").exists()
            obstacle.unlink()
            extract(executable, failed_directory)
            verify(failed_directory)

            # A native lock is released by the OS even if its owner terminates
            # before Kinal can close it. Persistent lock files are not owners.
            interrupted = Path(temporary) / "interrupted"
            holder = subprocess.Popen([str(executable), str(package), str(interrupted), "hold"],
                                      cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                      text=True)
            blocked = None
            try:
                assert holder.stdout is not None
                with concurrent.futures.ThreadPoolExecutor(max_workers=1) as reader:
                    line = reader.submit(holder.stdout.readline)
                    try:
                        assert line.result(timeout=15) == "locked\n"
                    except BaseException:
                        holder.terminate()
                        holder.wait(timeout=15)
                        raise
                blocked = subprocess.Popen([str(executable), str(package), str(interrupted)],
                                           cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                           text=True)
                try:
                    blocked.communicate(timeout=0.2)
                    raise AssertionError("second process bypassed the exclusive lock")
                except subprocess.TimeoutExpired:
                    pass
                assert not (interrupted / ".klib-ready").exists()
                holder.terminate()
                holder.wait(timeout=15)
                stdout, stderr = blocked.communicate(timeout=90)
                assert blocked.returncode == 0 and stdout == "cache-ok\n" and not stderr, \
                    (blocked.returncode, stdout, stderr)
                verify(interrupted)
            finally:
                for process in (holder, blocked):
                    if process is not None and process.poll() is None:
                        process.kill()
                        process.wait(timeout=15)
                for stream in (holder.stdout, holder.stderr):
                    if stream is not None:
                        stream.close()
        print(f"[OK] {label} archive cache concurrency/failure/owner termination", flush=True)
    return {"name": "archive_cache", "ok": True, "compilers": len(tools),
            "workers": 4, "payload_files": len(payloads), "stage0_reference": stage0_reference}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    print(check_archive_cache(args.compiler.resolve(), args.stage0.resolve(),
                              Path(__file__).resolve().parents[2], args.out_dir.resolve()))
