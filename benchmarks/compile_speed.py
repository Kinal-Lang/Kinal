"""Compare Windows x64 compiler builds on identical inputs and record each observation."""
import argparse
import csv
from datetime import datetime
import json
import mmap
import os
from pathlib import Path
import random
from statistics import median
import subprocess
import sys

from benchmark_process import capture, sha
from suite import host_name, save_json

BENCH = Path(__file__).resolve().parent
REPO = BENCH.parent
FRONTEND_CASES = {"declarations-13000": 13000, "call-chain-6500": 6500,
                  "named-types-2400": 2400, "property-access-120": 120}
ALIAS_PROJECT = "module-aliases-64"
ALIAS_CALLS = 1800
PROPERTY_UPDATES = 16
LEXER_CASE = "lexer-spans-1200"
LEXER_FUNCTIONS = 1200


def lexer_source():
    lines = ["Unit Bench.LexerSpans;"]
    for i in range(LEXER_FUNCTIONS):
        lines.extend([f"/* source row {i}:", "  注释 " * 12 + "*/",
                      f"Function string ReadIdentifier_{i}() {{",
                      r'    Return "a long literal with \"quotes\"; closing punctuation )} and UTF-8 字符";',
                      "} // " + "trailing comment " * 5])
    return "\r\n".join(lines) + "\r\n"


def frontend_source(case, count):
    lines = ["Unit Bench.Frontend;"]
    if case == "property-access-120":
        for i in range(count):
            lines.append(f"Class B{i} {{ Public int Stored; Public Property int Value {{ "
                         "Get { Return Stored; } Set { Stored=value; } } }")
            lines.append(f"Function int Work{i}(B{i} item) {{")
            lines.extend(["    item.Value = item.Value + item.Stored + 1;"] * PROPERTY_UPDATES)
            lines.append("    Return item.Value; }")
        expected = (1 << PROPERTY_UPDATES) - 1
        lines.append(f"Static Function int Main() {{ Return Work{count - 1}(New B{count - 1}()) == {expected} ? 0 : 1; }}")
        return "\n".join(lines) + "\n"
    if case == "named-types-2400":
        lines.extend(f"Class T{i} {{ Public int Value; }}" for i in range(count))
        lines.extend(f"Function T{i} F{i}(T{i} x) {{ Return x; }}" for i in range(count))
        lines.append(f"Static Function int Main() {{ F{count - 1}(New T{count - 1}()); Return 0; }}")
        return "\n".join(lines) + "\n"
    for i in range(count):
        expr = f"F{i - 1}(x) + 1" if case == "call-chain-6500" and i else "x + 1"
        lines.append(f"Function int F{i}(int x) {{ Return {expr}; }}")
    lines.append(f"Static Function int Main() {{ Return F{count - 1}(0); }}")
    return "\n".join(lines) + "\n"


def write_alias_project(directory):
    directory.mkdir()
    (directory / "kinal.knproj").write_text(
        'Project AliasBench { DefaultProfile="test"; SourceSet "s" { Files=["Entry.kn","Library.kn"]; } '
        'Profile "test" { Source { Entry="Entry.kn"; Sets=["s"]; Mode=AllSources; } } }\n',
        encoding="ascii")
    (directory / "Library.kn").write_text(
        "Unit Bench.Api; Function int Plus(int a, int b) { Return a+b; }\n", encoding="ascii")
    lines = ["Unit Bench.Main;"]
    lines.extend(f"Get N{i} By Bench.Api;" for i in range(64))
    lines.append("Function int Work(int v) {")
    lines.extend(f"    v=N{(i * 37) % 64}.Plus(v,1);" for i in range(ALIAS_CALLS))
    lines.extend(["    Return v;", "}", f"Static Function int Main() {{ Return Work(0)=={ALIAS_CALLS} ? 0 : 1; }}"])
    (directory / "Entry.kn").write_text("\n".join(lines) + "\n", encoding="ascii")


def scale_source():
    lines = ["Unit Bench.Scale;", "Get Console By IO.Console;"]
    for i in range(1000):
        lines.append(f"Static Function int F{i}(int x) {{ int a=x+{i}; "
                     "If(a%3==0) { a=a*2; } Return a%65521; }")
    lines.extend(["Static Function int Main(string[] args) {", " int x=[int](args[0]);"])
    lines.extend(f"    x=F{i}(x);" for i in range(1000))
    lines.append(" Console.PrintLine(x); Return 0; }")
    return ("\r\n".join(lines) + "\r\n").encode("ascii")


