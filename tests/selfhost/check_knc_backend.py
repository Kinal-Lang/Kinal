"""Reviewed pure-Kinal HIR -> KNC and integrated-CLI acceptance checks.

A native test harness or the integrated selfhost CLI emits KNC from the real
parser, dependency resolver and semantic HIR. Source-only official packages
prevent stale .klib archives from hiding current KNC annotations. Explicit
suites cover scalar/control/array behavior, closures, objects, pointers,
side-effect ordering, and the approved file/time and runtime-FFI fixtures.
Optional workflow checks exercise listings, loop-fusion toggles, repeated
emission and real I/O failures. Source bytes, expected outputs, tool hashes
and existing fixture setup are preserved; unrelated probes are not discovered.
"""
from __future__ import annotations

import argparse
import ast
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import struct
import subprocess
import sys


# This is the reviewed pure-KNC registration set, not every native manifest
# case. Keep additions explicit so unrelated probes cannot enter this suite.
REGISTERED_LABELS = (
    'knc_escaping_capture_contract',
    'knc_vm_frontend_char_semantics',
    'knc_vm_frontend_enum_semantics',
    'knc_vm_compound_assignment_once',
    'knc_vm_frontend_block_jump_state_machine',
    'knc_vm_struct_value_semantics',
    'knc_vm_wide_integer_literals',
    'knc_vm_narrow_integer_switch',
    'knc_vm_f32_rounding',
    'knc_vm_property_compound_receiver_once',
    'knc_vm_property_static_accessors',
    'knc_vm_array_add_value_semantics',
    'knc_vm_integer_widening',
    'knc_vm_unsigned_ops',
    'knc_vm_global_block_records',
    'knc_vm_f32_compound_operand_promotion',
    'knc_vm_if_numeric_join',
    'knc_vm_selfhost_property_compound_order',
    'knc_vm_switch_default_prefix',
    'knc_vm_cast_more',
    'knc_vm_object_casts',
    'knc_vm_any_object_casts',
    'knc_vm_typeof',
    'knc_vm_builtin_string_length_reference',
    'knc_vm_string_builtin_references',
    'knc_vm_string_builtin_conversions',
    'knc_vm_typeof_static_query',
    'knc_vm_pointer_rhs_addition',
    'knc_vm_pointers',
    'knc_vm_pointer_depth',
    'knc_vm_function_pointer',
    'knc_vm_string_to_char_ptr',
    'knc_vm_global_null_sugar',
    'knc_vm_const_if',
    'knc_vm_console_varargs',
    'knc_vm_superloop',
    'knc_vm_superloop_branches',
    'knc_vm_gc_new_arg_root',
    'knc_vm_gc_call_arg_root',
    'knc_vm_runtime_ffi',
)


# Explicit positive KNC builds in the same accepted driver workflow. These
# supplement its run_knc_case calls without importing native-only manifest cases.
REGISTERED_EXPLICIT = {
    "knc_vm_static_property_inheritance": ("tests/selfhost/fixtures/property_static_inherited/Main.kn", "ok\n", ()),
    "knc_vm_hello": ("tests/common/hello.kn", "hello\n", ()),
    "capture_control_flow_knc": ("tests/common/capture_control_flow.kn",
        "8\n-8\n5\n6\n2\n10\n11\n12\n20\n21\n22\n3\n2\n31\ncaught\n41\n42\n", ()),
    "capture_increment_types_knc": ("tests/selfhost/fixtures/capture_increment_types/Main.kn",
        "1.5\n2.5\na\nb\n20\n", ()),
    "float_string_roundtrip_knc": ("tests/common/float_string_roundtrip.kn",
        "1.25\n-2.5\n0.125\ntrue\ntrue\ntrue\n", ()),
    "knc_vm_arith": ("tests/common/knc_arith.kn", "42\n", ()),
    "knc_vm_std_semantics": ("tests/common/knc_std_semantics.kn", "3\ntrue\nabc\nfalse\ntrue\n42\nfalse\n", ()),
    "knc_vm_control": ("tests/common/control.kn", "ok\n", ()),
    "knc_vm_functions": ("tests/common/functions.kn", "ok\n", ()),
    "knc_vm_expr_ops": ("tests/common/knc_expr_ops.kn", "1\n3\n9\n9\n3\n", ()),
    "knc_vm_char_literals": ("tests/common/char_literals.kn", "A\n65\n10\n8\n39\n", ()),
    "knc_vm_char_to_string": ("tests/common/char_to_string.kn", "a\nxa\nay\na\n", ()),
    "knc_vm_bitwise": ("tests/common/bitwise.kn", "1\n7\n6\n10\n2\n-6\n", ()),
    "knc_vm_any_cast": ("tests/common/any_cast.kn", "123\n456\ntrue\n42\ntrue\ntrue\nA\n", ()),
    "knc_vm_time_file": ("tests/common/knc_time_file.kn",
        "ok\ntrue\ntrue\nalpha\nfallback\ntrue\nalphabeta\ntrue\nfalse\n", ()),
    "knc_vm_loop_backedge": ("tests/common/knc_loop_backedge.kn", "40\n15\n4\n1\n", ("--no-superloop",)),
}


