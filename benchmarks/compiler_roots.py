"""Measure compiler scaling for argument roots or lexical bindings (Windows x64)."""
import argparse
import csv
import os
from pathlib import Path
import random
import re
import tempfile

from benchmark_process import sha
from heap_profile import measure


def source(count):
    lines = ["Unit Bench.NativeRoots;", "Get IO.Console;",
             "Class Box { Public int Value; Public Constructor(int n) { Value = n; } }",
             "Function int Read(Box a, Box b) { Return a.Value + b.Value; }",
             "Static Function int Main() {", "    int sum = 0;"]
    lines.extend(f"    sum += Read(New Box({i % 31}), New Box({i % 17}));" for i in range(count))
    lines.extend(["    IO.Console.PrintLine(sum);", "    Return 0;", "}"])
    return "\n".join(lines) + "\n"


def local_source(count):
    lines = ["Unit Bench.NativeLocals;", "Get IO.Console;", "Get IO.Text;",
             "Function int Work(string part)", "{", "    int sum = 0;"]
    lines.extend(f"    string slot{i} = part;" for i in range(count))
    lines.extend(f"    sum += IO.Text.Length(slot{i});" for i in range(count))
    lines.extend(["    Return sum;", "}",
                  'Static Function int Main() { IO.Console.PrintLine(Work("xy")); Return 0; }'])
    return "\n".join(lines) + "\n"


def benchmark(args, tools, folder):
    rows = []
    rng = random.Random(193)
    count_label = "locals" if args.workload == "locals" else "calls"
    for count in args.sizes:
        src = folder / f"roots-{count}.kn"
        src.write_text(local_source(count) if args.workload == "locals" else source(count), encoding="ascii")
        digest = sha(src)
        expected = str(2 * count if args.workload == "locals" else sum(i % 31 + i % 17 for i in range(count)))
        cases = [(key, mode) for key in tools for mode in ("check", "ir")]
        artifacts = {}
        for key, (compiler, _) in tools.items():
            exe = folder / "roots.exe"
            measure([compiler, "build", "--no-module-discovery", src, "-o", exe])
            output, _ = measure([exe])
            if output != expected:
                raise ValueError(f"{key}, {count} {count_label}: expected {expected}, got {output!r}")
        for trial in range(args.repeat + 1):
            rng.shuffle(cases)
            for key, mode in cases:
                compiler, compiler_hash = tools[key]
                artifact = folder / ("roots." + mode)
                artifact.unlink(missing_ok=True)
                profile, counters = measure([compiler, "build", "--no-module-discovery", src, "--emit", mode,
                                             "--time", "-o", artifact], include_stderr=True)
                phases = dict(re.findall(r"\[Profile\] (\w+):\s*(\d+) ms", profile))
                if not {"parse", "sema", "total"} <= phases.keys() or mode == "ir" and "codegen" not in phases:
                    raise ValueError(f"missing compiler timings: {profile}")
                artifact_hash = sha(artifact)
                identity = mode if args.identical_artifacts else (key, mode)
                if artifact_hash != artifacts.setdefault(identity, artifact_hash):
                    raise ValueError(f"{key}, {count} {count_label}: {mode} differs between builds")
                if trial:
                    row = {count_label: count}
                    row.update(compiler=key, emit=mode, trial=trial, **counters,
                               parse_ms=phases["parse"], sema_ms=phases["sema"],
                               codegen_ms=phases.get("codegen", ""), total_ms=phases["total"],
                               checksum=expected, source_sha256=digest, artifact_sha256=artifact_hash,
                               compiler_sha256=compiler_hash)
                    rows.append(row)
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", action="append", required=True, metavar="LABEL=PATH")
    parser.add_argument("--sizes", nargs="+", type=int, default=[256, 1024, 4096])
    parser.add_argument("--repeat", type=int, default=7)
    parser.add_argument("--workload", choices=("calls", "locals"), default="calls",
                        help="repeated call arguments or a function with a wide lexical scope")
    parser.add_argument("--identical-artifacts", action="store_true",
                        help="also require check output and IR to match across compiler versions")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "nt" or args.repeat < 1 or min(args.sizes) < 1:
        parser.error("requires Windows and positive sample and workload sizes")
    tools = {}
    for spec in args.compiler:
        label, sep, path = spec.partition("=")
        if not sep or not label or not path or label in tools:
            parser.error("each compiler needs a unique label and an executable path")
        compiler = Path(path).resolve(strict=True)
        tools[label] = compiler, sha(compiler)
    with tempfile.TemporaryDirectory() as temp:
        rows = benchmark(args, tools, Path(temp))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


if __name__ == "__main__":
    main()
