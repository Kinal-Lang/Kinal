# Releasing Kinal

The public SDK ships the stage3 compiler under the name `kinal`. The last C feature release is v0.10.0. Its maintenance branch, `maintenance/c-0.10`, retains the C compiler and its manually dispatched CI and release workflows.

## Prepare the version

Set the compiler and VM component versions independently in `VERSION`. Regenerate both checked-in metadata units and add release notes in `dev/releases/<compiler-version>.md`.

```sh
python -m infra.scripts.x.compiler_metadata
python -m infra.scripts.x.vm_metadata
```

The KNC wire revision is independent of both components. Revision 4 embeds the producing compiler version.

## Validate the SDK

Push the release commit to the default branch. **CI** runs on Windows, Linux and macOS on x64 and Arm64. Every host verifies the checksum of its pinned C seed, bootstraps through stage3 and runs the stage3 regression suite. It extracts the public ZIP, checks that its compiler matches the converged binary, and builds and executes Native and KNC programs with the extracted SDK. Pull requests run the same checks. Default-branch pushes also upload the release archives and per-host checksums.

To reproduce convergence locally, pass an extracted v0.10.0 SDK to `python x.py selfhost-bootstrap --stage0-bundle <sdk>`. To run the suite, use `tests/selfhost/run_selfhost_tests.py` with the stage3 compiler and the frozen stage0. The Linux and macOS linker wrapper uses LLVM 21; `KINAL_CLANG` can select its executable explicitly. On macOS, freestanding ELF builds run the linker from the Homebrew lld@21 keg.

## Create a GitHub release

After all six CI jobs pass, dispatch **Release** on the same commit. Optionally supply the CI run ID. A matching `v<compiler-version>` tag also triggers draft preparation. The workflow verifies the default branch, commit, workflow, six successful jobs and all archive checksums before assembling the release. It reuses the tested SDKs. Manual dispatch defaults to a draft; explicitly select `publish` to publish after uploading the assets.

GitHub Release publication and website deployment have separate workflows. The installer and production download feed should continue to point to the latest public release while a newer draft is being reviewed.