def registered_cases(root: Path) -> dict[str, dict]:
    tree = ast.parse((root / "tests/run_tests.py").read_text(encoding="utf-8"))
    calls = sorted((node for node in ast.walk(tree) if isinstance(node, ast.Call)
                    and isinstance(node.func, ast.Name) and node.func.id == "run_knc_case"),
                   key=lambda node: node.lineno)
    actual_labels = {ast.literal_eval(call.args[0]) for call in calls}
    assert actual_labels == set(REGISTERED_LABELS), (
        "KNC registration drift; review every added or removed positive before changing the corpus",
        sorted(actual_labels - set(REGISTERED_LABELS)),
        sorted(set(REGISTERED_LABELS) - actual_labels))
    cases = {}
    for call in calls:
        label = ast.literal_eval(call.args[0])
        if label not in REGISTERED_LABELS:
            continue
        if label == "knc_vm_const_if":
            platform = "windows" if sys.platform == "win32" else ("macos" if sys.platform == "darwin" else "linux")
            source = root / "tests" / platform / "const_if.kn"
            expected = "win\n" if platform == "windows" else platform + "\n"
        else:
            source = root / "tests/common" / ast.literal_eval(call.args[1])
            expected = ast.literal_eval(call.args[2])
        exit_code = ast.literal_eval(call.args[3]) if len(call.args) > 3 else 0
        cases[label] = {"source": source, "expected": expected, "expected_exit": exit_code,
                        "registration_line": call.lineno}
    assert set(cases) == set(REGISTERED_LABELS), "reviewed KNC registrations changed"
    for label, (source, expected, flags) in REGISTERED_EXPLICIT.items():
        cases[label] = {"source": root / source, "expected": expected, "expected_exit": 0,
                        "reference_flags": flags, "registration": "explicit KNC driver check"}
    return cases


MANIFEST_CASES = (
    "hello", "control", "functions", "elseif", "numeric", "literals",
    "char_literals", "string_escapes", "string_compare", "logical_short_circuit",
    "bitwise", "knc_wide_integer_literals", "knc_narrow_integer_switch",
    "knc_integer_widening", "arrays", "console_varargs", "variadic_functions",
    "overload_functions", "array_index", "switch_matcher",
)

AGGREGATE_MANIFEST_CASES = (
    "property_compound_receiver_once", "struct_value_semantics", "oop", "oop_features",
    "struct_enum", "default_args", "nested_interface_in_class", "object_casts",
    "any_object_casts", "package_basic", "array_add_value_semantics",
    "escaping_capture_contract",
)
EXTENDED_MANIFEST_CASES = (
    "function_objects", "builtin_function_ref", "frontend_block_jump_state_machine",
    "block_features", "block_hierarchy", "ctor_named_function", "is_pattern_if",
    "any_cast", "custom_cast_simple", "array_contextual",
    "array_const_len", "knc_f32_rounding", "float_literal_rounding",
)

AGGREGATE_CASES = {
    "nested-capture-forwarding": ('''
Function IO.Type.Object.Function MakeNested(int start) {
    int value = start;
    IO.Type.Object.Function factory = Function IO.Type.Object.Function() {
        Return Function int() { value++; Return value; };
    };
    IO.Type.Object.Function counter = factory();
    Return counter;
}
Static Function int Main() {
    IO.Type.Object.Function first = MakeNested(10);
    IO.Type.Object.Function second = MakeNested(20);
    IO.Console.PrintLine(first());
    IO.Console.PrintLine(second());
    IO.Console.PrintLine(first());
    Return 0;
}
''', "11\n21\n12\n"),
    "foreach-capture-cells": ('''
Function IO.Type.Object.Function[] MakeCells() {
    int[] values = {10, 20, 30};
    IO.Type.Object.Function[3] functions;
    int index = 0;
    Foreach (int value Is values) {
        functions[index] = Function int() { Return value; };
        index++;
    }
    Return functions;
}
Static Function int Main() {
    IO.Type.Object.Function[] functions = MakeCells();
    IO.Console.PrintLine(functions[0]());
    IO.Console.PrintLine(functions[1]());
    IO.Console.PrintLine(functions[2]());
    Return 0;
}
''', "10\n20\n30\n"),
    "object-allocation": ('''
Class Box { Public int Value; }
Static Function int Main() {
    Box box = New Box(); box.Value = 2;
    If (box.Value != 2) Return 1;
    Return 0;
}
''', ""),
}

