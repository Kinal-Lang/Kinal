"""Compare KinalVM runners on identical bytecode, excluding compilation and process startup."""
import argparse
import csv
import math
import os
from pathlib import Path
import random
import statistics
import subprocess
import tempfile

from benchmark_process import sha
from suite import CATALOG, ROOT, worker


def call_checksum(case, count, reps, seed):
    total = 0
    for rep in range(reps):
        value = seed + rep % 7
        if case == "mutual":
            for step in range(count, 0, -1):
                value = (33 * value + step + ((count - step) % 2)) & 65535
        else:
            for step in range(count):
                value = (33 * value + step) & 65535
                if case == "chain":
                    value = (33 * value + step + 1) & 65535
        total += value
    return total


def workloads():
    for index, (case, count, reps) in enumerate([("chain", 4500, 2), ("mutual", 32, 380), ("checked", 9000, 2)]):
        yield "calls", index, case, count, reps, [call_checksum(case, count, reps, seed) for seed in range(7, 14)]
    for index, case in enumerate(CATALOG):
        if case["name"] in ("lcg", "branch", "float_series", "sliding_max"):
            count = min(map(int, case["expected"]))
            yield "bench", index, case["name"], count, 1, case["expected"][str(count)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--runner", action="append", required=True, metavar="LABEL=PATH")
    parser.add_argument("--repeat", type=int, default=7)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.repeat < 1:
        parser.error("--repeat must be positive")
    compiler = args.compiler.resolve(strict=True)
    compiler_hash = sha(compiler)
    runners = {}
    for argument in args.runner:
        label, separator, path = argument.partition("=")
        if not label or not separator or not path or label in runners:
            parser.error("each --runner needs a unique label and an executable path")
        runner = Path(path).resolve(strict=True)
        runners[label] = runner, sha(runner)

    flags = subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS if os.name == "nt" else 0
    rows = []
    with tempfile.TemporaryDirectory() as folder:
        directory = Path(folder)
        artifacts = {}
        for source in ("calls", "bench"):
            knc = directory / (source + ".knc")
            subprocess.run([compiler, "vm", "build", "--no-module-discovery", ROOT / (source + ".kn"), "-o", knc],
                           cwd=directory, check=True, timeout=120, creationflags=flags)
            artifacts[source] = knc, sha(knc)
        cases = [(label, case) for case in workloads() for label in runners]
        rng = random.Random(2399)
        for sample in range(args.repeat + 1):
            seed = 7 + max(sample - 1, 0) % 7
            rng.shuffle(cases)
            for label, (source, index, name, count, reps, answers) in cases:
                runner, runner_hash = runners[label]
                knc, knc_hash = artifacts[source]
                variant = dict(run=[str(runner), str(knc)], cwd=directory, creationflags=flags)
                with worker(variant, index, count) as child:
                    result = child.batch(reps, seed)
                want = answers[seed - 7]
                matches = (math.isclose(result["value"], want, rel_tol=2e-6, abs_tol=1e-6)
                           if name == "float_series" else int(result["output"]) == want)
                if not matches:
                    raise ValueError(f"{label} / {name}, seed {seed}: expected {want}, got {result['output']}")
                if sample:
                    rows.append(dict(runner=label, case=name, size=count, reps=reps, seed=seed, sample=sample,
                                     output=result["output"], wall_ms=result["wall_ms"],
                                     peak_commit_bytes=result["peak_commit_bytes"], compiler_sha256=compiler_hash,
                                     knc_sha256=knc_hash, runner_sha256=runner_hash))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=rows[0], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    for label, name in sorted({(row["runner"], row["case"]) for row in rows}):
        times = [row["wall_ms"] for row in rows if (row["runner"], row["case"]) == (label, name)]
        print(f"{label:12} {name:13} {statistics.median(times):9.3f} ms")


if __name__ == "__main__":
    main()
