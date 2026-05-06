#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = ROOT / "out" / "stress"


@dataclass(frozen=True)
class StressProfile:
    name: str
    many_functions: int
    mega_expr_terms: int
    many_locals: int
    switch_cases: int
    nested_if_depth: int
    many_globals: int
    big_bss_len: int
    init_array_len: int
    large_string_len: int
    module_chain: int
    vm_functions: int
    pipeline_functions: int
    timeout_sec: int


@dataclass
class GeneratedCase:
    name: str
    mode: str
    description: str
    files: list[Path]
    build_inputs: list[Path]
    expected_output: str
    work_dir: Path | None = None
    expected_error_substring: str = ""
    compiler_args: list[str] = field(default_factory=list)
    use_module_discovery: bool = False
    timeout_sec: int = 180


@dataclass
class CaseResult:
    name: str
    mode: str
    ok: bool
    description: str
    duration_sec: float
    source_files: int
    total_source_bytes: int
    total_source_lines: int
    artifact: str
    artifact_bytes: int
    output: str
    error: str
    case_dir: str


PROFILES: dict[str, StressProfile] = {
    "quick": StressProfile(
        name="quick",
        many_functions=300,
        mega_expr_terms=1200,
        many_locals=1200,
        switch_cases=300,
        nested_if_depth=120,
        many_globals=1200,
        big_bss_len=65536,
        init_array_len=1024,
        large_string_len=65536,
        module_chain=12,
        vm_functions=180,
        pipeline_functions=220,
        timeout_sec=90,
    ),
    "full": StressProfile(
        name="full",
        many_functions=1800,
        mega_expr_terms=6000,
        many_locals=5000,
        switch_cases=1800,
        nested_if_depth=420,
        many_globals=5000,
        big_bss_len=262144,
        init_array_len=4096,
        large_string_len=262144,
        module_chain=48,
        vm_functions=900,
        pipeline_functions=1200,
        timeout_sec=180,
    ),
    "max": StressProfile(
        name="max",
        many_functions=4000,
        mega_expr_terms=14000,
        many_locals=10000,
        switch_cases=4000,
        nested_if_depth=900,
        many_globals=12000,
        big_bss_len=524288,
        init_array_len=8192,
        large_string_len=524288,
        module_chain=96,
        vm_functions=1800,
        pipeline_functions=2600,
        timeout_sec=360,
    ),
}


def ensure_clean_dir(path: Path, *, keep: bool) -> None:
    if path.exists() and not keep:
        shutil.rmtree(path)
    path.mkdir(parents=True, exist_ok=True)


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def artifact_path(base: Path) -> Path:
    return base.with_suffix(".exe") if sys.platform == "win32" else base


def normalize_output(text: str) -> str:
    return text.replace("\r\n", "\n")


def run_capture(cmd: list[str], *, timeout_sec: int, cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=str(cwd),
        text=True,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout_sec,
    )


def compiler_version_text(compiler: Path) -> str:
    proc = run_capture([str(compiler), "--version"], timeout_sec=30, cwd=ROOT)
    return normalize_output((proc.stdout or "") + (proc.stderr or "")).strip()


def source_metrics(paths: list[Path]) -> tuple[int, int]:
    total_bytes = 0
    total_lines = 0
    for path in paths:
        text = path.read_text(encoding="utf-8")
        total_bytes += len(text.encode("utf-8"))
        total_lines += text.count("\n")
    return total_bytes, total_lines


def make_header(unit_name: str, *, with_console: bool = True) -> list[str]:
    lines = [f"Unit {unit_name};", ""]
    if with_console:
        lines.extend(["Get Console By IO.Console;", ""])
    return lines