# These isolate supported features from broader manifest cases which also use
# objects, pointers or runtime facilities outside the initial emitter's scope.
CASES = {
    "main-empty-arguments": ('''
Static Function int Main(string[] args) {
    IO.Console.PrintLine(args.Length()); Return args.Length() == 0 ? 0 : 1;
}
''', "0\n"),
    "scalar-globals": ('''
int Seed = 7;
int Total = Seed + 3;
Function int Bump(int value) { Total += value; Return Total; }
Static Function int Main() {
    If (Seed != 7 || Total != 10 || Bump(4) != 14 || Total != 14) Return 1;
    IO.Console.PrintLine(Total); Return 0;
}
''', "14\n"),
    "for-continue-break": ('''
Static Function int Main() {
    int sum = 0;
    For (int i = 0; i < 8; i++) {
        If (i == 2) Continue;
        If (i == 6) Break;
        sum += i;
    }
    IO.Console.PrintLine(sum); Return sum == 13 ? 0 : 1;
}
''', "13\n"),
    "foreach-array": ('''
Static Function int Main() {
    int[] values = {2, 3, 5, 7}; int sum = 0;
    Foreach (int value In values) {
        If (value == 3) Continue;
        sum += value;
    }
    IO.Console.PrintLine(sum); Return sum == 14 ? 0 : 1;
}
''', "14\n"),
    "array-value-add": ('''
Static Function int Main() {
    int[] empty = {}; int[] first = empty.Add(3);
    int[] second = first.Add(4); int[] alias = first;
    second[0] = 9; first.Add(100); alias[0] = 7;
    If (empty.Length() != 0 || first.Length() != 1 || first[0] != 7 ||
        second.Length() != 2 || second[0] != 9 || second[1] != 4) Return 1;
    IO.Console.PrintLine("ok"); Return 0;
}
''', "ok\n"),
    "scalar-f32-transfers": ('''
f32 Saved = 16777217.0;
Function f32 Echo(f32 value) { Return value; }
Function f32 RoundReturn() { Return 16777217.0; }
Static Function int Main() {
    f32 local = 16777217.0; f32[] values = {16777219.0};
    If ([float](Saved) != 16777216.0 || [float](local) != 16777216.0 ||
        [float](values[0]) != 16777220.0 || [float](Echo(16777217.0)) != 16777216.0 ||
        [float](RoundReturn()) != 16777216.0) Return 1;
    local = 16777219.0;
    If ([float](local) != 16777220.0) Return 2;
    IO.Console.PrintLine("ok"); Return 0;
}
''', "ok\n"),
    "scalar-f32-arithmetic": ('''
Static Function int Main() {
    f32 big = 16777216.0; f32 one = 1.0; f32 three = 3.0;
    If ([float](big + one) != 16777216.0 || [float]((big + one) - big) != 0.0 ||
        [float](one / three) != 0.3333333432674407958984375) Return 1;
    IO.Console.PrintLine("ok"); Return 0;
}
''', "ok\n"),
    "scalar-int-f32-rounding": ('''
Static Function int Main() {
    int signedTrap = 4611686293305294849;
    u64 unsignedTrap = 9223372586610589697;
    f32 signedValue = signedTrap; f32 unsignedValue = unsignedTrap;
    If ([float](signedValue) != 4611686568183201792.0 ||
        [float](unsignedValue) != 9223373136366403584.0) Return 1;
    IO.Console.PrintLine("ok"); Return 0;
}
''', "ok\n"),
    "unsigned-division-shifts": ('''
Static Function int Main() {
    u64 high = 18446744073709551615; u64 two = 2;
    If (high / two != [u64](9223372036854775807) || high % two != [u64](1) ||
        (high >> 63) != [u64](1) || !(high > two) || high < two) Return 1;
    u8 narrow = 255; u8 divisor = 2;
    If (narrow / divisor != [u8](127) || narrow % divisor != [u8](1)) Return 2;
    IO.Console.PrintLine("ok"); Return 0;
}
''', "ok\n"),
    "switch-loop-control": ('''
Static Function int Main() {
    int sum = 0;
    For (int i = 0; i < 4; i++) {
        Switch (i) {
            Case (1) { Continue; }
            Case (2) { sum += 20; Break; }
            Case (default) { sum += i; }
        }
        sum += 1;
    }
    IO.Console.PrintLine(sum); Return sum == 21 ? 0 : 1;
}
''', "21\n"),
    "recursive-call": ('''
Function int Factorial(int value) { If (value < 2) Return 1; Return value * Factorial(value - 1); }
Static Function int Main() { IO.Console.PrintLine(Factorial(6)); Return 0; }
''', "720\n"),
    "four-argument-call": ('''
Function int Sum(int a, int b, int c, int d) { Return a + b + c + d; }
Static Function int Main() { IO.Console.PrintLine(Sum(1, 2, 3, 4)); Return 0; }
''', "10\n"),
}

