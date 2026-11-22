"""Measure Kinal builds, startup, working set and runtime in a fresh result directory."""
import argparse
from contextlib import contextmanager
from datetime import datetime
from decimal import Decimal
import json
import math
import os
from pathlib import Path
import platform
import random
import shutil
import sys
import time

import benchmark_process as lab
import suite_report

ROOT = Path(__file__).resolve().parent
CATALOG = json.loads((ROOT / "cases.json").read_text(encoding="utf-8"))
EXPECTED = {case["name"]: case["expected"] for case in CATALOG}


def host_name():
    import winreg

    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DESCRIPTION\System\CentralProcessor\0") as key:
        cpu = winreg.QueryValueEx(key, "ProcessorNameString")[0].strip()
    return f"{platform.platform()} / {cpu}"


def save_json(path, data):
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


class Journal:
    def __init__(self, directory):
        self.path = directory / "samples.jsonl"

    def write(self, phase, **data):
        with self.path.open("a", encoding="utf-8") as stream:
            stream.write(json.dumps(dict(phase=phase, **data), ensure_ascii=False) + "\n")


@contextmanager
def worker(variant, case_id, size):
    child = lab.Worker(variant, case_id, size)
    try:
        yield child
    finally:
        exit_code = child.close()
        if exit_code and sys.exc_info()[0] is None:
            raise RuntimeError(f"worker exited with {exit_code}")


def checked_batch(child, journal, phase, variant, name, size, reps, seed=7, **indices):
    values = EXPECTED[name][str(size)]
    cycles, extra = divmod(reps, 7)
    if seed == 7:
        want = sum(values) * cycles + sum(values[:extra])
    else:
        assert reps == 1
        want = values[seed - 7]
    observation = child.batch(reps, seed)
    if name in ("matrix_float", "float_series"):
        ok = math.isclose(observation["value"], want, rel_tol=2e-6, abs_tol=1e-6)
    else:
        ok = Decimal(observation["output"]) == want
    journal.write(phase, variant=variant["id"], case=name, n=size, reps=reps,
                  seed=seed, expected=want, ok=ok, **indices, **observation)
    if not ok:
        raise ValueError(f"{name} n={size} seed={seed} reps={reps}: got {observation['output']}, expected {want}")
    return observation


