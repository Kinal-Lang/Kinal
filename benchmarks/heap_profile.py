"""Measure Windows memory high-water marks of the compiler, generated programs and KinalVM."""
import argparse
import csv
import os
from pathlib import Path
import random
import subprocess
import tempfile
import time

from benchmark_process import sha, usage


def measure(command, data=None, *, include_stderr=False, timeout=120):
    start = time.perf_counter()
    with subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS) as process:
        try:
            out, err = process.communicate(data, timeout=timeout)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate()
            raise
        elapsed = (time.perf_counter() - start) * 1000
        if process.returncode:
            raise RuntimeError((command, process.returncode, (out + err).decode("utf8", "replace")))
        counters = usage(process)
        if not counters["peak_working_set_bytes"] or not counters["peak_commit_bytes"]:
            raise RuntimeError("Windows process memory counters unavailable")
        output = out + err if include_stderr else out
        return output.decode("utf8").strip(), dict(peak_working_set_bytes=counters["peak_working_set_bytes"],
                                                peak_commit_bytes=counters["peak_commit_bytes"], wall_ms=elapsed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", action="append", required=True, metavar="LABEL=COMPILER")
    parser.add_argument("--count", type=int, default=300000)
    parser.add_argument("--vm-count", type=int, default=2048)
    parser.add_argument("--workload", choices=("heap_objects", "transient_arrays", "fixed_array_reentry",
                                               "paired_fixed_arrays", "nested_arrays", "typed_packages", "vm_handles"),
                        default="heap_objects")
    parser.add_argument("--repeat", type=int, default=5)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "nt" or min(args.repeat, args.count, args.vm_count) < 1:
        parser.error("requires Windows and positive counts")
    if args.workload == "vm_handles" and max(args.count, args.vm_count) > 1000000:
        parser.error("vm_handles supports up to 1000000 objects per process")
    source = Path(__file__).with_name(args.workload + ".kn").resolve()
    cases = []
    for argument in args.sdk:
        label, compiler = argument.split("=", 1)
        compiler = Path(compiler).resolve()
        cases.extend((label, compiler, backend, n) for backend, n in
                     [("native", args.count), ("vm", args.vm_count)])
    rows = []
    rng = random.Random(19)
    with tempfile.TemporaryDirectory() as name:
        folder = Path(name)
        for sample in range(args.repeat + 1):
            rng.shuffle(cases)
            for label, compiler, backend, count in cases:
                vm = backend == "vm"
                output = folder / ("heap.knc" if vm else "heap.exe")
                cmd = [compiler] + (["vm"] if vm else []) + ["build", source, "--no-module-discovery", "-o", output]
                _, build = measure(cmd)
                runner = compiler.with_name("kinalvm.exe") if vm else output
                command = [runner, output] if vm else [runner]
                phases = {}
                if args.workload == "vm_handles":
                    # The object fixture validates every node, then collects again after its frame returns.
                    result, run = measure([*command, str(count)])
                    values = list(map(int, result.split()))
                    if len(values) != 4 or min(values) < 0:
                        raise ValueError(result)
                    phases = dict(zip(("build_ns", "mark_ns", "walk_ns", "release_ns"), values))
                    ns = sum(values)
                else:
                    result, run = measure(command, f"{count}\n".encode())
                    observed, ns = map(int, result.split())
                    if observed != count or ns < 0:
                        raise ValueError(result)
                if sample:
                    common = dict(sdk=label, backend=backend, workload=args.workload, count=count, sample=sample,
                                  compiler_sha256=sha(compiler), runner_sha256=sha(runner))
                    rows.append(dict(**common, phase="compile", workload_ns="", **dict.fromkeys(phases, ""), **build))
                    rows.append(dict(**common, phase="run", workload_ns=ns, **phases, **run))
    with args.output.open("w", newline="", encoding="utf8") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0])
        writer.writeheader()
        writer.writerows(rows)


if __name__ == "__main__":
    main()
