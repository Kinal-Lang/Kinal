from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path


def check_function_summary(compiler: Path, stage0: Path, root: Path, out: Path,
                           *, stage0_reference: bool = True) -> dict[str, object]:
    out.mkdir(parents=True, exist_ok=True)
    tools = [("stage0", stage0)] if stage0_reference else []
    tools.append(("selfhost", compiler))
    records = ["format=kinal-selfhost-functions-v1"]
    for index in range(5000):
        name = "Tests.Summary." + ("Owner." if index % 3 == 1 else "") + f"Function{index}"
        instance = index % 11 == 0
        if instance:
            name += "$int"
        records.append(f"function={name}|parameters={index % 4}"
                       f"|static={int(index % 2 == 0)}|constructor={int(index % 5 == 0)}"
                       f"|generic_template={int(index % 7 == 0 and not instance)}"
                       f"|generic_instance={int(instance)}")
    expected = "\n".join(records) + "\n"
    project = root / "tests/selfhost/fixtures/function_summary/kinal.knproj"
    for label, tool in tools:
        executable = out / (label + (".exe" if os.name == "nt" else ""))
        build = subprocess.run([str(tool), "build", "--project", str(project), "--profile", "test",
                                "-o", str(executable)], cwd=root, capture_output=True,
                               text=True, timeout=900)
        assert build.returncode == 0, (label, build.returncode, build.stdout, build.stderr)
        for repeat in range(2):
            result = subprocess.run([str(executable)], cwd=root, capture_output=True,
                                    text=True, timeout=120)
            assert result.returncode == 0 and result.stdout == expected and not result.stderr, \
                (label, repeat, result.returncode, result.stdout[:1000], result.stderr)
        print(f"[OK] {label} function summary: 5000 exact ordered records", flush=True)
    return {"name": "function_summary", "ok": True, "compilers": len(tools),
            "records": 5000, "repeats": 2, "stage0_reference": stage0_reference}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    print(check_function_summary(args.compiler.resolve(), args.stage0.resolve(),
                                 Path(__file__).resolve().parents[2], args.out_dir.resolve()))