def runtime(variant, case_id, case, journal, settings):
    name, size, *_ = case
    peak_bytes = 0
    for n in (max(2, size // 2), size):
        with worker(variant, case_id, n) as child:
            for seed in range(7, 14):
                batch = checked_batch(child, journal, "validate", variant, name, n, 1, seed)
                if n == size:
                    peak_bytes = max(peak_bytes, batch["peak_working_set_bytes"] or 0)

    reps = 7
    elapsed = []
    for process in range(settings["processes"]):
        with worker(variant, case_id, size) as child:
            started = time.perf_counter()
            warmups = 0
            while warmups < 3 or time.perf_counter() - started < settings["warmup_seconds"]:
                result = checked_batch(child, journal, "warmup", variant, name, size, reps, process=process)
                warmups += 1
                if process == 0:
                    ideal = reps * settings["target_batch_ms"] / max(result["wall_ms"], 0.01)
                    reps = 7 * max(1, min(10000, round(ideal / 7)))
            for sample in range(settings["samples_per_process"]):
                result = checked_batch(child, journal, "runtime", variant, name, size, reps,
                                       process=process, sample=sample)
                elapsed.append(result["wall_ms"] / reps)
    return dict(n=size, reps=reps, ms=elapsed, peak_mib=peak_bytes / 2**20 if peak_bytes else None)


def command(journal, phase, variant, args, *, env=None, timeout=300, **info):
    result = lab.capture(args, timeout=timeout, env=env, cwd=variant["cwd"])
    journal.write(phase, variant=variant["id"], **info, **result)
    if result["status"] != "ok":
        raise RuntimeError((result.get("stderr") or result.get("stdout") or result["status"])[-1500:])
    return result


def build(variant, src, builds, journal, count):
    if not variant["build"]:
        return []
    elapsed = []
    for index in range(count + 1):
        result = command(journal, "build", variant, variant["build"], sample=index, warmup=index == 0)
        if not Path(variant["artifact"]).is_file():
            raise RuntimeError(f"build returned success but {variant['artifact']} is absent")
        if index:
            elapsed.append(result["wall_ms"])
    return elapsed


def benchmark(directory, rows, cases, settings):
    started = datetime.now().astimezone().isoformat(timespec="seconds")
    info = dict(started=started, host=host_name(), settings=settings,
                cases=[c[0] for _, c in cases],
                descriptions={c[0]: c[2:] for _, c in cases},
                case_sizes={c[0]: c[1] for _, c in cases},
                selected_variants=[v["id"] for v in rows], variants=[])
    journal = Journal(directory)
    startup_env = dict(lab.ENV, BENCH_STARTUP="1")
    for variant in rows:
        variant["cwd"] = directory
        name = variant["id"]
        print(f"{name} ...", flush=True)
        row = dict(id=name, language=variant["language"], status="pending", version="",
                   build_command=variant["build"], run_command=variant["run"],
                   build_ms=[], startup_ms=[], runtime={})
        info["variants"].append(row)
        save_json(directory / "summary.json", info)
        try:
            if variant.get("unavailable"):
                row.update(status="skipped", reason=variant["unavailable"])
            else:
                version_cmd = variant["version_command"]
                version = command(journal, "version", variant, version_cmd)
                row["version"] = (version["stdout"].strip() or version["stderr"].strip()).splitlines()[0]
                row["tool_path"] = version_cmd[0]
                row["tool_sha256"] = lab.sha(version_cmd[0])
                builder = variant.get("builder", build)
                row["build_ms"] = builder(variant, directory / "src", directory / "build", journal, settings["build_samples"])
                row["artifact_sha256"] = lab.sha(variant["artifact"])
                row["artifact_bytes"] = Path(variant["artifact"]).stat().st_size
                for index in range(settings["startup_samples"] + 1):
                    result = command(journal, "startup", variant, variant["run"] + ["startup"],
                                     env=startup_env, timeout=45, sample=index, warmup=index == 0)
                    if result["stdout"].strip() != "42":
                        raise ValueError(f"unexpected startup output {result['stdout']!r}")
                    if index:
                        row["startup_ms"].append(result["wall_ms"])
                order = list(cases)
                random.Random(601 + len(info["variants"])).shuffle(order)
                for case_id, case in order:
                    journal.write("case_start", variant=name, case=case[0], n=case[1])
                    row["runtime"][case[0]] = runtime(variant, case_id, case, journal, settings)
                    save_json(directory / "summary.json", info)
                row["status"] = "passed"
        except Exception as exc:
            row.update(status="failed", reason=f"{type(exc).__name__}: {exc}")
            journal.write("failure", variant=name, reason=row["reason"])
        print(f"{name} {row['status']} {row.get('reason', '')}", flush=True)
        save_json(directory / "summary.json", info)
    suite_report.render(directory)
    return all(r["status"] != "failed" for r in info["variants"]) and any(r["status"] == "passed" for r in info["variants"])


def kinal_variants(src, builds):
    stages = ROOT.parent / "out" / "selfhost"
    stage0 = Path(os.environ.get("KINAL_STAGE0", stages / "stage0-host" / "kinal.exe")).resolve()
    stage3 = Path(os.environ.get("KINAL_STAGE3", stages / "stage3" / "kinal.exe")).resolve()
    rows = []
    for name, compiler in (("kinal-c", stage0), ("kinal-self", stage3)):
        for lto in (False, True):
            config = name + ("-lto" if lto else "")
            exe = str(builds / (config + ".exe"))
            command = [str(compiler), "build", str(src / "bench.kn"), "--no-module-discovery", "--color", "never"]
            if lto:
                command.append("--lto=full")
            rows.append(dict(id=config, language="Kinal", artifact=exe, build=command + ["-o", exe],
                             run=[exe], version_command=[str(compiler), "--version"],
                             unavailable=None if compiler.is_file() else f"missing compiler: {compiler}"))
    return rows


def main(extension=None):
    parser = argparse.ArgumentParser(description=extension.__doc__ if extension else __doc__)
    parser.add_argument("--only", help="comma-separated configuration IDs")
    parser.add_argument("--cases", default="extended", help="extended, all, or comma-separated case names")
    parser.add_argument("--out", type=Path, help="new result directory")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("This benchmark uses the Windows process working-set counters.")
    cases = [(c["name"], c["size"], c["title"], c["description"]) for c in CATALOG]
    if args.cases == "all":
        names = [c["name"] for c in CATALOG]
    elif args.cases == "extended":
        names = [c["name"] for c in CATALOG if c["extended"]]
    else:
        names = args.cases.split(",")
    selected = [(i, c) for i, c in enumerate(cases) if c[0] in names]
    if len(selected) != len(names):
        parser.error("unknown or duplicate workload name")
    root = extension.ROOT if extension else ROOT
    directory = (args.out or root / "results" / "runs" / datetime.now().strftime("%Y%m%d-%H%M%S-%f")).resolve()
    src = directory / "src"
    builds = directory / "build"
    rows = kinal_variants(src, builds)
    if extension:
        rows.extend(extension.variants(src, builds))
    if args.only:
        requested = args.only.split(",")
        rows = [v for v in rows if v["id"] in requested]
        if len(rows) != len(requested):
            parser.error("unknown or duplicate configuration ID")
    for variant in rows.copy():
        supported = variant.get("supported_cases")
        if supported is not None and not supported.issuperset(names):
            if args.only:
                parser.error(f"{variant['id']} supports only: {', '.join(sorted(supported))}")
            rows.remove(variant)
    rows.sort(key=lambda v: (v["language"] != "Kinal", v["id"] != "kinal-self-lto" if v["language"] == "Kinal" else False))
    if directory.exists():
        parser.error(f"output directory already exists: {directory}")
    directory.mkdir(parents=True)
    builds.mkdir()
    src.mkdir()
    shutil.copyfile(ROOT / "bench.kn", src / "bench.kn")
    harness = [ROOT / name for name in ("suite.py", "benchmark_process.py", "suite_report.py",
                                        "verify_suite.py", "cases.json")]
    if extension:
        extension.prepare(directory, src, builds)
        harness.extend(extension.harness_files())
    snapshot = directory / "harness"
    snapshot.mkdir()
    for script in harness:
        target = snapshot / script.name
        if target.exists():
            raise ValueError(f"duplicate harness file: {script.name}")
        shutil.copyfile(script, target)
    files = [p for folder in (src, snapshot) for p in folder.iterdir() if p.is_file()]
    if (directory / "global.json").is_file():
        files.append(directory / "global.json")
    save_json(directory / "sources.json", {p.relative_to(directory).as_posix(): lab.sha(p) for p in files})
    settings = dict(processes=3, samples_per_process=3, startup_samples=9, build_samples=3,
                    target_batch_ms=80, warmup_seconds=0.6, generated_cases=len(CATALOG))
    print(directory, flush=True)
    return 0 if benchmark(directory, rows, selected, settings) else 1


if __name__ == "__main__":
    sys.exit(main())

