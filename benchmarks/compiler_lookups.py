"""Measure Native and VM compilation of record types, reachable functions and global initializers."""
import argparse
import csv
from pathlib import Path
import random
import statistics
import subprocess
import sys

from benchmark_process import sha
from heap_profile import measure


def source_text(kind, size):
    lines = ["Unit CompilerLookupBench;"]
    if kind == "types":
        for i in range(size):
            lines.extend([f"Class R{i} {{ Public int X; }}",
                          f"Function int Read{i}(R{i} x) {{ Return x.X; }}"])
        last = size - 1
        lines.append(f"Static Function int Main() {{ R{last} x = New R{last}(); "
                     f"x.X=7; Return Read{last}(x) == 7 ? 0 : 1; }}")
        return "\n".join(lines) + "\n"

    globals_case = kind == "globals"
    for i in range(size):
        lines.append(f"int G{i} = {i % 31};" if globals_case else
                     f"Function int F{i}(int x) {{ Return x + {i % 31}; }}")
    # Small independent functions keep individual frames bounded on the VM.
    groups = []
    expected = 0
    for offset in range(0, size, 32):
        group = f"Batch{offset // 32}"
        groups.append(group)
        lines.append(f"Function int {group}() {{ int sum=0;")
        for i in range(offset, min(size, offset + 32)):
            operand = f"G{i}" if globals_case else f"F{i}(1)"
            lines.append(f"sum=sum+{operand};")
            expected += i % 31 + (0 if globals_case else 1)
        lines.append("Return sum; }")
    lines.append("Static Function int Main() { int total=0;")
    lines.extend(f"total=total+{group}();" for group in groups)
    lines.extend([f"Return total == {expected} ? 0 : 1;", "}"])
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", action="append", required=True, metavar="LABEL=COMPILER")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=5)
    parser.add_argument("--timeout", type=int, default=120, help="Per-build time limit in seconds")
    args = parser.parse_args()
    if sys.platform != "win32" or min(args.samples, args.timeout) < 1:
        parser.error("requires Windows and a positive sample count")
    tools = {}
    for spec in args.sdk:
        label, sep, path = spec.partition("=")
        if not sep or not label or label in tools:
            parser.error("each SDK needs a unique label")
        tools[label] = Path(path).resolve(strict=True)
    identities = {label: sha(tool) for label, tool in tools.items()}
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    rows = []
    cases = [("types", 2500, "obj"), ("calls", 512, "vm"), ("calls", 4000, "vm"),
             ("globals", 512, "obj"), ("globals", 4000, "obj"), ("globals", 4000, "vm")]
    for kind, size, mode in cases:
        name = f"{kind}-{size}"
        source = out / (name + ".kn")
        source.write_text(source_text(kind, size), encoding="ascii")
        source_hash = sha(source)
        artifact = out / (name + (".knc" if mode == "vm" else ".obj"))
        expected_hash = None
        timed_out = set()
        orders = list(tools)
        random.Random(531 + size).shuffle(orders)
        for trial in range(args.samples + 1):
            pos = trial % len(orders)
            for label in orders[pos:] + orders[:pos]:
                if label in timed_out:
                    continue
                compiler = tools[label]
                artifact.unlink(missing_ok=True)
                command = [compiler, *(["vm"] if mode == "vm" else []), "build", source,
                           "--no-module-discovery", *([] if mode == "vm" else ["--emit", "obj"])]
                sample = dict(case=name, mode=mode, compiler=label, sample=trial,
                              source_sha256=source_hash, compiler_sha256=identities[label],
                              timeout_seconds=args.timeout)
                try:
                    _, metrics = measure([*command, "-o", artifact], include_stderr=True, timeout=args.timeout)
                except subprocess.TimeoutExpired:
                    timed_out.add(label)
                    rows.append(dict(**sample, status="timeout", artifact_sha256="", wall_ms="",
                                     peak_working_set_bytes="", peak_commit_bytes=""))
                    print(f"{name} {mode} {label} exceeded {args.timeout} s", flush=True)
                    continue
                checksum = sha(artifact)
                if expected_hash is not None and checksum != expected_hash:
                    raise ValueError(f"{name}/{mode}/{label}: output differs from first SDK")
                expected_hash = checksum
                if trial == 0:
                    if mode == "vm":
                        measure([compiler.with_name("kinalvm.exe"), artifact])
                    else:
                        exe = artifact.with_suffix(".exe")
                        measure([compiler, "build", source, "--no-module-discovery", "-o", exe])
                        measure([exe])
                else:
                    rows.append(dict(**sample, status="ok", artifact_sha256=checksum, **metrics))
        if len(timed_out) == len(tools):
            raise RuntimeError(f"{name}/{mode} exceeded the limit for every SDK")
        for label in tools:
            if label in timed_out:
                continue
            times = [row["wall_ms"] for row in rows if (row["case"], row["mode"], row["compiler"]) ==
                     (name, mode, label)]
            print(f"{name} {mode} {label}: {statistics.median(times):.2f} ms", flush=True)
    with (out / "samples.csv").open("w", encoding="utf8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


if __name__ == "__main__":
    main()