def scale_result(seed):
    value = seed
    for offset in range(1000):
        value += offset
        if value % 3 == 0:
            value *= 2
        value %= 65521
    return str(value)


def prepare(case, mode, trial, out, project):
    if case == "parser-tokens":
        src = REPO / "apps/kinal-selfhost/src/IO/Kinal/Compiler/Syntax/Parser.kn"
        return src, [src, "--no-module-discovery", "--emit", "tokens", "--time"], None
    if case == LEXER_CASE:
        src = out / (case + ".kn")
        return src, [src, "--no-module-discovery", "--emit", "ast", "--time"], LEXER_FUNCTIONS
    if case == "compiler-ast":
        src = REPO / "apps/kinal-selfhost/kinal.knproj"
        return src, ["--project", src, "--profile", "stage1", "--emit", "ast", "--time"], None
    if case in FRONTEND_CASES:
        src = out / f"{case}.kn"
        return src, [src, "--no-module-discovery", "--emit", "check", "--time"], None
    if mode == "check":
        if case == ALIAS_PROJECT:
            src = out / case / "kinal.knproj"
            return src, ["--project", src, "--emit", "check", "--time"], None
        compiler = case == "compiler-check"
        src = REPO / "apps/kinal-selfhost/kinal.knproj" if compiler else project
        profile = ["--profile", "stage1"] if compiler else []
        return src, ["--project", src, *profile, "--emit", "check", "--time"], None
    if case in ("compiler-ir", "compiler-object"):
        src = REPO / "apps/kinal-selfhost/kinal.knproj"
        emit = "ir" if case == "compiler-ir" else "obj"
        return src, ["--project", src, "--profile", "stage1", "--emit", emit], None
    if case == "project":
        return project, ["--project", project, "--lto=full"], None
    if case == "bench":
        src = out / "bench.kn"
        template = (BENCH / "bench.kn").read_bytes()
        old = b'Console.PrintLine("42")'
        if template.count(old) != 1:
            raise ValueError("Expected exactly one startup sentinel in bench.kn")
        expected = str(42 + trial)
        src.write_bytes(template.replace(old, f'Console.PrintLine("{expected}")'.encode()))
    elif case == "hello":
        src, expected = REPO / "tests/common/hello.kn", "hello"
    else:
        src, expected = out / "scale1000.kn", scale_result(7)
    return src, [src, "--no-module-discovery", "--color", "never", "--lto=" + mode], expected


