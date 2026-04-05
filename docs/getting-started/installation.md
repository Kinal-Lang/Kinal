# Installation Guide

This document explains how to install the Kinal compiler toolchain on various platforms.

## System Requirements

| Platform | Requirements |
|----------|-------------|
| Windows | Windows 10/11, x86-64 |
| Linux | glibc 2.17+, x86-64 or AArch64 |
| macOS | macOS 11+, x86-64 or Apple Silicon |

> The Kinal compiler itself has no runtime dependencies; the generated binaries run standalone on the above systems.

## Installing the Compiler

### Install from a Pre-built Package

Download the `kinal` executable for your platform from the Releases page and place it in your `PATH`:

```bash
# Linux / macOS
chmod +x kinal
sudo mv kinal /usr/local/bin/

# Windows: place kinal.exe in any directory on PATH
```

### Verify the Installation

```bash
kinal --help
```

If the help text appears, the installation was successful.

## Installing KinalVM (Optional)

KinalVM is Kinal's bytecode virtual machine, used for running `.knc` files or executing `.kn` source files via `kinal vm run` (which first compiles to a temporary `.knc` and then runs it).

The `kinalvm` executable is typically distributed alongside the compiler. Place it in the same directory or add it to `PATH`:

```bash
# Verify
kinalvm --help
```

## Directory Layout (Typical Installation)

```
kinal/
├── kinal          # Main compiler
├── kinalvm        # Bytecode virtual machine
└── locales/       # Diagnostic message locale files
    └── zh-CN.json
```

## Diagnostic Language

Kinal supports Chinese diagnostic messages:

```bash
kinal --lang zh hello.kn
```

When a `zh-CN.json` locale file is present, `--lang zh` will automatically load it.

## Developer Tooling

### VS Code

Install the **Kinal Language Server** extension (if available) for syntax highlighting, code completion, and live diagnostics.

The LSP server is located at `apps/kinal-lsp/` and can be built locally and configured in your editor.

## Building from Source

To build Kinal from source:

**Dependencies:**
- CMake 3.20+
- LLVM 14+ (`llvm-config` must be on PATH)
- A C compiler (MSVC / GCC / Clang)
- Python 3 (for `infra/` build scripts)

```bash
# Configure
cmake --preset <platform>

# Build
cmake --build build/ --target kinal kinalvm
```

See `CMakePresets.json` for available preset names.

## Next Steps

- [Hello World](hello-world.md) — Write and run your first Kinal program
- [Project Structure](project-structure.md) — Learn how to organize source files