def generate_many_functions(case_dir: Path, profile: StressProfile) -> GeneratedCase:
    unit = "Stress.ManyFunctions"
    path = case_dir / "main.kn"
    lines = make_header(unit)
    for i in range(profile.many_functions):
        lines.extend(
            [
                f"Function int F{i}()",
                "{",
                f"    Return {i};",
                "}",
                "",
            ]
        )
    sample = sorted({0, profile.many_functions // 3, (profile.many_functions * 2) // 3, profile.many_functions - 1})
    sum_expr = "+ ".join(f"F{i}()" for i in sample)
    expected = sum(sample)
    lines.extend(
        [
            "Static Function int Main()",
            "{",
            f"    int sum= {sum_expr};",
            "    Console.PrintLine(sum);",
            "    Return 0;",
            "}",
            "",
        ]
    )
    write_text(path, "\n".join(lines))
    return GeneratedCase(
        name="many_functions_native",
        mode="native-run",
        description="many top-level functions and symbol-table/codegen pressure",
        files=[path],
        build_inputs=[path],
        expected_output=f"{expected}\n",
        timeout_sec=profile.timeout_sec,
    )


def generate_mega_expression(case_dir: Path, profile: StressProfile) -> GeneratedCase:
    unit = "Stress.MegaExpression"
    path = case_dir / "main.kn"
    terms = list(range(1, profile.mega_expr_terms + 1))
    expr = "+".join(str(v) for v in terms)
    expected = sum(terms)
    text = "\n".join(
        make_header(unit)
        + [
            "Static Function int Calc()",
            "{",
            f"    Return {expr};",
            "}",
            "",
            "Static Function int Main()",
            "{",
            "    Console.PrintLine(Calc());",
            "    Return 0;",
            "}",
            "",
        ]
    )
    write_text(path, text)
    return GeneratedCase(
        name="mega_expression_native",
        mode="native-run",
        description="single-line giant expression parser/codegen pressure",
        files=[path],
        build_inputs=[path],
        expected_output=f"{expected}\n",
        expected_error_substring="Expression Too Complex",
        timeout_sec=profile.timeout_sec,
    )


def generate_many_locals(case_dir: Path, profile: StressProfile) -> GeneratedCase:
    unit = "Stress.ManyLocals"
    path = case_dir / "main.kn"
    lines = make_header(unit)
    lines.extend(["Static Function int Main()", "{"])
    for i in range(profile.many_locals):
        lines.append(f"    int v{i}= {i};")
    sample = [0, profile.many_locals // 2, profile.many_locals - 1]
    expected = sum(sample)
    expr = "+ ".join(f"v{i}" for i in sample)
    lines.extend(
        [
            f"    int probe= {expr};",
            "    Console.PrintLine(probe);",
            "    Return 0;",
            "}",
            "",
        ]
    )
    write_text(path, "\n".join(lines))
    return GeneratedCase(
        name="many_locals_native",
        mode="native-run",
        description="large local variable table and long function body",
        files=[path],
        build_inputs=[path],
        expected_output=f"{expected}\n",
        timeout_sec=profile.timeout_sec,
    )


def generate_huge_switch(case_dir: Path, profile: StressProfile) -> GeneratedCase:
    unit = "Stress.HugeSwitch"
    path = case_dir / "main.kn"
    lines = make_header(unit)
    lines.extend(["Static Function int Pick(int value)", "{", "    Return Switch (value)", "    {"])
    for i in range(profile.switch_cases):
        lines.extend(
            [
                f"        Case({i})",
                "        {",
                f"            {i * 3 + 1};",
                "        }",
            ]
        )
    lines.extend(
        [
            "        Case(",
            "        default)",
            "        {",
            "            -1;",
            "        }",
            "    };",
            "}",
            "",
            "Static Function int Main()",
            "{",
            "    Console.PrintLine(Pick(0));",
            f"    Console.PrintLine(Pick({profile.switch_cases // 2}));",
            f"    Console.PrintLine(Pick({profile.switch_cases - 1}));",
            "    Console.PrintLine(Pick(-5));",
            "    Return 0;",
            "}",
            "",
        ]
    )
    expected = [
        1,
        (profile.switch_cases // 2) * 3 + 1,
        (profile.switch_cases - 1) * 3 + 1,
        -1,
    ]
    write_text(path, "\n".join(lines))
    return GeneratedCase(
        name="huge_switch_native",
        mode="native-run",
        description="large switch matcher and expression lowering pressure",
        files=[path],
        build_inputs=[path],
        expected_output="".join(f"{value}\n" for value in expected),
        timeout_sec=profile.timeout_sec,
    )


def generate_deep_if(case_dir: Path, profile: StressProfile) -> GeneratedCase:
    unit = "Stress.DeepIf"
    path = case_dir / "main.kn"
    lines = make_header(unit)
    lines.extend(["Static Function int Main()", "{", "    int value= 0;"])
    for _ in range(profile.nested_if_depth):
        lines.extend(["    If (true)", "    {"])
    lines.append("    value= 1;")
    for _ in range(profile.nested_if_depth):
        lines.append("    }")
    lines.extend(["    Console.PrintLine(value);", "    Return 0;", "}", ""])
    write_text(path, "\n".join(lines))
    return GeneratedCase(
        name="deep_if_native",
        mode="native-run",
        description="deep recursive statement nesting pressure",
        files=[path],
        build_inputs=[path],
        expected_output="1\n",
        expected_error_substring="Statement Too Deep",
        timeout_sec=profile.timeout_sec,
    )


def generate_many_globals(case_dir: Path, profile: StressProfile) -> GeneratedCase:
    unit = "Stress.ManyGlobals"
    path = case_dir / "main.kn"
    lines = make_header(unit)
    for i in range(profile.many_globals):
        lines.append(f"int G{i}= {i};")
    lines.append("")
    sample = [0, profile.many_globals // 2, profile.many_globals - 1]
    expected = sum(sample)
    lines.extend(
        [
            "Static Function int Main()",
            "{",
            f"    int probe= {'+ '.join(f'G{i}' for i in sample)};",
            "    Console.PrintLine(probe);",
            "    Return 0;",
            "}",
            "",
        ]
    )
    write_text(path, "\n".join(lines))
    return GeneratedCase(
        name="many_globals_native",
        mode="native-run",
        description="large top-level declaration and global-symbol pressure",
        files=[path],
        build_inputs=[path],
        expected_output=f"{expected}\n",
        timeout_sec=profile.timeout_sec,
    )


def generate_big_bss_array(case_dir: Path, profile: StressProfile) -> GeneratedCase:
    unit = "Stress.BigBssArray"
    path = case_dir / "main.kn"
    last = profile.big_bss_len - 1
    mid = profile.big_bss_len // 2
    text = "\n".join(
        make_header(unit)
        + [
            f"int gData[{profile.big_bss_len}];",
            "",
            "Static Function int Main()",
            "{",
            "    gData[0]= 1;",
            f"    gData[{mid}]= 2;",
            f"    gData[{last}]= 3;",
            f"    Console.PrintLine(gData[0]+ gData[{mid}]+ gData[{last}]);",
            "    Console.PrintLine(gData.Length());",
            "    Return 0;",
            "}",
            "",
        ]
    )
    write_text(path, text)
    return GeneratedCase(
        name="big_bss_array_native",
        mode="native-run",
        description="large zero-initialized global array and backing-storage pressure",
        files=[path],
        build_inputs=[path],
        expected_output=f"6\n{profile.big_bss_len}\n",
        timeout_sec=profile.timeout_sec,
    )


def generate_big_initializer_array(case_dir: Path, profile: StressProfile) -> GeneratedCase:
    unit = "Stress.BigInitArray"
    path = case_dir / "main.kn"
    values = list(range(profile.init_array_len))
    last = profile.init_array_len - 1
    mid = profile.init_array_len // 2
    lines = make_header(unit)
    lines.append(f"int gData[{profile.init_array_len}]= {{")
    chunk: list[str] = []
    for i, value in enumerate(values):
        chunk.append(str(value))
        if len(chunk) == 16 or i == last:
            suffix = "," if i != last else ""
            lines.append("    " + ", ".join(chunk) + suffix)
            chunk = []
    lines.extend(
        [
            "};",
            "",
            "Function int Clobber()",
            "{",
            f"    int scratch[{max(profile.init_array_len // 2, 512)}];",
            "    scratch[0]= 11;",
            f"    scratch[{max(profile.init_array_len // 2, 512) - 1}]= 29;",
            f"    Return scratch[0]+ scratch[{max(profile.init_array_len // 2, 512) - 1}];",
            "}",
            "",
            "Static Function int Main()",
            "{",
            "    Console.PrintLine(Clobber());",
            f"    Console.PrintLine(gData[0]+ gData[{mid}]+ gData[{last}]);",
            "    Console.PrintLine(gData.Length());",
            "    Return 0;",
            "}",
            "",
        ]
    )
    expected = values[0] + values[mid] + values[last]
    write_text(path, "\n".join(lines))
    return GeneratedCase(
        name="big_initializer_array_native",
        mode="native-run",
        description="large initialized global array and data-emission pressure",
        files=[path],
        build_inputs=[path],
        expected_output=f"40\n{expected}\n{profile.init_array_len}\n",
        timeout_sec=profile.timeout_sec,
    )


def generate_array_escape(case_dir: Path, profile: StressProfile) -> GeneratedCase:
    unit = "Stress.ArrayEscape"
    path = case_dir / "main.kn"
    count = min(max(profile.init_array_len // 4, 128), 512)
    scratch_len = count * 4
    values = list(range(count))
    last = count - 1
    mid = count // 2
    lines = make_header(unit)
    lines.append(f"Function int[] MakeData()")
    lines.append("{")
    lines.append(f"    int data[{count}]= {{")
    chunk: list[str] = []
    for i, value in enumerate(values):
        chunk.append(str(value))
        if len(chunk) == 16 or i == last:
            suffix = "," if i != last else ""
            lines.append("        " + ", ".join(chunk) + suffix)
            chunk = []
    lines.extend(
        [
            "    };",
            "    Return data;",
            "}",
            "",
            "Function int Clobber()",
            "{",
            f"    int scratch[{scratch_len}];",
            "    scratch[0]= 7;",
            f"    scratch[{scratch_len - 1}]= 11;",
            f"    Return scratch[0]+ scratch[{scratch_len - 1}];",
            "}",
            "",
            "Static Function int Main()",
            "{",
            "    int[] data= MakeData();",
            "    Console.PrintLine(Clobber());",
            f"    Console.PrintLine(data[0]+ data[{mid}]+ data[{last}]);",
            "    Console.PrintLine(data.Length());",
            "    Return 0;",
            "}",
            "",
        ]
    )
    expected = values[0] + values[mid] + values[last]
    write_text(path, "\n".join(lines))
    return GeneratedCase(
        name="array_escape_native",
        mode="native-run",
        description="array literal escape and lifetime pressure across function boundaries",
        files=[path],
        build_inputs=[path],
        expected_output=f"18\n{expected}\n{count}\n",
        timeout_sec=profile.timeout_sec,
    )


def generate_large_string(case_dir: Path, profile: StressProfile) -> GeneratedCase:
    unit = "Stress.LargeString"
    path = case_dir / "main.kn"
    fill_len = max(profile.large_string_len - 2, 0)
    payload = "A" + ("b" * fill_len) + "Z"
    text = "\n".join(
        make_header(unit)
        + [
            "Static Function int Main()",
            "{",
            f'    string text= "{payload}";',
            "    Console.PrintLine(text.Length());",
            "    Console.PrintLine(text[0]);",
            "    Console.PrintLine(text[text.Length()- 1]);",
            "    Return 0;",
            "}",
            "",
        ]
    )
    write_text(path, text)
    return GeneratedCase(
        name="large_string_native",
        mode="native-run",
        description="large literal lexer/parser/string-lowering pressure",
        files=[path],
        build_inputs=[path],
        expected_output=f"{len(payload)}\nA\nZ\n",
        timeout_sec=profile.timeout_sec,
    )


def generate_multifile_chain(case_dir: Path, profile: StressProfile) -> GeneratedCase:
    base_dir = case_dir / "src"
    files: list[Path] = []
    total = 0
    for i in range(profile.module_chain):
        total += i
        unit = f"Stress.Multi.M{i}"
        path = base_dir / f"M{i}.kn"
        lines = [f"Unit {unit};", ""]
        if i + 1 < profile.module_chain:
            lines.extend(
                [
                    f"Get Next By Stress.Multi.M{i + 1};",
                    "",
                    "Function int Value()",
                    "{",
                    f"    Return {i}+ Next.Value();",
                    "}",
                    "",
                ]
            )
        else:
            lines.extend(
                [
                    "Function int Value()",
                    "{",
                    f"    Return {i};",
                    "}",
                    "",
                ]
            )
        write_text(path, "\n".join(lines))
        files.append(path)

    main = base_dir / "Main.kn"
    write_text(
        main,
        "\n".join(
            [
                "Unit Stress.Multi.Main;",
                "",
                "Get Console By IO.Console;",
                "Get Root By Stress.Multi.M0;",
                "",
                "Static Function int Main()",
                "{",
                "    Console.PrintLine(Root.Value());",
                "    Return 0;",
                "}",
                "",
            ]
        ),
    )
    files.append(main)
    return GeneratedCase(
        name="multifile_chain_native",
        mode="native-run",
        description="transitive multi-file module discovery and link pressure",
        files=files,
        build_inputs=[main],
        expected_output=f"{total}\n",
        use_module_discovery=True,
        timeout_sec=profile.timeout_sec,
    )


def generate_vm_case(case_dir: Path, profile: StressProfile) -> GeneratedCase:
    unit = "Stress.VMManyFunctions"
    path = case_dir / "main.kn"
    lines = make_header(unit)
    for i in range(profile.vm_functions):
        lines.extend(
            [
                f"Function int F{i}()",
                "{",
                f"    Return {i};",
                "}",
                "",
            ]
        )
    sample = sorted({0, profile.vm_functions // 2, profile.vm_functions - 1})
    expected = sum(sample)
    lines.extend(
        [
            "Static Function int Main()",
            "{",
            f"    Console.PrintLine({' + '.join(f'F{i}()' for i in sample)});",
            "    Return 0;",
            "}",
            "",
        ]
    )
    write_text(path, "\n".join(lines))
    return GeneratedCase(
        name="vm_many_functions",
        mode="vm-run",
        description="frontend plus VM build/run path under larger source load",
        files=[path],
        build_inputs=[path],
        expected_output=f"{expected}\n",
        timeout_sec=profile.timeout_sec,
    )


def generate_pipeline_case(case_dir: Path, profile: StressProfile) -> GeneratedCase:
    unit = "Stress.PipelineManyFunctions"
    path = case_dir / "main.kn"
    lines = make_header(unit)
    for i in range(profile.pipeline_functions):
        lines.extend(
            [
                f"Function int F{i}()",
                "{",
                f"    Return {i};",
                "}",
                "",
            ]
        )
    sample = sorted({0, profile.pipeline_functions // 4, profile.pipeline_functions // 2, profile.pipeline_functions - 1})
    expected = sum(sample)
    lines.extend(
        [
            "Static Function int Main()",
            "{",
            f"    Console.PrintLine({' + '.join(f'F{i}()' for i in sample)});",
            "    Return 0;",
            "}",
            "",
        ]
    )
    write_text(path, "\n".join(lines))
    return GeneratedCase(
        name="pipeline_many_functions",
        mode="pipeline-run",
        description="IR -> ASM -> OBJ -> EXE pipeline on larger generated source",
        files=[path],
        build_inputs=[path],
        expected_output=f"{expected}\n",
        timeout_sec=profile.timeout_sec,
    )


CASE_BUILDERS = [
    generate_many_functions,
    generate_mega_expression,
    generate_many_locals,
    generate_huge_switch,
    generate_deep_if,
    generate_many_globals,
    generate_big_bss_array,
    generate_big_initializer_array,
    generate_array_escape,
    generate_large_string,
    generate_multifile_chain,
    generate_vm_case,
    generate_pipeline_case,
]


def compile_native(compiler: Path, case: GeneratedCase, case_dir: Path) -> tuple[bool, str, str, Path, float]:
    artifact = artifact_path(case_dir / case.name)
    cmd = [str(compiler), "build", "--color", "never"]
    if not case.use_module_discovery:
        cmd.append("--no-module-discovery")
    cmd.extend(case.compiler_args)
    cmd.extend(str(path) for path in case.build_inputs)
    cmd.extend(["-o", str(artifact)])

    start = time.perf_counter()
    proc = run_capture(cmd, timeout_sec=case.timeout_sec, cwd=case_dir)
    build_time = time.perf_counter() - start
    build_text = normalize_output((proc.stdout or "") + (proc.stderr or ""))
    if proc.returncode != 0:
        build_text = (f"<compile-exit:{proc.returncode}>\n{build_text}").strip() + "\n"
        if case.expected_error_substring and case.expected_error_substring in build_text:
            return True, build_text, build_text, artifact, build_time
        return False, build_text, "", artifact, build_time

    run_proc = run_capture([str(artifact)], timeout_sec=case.timeout_sec, cwd=case_dir)
    run_text = normalize_output((run_proc.stdout or "") + (run_proc.stderr or ""))
    if run_proc.returncode != 0:
        run_text = (f"<run-exit:{run_proc.returncode}>\n{run_text}").strip() + "\n"
        return False, build_text, run_text, artifact, build_time
    if run_text != case.expected_output:
        mismatch = (
            "<output-mismatch>\n"
            f"<expected>\n{case.expected_output}</expected>\n"
            f"<actual>\n{run_text}</actual>\n"
        )
        return False, mismatch, run_text, artifact, build_time
    return True, build_text, run_text, artifact, build_time


def compile_vm(compiler: Path, case: GeneratedCase, case_dir: Path) -> tuple[bool, str, str, Path, float]:
    artifact = case_dir / f"{case.name}.knc"
    build_cmd = [str(compiler), "vm", "build", "--color", "never", str(case.build_inputs[0]), "-o", str(artifact)]
    start = time.perf_counter()
    proc = run_capture(build_cmd, timeout_sec=case.timeout_sec, cwd=case_dir)
    build_time = time.perf_counter() - start
    build_text = normalize_output((proc.stdout or "") + (proc.stderr or ""))
    if proc.returncode != 0:
        build_text = (f"<compile-exit:{proc.returncode}>\n{build_text}").strip() + "\n"
        if case.expected_error_substring and case.expected_error_substring in build_text:
            return True, build_text, build_text, artifact, build_time
        return False, build_text, "", artifact, build_time

    run_proc = run_capture([str(compiler), "vm", "run", "--color", "never", str(artifact)], timeout_sec=case.timeout_sec, cwd=case_dir)
    run_text = normalize_output((run_proc.stdout or "") + (run_proc.stderr or ""))
    if run_proc.returncode != 0:
        run_text = (f"<run-exit:{run_proc.returncode}>\n{run_text}").strip() + "\n"
        return False, build_text, run_text, artifact, build_time
    if run_text != case.expected_output:
        mismatch = (
            "<output-mismatch>\n"
            f"<expected>\n{case.expected_output}</expected>\n"
            f"<actual>\n{run_text}</actual>\n"
        )
        return False, mismatch, run_text, artifact, build_time
    return True, build_text, run_text, artifact, build_time


def compile_pipeline(compiler: Path, case: GeneratedCase, case_dir: Path) -> tuple[bool, str, str, Path, float]:
    ir_path = case_dir / f"{case.name}.ll"
    asm_path = case_dir / f"{case.name}.s"
    obj_path = case_dir / (f"{case.name}.obj" if sys.platform == "win32" else f"{case.name}.o")
    exe_path = artifact_path(case_dir / case.name)
    log_parts: list[str] = []
    start = time.perf_counter()
    commands = [
        [str(compiler), "build", "--color", "never", "--no-module-discovery", "--emit", "ir", str(case.build_inputs[0]), "-o", str(ir_path)],
        [str(compiler), "build", "--color", "never", "--no-module-discovery", "--emit", "asm", str(ir_path), "-o", str(asm_path)],
        [str(compiler), "build", "--color", "never", "--no-module-discovery", "--emit", "obj", str(asm_path), "-o", str(obj_path)],
        [str(compiler), "build", "--color", "never", "--no-module-discovery", str(obj_path), "-o", str(exe_path)],
    ]
    for cmd in commands:
        proc = run_capture(cmd, timeout_sec=case.timeout_sec, cwd=case_dir)
        text = normalize_output((proc.stdout or "") + (proc.stderr or ""))
        log_parts.append("$ " + " ".join(cmd) + "\n" + f"<exit:{proc.returncode}>\n" + text)
        if proc.returncode != 0:
            return False, "\n".join(log_parts), "", exe_path, time.perf_counter() - start

    run_proc = run_capture([str(exe_path)], timeout_sec=case.timeout_sec, cwd=case_dir)
    run_text = normalize_output((run_proc.stdout or "") + (run_proc.stderr or ""))
    if run_proc.returncode != 0:
        run_text = (f"<run-exit:{run_proc.returncode}>\n{run_text}").strip() + "\n"
        return False, "\n".join(log_parts), run_text, exe_path, time.perf_counter() - start
    if run_text != case.expected_output:
        mismatch = (
            "<output-mismatch>\n"
            f"<expected>\n{case.expected_output}</expected>\n"
            f"<actual>\n{run_text}</actual>\n"
        )
        return False, mismatch, run_text, exe_path, time.perf_counter() - start
    return True, "\n".join(log_parts), run_text, exe_path, time.perf_counter() - start


def run_case(compiler: Path, case: GeneratedCase, case_dir: Path) -> CaseResult:
    total_bytes, total_lines = source_metrics(case.files)
    try:
        if case.mode == "native-run":
            ok, build_log, run_text, artifact, duration_sec = compile_native(compiler, case, case_dir)
        elif case.mode == "vm-run":
            ok, build_log, run_text, artifact, duration_sec = compile_vm(compiler, case, case_dir)
        elif case.mode == "pipeline-run":
            ok, build_log, run_text, artifact, duration_sec = compile_pipeline(compiler, case, case_dir)
        else:
            raise RuntimeError(f"unsupported mode: {case.mode}")
        error = "" if ok else build_log
    except subprocess.TimeoutExpired as exc:
        ok = False
        duration_sec = float(case.timeout_sec)
        artifact = case_dir / "<timeout>"
        run_text = ""
        error = f"timeout after {case.timeout_sec}s while running: {' '.join(exc.cmd)}"
    artifact_bytes = artifact.stat().st_size if artifact.exists() else 0
    return CaseResult(
        name=case.name,
        mode=case.mode,
        ok=ok,
        description=case.description,
        duration_sec=duration_sec,
        source_files=len(case.files),
        total_source_bytes=total_bytes,
        total_source_lines=total_lines,
        artifact=str(artifact),
        artifact_bytes=artifact_bytes,
        output=run_text,
        error=error,
        case_dir=str(case_dir),
    )


def build_cases(profile: StressProfile, generated_root: Path, selected: set[str] | None) -> list[GeneratedCase]:
    cases: list[GeneratedCase] = []
    for builder in CASE_BUILDERS:
        probe_dir = generated_root / builder.__name__
        ensure_clean_dir(probe_dir, keep=False)
        case = builder(probe_dir, profile)
        if case.work_dir is None:
            case.work_dir = probe_dir
        if selected and case.name not in selected:
            continue
        cases.append(case)
    return cases


def case_names() -> list[str]:
    names: list[str] = []
    temp_root = DEFAULT_OUT / "_case_probe"
    profile = PROFILES["quick"]
    for builder in CASE_BUILDERS:
        case = builder(temp_root / builder.__name__, profile)
        names.append(case.name)
    if temp_root.exists():
        shutil.rmtree(temp_root)
    return names


def run_suite(compiler: Path, out_dir: Path, profile: StressProfile, selected: set[str] | None, keep_generated: bool) -> int:
    ensure_clean_dir(out_dir, keep=keep_generated)
    generated_root = out_dir / "generated"
    logs_root = out_dir / "logs"
    ensure_clean_dir(generated_root, keep=keep_generated)
    ensure_clean_dir(logs_root, keep=keep_generated)

    cases = build_cases(profile, generated_root, selected)
    results: list[CaseResult] = []
    started = time.perf_counter()
    for index, case in enumerate(cases, start=1):
        case_dir = case.work_dir if case.work_dir else (generated_root / case.name)
        print(f"[RUN] {index}/{len(cases)} {case.name} :: {case.description}")
        result = run_case(compiler, case, case_dir)
        log_parts = [part for part in [result.error, result.output] if part]
        log_text = "\n".join(log_parts)
        if log_text and not log_text.endswith("\n"):
            log_text += "\n"
        (logs_root / f"{case.name}.log").write_text(log_text, encoding="utf-8", newline="\n")
        status = "OK" if result.ok else "FAIL"
        print(
            f"[{status}] {case.name} "
            f"(files={result.source_files}, lines={result.total_source_lines}, "
            f"bytes={result.total_source_bytes}, artifact={result.artifact_bytes}, "
            f"time={result.duration_sec:.2f}s)"
        )
        results.append(result)

    total_duration = time.perf_counter() - started
    summary = {
        "compiler": str(compiler),
        "compiler_version": compiler_version_text(compiler),
        "profile": profile.name,
        "total_cases": len(results),
        "passed": sum(1 for result in results if result.ok),
        "failed": sum(1 for result in results if not result.ok),
        "total_duration_sec": total_duration,
        "results": [result.__dict__ for result in results],
    }
    summary_path = out_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    print(f"[INFO] summary: {summary_path}")
    return 0 if summary["failed"] == 0 else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Kinal compiler stress suite")
    parser.add_argument("--compiler", required=True, help="path to kinal compiler executable")
    parser.add_argument("--out-dir", default=str(DEFAULT_OUT), help="directory for generated sources, artifacts, and logs")
    parser.add_argument("--profile", choices=sorted(PROFILES.keys()), default="full", help="stress profile size")
    parser.add_argument("--case", action="append", default=[], help="run only the named stress case (repeatable)")
    parser.add_argument("--keep-generated", action="store_true", help="preserve existing out-dir contents instead of wiping first")
    parser.add_argument("--list-cases", action="store_true", help="list case names and exit")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.list_cases:
        for name in case_names():
            print(name)
        return 0

    compiler = Path(args.compiler).resolve()
    if not compiler.exists():
        raise SystemExit(f"compiler not found: {compiler}")
    selected = set(args.case) if args.case else None
    return run_suite(
        compiler,
        Path(args.out_dir).resolve(),
        PROFILES[args.profile],
        selected,
        keep_generated=args.keep_generated,
    )


if __name__ == "__main__":
    raise SystemExit(main())