def verify_output(case, artifact, expected):
    if case == "compiler-ir":
        with artifact.open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as ir:
            if ir.find(b"target triple = ", 0, 4096) < 0 or ir.find(b"Tokenize") < 0 or ir.find(b"@main(") < 0:
                raise ValueError("Missing target, lexer, or entry point in compiler IR")
        return
    if artifact.suffix == ".ktokens":
        lines = artifact.read_text("utf8").splitlines()
        if lines[:3] != ["format=kinal-tokens-v1", "sources=1", "source=0"] or len(lines) < 5:
            raise ValueError("Incomplete compiler token dump")
        if not lines[3].endswith("\t556E6974") or not lines[-1].startswith("0\t"):
            raise ValueError("Missing Unit or EOF token")
        return
    if artifact.suffix == ".kast":
        fields = dict(line.split("=", 1) for line in artifact.read_text("utf8").splitlines())
        if fields["format"] != "kinal-syntax-v1":
            raise ValueError("Unexpected AST summary")
        if case == LEXER_CASE:
            if (fields["unit"], int(fields["sources"]), int(fields["functions"])) != ("Bench.LexerSpans", 1, expected):
                raise ValueError("Incomplete lexer workload")
        elif int(fields["sources"]) < 2 or int(fields["functions"]) < 1:
            raise ValueError("Incomplete compiler project AST")
        return
    if artifact.suffix == ".kcheck":
        fields = dict(line.split("=", 1) for line in artifact.read_text("utf8").splitlines())
        if case == ALIAS_PROJECT and int(fields["imports"]) < 64:
            raise ValueError("Incomplete module alias imports")
        if case in FRONTEND_CASES and int(fields["functions"]) < FRONTEND_CASES[case] + 1:
            raise ValueError("Incomplete function table")
        if case in ("named-types-2400", "property-access-120"):
            type_count = fields["classes"] if fields["format"] == "kinal-sema-v1" else fields["types"]
            if int(type_count) < FRONTEND_CASES[case]:
                raise ValueError("Incomplete named type table")
        if fields["format"] == "kinal-sema-v1":
            if fields["hir_unresolved_calls"] != "0":
                raise ValueError("Unexpected C frontend call resolution")
            if case == ALIAS_PROJECT and int(fields["hir_function_calls"]) != ALIAS_CALLS + 1:
                raise ValueError("Incomplete module alias calls")
            if case in FRONTEND_CASES:
                calls = FRONTEND_CASES[case] if case == "call-chain-6500" else 1
                if int(fields["hir_function_calls"]) != calls:
                    raise ValueError("Incomplete call graph")
            if case == "property-access-120":
                accessor_calls = FRONTEND_CASES[case] * (2 * PROPERTY_UPDATES + 1)
                if int(fields["hir_direct_method_calls"]) != accessor_calls:
                    raise ValueError("Missing property accessor calls")
        elif fields["format"] != "kinal-selfhost-sema-v1" or fields["unresolved_expressions"] != "0":
            raise ValueError("Unexpected selfhost frontend result")
        return
    with artifact.open("rb") as stream:
        header = stream.read(2)
    if header != (b"\x64\x86" if case == "compiler-object" else b"MZ"):
        raise ValueError(f"Invalid artifact header in {artifact}")
    if expected is not None:
        args = [artifact, *{"hello": [], "bench": ["startup"], "scale1000": ["7"]}[case]]
        actual = subprocess.check_output(args, cwd=REPO, timeout=10).decode().strip()
        if actual != expected:
            raise ValueError(f"{artifact} printed {actual!r}, expected {expected!r}")


def project_inventory(manifest, out):
    root = manifest.parent
    files = [manifest]
    for directory, subdirs, names in os.walk(root):
        folder = Path(directory)
        subdirs[:] = [name for name in subdirs if name != ".git" and folder / name != out]
        files.extend(folder / name for name in names if name.lower().endswith(".kn"))
    return {p.relative_to(root).as_posix(): sha(p) for p in sorted(files)}


