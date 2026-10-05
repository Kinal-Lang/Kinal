"""Compare supported compiler/backend behavior with a bounded Python oracle.

This is a functional regression check, not a claim of full language parity.
Only host executables are run; the selfhost compiler has no KNC backend.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
from pathlib import Path


SEEDS = (0, 1, 2, 3, 7, 19, 42, 97, 128, 257, 500, 996)
ROUNDS = 23


def evaluate(seed: int) -> int:
    """The operands stay small, positive, and within signed 64-bit range."""
    state = seed
    checksum = 0
    values = [1, 2, 3, 0, 0, 0]
    alias = values
    for round_index in range(ROUNDS):
        for index in range(6):
            state = (state * 17 + 11) % 997
            values[index] = (values[index] + state) % 101
            if values[index] % 5 == 0:
                checksum += values[index]
                continue
            if index == 4 and values[index] > 70:
                break
            checksum += values[index] ^ (round_index + 3)
        cursor = 0
        while cursor < 6:
            checksum += alias[cursor]
            cursor += 1
    return checksum


def source() -> str:
    return '''Unit Tests.VersionParity;
Get IO.Console;

Function int Evaluate(int seed)
{
    int state = seed;
    int checksum = 0;
    int values[6] = {1, 2, 3};
    int[] alias = values;
    For (int round = 0; round < ''' + str(ROUNDS) + '''; round++)
    {
        For (int index = 0; index < 6; index++)
        {
            state = (state * 17 + 11) % 997;
            values[index] = (values[index] + state) % 101;
            If (values[index] % 5 == 0)
            {
                checksum += values[index];
                Continue;
            }
            If (index == 4 && values[index] > 70) Break;
            checksum += values[index] ^ (round + 3);
        }
        int cursor = 0;
        While (cursor < 6)
        {
            checksum += alias[cursor];
            cursor++;
        }
    }
    Return checksum;
}

Static Function int Main()
{
''' + ''.join(f'    IO.Console.PrintLine(Evaluate({seed}));\n' for seed in SEEDS) + '''    Return 0;
}
'''


def invoke(command: list[str], root: Path, log: Path, timeout: int) -> dict[str, object]:
    try:
        process = subprocess.run(command, cwd=root, text=True, capture_output=True,
                                 timeout=timeout, check=False)
        result: dict[str, object] = {
            "command": command, "returncode": process.returncode,
            "stdout": process.stdout, "stderr": process.stderr,
        }
    except subprocess.TimeoutExpired as error:
        result = {"command": command, "returncode": "timeout",
                  "stdout": str(error.stdout or ""), "stderr": str(error.stderr or "")}
    log.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def check_version_parity(compiler: Path, root: Path, out: Path, *,
                         vm: Path | None = None,
                         selfhost: Path | None = None) -> dict[str, object]:
    compiler, root, out = compiler.resolve(), root.resolve(), out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source_path = out / "Main.kn"
    source_path.write_text(source(), encoding="utf-8")
    project = out / "kinal.knproj"
    project.write_text('''Project VersionParity
{
    DefaultProfile = "test";
    SourceSet "source" { Files = ["Main.kn"]; RequireUnit = true; }
    Profile "test"
    {
        Source { Entry = "Main.kn"; Sets = ["source"]; Mode = FileOnly; }
        Build { Backend = Native; Environment = Hosted; }
    }
}
''', encoding="utf-8")
    expected = ''.join(f"{evaluate(seed)}\n" for seed in SEEDS)
    (out / "expected.txt").write_text(expected, encoding="utf-8")
    executables = [("c-native", compiler, None)]
    if vm is not None:
        executables.append(("c-knc", compiler, vm.resolve()))
    if selfhost is not None:
        executables.append(("selfhost-native", selfhost.resolve(), None))
    results = []
    for role, tool, runner in executables:
        suffix = ".knc" if runner is not None else ".exe" if os.name == "nt" else ""
        output = out / (role + suffix)
        # A compiler that returns success without emitting must not reuse a
        # passing executable from an earlier invocation of this check.
        output.unlink(missing_ok=True)
        if runner is not None:
            command = [str(tool), "vm", "build", "--no-module-discovery",
                       str(source_path), "-o", str(output)]
        else:
            command = [str(tool), "build", "--project", str(project), "-o", str(output)]
        build = invoke(command, root, out / (role + ".build.json"), 180)
        result = {"role": role, "build_returncode": build["returncode"], "ok": False}
        if build["returncode"] == 0 and output.is_file():
            run_command = [str(runner), str(output)] if runner is not None else [str(output)]
            execution = invoke(run_command, root, out / (role + ".run.json"), 30)
            result["run_returncode"] = execution["returncode"]
            result["ok"] = (execution["returncode"] == 0
                            and str(execution["stdout"]).replace("\r\n", "\n") == expected
                            and execution["stderr"] == "")
        results.append(result)
        print(f"[{'OK' if result['ok'] else 'FAIL'}] version parity: {role}", flush=True)
    report = {"name": "version_parity", "ok": all(row["ok"] for row in results),
              "seeds": list(SEEDS), "rounds": ROUNDS, "results": results}
    report_path = out / "report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if not report["ok"]:
        raise AssertionError(f"version parity failed; inspect {report_path}")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--vm", type=Path)
    parser.add_argument("--selfhost", type=Path)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(check_version_parity(args.compiler, Path(__file__).resolve().parents[1],
                                        args.out_dir, vm=args.vm, selfhost=args.selfhost)))
