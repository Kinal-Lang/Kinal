"""Verify hashes, observations and timing summaries in an archived suite run."""
import argparse
from collections import Counter, defaultdict
from decimal import Decimal
import hashlib
import json
import math
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def verify_sources(directory):
    manifest = read_json(directory / "sources.json")
    wanted = {"global.json"} if (directory / "global.json").is_file() else set()
    for folder in ("src", "harness"):
        files = [p for p in (directory / folder).iterdir() if p.is_file()]
        require(files, f"no files found in {folder}")
        wanted.update(p.relative_to(directory).as_posix() for p in files)
    require(set(manifest) == wanted, f"source manifest file list differs: {sorted(set(manifest) ^ wanted)}")
    for name, digest in manifest.items():
        actual = hashlib.sha256((directory / name).read_bytes()).hexdigest()
        require(actual == digest, f"SHA-256 mismatch: {name}")
    print(f"Verified {len(manifest)} source hashes.")


def check_command(record):
    require(record["status"] == "ok" and record["exit_code"] == 0 and record["command"],
            f"unsuccessful {record['phase']} command")
    require(math.isfinite(record["wall_ms"]) and record["wall_ms"] > 0, "invalid command duration")


def check_commands(phase, records, count, summary):
    expected = range(count + 1) if count else ()
    require([r["sample"] for r in records] == list(expected), f"{phase} sample indices differ")
    for index, r in enumerate(records):
        check_command(r)
        require(r["warmup"] is (index == 0), f"{phase} warmup flag at {index}")
        if phase == "startup":
            require(r["stdout"].strip() == "42", f"startup returned {r['stdout']!r}")
    require([r["wall_ms"] for r in records[1:]] == summary, f"{phase} durations differ from summary")


def check_value(record, expected):
    value = record["value"]
    if record["case"] in ("matrix_float", "float_series"):
        match = math.isclose(value, expected, rel_tol=2e-6, abs_tol=1e-6)
    else:
        match = Decimal(record["output"]) == expected
    require(record["ok"] is True and math.isfinite(value) and float(record["output"]) == value
            and record["expected"] == expected and match, f"incorrect {record['phase']} observation")
    require(math.isfinite(record["wall_ms"]) and record["wall_ms"] > 0, "invalid batch duration")