# Stage0 does not initialize Main's string[] parameter; an args.Length() read
# can alias an unrelated VM array. Check the new empty-array invariant directly
# rather than treating that old undefined value as a differential oracle.
SELFHOST_FIXTURES = {
    "conditional-numeric-join-global-increments": ("tests/selfhost/fixtures/knc_backend/if_numeric_join_global_increments.kn", ""),
    "switch-default-prefix": ("tests/selfhost/fixtures/knc_backend/switch_default_prefix.kn", ""),
}
AGGREGATE_REFERENCE_GAPS = {
    "nested_interface_in_class": "stage0 VM reports Virtual slot out of range",
    "object_casts": "stage0 interface dispatch differs from expected manifest output",
    "any_object_casts": "stage0 does not map list.Create in this case",
}
SELFHOST_ONLY = {"main-empty-arguments", *SELFHOST_FIXTURES, *AGGREGATE_REFERENCE_GAPS}

UNSUPPORTED = {
    "extern-call": '''
Extern Function int KncUnavailableNative(int value) By C;
Trusted Static Function int Main() { Return KncUnavailableNative(1); }
''',
    "five-argument-call": '''
Function int Sum(int a, int b, int c, int d, int e) { Return a+b+c+d+e; }
Static Function int Main() { Return Sum(1,2,3,4,5); }
''',
    "extended-float": '''
Static Function int Main() { f128 value = [f128](1.0); Return [int](value); }
''',
}


def _run(command: list[str], root: Path, log: Path, timeout: int = 120) -> subprocess.CompletedProcess:
    result = subprocess.run(command, cwd=root, text=True, capture_output=True, timeout=timeout)
    log.write_text(result.stdout + result.stderr, encoding="utf-8")
    return result


def compiler_input_hashes(compiler: Path) -> dict[str, str]:
    """Hash the code payload as well as the repository's POSIX launcher."""
    paths = [compiler]
    data = compiler.read_bytes()
    payload = compiler.with_name("kinal.bin")
    if data.startswith(b"#!") and b"kinal.bin" in data and payload.is_file():
        paths.append(payload)
    return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}


def stage_source_packages(root: Path, destination: Path) -> Path:
    """Copy source manifests, never rewrite the repository's package metadata."""
    destination.mkdir(parents=True, exist_ok=True)
    source_root = root / "libs/std"
    for manifest in sorted(source_root.glob("*/*/package.knpkg.json")):
        relative = manifest.parent.relative_to(source_root)
        target = destination / relative
        target.mkdir(parents=True, exist_ok=True)
        metadata = json.loads(manifest.read_text(encoding="utf-8"))
        metadata.pop("klib", None)
        (target / manifest.name).write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        source = manifest.parent / str(metadata.get("source_root", "src"))
        if source.is_dir():
            shutil.copytree(source, target / str(metadata.get("source_root", "src")), dirs_exist_ok=True)
    return destination


def _project(path: Path, source: Path, *, require_unit: bool = True,
             auto_discovery: bool = True) -> None:
    # Keep Build.Backend's default Native so this independent harness can be
    # compiled before Driver exposes KNC as a project/profile backend.
    quoted = json.dumps(str(source))
    required = "true" if require_unit else "false"
    discovery = "true" if auto_discovery else "false"
    path.write_text('Project KncCase { DefaultProfile = "test"; '
                    f'SourceSet "main" {{ Files = [{quoted}]; RequireUnit = {required}; }} '
                    f'Profile "test" {{ Source {{ Entry = {quoted}; Sets = ["main"]; '
                    f'Mode = FileOnly; AutoDiscovery = {discovery}; }} }} }}\n', encoding="utf-8")


def build_harness(stage0: Path, root: Path, out: Path, packages: Path,
                  link_args: tuple[str, ...]) -> Path:
    executable = out / ("knc-backend-driver.exe" if os.name == "nt" else "knc-backend-driver")
    project = root / "tests/selfhost/fixtures/knc_backend/kinal.knproj"
    command = [str(stage0), "build", "--project", str(project), "--stdpkg-root", str(packages),
               "-o", str(executable)]
    for argument in link_args:
        command += ["--link-arg", argument]
    result = _run(command, root, out / "harness.build.log", timeout=900)
    assert result.returncode == 0, ("harness compilation failed", out / "harness.build.log")
    return executable


def _emission_command(harness: Path | None, compiler: Path | None, project: Path,
                      output: Path, packages: Path, cache: Path, *, listing: Path | None = None,
                      no_superloop: bool = False, repeat: Path | None = None) -> list[str]:
    if compiler is not None:
        assert repeat is None, "same-object repeat is available only in the test harness"
        command = [str(compiler), "vm", "build", "--project", str(project), "--profile", "test",
                   "--stdpkg-root", str(packages), "-o", str(output)]
        if listing is not None:
            command += ["--listing", str(listing)]
    else:
        assert harness is not None, "missing KNC compiler or harness"
        command = [str(harness), str(project), str(output), str(packages), str(cache)]
        if listing is not None:
            command += ["--listing", str(listing)]
        if repeat is not None:
            command += ["--repeat", str(repeat)]
    if no_superloop:
        command += ["--no-superloop"]
    return command


