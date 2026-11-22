# Build Guide

## Prerequisites

- Python 3.10+
- CMake 3.20+
- Ninja
- LLVM/Clang toolchain (obtain via `python x.py fetch llvm-prebuilt`)
- Zig (optional, for cross-compilation; obtain via `python x.py fetch zig-prebuilt`)
- OpenSSL command-line tool on `PATH` for HTTPS fixture tests (OpenSSL 1.1.1/3.x command syntax)

HTTPS tests generate a fresh one-day localhost certificate and private key at
runtime. Generated files stay in a private temporary directory and are removed
before the fixture serves requests. Missing or failing OpenSSL is a test failure,
not a skipped test. No certificate is added to the system trust store.
Run the compiler-independent fixture checks with
`python -m unittest discover -s tests -p test_request_https_fixture.py`.
Their Python TLS client verifies trust and both localhost/127.0.0.1 identities;
this does not change the current `IO.Request` peer-verification limitation
documented in [IO.Request](../docs/stdlib/request.md).

On Linux, the LLVM bootstrap builds `libLLVM.so` from the official archive's
static components when necessary. This requires a host C++ development toolchain
and LLVM's system libraries (such as zlib, zstd, and libxml2). The shared runtime
is built atomically and a failed link is reported before the toolchain is marked
ready. A complete system installation can also be selected with `LLVM_DIR`.

## Common Commands

```bash
# Check environment
python x.py doctor

# Development build (Debug)
python x.py dev

# Release build
python x.py dev --release

# Run tests
python x.py test

# Build a release distribution
python x.py dist
```

## Build Outputs

| Path | Description |
|------|-------------|
| `out/build/host-debug/` | CMake build directory |
| `out/stage/host-debug/` | Staged artifacts (compiler + VM + runtime) |
| `artifacts/release/` | Release packages |

## Version Numbers

Version numbers are controlled by the root `VERSION` file. CMake reads it during configuration and generates `generated/kn/version.h` in the build directory.

KinalVM embeds the `kinalvm` component version through the generated
`apps/kinalvm/src/IO/Kinal/VM/BuildInfo.kn` Unit. It is checked in so a clean
checkout supports direct source/project builds with a bootstrap compiler. After
editing `VERSION`, regenerate it with `python -m infra.scripts.x.vm_metadata`;
`python -m infra.scripts.x.vm_metadata --check` verifies synchronization. The C
and selfhost VM build helpers also refresh it automatically. Distribution bundles
include the matching canonical `VERSION` file.

See [releasing.md](releasing.md) for details.

## Selecting and Checking a C Toolchain

The commands in this guide build the C compiler, LSP and C-built VM; selfhost bootstrap is separate. The current hosted workflows select LLVM 21. When choosing a system LLVM, point LLVM_DIR at its CMake package directory and ensure the matching clang, libraries and target dependencies are available. Do not silently mix llvm-config/clang from another installed major version.

```bash
python x.py status
python x.py status --json
python -m infra.scripts.x.vm_metadata --check
```

`out/stage/host-release/` is the Release counterpart of the Debug staging directory. Use that exact compiler path during verification when PATH contains another installation. `python x.py test --release --build` rebuilds before validation; independent Python VM fixtures still need a matching KINAL_VM_PATH as explained in [Testing](testing.md).

For native cross-compilation, a target triple is not a complete SDK: target CRT/system libraries, compatible package assets, startup files and a linker remain required. Preserve the staged bundle layout for LLVM, linker, runtime and stdpkg discovery. `python x.py dist` packages the host release; there is no x.py release command.
