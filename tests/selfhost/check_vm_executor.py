"""Compile source inside a Native host and exchange KNC images through the public package."""
from pathlib import Path
import json
import struct
import subprocess


def check_vm_executor(compiler: Path, root: Path, out: Path) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    version = dict(line.split("=", 1) for line in (root / "VERSION").read_text().splitlines() if "=" in line)["kinal"]

    def build(*args):
        subprocess.run([compiler, *args], cwd=out, check=True, timeout=180)

    files = []
    for name, body in (("正常", "Return 42;"), ("error", 'Throw "guest failure"; Return 0;')):
        source = out / (name + ".kn")
        source.write_text("Static Function int Main() { " + body + " }", encoding="utf-8")
        image = source.with_suffix(".knc")
        build("vm", "build", source, "--no-module-discovery", "-o", image)
        data = image.read_bytes()
        assert data[:8] == b"KNC2" + struct.pack("<HH", 4, 0)
        length, = struct.unpack_from("<I", data, 8)
        assert data[12:12 + length].decode() == version
        producer = b"99.8.7"
        image.write_bytes(data[:8] + struct.pack("<I", len(producer)) + producer + data[12 + length:])
        files.append(image)
    ordinary = out / "ordinary.ll"
    build("build", source, "--no-module-discovery", "--emit", "ir", "-o", ordinary)
    ir = ordinary.read_text(encoding="utf-8")
    for unit in ("IO.Kinal.VM.Executor", "IO.Kinal.Compiler.Backend.KncBackend"):
        assert unit not in ir and unit.replace(".", "_") not in ir, unit
    host = out / ("executor.exe" if compiler.suffix == ".exe" else "executor")
    build("build", root / "tests/selfhost/fixtures/vm_executor.kn", "--no-module-discovery", "-o", host)
    result = subprocess.run([host, compiler.parent / "stdpkg", out / "cache", *files, version],
                            cwd=out, capture_output=True, text=True, timeout=180)
    assert result.returncode == 0 and not result.stdout and not result.stderr, (result.returncode, result.stdout, result.stderr)
    return {"name": "vm_executor", "ok": True, "format": 4, "producer": version}


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    options = parser.parse_args()
    print(json.dumps(check_vm_executor(options.compiler.resolve(), Path(__file__).resolve().parents[2],
                                       options.out_dir.resolve())))
