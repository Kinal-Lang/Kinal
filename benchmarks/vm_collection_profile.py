"""Compare guest collections, arrays or object handles on identical KNC (Windows)."""
import argparse
import csv
import os
from pathlib import Path
import random
import statistics
import tempfile

from benchmark_process import sha
from heap_profile import measure


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--runner", action="append", required=True, metavar="LABEL=PATH")
    parser.add_argument("--workload", choices=("collections", "array-init", "handles", "packages"), default="collections")
    parser.add_argument("--sizes", nargs="+", type=int, help="element or object counts")
    parser.add_argument("--arrays", type=int, help="8,192-element arrays per array-init sample (default 3)")
    parser.add_argument("--repeat", type=int, default=5)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "nt" or args.repeat < 1:
        parser.error("requires Windows and a positive repeat count")
    if args.workload == "array-init":
        if args.sizes or (args.arrays is not None and not 1 <= args.arrays <= 100000):
            parser.error("array-init uses fixed-size arrays and accepts --arrays 1..100000")
        arrays = args.arrays or 3
        source = "vm_array_init.kn"
        metrics = ("elapsed_ns",)
        jobs = [(dict(case=case, shape=shape, size=8192, arrays=arrays), [case, str(arrays), shape])
                for case in ("float", "bool", "string", "object") for shape in ("sparse", "dense")]
    else:
        sizes = args.sizes or [2048, 8192, 16384]
        if args.arrays is not None or any(n < 1 or n > 1000000 for n in sizes):
            parser.error(f"{args.workload} accepts --sizes in 1..1000000")
        if args.workload == "handles":
            source = "vm_handles.kn"
            metrics = ("build_ns", "mark_ns", "walk_ns", "release_ns")
            jobs = [(dict(case="objects", size=n), [str(n)]) for n in sizes]
        elif args.workload == "packages":
            source = "typed_packages.kn"
            metrics = ("build_ns", "gc_ns", "walk_ns")
            jobs = [(dict(case="typed_packages", size=n), [str(n)]) for n in sizes]
        else:
            source = "vm_collections.kn"
            metrics = ("build_ns", "gc_ns", "lookup_ns")
            jobs = [(dict(case=case, size=n), [case, str(n)]) for case in ("list", "dict") for n in sizes]
    compiler = args.compiler.resolve(strict=True)
    runners = {}
    for spec in args.runner:
        label, sep, path = spec.partition("=")
        if not sep or not label or not path or label in runners:
            parser.error("each --runner needs a unique label and an executable path")
        runner = Path(path).resolve(strict=True)
        runners[label] = runner, sha(runner)

    rows = []
    rng = random.Random(1119)
    with tempfile.TemporaryDirectory() as folder:
        guest = Path(folder) / "collections.knc"
        measure([compiler, "vm", "build", "--no-module-discovery", Path(__file__).with_name(source).resolve(),
                 "-o", guest])
        compiler_hash, guest_hash = sha(compiler), sha(guest)
        cases = [(label, index) for label in runners for index in range(len(jobs))]
        for sample in range(args.repeat + 1):
            rng.shuffle(cases)
            for label, index in cases:
                runner, runner_hash = runners[label]
                metadata, arguments = jobs[index]
                result, counters = measure([runner, guest, *arguments])
                timings = list(map(int, result.splitlines()))
                if len(timings) != len(metrics) or min(timings) < 0:
                    raise ValueError(result)
                if sample:
                    rows.append(dict(runner=label, **metadata, sample=sample, **dict(zip(metrics, timings)),
                                     **counters, compiler_sha256=compiler_hash,
                                     knc_sha256=guest_hash, runner_sha256=runner_hash))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    for label, index in sorted(cases):
        metadata, _ = jobs[index]
        group = [row for row in rows if row["runner"] == label and all(row[k] == v for k, v in metadata.items())]
        times = [f"{key[:-3]} {statistics.median(row[key] for row in group) / 1e6:.3f} ms" for key in metrics]
        print(label, *metadata.values(), *times)


if __name__ == "__main__":
    main()
