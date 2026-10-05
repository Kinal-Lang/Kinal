"""Stage0 profile ABI and selfhost package-boundary rejection contract.

Custom C hooks below test stage0 compatibility only, not a bundled runtime.
Selfhost rejects the extended profiles until they have Kinal package-backed
implementations. The supported runtime-free subset is check_freestanding.py.
"""
from __future__ import annotations

import argparse
import json
import platform
import re
import sys
from pathlib import Path

from check_freestanding import manifest
from check_targets import check_object, invoke


def check_profiles(compiler: Path, stage0: Path, root: Path, out: Path,
                   *, stage0_reference: bool = True) -> dict[str, object]:
    sys.path.insert(0, str(root))
    from infra.scripts.x.llvm import detect_llvm_dir, llvm_bin_dir
    tools = llvm_bin_dir(detect_llvm_dir())
    suffix = ".exe" if sys.platform == "win32" else ""
    clang, nm = tools / ("clang" + suffix), tools / ("llvm-nm" + suffix)
    out.mkdir(parents=True, exist_ok=True)
    fixtures = root / "tests/selfhost/fixtures/freestanding_profiles"
    binaries = [("selfhost", compiler)]
    if stage0_reference:
        binaries.append(("stage0", stage0))
    host_os = {"Windows": 1, "Linux": 2, "Darwin": 3}[platform.system()]
    host_arch = 3 if platform.machine().lower() in {"arm64", "aarch64"} else 2
    records = []
    for runtime in ("None", "Alloc", "GC"):
        source_name = "None" if runtime == "None" else "GC" if runtime == "GC" else "Optional"
        source = out / "Main.kn"
        source.write_text((fixtures / (source_name + ".kn")).read_text(encoding="utf-8"), encoding="utf-8")
        for target, os_id, arch in (("host", host_os, host_arch),
                                    ("bare64", 2, 2), ("bare-arm64", 2, 3)):
            project = out / "kinal.knproj"
            manifest(project, target=target, runtime=runtime)
            for role, binary in binaries:
                stem = f"{role}-{runtime.lower()}-{target}"
                ir_path, obj = out / (stem + ".ll"), out / (stem + ".o")
                command = [str(binary), "build", "--project", str(project)]
                ir_path.unlink(missing_ok=True)
                obj.unlink(missing_ok=True)
                if role == "selfhost":
                    expected = "Freestanding Core" if runtime == "None" else "requires a Kinal runtime package"
                    invoke(command + ["--emit", "ir", "-o", str(ir_path)], root, error=expected)
                    assert not ir_path.exists()
                    records.append({"compiler": role, "runtime": runtime, "target": target,
                                    "status": "rejected: Kinal package implementation pending",
                                    "host_executed": False})
                    print(f"[OK] selfhost Freestanding/{runtime} {target}: explicit package-boundary rejection", flush=True)
                    continue
                invoke(command + ["--emit", "ir", "-o", str(ir_path)], root)
                ir = ir_path.read_text(encoding="utf-8")
                assert not re.search(r"^define .*@main\(", ir, re.M)
                for forbidden in ("kn_native_", "kn_sh_rt_", "IO.Kinal.Runtime.GarbageCollector"):
                    assert forbidden not in ir, (stem, forbidden)
                assert "@__kn_entry(" in ir, stem
                invoke(command + ["--emit", "obj", "-o", str(obj)], root)
                check_object(obj, os_id, arch)
                output = invoke([str(nm), "--undefined-only", "--format=posix", str(obj)], root)
                undefined = {line.split()[0].lstrip("_") for line in output.splitlines() if line.strip()}
                # Normalize object-format leading underscores, not ABI prefixes.
                # Large frames may need the target's stack-probe ABI, supplied
                # by the normal compiler support library, not a managed runtime.
                allowed = {"fltused", "chkstk", "chkstk_ms", "chkstk_arm64ec"} if os_id == 1 else set()
                if runtime == "None":
                    assert undefined <= allowed, (stem, sorted(undefined))
                else:
                    hooks = {"kn_gc_collect", "kn_gc_alloc", "kn_gc_push_frame", "kn_gc_add_root", "kn_gc_add_global_root",
                             "kn_gc_pop_frame", "kn_str_concat", "kn_any_to_string", "kn_exc_get",
                             "kn_exc_has", "kn_exc_push", "kn_exc_trace", "kn_exc_last",
                             "kn_exc_set", "kn_exc_clear", "kn_list_new", "kn_list_add",
                             "kn_list_count", "kn_list_contains"}
                    assert undefined <= hooks | allowed, (stem, sorted(undefined))
                    assert {"kn_gc_alloc", "kn_gc_push_frame", "kn_gc_add_root", "kn_gc_pop_frame",
                            "kn_str_concat", "kn_any_to_string"} <= undefined, (stem, sorted(undefined))
                executed = target == "host"
                if executed:
                    harness = fixtures / ("none-consumer.c" if runtime == "None" else
                                          "gc-consumer.c" if runtime == "GC" else "optional-consumer.c")
                    exe = out / (stem + suffix)
                    exe.unlink(missing_ok=True)
                    link = [str(clang)]
                    if host_os == 2:
                        link.append("-no-pie")
                    if runtime != "None":
                        link += ["-Wno-override-module", str(fixtures / "any-runtime-adapter.ll")]
                    link.extend([str(harness), str(obj), "-o", str(exe)])
                    invoke(link, root)
                    invoke([str(exe)], root)
                records.append({"compiler": role, "runtime": runtime, "target": target,
                                "object": str(obj), "host_executed": executed})
                print(f"[OK] {role} Freestanding/{runtime} {target}: " +
                      ("custom-hook execution" if executed and runtime != "None" else
                       "runtime-free execution" if executed else "IR/object and hook ABI"), flush=True)
    # Both native lowerers must guard string helpers with both operand tags.
    # Keep this separate from profile policy so a future eager dereference is
    # caught even when the main fixture changes its type-predicate coverage.
    mixed_source = out / "AnyMixedKinds.kn"
    mixed_source.write_text((fixtures / "AnyMixedKinds.kn").read_text(encoding="utf-8"), encoding="utf-8")
    project = out / "mixed.knproj"
    manifest(project, runtime="None", source=mixed_source.name)
    mixed_roles = []
    for role, binary in binaries:
        obj = out / (role + "-any-mixed.o")
        exe = out / (role + "-any-mixed" + suffix)
        obj.unlink(missing_ok=True)
        exe.unlink(missing_ok=True)
        if role == "selfhost":
            invoke([str(binary), "build", "--project", str(project), "--emit", "obj", "-o", str(obj)],
                   root, error="Freestanding Core")
            assert not obj.exists()
            continue
        invoke([str(binary), "build", "--project", str(project), "--emit", "obj", "-o", str(obj)], root)
        output = invoke([str(nm), "--undefined-only", "--format=posix", str(obj)], root)
        undefined = {line.split()[0].lstrip("_") for line in output.splitlines() if line.strip()}
        assert undefined <= ({"fltused", "chkstk", "chkstk_ms", "chkstk_arm64ec"}
                             if host_os == 1 else set()), sorted(undefined)
        link = [str(clang)] + (["-no-pie"] if host_os == 2 else [])
        invoke(link + [str(fixtures / "none-consumer.c"), str(obj), "-o", str(exe)], root)
        invoke([str(exe)], root)
        mixed_roles.append(role)
        print(f"[OK] {role} runtime-free mixed-kind any equality safety", flush=True)
    return {"name": "freestanding_profiles", "ok": True, "checks": records,
            "mixed_any_equality_compilers": mixed_roles,
            "custom_runtime": "test-only hooks", "foreign_runtime_execution": "not tested"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--stage0-role", choices=("reference", "bootstrap"), default="reference")
    args = parser.parse_args()
    print(json.dumps(check_profiles(args.compiler.resolve(), args.stage0.resolve(), args.root.resolve(),
                                    args.out_dir.resolve(), stage0_reference=args.stage0_role == "reference")))
