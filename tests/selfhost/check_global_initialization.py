from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
from pathlib import Path

from audit_manifest_runtime import normalize_unhandled_runtime_output
from check_targets import TARGETS, function_body


EXPECTED = {
    "ordered": (0, ""),
    "callables": (0, "12\n13\n6\n18\n42\n0\n"),
    "multi_unit": (0, "0\n7\n7\n"),
    "lexical": (0, "13\n17\n20\n30\nfirst\nsecond\n"),
    "fixed_storage": (0, "3\n23\n23\n"),
    "aggregate_storage": (0, "ok\n"),
    "nested_dimensions": (0, "ok\n"),
    "package_storage": (0, "ok\n"),
    "static_gc": (0, "true\ntrue\ntrue\n"),
    "failure": (
        1,
        "global init failed\n"
        "Tests.GlobalInitializationError.Fail -> <global init> -> "
        "IO.Console.__PrintValues -> IO.Console.PrintLine -> "
        "Tests.GlobalInitializationError.Main\n",
    ),
    "pending_scalar": (
        1,
        "pending before scalar call\n"
        "Tests.PendingScalar.Fail -> <global init> -> Tests.PendingScalar.Odd -> "
        "Tests.PendingScalar.Even -> Tests.PendingScalar.Main\n",
    ),
}


def verify_compiler(compiler: Path, label: str, root: Path, out: Path) -> None:
    out.mkdir(parents=True, exist_ok=True)
    fixtures = root / "tests/selfhost/fixtures/global_initialization"
    suffix = ".exe" if os.name == "nt" else ""
    env = os.environ.copy()
    env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
    for name, (expected_code, expected_output) in EXPECTED.items():
        executable = out / f"{label}-{name}{suffix}"
        command = [str(compiler), "build", "--project", str(fixtures / name),
                   "--profile", "test", "-o", str(executable)]
        build = subprocess.run(command, cwd=root, env=env, text=True, capture_output=True)
        if build.returncode != 0:
            raise AssertionError(
                f"{label} {name}: build exit={build.returncode}\n{build.stdout}{build.stderr}")
        result = subprocess.run([str(executable)], cwd=root, env=env, text=True, capture_output=True)
        actual = normalize_unhandled_runtime_output(result.stdout)
        if result.returncode != expected_code or actual != expected_output or result.stderr:
            raise AssertionError(
                f"{label} {name}: expected exit={expected_code}, stdout={expected_output!r}\n"
                f"actual exit={result.returncode}, stdout={actual!r}, stderr={result.stderr!r}")
        print(f"[OK] {label} global initialization: {name}", flush=True)

    # Each initializer releases its temporaries before the next one starts.
    many = out / f"{label}-many-globals.kn"
    declarations = "\n".join(f"int G{i} = {i};" for i in range(320))
    many.write_text(f"Unit GlobalCount;\n{declarations}\n"
                    "Static Function int Main() { Return G319 == 319 ? 0 : 1; }\n", encoding="utf-8")
    for source, expected in [(fixtures / "aggregate_storage/Main.kn", "ok\n"),
                             (fixtures / "nested_dimensions/Main.kn", "ok\n"),
                             (fixtures / "package_storage/Main.kn", "ok\n"), (many, "")]:
        result = subprocess.run([str(compiler), "vm", "run", "--no-module-discovery", str(source)],
                                cwd=out, env=env, text=True, capture_output=True)
        assert (result.returncode, result.stdout, result.stderr) == (0, expected, ""), (
            label, source, result.returncode, result.stdout, result.stderr)

    # Enlarging a partially initialized struct array changes the loop bound.
    # It should not emit a fresh sequence of instructions for every element.
    source = out / f"{label}-padding.kn"
    bytecode = source.with_suffix(".knc")
    sizes = []
    for length in (16, 8192):
        source.write_text(
            "Unit Padding;\nStruct Row { int[3] Cells; }\n"
            f"Static Function int Main() {{ Row first; Row[{length}] rows = {{first}};\n"
            f"    Return rows[{length - 1}].Cells.Length() == 3 ? 0 : 1; }}\n",
            encoding="utf-8")
        subprocess.run([str(compiler), "vm", "build", "--no-module-discovery", str(source),
                        "-o", str(bytecode)], cwd=out, env=env, check=True)
        subprocess.run([str(compiler), "vm", "run", str(bytecode)], cwd=out, env=env, check=True)
        sizes.append(bytecode.stat().st_size)
    assert sizes[1] < 2 * sizes[0], (label, "array padding bytecode grew with its bound", sizes)


