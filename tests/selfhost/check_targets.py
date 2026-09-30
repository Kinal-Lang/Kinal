"""Cross-target IR/object checks; no foreign executable is run on the host."""
from __future__ import annotations

import argparse
import json
import platform
import re
import struct
import subprocess
from pathlib import Path


TARGETS = (
    ("win64", "x86_64-pc-windows-msvc", 1, 2),
    ("win-arm64", "aarch64-pc-windows-msvc", 1, 3),
    ("linux64", "x86_64-pc-linux-gnu", 2, 2),
    ("linux-arm64", "aarch64-unknown-linux-gnu", 2, 3),
    ("mac64", "x86_64-apple-darwin", 3, 2),
    ("macos-arm64", "aarch64-apple-darwin", 3, 3),
)

SOURCE = '''Unit Tests.CrossTarget;
Get IO.Target;
Get IO.Host;
Get IO.Path;
Static Function int probe_target_os() { Return IO.Target.OS; }
Static Function int probe_target_arch() { Return IO.Target.Arch; }
Static Function int probe_target_bits() { Return IO.Target.PointerBits; }
Static Function int probe_target_env() { Return IO.Target.Env; }
Static Function int probe_host_os() { Return IO.Host.OS; }
Static Function int probe_host_arch() { Return IO.Host.Arch; }
Static Function int probe_host_bits() { Return IO.Host.PointerBits; }
Static Function int probe_target_branch()
{
    If (IO.Target.OS == IO.Target.OS.Windows) Return 11;
    ElseIf (IO.Target.OS == IO.Target.OS.Linux) Return 22;
    Else Return 33;
}
Static Function string probe_target_separator() { Return IO.Target.PathSeparator; }
Static Function string probe_target_newline() { Return IO.Target.NewLine; }
Static Function string probe_target_exe() { Return IO.Target.ExeSuffix; }
Static Function string probe_target_object() { Return IO.Target.ObjectSuffix; }
Static Function string probe_target_dynamic() { Return IO.Target.DynamicLibrarySuffix; }
Static Function string probe_target_static() { Return IO.Target.StaticLibrarySuffix; }
Static Function string probe_host_dynamic() { Return IO.Host.DynamicLibrarySuffix; }
Static Function int Main()
{
    If (probe_target_os() != probe_host_os()) Return 1;
    If (probe_target_arch() != probe_host_arch()) Return 2;
    Return 0;
}
'''


def invoke(command: list[str], root: Path, *, error: str | None = None) -> str:
    proc = subprocess.run(command, cwd=root, text=True, capture_output=True, timeout=180)
    output = proc.stdout + proc.stderr
    if error is None:
        if proc.returncode != 0:
            raise AssertionError(f"command failed: {command}\n{output}")
    elif proc.returncode != 1 or error not in output:
        raise AssertionError(f"expected exit 1 / {error!r}: {command}\n{output}")
    return output


def function_body(ir: str, name: str) -> str:
    match = re.search(r"^define [^\n]*@[^\n(]*" + re.escape(name) + r"(?:_\d+)?\([^\n]*\).*?\n}\n", ir, re.M | re.S)
    if match is None:
        raise AssertionError(f"missing function {name}")
    return match.group()


def check_ir(path: Path, triple: str, os_id: int, arch: int, *, legacy_stage0: bool = False,
             check_path: bool = False) -> None:
    ir = path.read_text(encoding="utf-8")
    assert f'target triple = "{triple}"' in ir, path
    assert 'target datalayout = "' in ir, path
    host_os = {"Windows": 1, "Linux": 2, "Darwin": 3}[platform.system()]
    host_arch = 3 if platform.machine().lower() in {"arm64", "aarch64"} else 2
    values = {"target_os": os_id, "target_arch": arch, "target_bits": 64,
              "target_env": 1, "target_branch": os_id * 11,
              "host_os": host_os, "host_arch": host_arch, "host_bits": 64}
    for name, value in values.items():
        body = function_body(ir, "probe_" + name)
        assert f"ret i64 {value}" in body, (path, name, body)
    for name, value in {
        "target_separator": "\\" if os_id == 1 else "/",
        "target_newline": "\r\n" if os_id == 1 else "\n",
        "target_exe": ".exe" if os_id == 1 else "",
        "target_object": ".obj" if os_id == 1 else ".o",
        "target_dynamic": {1: ".dll", 2: ".so", 3: ".dylib"}[os_id],
        "target_static": ".lib" if os_id == 1 else ".a",
        "host_dynamic": {1: ".dll", 2: ".so", 3: ".dylib"}[host_os],
    }.items():
        if name == "host_dynamic" and legacy_stage0 and host_os == 3:
            continue  # Old macOS releases incorrectly report .so; current compilers must not.
        body = function_body(ir, "probe_" + name)
        # String constants may be returned directly or through a GEP. Inspect
        # only globals referenced by this probe, not unrelated runtime strings.
        globals_used = re.findall(r"@([\w.]+)", body)
        lines = [line for line in ir.splitlines()
                 if any(line.startswith("@" + symbol + " =") for symbol in globals_used)]
        escaped = "".join("\\\\" if byte == 92 else
                          (chr(byte) if 32 <= byte < 127 and byte != 34 else f"\\{byte:02X}")
                          for byte in value.encode("utf-8"))
        assert any(f'c"{escaped}\\00"' in line or
                   (not value and "[1 x i8] zeroinitializer" in line)
                   for line in lines), (path, name, body, lines)
    # IO.Path policy must follow the output platform, not the compilation host.
    if check_path:
        path_body = function_body(ir, "__PathTargetSeparator")
        assert f"ret i8 {92 if os_id == 1 else 47}" in path_body, (path, path_body)


