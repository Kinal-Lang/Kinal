"""Compare Windows Native and KinalVM JSON formatting time and process memory."""

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
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=5)
    args = parser.parse_args()
    if args.samples < 1:
        parser.error("--samples must be positive")
    if len({sdk.partition("=")[0] for sdk in args.sdk}) != len(args.sdk):
        parser.error("SDK labels must be unique")

    source = Path(__file__).with_name("json_format.kn")
    workloads = [("plain", 800000, "compact"), ("escaped", 12000, "compact"),
                 ("array", 8192, "compact"), ("objects", 2048, "compact"),
                 ("objects", 2048, "pretty"), ("floats", 8192, "compact")]
    rows = []
    with tempfile.TemporaryDirectory() as folder:
        cases = []
        for index, spec in enumerate(args.sdk):
            label, separator, path = spec.partition("=")
            if not label or not separator or not path:
                parser.error("use --sdk LABEL=COMPILER")
            cc = Path(path).resolve(strict=True)
            package = cc.parent / "stdpkg/IO.Json/1.0.0/lib/IO.Json.klib"
            for backend in ["native", "vm"]:
                vm = backend == "vm"
                output = Path(folder) / (f"{index}.knc" if vm else f"{index}.exe")
                measure([cc, *(["vm"] if vm else []), "build", "--no-module-discovery", source, "-o", output])
                runner = cc.with_name("kinalvm.exe") if vm else output
                record = dict(sdk=label, backend=backend, compiler_sha256=sha(cc),
                              json_package_sha256=sha(package), runner_sha256=sha(runner))
                command = [runner, output] if vm else [runner]
                for mode, count, layout in workloads:
                    cases.append((dict(**record, workload=mode, count=count, layout=layout),
                                  [*command, str(count), mode, layout]))
        rng = random.Random(9064)
        for sample in range(args.samples + 1):
            rng.shuffle(cases)
            for record, command in cases:
                result, counters = measure(command)
                elapsed = int(result)
                if elapsed < 0:
                    raise ValueError(result)
                if sample:
                    rows.append(dict(**record, sample=sample, format_ns=elapsed, **counters))
    with args.output.open("w", encoding="utf8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=rows[0])
        writer.writeheader()
        writer.writerows(rows)


if __name__ == "__main__":
    main()
