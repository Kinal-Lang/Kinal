"""Select a native Clang frontend compatible with the Windows runner STL."""
import hashlib
import os
from pathlib import Path
import platform
import re
import subprocess


RELEASE = "22.1.8"
INSTALLERS = {
    "amd64": ("win64", "16e5709785fef73c854646241c4a92c5cd574318d1b33c63330dd7721903e55c"),
    "arm64": ("woa64", "76f44ef1ba6eeb5a65904e9500f042f588fade49952778ce48f0374daa934396"),
}


def matches(driver: Path, arch: str) -> bool:
    if not driver.is_file():
        return False
    output = subprocess.check_output([driver, "--version"], text=True)
    version = re.search(r"clang version (\d+)", output)
    target = "aarch64" if arch == "arm64" else "x86_64"
    return bool(version and int(version[1]) >= 22 and f"Target: {target}-" in output)


def main() -> None:
    arch = platform.machine().lower()
    vs_llvm = Path(os.environ["VCINSTALLDIR"]) / "Tools/Llvm"
    native = "ARM64" if arch == "arm64" else "x64"
    candidates = [vs_llvm / native / "bin", vs_llvm / "bin", Path(os.environ["ProgramFiles"]) / "LLVM/bin"]
    for folder in candidates:
        driver = folder / "clang-cl.exe"
        if matches(driver, arch):
            break
    else:
        suffix, checksum = INSTALLERS[arch]
        installer = f"LLVM-{RELEASE}-{suffix}.exe"
        work = Path(os.environ["RUNNER_TEMP"]) / "kinal-host-clang"
        work.mkdir(exist_ok=True)
        subprocess.run(["gh", "release", "download", f"llvmorg-{RELEASE}", "--repo", "llvm/llvm-project",
                        "--pattern", installer, "--dir", work, "--clobber"], check=True)
        with (work / installer).open("rb") as binary:
            if hashlib.file_digest(binary, "sha256").hexdigest() != checksum:
                raise SystemExit("Host Clang installer checksum mismatch")
        subprocess.run(["7z", "x", "-y", f"-o{work}", work / installer, "bin/clang*.exe", "bin/*.dll",
                        "lib/clang/22/include/*"], check=True, stdout=subprocess.DEVNULL)
        driver = work / "bin/clang-cl.exe"
        if not matches(driver, arch):
            raise SystemExit(f"Expected a native {arch} Clang 22+ frontend at {driver}")
    print(f"Host C/C++ frontend: {driver}")
    with Path(os.environ["GITHUB_ENV"]).open("a", encoding="utf-8") as env:
        for key in ("KINAL_HOST_CLANG", "CC", "CXX"):
            print(f"{key}={driver}", file=env)


if __name__ == "__main__":
    main()