def verify_root_ir(compiler: Path, label: str, root: Path, out: Path) -> None:
    project = root / "tests/selfhost/fixtures/global_initialization/static_gc/kinal.knproj"
    env = os.environ.copy()
    env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
    for target, _, _, _ in TARGETS:
        output = out / f"{label}-global-roots-{target}.ll"
        command = [str(compiler), "build", "--project", str(project), "--profile", "test",
                   "--emit", "ir", "--target", target, "-o", str(output)]
        result = subprocess.run(command, cwd=root, env=env, text=True, capture_output=True)
        if result.returncode:
            raise AssertionError(f"{label} {target}: IR build failed\n{result.stdout}{result.stderr}")
        ir = output.read_text(encoding="utf-8")
        initializer = "__kn_global_init" if label == "stage0" else "__kn_sh_global_init"
        if label == "selfhost":
            dispatch = function_body(ir, "__kn_sh_ensure_global_init")
            assert "load i1, ptr @__kn_sh_global_init_done" in dispatch
            assert "br i1 %initialized, label %done, label %initialize" in dispatch
            assert dispatch.count("call void @__kn_sh_global_init()") == 1
        body = function_body(ir, initializer)
        populate = re.search(r"call i64 @[^\n(]*Populate[^\n(]*\(", body)
        assert populate, f"{label} {target}: initializer no longer calls Populate"
        setup = body[:populate.start()]
        backings = re.findall(r"^(@[\w.]+) = private global \[([12]) x ptr\] zeroinitializer$",
                              ir, re.M)
        assert sorted(length for _, length in backings) == ["1", "2"], \
            f"{label} {target}: fixed global/static backing is missing"
        root_name = "__kn_gc_add_global_root" if label == "stage0" else \
            "__kn_sh_IO_Kinal_Runtime_GarbageCollector_AddGlobalRoot_2"
        for storage, length in backings:
            assert f"call void @{root_name}(ptr {storage}," in setup, \
                f"{label} {target}: {storage} is not rooted before initialization"
            assert f"getelementptr ([{length} x ptr], ptr null, i32 1)" in setup, \
                f"{label} {target}: backing scan size does not cover the array"
        guard = re.search(r"store i1 true, ptr @[^\n]*global_init_done", setup)
        first_call = setup.find("call ")
        assert guard and 0 <= guard.start() < first_call, \
            f"{label} {target}: reentrancy guard must precede calls"
        print(f"[OK] {label} global roots/guard IR: {target}", flush=True)


def check_global_initialization(compiler: Path, stage0: Path, root: Path, out: Path,
                                *, stage0_reference: bool = True) -> dict:
    if stage0_reference:
        verify_compiler(stage0, "stage0", root, out)
        verify_root_ir(stage0, "stage0", root, out)
    verify_compiler(compiler, "selfhost", root, out)
    verify_root_ir(compiler, "selfhost", root, out)
    return {"name": "global_initialization", "ok": True, "cases": len(EXPECTED),
            "runtime_compilers": 2 if stage0_reference else 1,
            "stage0_reference": stage0_reference, "root_ir_targets": len(TARGETS)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--stage0-role", choices=("reference", "bootstrap"), default="reference")
    args = parser.parse_args()
    print(json.dumps(check_global_initialization(
        args.compiler.resolve(), args.stage0.resolve(), Path(__file__).resolve().parents[2],
        args.out_dir.resolve(), stage0_reference=args.stage0_role == "reference")))