def check_knc_backend(stage0: Path, vm: Path, root: Path, out: Path, *,
                      harness: Path | None = None, compiler: Path | None = None,
                      baseline_vm: Path | None = None, selected: tuple[str, ...] = (),
                      link_args: tuple[str, ...] = (), reference_only: bool = False,
                      suite: str = "scalar") -> dict:
    out.mkdir(parents=True, exist_ok=True)
    packages = stage_source_packages(root, out / "source-stdpkg")
    assert harness is None or compiler is None, "choose --harness or --compiler"
    if not reference_only and harness is None and compiler is None:
        harness = build_harness(stage0, root, out, packages, link_args)
    manifest = {case["name"]: case for case in json.loads((root / "tests/manifest.json").read_text())}
    registered = registered_cases(root)
    reference_hash = hashlib.sha256(stage0.read_bytes()).hexdigest()
    reference_inputs = compiler_input_hashes(stage0)
    vm_hash = hashlib.sha256(vm.read_bytes()).hexdigest()
    baseline_vm_hash = hashlib.sha256(baseline_vm.read_bytes()).hexdigest() if baseline_vm is not None else None
    harness_hash = hashlib.sha256(harness.read_bytes()).hexdigest() if harness is not None else None
    compiler_hash = hashlib.sha256(compiler.read_bytes()).hexdigest() if compiler is not None else None
    emitter = compiler if compiler is not None else harness
    emitter_hash = compiler_hash if compiler is not None else harness_hash
    provenance = {"reference_compiler": str(stage0), "reference_sha256": reference_hash,
                  "reference_input_sha256": reference_inputs,
                  "vm": str(vm), "vm_sha256": vm_hash,
                  "baseline_vm": str(baseline_vm) if baseline_vm is not None else None,
                  "baseline_vm_sha256": baseline_vm_hash,
                  "harness": str(harness) if harness is not None else None,
                  "harness_sha256": harness_hash,
                  "selfhost_compiler": str(compiler) if compiler is not None else None,
                  "selfhost_compiler_sha256": compiler_hash,
                  "emitter_kind": "integrated_cli" if compiler is not None else "harness"}
    (out / "execution-provenance.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    scalar_names = (*MANIFEST_CASES, *CASES, *SELFHOST_FIXTURES, *UNSUPPORTED)
    aggregate_names = (*AGGREGATE_MANIFEST_CASES, *AGGREGATE_CASES)
    defaults = (tuple(dict.fromkeys((*scalar_names, *aggregate_names, *EXTENDED_MANIFEST_CASES)))
                if suite == "all" else
                (aggregate_names if suite == "aggregates" else
                 (EXTENDED_MANIFEST_CASES if suite == "extended" else scalar_names)))
    if suite == "registered":
        defaults = tuple(registered)
    names = selected or defaults
    unknown = (set(names) - set(MANIFEST_CASES) - set(AGGREGATE_MANIFEST_CASES) - set(EXTENDED_MANIFEST_CASES) -
               set(CASES) - set(AGGREGATE_CASES) - set(SELFHOST_FIXTURES) - set(UNSUPPORTED) - set(registered))
    assert not unknown, ("unknown focused cases", sorted(unknown))
    results = []
    for name in names:
        directory = out / name
        directory.mkdir(exist_ok=True)
        aggregate = name in AGGREGATE_CASES
        negative = name in UNSUPPORTED and not aggregate
        expected_exit = 0
        if name in registered:
            source = registered[name]["source"]
            expected = registered[name]["expected"]
            expected_exit = registered[name]["expected_exit"]
        elif name in SELFHOST_FIXTURES:
            source = root / SELFHOST_FIXTURES[name][0]
            expected = SELFHOST_FIXTURES[name][1]
        elif name in manifest:
            source = root / manifest[name]["file"]
            expected = manifest[name]["expected"]
        else:
            source = directory / "Main.kn"
            body = (AGGREGATE_CASES[name][0] if aggregate else
                    (UNSUPPORTED[name] if negative else CASES[name][0]))
            source.write_text("Unit Tests.Selfhost.KncCase;\nGet IO.Console;\n" + body, encoding="utf-8")
            expected = AGGREGATE_CASES[name][1] if aggregate else ("" if negative else CASES[name][1])
        project = directory / "kinal.knproj"
        _project(project, source, require_unit=name not in manifest and name not in registered,
                 auto_discovery=name not in registered)
        row: dict = {"name": name, "ok": False, "negative": negative}
        no_superloop = name in registered and "--no-superloop" in registered[name].get("reference_flags", ())
        if name in registered:
            row["reference_flags"] = list(registered[name].get("reference_flags", ()))
            row["selfhost_flags"] = ["--no-superloop"] if no_superloop else []
        if name in AGGREGATE_REFERENCE_GAPS:
            row["reference_gap"] = AGGREGATE_REFERENCE_GAPS[name]
        try:
            if name == "knc_vm_time_file":
                # Match the accepted workflow's ordinary temporary-file setup,
                # inside this isolated repository rather than the main tree.
                (root / "out/test").mkdir(parents=True, exist_ok=True)
            if not negative and name not in SELFHOST_ONLY:
                reference = directory / "stage0.knc"
                command = ([str(stage0), "vm", "build", "--no-module-discovery",
                            *registered[name].get("reference_flags", ()), str(source), "-o", str(reference)]
                           if name in registered else
                           [str(stage0), "vm", "build", "--project", str(project), "-o", str(reference)])
                assert hashlib.sha256(stage0.read_bytes()).hexdigest() == reference_hash, "reference-compiler-changed"
                assert compiler_input_hashes(stage0) == reference_inputs, "reference-compiler-payload-changed"
                built = _run(command, root, directory / "stage0.build.log")
                assert hashlib.sha256(stage0.read_bytes()).hexdigest() == reference_hash, "reference-compiler-changed"
                assert compiler_input_hashes(stage0) == reference_inputs, "reference-compiler-payload-changed"
                assert built.returncode == 0, "stage0-build"
                assert hashlib.sha256(vm.read_bytes()).hexdigest() == vm_hash, "VM-changed"
                run = _run([str(vm), str(reference)], root, directory / "stage0.run.log", timeout=30)
                assert run.returncode == expected_exit and run.stdout.replace("\r\n", "\n") == expected, "stage0-run"
                if baseline_vm is not None:
                    assert hashlib.sha256(baseline_vm.read_bytes()).hexdigest() == baseline_vm_hash, "baseline-VM-changed"
                    baseline = _run([str(baseline_vm), str(reference)], root, directory / "stage0.baseline.run.log", timeout=30)
                    assert baseline.returncode == expected_exit and baseline.stdout.replace("\r\n", "\n") == expected, "stage0-baseline-VM-run"
                row["stage0_ok"] = True
            if reference_only:
                row["ok"] = not negative and name not in SELFHOST_ONLY
                if negative or name in SELFHOST_ONLY:
                    row["skipped"] = "case needs the selfhost harness"
            else:
                output = directory / "selfhost.knc"
                # Tests may be rerun after a successful build becomes an
                # unsupported case; never mistake a stale file for new output.
                if output.exists():
                    output.unlink()
                command = _emission_command(harness, compiler, project, output, packages, out / "package-cache",
                                            no_superloop=no_superloop)
                assert emitter is not None and hashlib.sha256(emitter.read_bytes()).hexdigest() == emitter_hash, "selfhost-emitter-changed"
                built = _run(command, root, directory / "selfhost.build.log")
                assert hashlib.sha256(emitter.read_bytes()).hexdigest() == emitter_hash, "selfhost-emitter-changed"
                if negative:
                    assert built.returncode != 0, "unsupported-build-succeeded"
                    assert "KNC:" in built.stdout + built.stderr, "missing-KNC-diagnostic"
                    assert not output.exists(), "unsupported-created-output"
                    protected = directory / "selfhost.protected.knc"
                    protected.write_bytes(b"existing artifact must be preserved")
                    preserve_command = _emission_command(harness, compiler, project, protected, packages, out / "package-cache",
                                                         no_superloop=no_superloop)
                    repeated = _run(preserve_command, root, directory / "selfhost.preserve.log")
                    assert repeated.returncode != 0, "unsupported-repeat-succeeded"
                    assert protected.read_bytes() == b"existing artifact must be preserved", "unsupported-overwrote-output"
                else:
                    assert built.returncode == 0, "selfhost-build"
                    binary = output.read_bytes()
                    assert binary[:8] == b"KNC2" + struct.pack("<HH", 3, 0), "wrong-KNC-version"
                    assert hashlib.sha256(vm.read_bytes()).hexdigest() == vm_hash, "VM-changed"
                    run = _run([str(vm), str(output)], root, directory / "selfhost.run.log", timeout=30)
                    assert run.returncode == expected_exit and run.stdout.replace("\r\n", "\n") == expected, "selfhost-run"
                    if baseline_vm is not None:
                        assert hashlib.sha256(baseline_vm.read_bytes()).hexdigest() == baseline_vm_hash, "baseline-VM-changed"
                        baseline = _run([str(baseline_vm), str(output)], root, directory / "selfhost.baseline.run.log", timeout=30)
                        assert baseline.returncode == expected_exit and baseline.stdout.replace("\r\n", "\n") == expected, "selfhost-baseline-VM-run"
                    again = directory / "selfhost.repeat.knc"
                    repeated = _run(_emission_command(harness, compiler, project, again, packages, out / "package-cache",
                                                       no_superloop=no_superloop),
                                    root, directory / "selfhost.repeat.log")
                    assert repeated.returncode == 0 and again.read_bytes() == binary, "nondeterministic-output"
                row["ok"] = True
        except (AssertionError, subprocess.TimeoutExpired, OSError) as error:
            row["failure"] = str(error)
        results.append(row)
        status = "SKIP" if row.get("skipped") else ("OK" if row["ok"] else "FAIL")
        print(f"[{status}] {name}" + (f": {row['failure']}" if "failure" in row else ""), flush=True)
    summary = {"name": "knc_backend", "ok": all(row["ok"] or row.get("skipped") for row in results),
               "suite": suite, "reference_only": reference_only, "reference_sha256": reference_hash,
               "reference_input_sha256": reference_inputs,
               "harness_sha256": harness_hash, "selfhost_compiler_sha256": compiler_hash,
               "emitter_kind": "integrated_cli" if compiler is not None else "harness",
               "vm_sha256": vm_hash, "baseline_vm_sha256": baseline_vm_hash, "results": results}
    (out / "results.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    return summary


def check_knc_workflow(stage0: Path, vm: Path, root: Path, out: Path, *,
                       harness: Path | None = None, compiler: Path | None = None,
                       baseline_vm: Path | None = None) -> dict:
    """Listing, optimization toggle, repeated emission, and real I/O failures."""
    assert (harness is None) != (compiler is None), "workflow requires --harness or --compiler"
    out.mkdir(parents=True, exist_ok=True)
    packages = stage_source_packages(root, out / "source-stdpkg")
    source = root / "tests/common/knc_superloop.kn"
    project = out / "kinal.knproj"
    _project(project, source, require_unit=False, auto_discovery=False)
    emitter = compiler if compiler is not None else harness
    paths = [stage0, vm, emitter] + ([baseline_vm] if baseline_vm is not None else [])
    hashes = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
    hashes.update(compiler_input_hashes(stage0))
    (out / "execution-provenance.json").write_text(json.dumps(hashes, indent=2) + "\n", encoding="utf-8")
    expected = "5\n0\n1000\n"
    reference = out / "stage0.knc"
    result = _run([str(stage0), "vm", "build", "--no-module-discovery", str(source), "-o", str(reference)],
                  root, out / "stage0.build.log")
    assert result.returncode == 0, "workflow stage0 build failed"
    runtimes = [("primary", vm)] + ([("baseline", baseline_vm)] if baseline_vm is not None else [])
    for label, runtime in runtimes:
        run = _run([str(runtime), str(reference)], root, out / f"stage0.{label}.run.log", timeout=30)
        assert run.returncode == 0 and run.stdout.replace("\r\n", "\n") == expected
    for disabled in (False, True):
        name = "disabled" if disabled else "enabled"
        binary = out / (name + ".knc")
        repeated = out / (name + ".repeat.knc")
        listing = out / "nested/listings" / (name + ".knasm")
        command = _emission_command(harness, compiler, project, binary, packages, out / "cache",
                                    listing=listing, no_superloop=disabled,
                                    repeat=repeated if harness is not None else None)
        built = _run(command, root, out / (name + ".build.log"))
        assert built.returncode == 0, (name, "workflow build failed")
        if compiler is not None:
            again = _run(_emission_command(harness, compiler, project, repeated, packages, out / "cache",
                                            no_superloop=disabled), root, out / (name + ".repeat.log"))
            assert again.returncode == 0, "repeated CLI emission failed"
        assert binary.read_bytes() == repeated.read_bytes(), "repeat emission changed binary bytes"
        text = listing.read_text(encoding="utf-8")
        assert "KNC2 v3" in text and ".function" in text and "Tests.KncSuperloop.Main" in text
        assert re.search(r"r\d+\(i\)", text), "local register names missing from listing"
        fused = re.search(r"^\d+: LoopInt(?:Lt|Le|Gt|Ge|Eq|Ne)(?:Inc|Dec)\s", text, re.M)
        assert bool(fused) != disabled, (name, "unexpected fused-loop listing")
        if not disabled:
            assert "fused loop on" in text, "fused-loop source comment missing"
        for label, runtime in runtimes:
            run = _run([str(runtime), str(binary)], root, out / f"{name}.{label}.run.log", timeout=30)
            assert run.returncode == 0 and run.stdout.replace("\r\n", "\n") == expected, (name, label)
    semantics = out / "superloop-semantics"
    semantics.mkdir(exist_ok=True)
    semantics_source = root / "tests/selfhost/fixtures/knc_backend/superloop_semantics.kn"
    semantics_project = semantics / "kinal.knproj"
    _project(semantics_project, semantics_source, auto_discovery=False)
    semantics_reference = semantics / "stage0.knc"
    built = _run([str(stage0), "vm", "build", "--no-module-discovery", str(semantics_source),
                  "-o", str(semantics_reference)], root, semantics / "stage0.build.log")
    assert built.returncode == 0, "superloop semantic reference build failed"
    for label, runtime in runtimes:
        run = _run([str(runtime), str(semantics_reference)], root, semantics / f"stage0.{label}.run.log", timeout=30)
        assert run.returncode == 0 and not run.stdout and not run.stderr, "superloop semantic reference failed"
    fused_names = ("LoopIntLtInc", "LoopIntLeInc", "LoopIntGtDec", "LoopIntGeDec",
                   "LoopIntEqInc", "LoopIntNeInc", "LoopIntEqDec", "LoopIntNeDec")
    for disabled in (False, True):
        name = "disabled" if disabled else "enabled"
        binary = semantics / (name + ".knc")
        listing = semantics / "listings" / (name + ".knasm")
        built = _run(_emission_command(harness, compiler, semantics_project, binary, packages, out / "cache",
                                       listing=listing, no_superloop=disabled),
                     root, semantics / (name + ".build.log"))
        assert built.returncode == 0, (name, "superloop semantic emission failed")
        text = listing.read_text(encoding="utf-8")
        for opcode in fused_names:
            present = re.search(r"^\d+: " + opcode + r"\s", text, re.M) is not None
            assert present != disabled, (name, opcode)
        for counter in ("changing", "body", "stride", "unsignedCounter", "narrowCounter", "captured", "moving"):
            assert re.search(r"fused loop on " + counter + r"\b", text) is None, ("unsafe loop fused", counter)
        for label, runtime in runtimes:
            run = _run([str(runtime), str(binary)], root, semantics / f"{name}.{label}.run.log", timeout=30)
            assert run.returncode == 0 and not run.stdout and not run.stderr, (name, label, "superloop semantics")
    writer_directory = out / "writer-directory"
    writer_directory.mkdir(exist_ok=True)
    (writer_directory / "sentinel").write_text("preserved", encoding="utf-8")
    writer = _run(_emission_command(harness, compiler, project, writer_directory, packages, out / "cache"),
                  root, out / "writer-failure.log")
    assert writer.returncode != 0 and "write" in (writer.stdout + writer.stderr).lower(), "writer I/O failure not reported"
    assert (writer_directory / "sentinel").read_text() == "preserved"
    listing_directory = out / "listing-directory"
    listing_directory.mkdir(exist_ok=True)
    (listing_directory / "sentinel").write_text("preserved", encoding="utf-8")
    listing_failure = _run(_emission_command(harness, compiler, project, out / "listing-failure.knc", packages,
                                             out / "cache", listing=listing_directory),
                           root, out / "listing-failure.log")
    assert listing_failure.returncode != 0 and "listing" in (listing_failure.stdout + listing_failure.stderr).lower(), "listing I/O failure not reported"
    assert (listing_directory / "sentinel").read_text() == "preserved"
    assert all(hashlib.sha256(Path(path).read_bytes()).hexdigest() == value for path, value in hashes.items()), "workflow tool changed during run"
    summary = {"name": "knc_workflow", "ok": True, "same_backend_repeat": harness is not None,
               "listing": True, "superloop_toggle": True, "writer_failure": True,
               "listing_failure": True, "superloop_semantic_checks": 19, "fused_opcode_count": 8,
               "hashes": hashes}
    (out / "results.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    return summary


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--vm", type=Path, required=True)
    parser.add_argument("--baseline-vm", type=Path, help="also execute every positive KNC file in this reference VM")
    parser.add_argument("--out-dir", type=Path, required=True)
    emitter = parser.add_mutually_exclusive_group()
    emitter.add_argument("--harness", type=Path)
    emitter.add_argument("--compiler", type=Path, help="integrated selfhost CLI; uses vm build, no harness build")
    parser.add_argument("--case", action="append", default=[])
    parser.add_argument("--link-arg", action="append", default=[])
    parser.add_argument("--reference-only", action="store_true")
    parser.add_argument("--workflow", action="store_true", help="test listing, superloop toggles, repetition and I/O errors")
    parser.add_argument("--suite", choices=("scalar", "aggregates", "extended", "all", "registered"), default="scalar")
    args = parser.parse_args()
    common = {"harness": args.harness.resolve() if args.harness else None,
              "compiler": args.compiler.resolve() if args.compiler else None,
              "baseline_vm": args.baseline_vm.resolve() if args.baseline_vm else None}
    if args.workflow:
        result = check_knc_workflow(args.stage0.resolve(), args.vm.resolve(), Path(__file__).resolve().parents[2],
                                    args.out_dir.resolve(), **common)
    else:
        result = check_knc_backend(args.stage0.resolve(), args.vm.resolve(), Path(__file__).resolve().parents[2],
                                  args.out_dir.resolve(), **common, selected=tuple(args.case),
                                  link_args=tuple(args.link_arg), reference_only=args.reference_only, suite=args.suite)
    print(json.dumps(result))
    raise SystemExit(0 if result["ok"] else 1)
