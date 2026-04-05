# Build Guide

## Prerequisites

- Python 3.10+
- CMake 3.20+
- Ninja
- LLVM/Clang toolchain (obtain via `python x.py fetch llvm-prebuilt`)
- Zig (optional, for cross-compilation; obtain via `python x.py fetch zig-prebuilt`)

## Common Commands

```bash
# Check environment
python x.py doctor

# Development build (Debug)
python x.py dev

# Release build
python x.py release

# Run tests
python x.py test

# CMake configuration only
python x.py configure
```

## Build Outputs

| Path | Description |
|------|-------------|
| `out/build/host-debug/` | CMake build directory |
| `out/stage/host-debug/` | Staged artifacts (compiler + VM + runtime) |
| `artifacts/release/` | Release packages |

## Version Numbers

Version numbers are controlled by the `VERSION` and `apps/kinalvm/VERSION` files. CMake reads them during the configuration phase and generates `apps/kinal/include/kn/version.h`.

See [releasing.md](releasing.md) for details.