def measure(matrix, tools, out, project, warmups, rounds, projects, rows):
    failed = set()
    for trial in range(warmups + rounds):
        cases = list(matrix)
        if not projects:
            random.Random(6394 + trial).shuffle(cases)
        for case, mode in cases:
            source, opts, expected = prepare(case, mode, trial, out, project)
            source_hash = sha(source)
            variants = list(tools)
            seed = 337 + trial if projects else 4981 + trial * 177 + matrix.index((case, mode))
            random.Random(seed).shuffle(variants)
            for variant in variants:
                if (case, mode, variant) in failed:
                    continue
                suffix = {"tokens": ".ktokens", "ast": ".kast", "check": ".kcheck", "ir": ".ll"}.get(mode, ".exe")
                if case == "compiler-object":
                    suffix = ".obj"
                artifact = out / f"{case}-{mode}-{variant}{suffix}"
                artifact.unlink(missing_ok=True)
                args = [tools[variant], "build", *opts]
                if projects:
                    args.extend(["--color", "never"])
                result = capture([*args, "-o", artifact], timeout=360, env=os.environ, cwd=REPO)
                row = dict(case=case, mode=mode, compiler=variant, trial=trial,
                           warmup=trial < warmups, source_sha256=source_hash,
                           expected_output=expected, **result)
                if row["status"] == "ok":
                    try:
                        verify_output(case, artifact, expected)
                    except (KeyError, ValueError, OSError, subprocess.SubprocessError) as exc:
                        row.update(status="invalid", detail=str(exc))
                rows.append(row)
                with (out / "samples.jsonl").open("a", encoding="utf-8") as stream:
                    stream.write(json.dumps(row) + "\n")
                if row["status"] != "ok":
                    failed.add((case, mode, variant))
                    print(f"{case}/{mode}/{variant}: {row.get('detail') or row['stderr']}", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage0", type=Path, default=os.getenv("KINAL_STAGE0", REPO / "out/selfhost/stage0-host/kinal.exe"))
    parser.add_argument("--previous", type=Path, help="An earlier stage3 in its complete toolchain directory")
    parser.add_argument("--stage3", type=Path, default=os.getenv("KINAL_STAGE3", REPO / "out/selfhost/stage3/kinal.exe"))
    parser.add_argument("--projects", action="store_true", help="Include the whole compiler project")
    parser.add_argument("--project", type=Path, help="Include this project in the selected measurements")
    parser.add_argument("--frontend-only", action="store_true",
                        help="Measure tokenization and parsing plus semantic checks on declarations, calls, types and aliases")
    parser.add_argument("--only", metavar="WORKLOADS", help="Run these comma-separated workloads from the selected matrix")
    parser.add_argument("--rounds", type=int, default=9)
    parser.add_argument("--project-rounds", type=int, default=3)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    if sys.platform != "win32" or min(args.rounds, args.project_rounds) < 1:
        parser.error("Run on Windows x64 with positive round counts")
    tools = {"seed": args.stage0, "previous": args.previous, "new": args.stage3}
    tools = {k: v.resolve(strict=True) for k, v in tools.items() if v is not None}
    project = args.project.resolve(strict=True) if args.project else None
    out = (args.out or BENCH / "results/runs" / datetime.now().strftime("compile-%Y%m%d-%H%M%S")).resolve()
    out.mkdir(parents=True, exist_ok=False)
    save_json(out / "manifest.json", dict(host=host_name(), argv=sys.argv,
              compilers={k: dict(path=str(v), sha256=sha(v)) for k, v in tools.items()}))
    if args.frontend_only:
        workloads = [(name, "check") for name in FRONTEND_CASES]
        (out / (LEXER_CASE + ".kn")).write_bytes(lexer_source().encode("utf-8"))
        workloads.append((LEXER_CASE, "ast"))
        for name, count in FRONTEND_CASES.items():
            (out / f"{name}.kn").write_text(frontend_source(name, count), encoding="ascii")
    else:
        (out / "scale1000.kn").write_bytes(scale_source())
        workloads = [(name, mode) for name in ("hello", "bench") for mode in ("off", "full")]
        workloads.append(("scale1000", "off"))
    projects = []
    if args.frontend_only:
        write_alias_project(out / ALIAS_PROJECT)
        projects.append((ALIAS_PROJECT, "check"))
    if project:
        projects.append(("project-check", "check") if args.frontend_only else ("project", "full"))
    if args.projects:
        if args.frontend_only:
            projects.extend([("compiler-check", "check"), ("compiler-ast", "ast")])
            workloads.append(("parser-tokens", "tokens"))
        else:
            projects.extend([("compiler-ir", "ir"), ("compiler-object", "profile-stage1")])
    if args.only is not None:
        selected = {name.strip() for name in args.only.split(",")}
        unknown = selected - {name for name, _ in workloads + projects}
        if unknown:
            parser.error("unavailable workload in this matrix: " + ", ".join(sorted(unknown)))
        workloads = [pair for pair in workloads if pair[0] in selected]
        projects = [pair for pair in projects if pair[0] in selected]
    rows = []
    measure(workloads, tools, out, project, 2, args.rounds, False, rows)
    if projects:
        manifests = {case: prepare(case, mode, 0, out, project)[0] for case, mode in projects}
        snapshots = {k: project_inventory(m, out) for k, m in manifests.items()}
        save_json(out / "project-inputs.json", snapshots)
        measure(projects, tools, out, project, 1, args.project_rounds, True, rows)
        if any(project_inventory(m, out) != snapshots[k] for k, m in manifests.items()):
            raise RuntimeError("Project sources changed during the measurements")
    with (out / "comparison.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["workload", "mode", "compiler", "status", "samples", "median_ms", "min_ms", "max_ms"])
        for case, mode in workloads + projects:
            for variant in tools:
                group = [r for r in rows if (r["case"], r["mode"], r["compiler"]) == (case, mode, variant)]
                valid = all(r["status"] == "ok" for r in group)
                times = [r["wall_ms"] for r in group if not r["warmup"] and r["status"] == "ok"]
                metrics = (median(times), min(times), max(times)) if valid else ("", "", "")
                writer.writerow([case, mode, variant, "ok" if valid else "failed", len(times), *metrics])
    print(out / "comparison.csv")
    return int(any(r["status"] != "ok" for r in rows))


if __name__ == "__main__":
    sys.exit(main())
