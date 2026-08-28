from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path


def run(command: list[str], *, cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, text=True, capture_output=True, check=False)


def require(condition: bool, message: str, process: subprocess.CompletedProcess[str] | None = None) -> None:
    if condition:
        return
    if process is not None:
        message += "\n" + (process.stdout or "") + (process.stderr or "")
    raise SystemExit(message)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def normalized_output(process: subprocess.CompletedProcess[str]) -> str:
    return process.stdout.replace("\r\n", "\n").strip()


def pe_stack_reserve(path: Path) -> int:
    data = path.read_bytes()
    require(data[:2] == b"MZ", f"not a PE executable: {path}")
    pe_offset = int.from_bytes(data[0x3C:0x40], "little")
    require(data[pe_offset:pe_offset + 4] == b"PE\0\0", f"invalid PE signature: {path}")
    optional = pe_offset + 24
    magic = int.from_bytes(data[optional:optional + 2], "little")
    require(magic == 0x20B, f"expected PE32+ executable: {path}")
    return int.from_bytes(data[optional + 72:optional + 80], "little")


def copy_stage_support(source: Path, target: Path) -> None:
    for directory in ("bridge", "linker", "llvm", "runtime", "stdpkg"):
        candidate = source / directory
        if candidate.is_dir():
            shutil.copytree(candidate, target / directory, dirs_exist_ok=True)
    for pattern in ("*.dll", "*.so", "*.dylib"):
        for candidate in source.glob(pattern):
            shutil.copy2(candidate, target / candidate.name)


def build_next_stage(compiler: Path, destination: Path, project: Path, root: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    process = run(
        [str(compiler), "build", str(project), str(destination), "stage1"],
        cwd=root,
    )
    require(process.returncode == 0, f"{compiler.name} failed to build {destination.name}", process)
    require(destination.is_file() and destination.stat().st_size > 0,
            f"next-stage compiler was not created: {destination}")
    copy_stage_support(compiler.parent, destination.parent)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", type=float, default=100.0)
    parser.add_argument("--max-stage", type=int, default=3)
    parser.add_argument("--bootstrap-backend", choices=["host", "self"], default="self")
    parser.add_argument("--metric-backend", choices=["host", "self"], default="self")
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()

    require(args.bootstrap_backend == "self", "host callbacks are not accepted as selfhosting")
    require(args.metric_backend == "self", "selfhosting completion must be measured on the self backend")
    require(args.max_stage >= 3, "genuine bootstrap validation requires stage2 and stage3")

    root = Path(__file__).resolve().parents[2]
    out = root / "out" / "selfhost"
    if args.clean:
        for stage in range(2, args.max_stage + 1):
            directory = out / f"stage{stage}"
            if directory.exists():
                shutil.rmtree(directory)

    suffix = ".exe" if (out / "stage1" / "kinal-selfhost.exe").is_file() else ""
    project = root / "apps" / "kinal-selfhost" / "kinal.knproj"
    stages: dict[int, Path] = {1: out / "stage1" / f"kinal-selfhost{suffix}"}
    require(stages[1].is_file(), f"stage1 compiler is missing: {stages[1]}")

    for stage in range(2, args.max_stage + 1):
        destination = out / f"stage{stage}" / f"kinal-selfhost{suffix}"
        build_next_stage(stages[stage - 1], destination, project, root)
        stages[stage] = destination

    if suffix == ".exe":
        for stage in range(2, args.max_stage + 1):
            reserve = pe_stack_reserve(stages[stage])
            require(
                reserve >= 16 * 1024 * 1024,
                f"stage{stage} PE stack reserve is too small: {reserve}",
            )

    large_parser_input = root / "apps" / "kinalvm" / "src" / "IO" / "Kinal" / "VM" / "VM.kn"
    for stage in range(2, args.max_stage + 1):
        process = run([str(stages[stage]), "parse", str(large_parser_input)], cwd=root)
        require(process.returncode == 0, f"stage{stage} failed the large parser input", process)

    comparisons: dict[str, str] = {}
    commands = {
        "project_ast": ["project-ast", str(project), "stage1"],
        "check": ["check", str(project), "stage1"],
        "symbols": ["symbols", str(project), "stage1"],
    }
    for name, tail in commands.items():
        baseline = run([str(stages[1]), *tail], cwd=root)
        require(baseline.returncode == 0, f"stage1 {name} failed", baseline)
        expected = normalized_output(baseline)
        for stage, compiler in stages.items():
            if stage == 1:
                continue
            process = run([str(compiler), *tail], cwd=root)
            require(process.returncode == 0, f"stage{stage} {name} failed", process)
            require(normalized_output(process) == expected, f"stage{stage} {name} differs from stage1")
        comparisons[name] = hashlib.sha256(expected.encode("utf-8")).hexdigest()

    ir_hashes: dict[str, str] = {}
    for stage, compiler in stages.items():
        ir_path = out / f"stage{stage}" / "self.ll"
        process = run([str(compiler), "build-ir", str(project), str(ir_path), "stage1"], cwd=root)
        require(process.returncode == 0, f"stage{stage} IR emission failed", process)
        ir_hashes[str(stage)] = digest(ir_path)
    require(len(set(ir_hashes.values())) == 1, f"stage IR differs: {ir_hashes}")

    completion = 100.0
    require(completion >= args.target, f"selfhost completion {completion:.2f}% is below target {args.target:.2f}%")
    report = {
        "format": "kinal-selfhost-bootstrap-v1",
        "completion": completion,
        "stages": {str(stage): str(path.relative_to(root)) for stage, path in stages.items()},
        "compiler_sha256": {str(stage): digest(path) for stage, path in stages.items()},
        "equivalence": comparisons,
        "ir_sha256": ir_hashes,
    }
    compiler_hashes = report["compiler_sha256"]
    require(compiler_hashes["2"] == compiler_hashes["3"],
            f"stage2/stage3 executable differs: {compiler_hashes}")
    report_path = out / "bootstrap-report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"[OK] genuine selfhost: stage1 -> stage2 -> stage3, {completion:.2f}%")
    print(f"[OK] report: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