def check_object(path: Path, os_id: int, arch: int) -> None:
    data = path.read_bytes()
    if os_id == 1:
        assert struct.unpack_from("<H", data)[0] == (0xAA64 if arch == 3 else 0x8664), path
    elif os_id == 2:
        assert data[:5] == b"\x7fELF\x02", path
        assert struct.unpack_from("<H", data, 18)[0] == (183 if arch == 3 else 62), path
    else:
        assert data[:4] == b"\xcf\xfa\xed\xfe", path
        assert struct.unpack_from("<I", data, 4)[0] == (0x100000C if arch == 3 else 0x1000007), path


def check_targets(compiler: Path, stage0: Path, root: Path, out: Path,
                  *, stage0_reference: bool = True) -> dict[str, object]:
    out.mkdir(parents=True, exist_ok=True)
    (out / "Main.kn").write_text(SOURCE, encoding="utf-8")
    manifest = out / "kinal.knproj"
    profiles = [f'''Profile "{alias}" {{
        Source {{ Entry = "Main.kn"; Sets = ["source"]; Mode = FileOnly; }}
        Build {{ Target = "{alias}"; }}
    }}''' for alias, _, _, _ in TARGETS]
    profiles.append('''Profile "invalid" {
        Source { Entry = "Main.kn"; Sets = ["source"]; Mode = FileOnly; }
        Build { Target = "not-a-target"; }
    }''')
    manifest.write_text('''Project CrossTarget {
    DefaultProfile = "win64";
    SourceSet "source" { Files = ["Main.kn"]; RequireUnit = true; }
    ''' + "\n".join(profiles) + "\n}\n", encoding="utf-8")
    for alias, triple, os_id, arch in TARGETS:
        for role, exe in (("self", compiler), ("stage0", stage0)):
            ir_path = out / f"{role}-{alias}.ll"
            command = [str(exe), "build", "--project", str(manifest), "--profile", alias]
            invoke(command + ["--emit", "ir", "-o", str(ir_path)], root)
            check_ir(ir_path, triple, os_id, arch,
                     legacy_stage0=role == "stage0" and not stage0_reference,
                     check_path=role == "self")
            obj = out / f"{role}-{alias}.obj"
            invoke(command + ["--emit", "obj", "-o", str(obj)], root)
            check_object(obj, os_id, arch)
        print(f"[OK] cross-target {alias}: constants, IR, object headers", flush=True)

    # CLI takes precedence over the profile, and exact triples work too.
    override = out / "override.ll"
    command = [str(compiler), "build", "--project", str(manifest), "--profile", "win64"]
    invoke(command + ["--target", "aarch64-unknown-linux-gnu", "--emit", "ir", "-o", str(override)], root)
    check_ir(override, "aarch64-unknown-linux-gnu", 2, 3, check_path=True)
    invoke(command + ["--target", "host", "--emit", "obj"], root)
    assert (out / ("CrossTarget.obj" if platform.system() == "Windows" else "CrossTarget.o")).is_file()
    executable = out / ("host.exe" if platform.system() == "Windows" else "host")
    invoke(command + ["--target", "host", "-o", str(executable)], root)
    invoke([str(executable)], root)
    invoke(command + ["--target", "linux64", "--emit", "obj"], root)
    check_object(out / "CrossTarget.o", 2, 2)
    for target in ("not-a-target", "i686-pc-windows-msvc"):
        invoke(command + ["--target", target, "--emit", "ir"], root, error="Unsupported target")
    invoke(command + ["--target", "bare64", "--emit", "ir"], root,
           error="bare targets require Environment=Freestanding and Runtime=None")
    invoke([str(compiler), "build", "--project", str(manifest), "--profile", "invalid"],
           root, error="Unsupported target")
    foreign = "linux64" if platform.system() == "Windows" else "win64"
    rejected = out / "must-not-be-written.exe"
    invoke(command + ["--target", foreign, "-o", str(rejected)], root,
           error="Cross-target linking is not supported")
    assert not rejected.exists()
    assert not rejected.with_suffix(".obj").exists()
    assert not rejected.with_suffix(".o").exists()
    return {"name": "cross_target_ir_objects", "ok": True, "targets": len(TARGETS)}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    result = check_targets(args.compiler.resolve(), args.stage0.resolve(),
                           Path(__file__).resolve().parents[2], args.out_dir.resolve())
    print(json.dumps(result))


if __name__ == "__main__":
    main()
