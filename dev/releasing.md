# Release Process

This guide describes the C compiler distribution. Read the actual workflow configuration before publishing; creating tags or releases can start builds and production synchronization.

## Component Versions

The root `VERSION` file is the source of truth:

```text
kinal=0.9.0
kinalvm=0.8.0
```

These are component versions, not a requirement that their major/minor numbers match. Change the component whose contract changes. Bytecode format version is a separate value: KNC format 3 is not implied by either semantic version.

CMake generates `kn/version.h` in the build tree; do not edit generated headers. KinalVM's checked-in `BuildInfo.kn` is generated from `VERSION:kinalvm`:

```bash
python -m infra.scripts.x.vm_metadata
python -m infra.scripts.x.vm_metadata --check
```

The checked-in generated Unit allows direct VM project builds with a bootstrap compiler. Keep it synchronized when updating the VM version.

## Local Validation and Packaging

```bash
python x.py doctor
python x.py dev --release
python x.py test --release --full
python x.py dist
```

`dist` builds and packages a host release bundle. There is no `x.py release` subcommand. Run platform-specific and LSP checks appropriate to the change in addition to the main suite.

Check the staged tools before packaging:

```bash
kinal --version --verbose
kinalvm --version
```

Use the executable in `out/stage/host-release/` explicitly when another installation is on `PATH`. Verify compiler version, VM version, packaged `VERSION`, generated VM metadata, and the bundle's `README.txt` agree. A compiler version reported correctly does not prove that an older VM beside it is compatible with newly emitted bytecode.

The distribution includes compiler/LSP/VM executables, LLVM runtime/development files, linker tools, locales, runtime objects/sources, standard packages, and documentation. Preserve the complete layout during relocation. Release standard packages are distributed as `.klib`; their source directories are removed from the release bundle. Packaging outputs live under `artifacts/release/`.

## GitHub Release Workflow

The `Release` workflow triggers on `v*` tags and supports manual dispatch. For a tag-triggered release, `v{VERSION:kinal}` must match the root compiler version exactly. For example, `kinal=0.9.0` corresponds to `v0.9.0`.

The current workflow builds Windows/Linux/macOS x64 and ARM64 bundles. Manual inputs control draft status, prerelease status, and generated release notes; draft defaults to true for manual runs. Inspect the workflow's matrix and uploaded artifacts rather than assuming a locally built host bundle covers every platform.

A VM-only change can update the `kinalvm` component value, but the current release workflow does not define a separate `vm/v*` publishing trigger. Do not document such a tag as an automated release path.

## Documentation, Feed, and Website

- The main repository's `Docs and Scripts Sync` workflow copies `docs/` and install scripts to configured website targets.
- `Published Release Sync` generates release-feed data and selects the compiler artifact used by the production site. Prerelease events are excluded from production compiler deployment.
- The website repository has its own deployment workflow for website code/assets.
- Updating local Markdown does not itself publish it. Publishing a release and deploying website code are separate actions.

Release notes should state language/API changes, runtime dependencies, migration steps, known limitations, and compiler/VM compatibility. Include both documentation languages when user-facing behavior changes. Do not present cross-target compilation as target-host runtime validation.

## Before Publishing

1. Confirm the source commit, component versions, and generated metadata.
2. Rebuild and run the relevant C compiler, runtime, VM, package, and editor checks.
3. Inspect the extracted bundle and compile/run a small program from that location.
4. Check that `.knc` files and packaged executables use a compatible VM.
5. Review the final documentation, release notes, artifacts, and tag before starting publication.

## See Also

- [Build Guide](build.md)
- [Testing Guide](testing.md)
- [KinalVM Compatibility](../docs/cli/vm.md)
