"""Compare string appends and IO.Text.Builder across Windows Native SDKs."""
import argparse
import csv
from pathlib import Path
import random
import tempfile

from benchmark_process import sha
from heap_profile import measure


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", action="append", required=True, metavar="LABEL=COMPILER")
    parser.add_argument("--sizes", nargs="+", type=int, default=[500, 5000])
    parser.add_argument("--repeat", type=int, default=7)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.repeat < 1 or not all(1 <= size <= 1000000 for size in args.sizes):
        parser.error("use positive repeats and sizes in 1..1000000")
    labels = set()
    cases = []
    rows = []
    source = Path(__file__).with_name("text_builder.kn").resolve()
    rng = random.Random(6281)
    with tempfile.TemporaryDirectory() as folder:
        for index, spec in enumerate(args.sdk):
            label, sep, path = spec.partition("=")
            if not label or not sep or not path or label in labels:
                parser.error("each --sdk needs a unique label and an executable path")
            labels.add(label)
            compiler = Path(path).resolve(strict=True)
            runtime = compiler.parent / "stdpkg/IO.Kinal.Runtime/1.0.0/lib/IO.Kinal.Runtime.klib"
            for lto in ("off", "full"):
                exe = Path(folder) / f"builder-{index}-{lto}.exe"
                measure([compiler, "build", "--no-module-discovery", source, "--lto=" + lto, "-o", exe])
                build = dict(sdk=label, lto=lto, compiler_sha256=sha(compiler),
                             runtime_sha256=sha(runtime), exe_sha256=sha(exe), source_sha256=sha(source))
                for size in args.sizes:
                    for mode in ("concat", "builder", "reserved"):
                        cases.append((exe, dict(**build, mode=mode, size=size)))
        for trial in range(args.repeat + 1):
            rng.shuffle(cases)
            for exe, case in cases:
                output, counters = measure([exe, str(case["size"]), case["mode"]])
                elapsed = int(output)
                if elapsed < 0:
                    raise ValueError(f"invalid duration {elapsed} for {case}")
                if trial:
                    rows.append(dict(**case, trial=trial, elapsed_ns=elapsed, **counters))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


if __name__ == "__main__":
    main()