def check_case(name, size, phases, point, settings):
    seeds = range(7, 14)
    validation = phases["validate"]
    half = max(2, size // 2)
    expected_keys = [(n, seed) for n in (half, size) for seed in seeds]
    require(Counter((r["n"], r["seed"]) for r in validation) == Counter(expected_keys),
            f"{name}: incomplete size/seed validation")
    for r in validation:
        require(r["reps"] == 1, f"{name}: validation batch size")
        check_value(r, r["expected"])
    full = [r for r in validation if r["n"] == size]
    seed_values = {r["seed"]: r["expected"] for r in full}
    seed_sum = sum(seed_values[seed] for seed in seeds)
    peak = max(r["peak_working_set_bytes"] or 0 for r in full)
    require(point["n"] == size and point["peak_mib"] == (peak / 2**20 if peak else None),
            f"{name}: size or peak memory differs from summary")
    procs = range(settings["processes"])
    samples = range(settings["samples_per_process"])
    warmups = Counter(r["process"] for r in phases["warmup"])
    require(set(warmups) == set(procs) and min(warmups.values()) >= 3, f"{name}: missing warmups")
    for r in phases["warmup"] + phases["runtime"]:
        require(r["n"] == size and r["seed"] == 7 and type(r["reps"]) is int
                and r["reps"] > 0 and r["reps"] % 7 == 0, f"{name}: invalid batch parameters")
        check_value(r, seed_sum * (r["reps"] // 7))
    records = phases["runtime"]
    require([(r["process"], r["sample"]) for r in records] == [(p, i) for p in procs for i in samples],
            f"{name}: runtime process/sample indices differ")
    require(all(r["reps"] == point["reps"] for r in records), f"{name}: reps differ from summary")
    require([r["wall_ms"] / r["reps"] for r in records] == point["ms"],
            f"{name}: per-invocation durations differ from summary")


def check_variant(row, records, sizes, settings):
    commands = defaultdict(list)
    batches = defaultdict(lambda: defaultdict(list))
    starts = []
    for r in records:
        if r["phase"] == "case_start":
            starts.append((r["case"], r["n"]))
        elif r["phase"] in ("validate", "warmup", "runtime"):
            batches[r["case"]][r["phase"]].append(r)
        else:
            require(r["phase"] in ("version", "prepare", "build", "startup"), f"unexpected phase {r['phase']}")
            commands[r["phase"]].append(r)
    require(len(commands["version"]) == 1, "expected one version probe")
    check_command(commands["version"][0])
    require(len(commands["prepare"]) == int(row["id"] == "dotnet"), "unexpected prepare record count")
    for r in commands["prepare"]:
        check_command(r)
    count = settings["build_samples"] if row["build_command"] else 0
    check_commands("build", commands["build"], count, row["build_ms"])
    check_commands("startup", commands["startup"], settings["startup_samples"], row["startup_ms"])
    require(set(row["runtime"]) == set(batches) == set(sizes), "runtime case names differ")
    if starts:
        require(Counter(starts) == Counter(sizes.items()), "incomplete case_start records")
    for case, size in sizes.items():
        check_case(case, size, batches[case], row["runtime"][case], settings)


def verify(directory):
    verify_sources(directory)
    summary = read_json(directory / "summary.json")
    sizes = summary["case_sizes"]
    rows = summary["variants"]
    ids = [r["id"] for r in rows]
    require(len(set(ids)) == len(ids), "duplicate variant ID")
    require(sizes and Counter(summary["cases"]) == Counter(sizes.keys()), "invalid list of cases")
    for key in ("processes", "samples_per_process", "startup_samples", "build_samples"):
        value = summary["settings"][key]
        require(type(value) is int and value > 0, f"invalid {key}")
    records = defaultdict(list)
    journal = directory / "samples.jsonl"
    if journal.exists():
        with journal.open(encoding="utf-8") as stream:
            for line, text in enumerate(stream, 1):
                try:
                    r = json.loads(text)
                    records[r["variant"]].append(r)
                except (ValueError, KeyError, TypeError) as exc:
                    raise ValueError(f"samples.jsonl line {line}: {exc}") from exc
    require(set(records).issubset(ids), f"unknown journal variants: {sorted(set(records) - set(ids))}")
    selected = summary.get("selected_variants", ids)
    counts = Counter()
    for row in rows:
        name, status = row["id"], row["status"]
        evidence = records[name]
        try:
            if status == "passed":
                check_variant(row, evidence, sizes, summary["settings"])
            elif status == "skipped":
                require(row["reason"] and not evidence, "missing skip reason or unexpected samples")
            elif status == "failed":
                failures = [r["reason"] for r in evidence if r["phase"] == "failure"]
                require(row["reason"] and failures == [row["reason"]], "missing or inconsistent failure reason")
            else:
                require(status == "pending", f"unknown status {status!r}")
            print(f"{name}: {status}" + (f" ({row['reason']})" if row.get("reason") else ""))
            counts[status] += 1
        except (KeyError, TypeError, ValueError, ArithmeticError) as exc:
            print(f"{name}: INVALID ({exc})")
            counts["invalid"] += 1
    require(ids == selected, f"measured variants differ from selection: {selected!r}")
    print(", ".join(f"{n} {s}" for s, n in sorted(counts.items())) or "No variants")
    return bool(counts["passed"]) and not any(counts[s] for s in ("invalid", "failed", "pending"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path)
    try:
        success = verify(parser.parse_args().run_directory)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        parser.exit(1, f"INVALID archive: {exc}\n")
    raise SystemExit(0 if success else 1)
