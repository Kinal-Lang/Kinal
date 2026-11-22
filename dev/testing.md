# Testing Guide

This guide covers the C compiler (`apps/kinal`), the VM built with that compiler, and the shared runtime/standard library. Run commands from the repository root. Selfhost bootstrap and parity tests are separate workflows.

## Build and Run

```bash
python x.py dev --release
python x.py test --release
python x.py test --release --full
```

`test` uses an existing staged compiler unless it is missing or `--build` is passed. Rebuild after changing compiler/runtime sources; otherwise a successful run may describe an older executable.

| Option | Behavior |
|--------|----------|
| `--release` | Select `out/stage/host-release/` instead of `host-debug/` |
| `--compiler <path>` | Use an explicit compiler executable |
| `--build` | Build and stage the selected host configuration first |
| `--full` | Run `tests/manifest.json` and driver integrations instead of the smoke and freestanding manifests |

Both modes also run Python unit discovery, builtin/opcode/stdlib ownership checks, package payload/cache checks, target spelling checks, version checks, and parser/Sema stage checks. `--full` describes the main regression manifest; it does not include every independent LSP, stress, or selfhost workflow.

Some Python VM fixtures independently default to `out/stage/host-debug/kinalvm`; `--release` does not redirect that default. To validate a freshly built release VM, set `KINAL_VM_PATH` before running the suite:

```bash
# Linux/macOS
export KINAL_VM_PATH="$PWD/out/stage/host-release/kinalvm"
```

```powershell
# Windows PowerShell
$env:KINAL_VM_PATH = (Resolve-Path out/stage/host-release/kinalvm.exe).Path
```

## Focused Checks

```bash
python tests/run_tests.py --compiler out/stage/host-release/kinal.exe --manifest tests/smoke.json --out-dir out/test/docs-smoke
python tests/run_stage_tests.py --compiler out/stage/host-release/kinal.exe
python -m unittest discover -s tests -p test_knc_numeric_format.py
python tests/check_builtin_registry.py
python tests/check_knc_opcode_registry.py
python tests/check_stdlib_coverage.py
```

The explicit compiler paths above use Windows spelling; on Linux/macOS omit `.exe`. `x.py test` does not accept a positional source filename. To investigate one program, use `kinal build`/`run`, or pass a small temporary manifest to `tests/run_tests.py`.

## Manifest Contract

A source file is not automatically a regression merely because it exists under `tests/`. Register it in the appropriate JSON manifest.

| Field | Meaning |
|-------|---------|
| `name` | Unique test/artifact name |
| `sources` | Source file list, relative to the repository root |
| `expected` | Expected stdout for an executed positive case |
| `expect_error` | Expected compiler-error text for a negative case |
| `compile_only` | Successful compilation and artifact creation are sufficient |
| `platforms` | Restrict execution to supported host platforms |
| `expected_exit_code` | Program exit code; default is zero |
| `stdin` | Text supplied to the program's standard input |
| `runtime_files` | Additional files copied beside the generated executable |

Inspect adjacent entries before adding a case: optional fixture, target, backend, and link settings depend on the runner. `error_` is a naming convention, not the mechanism that makes a test negative. The runner reads `expect_error`; comments in `.kn` files do not replace manifest expectations.

Tests commonly live in `tests/common/`, with host-specific programs in `tests/windows/`, `tests/linux/`, and `tests/macos/`. A manifest can refer to multiple source files and exercise imported units together.

## Writing Useful Regressions

1. Reduce the problem to a complete program or project that triggers the actual behavior.
2. For runtime changes, assert observable output/exit status; compilation alone is insufficient.
3. For errors, check the intended diagnostic and ensure no backend or host crash occurs.
4. Register the case in the main manifest and, when appropriate, smoke or freestanding coverage.
5. For builtin/official-package additions, update `tests/stdlib_coverage.json` and run its ownership check.
6. For KNC opcode changes, update emitter, declarations, sizes, loader/VM behavior, and the ABI registry check together.

Use a scratch name such as `out/test/my_case_data/` for files created by the program. On Unix, `out/test/my_case` may already be the generated executable. Keep network tests on local fixtures, distinguish optional prerequisites from failures, and preserve deterministic output.

## Stress, LSP, and Cross-Platform Evidence

```bash
python x.py stress --release --profile quick
python x.py stress --release --profile max
python tests/lsp/run_lsp_tests.py --help
```

Stress tests generate workloads under `out/stress/`; they complement the behavioral manifest. The LSP runner covers diagnostics and editor protocol behavior and has its own arguments and generated workspaces.

The hosted CI matrix includes Windows, Linux, and macOS on x64 and ARM64. Cross-target IR/object emission establishes code generation for a target, not successful execution on that target. Record the host, compiler path/version, manifest, skips, and failures when reporting results. A green C suite does not establish selfhost bootstrap stability.

## See Also

- [Build Guide](build.md)
- [Architecture](architecture.md)
- [Diagnostic Reference](../docs/advanced/error-codes/index.md)
- [KinalVM](../docs/cli/vm.md)
