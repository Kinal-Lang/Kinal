# Installation Guide

This document explains how to install the Kinal compiler toolchain on various platforms.

## System Requirements

| Platform | Architecture |
|------|------|
| Windows 10/11 | x86-64 / ARM64 |
| Linux (glibc 2.17+) | x86-64 / ARM64 |
| macOS 11+ | x86-64 / Apple Silicon |

> The Kinal compiler itself does not depend on any runtime; the generated binaries can run independently on the systems listed above.

## Installing the Compiler

### Install with the Official Script

For macOS and Linux, the recommended way is the official install script:

```bash
curl -fsSL https://kinal.org/install.sh | bash
```

Install a specific version:

```bash
curl -fsSL https://kinal.org/install.sh | bash -s -- --version v0.6.0
```

By default, the script installs the toolchain under `~/.local/share/kinal` and writes launcher scripts to `~/.local/bin`.

If `~/.local/bin` is not already on your `PATH`, add it:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

### Installing from a Pre-compiled Package

If you prefer a manual install, download the compressed package for your platform from the Release page and extract it:

```bash
# Linux / macOS
tar -xf kinal-*.tar.gz
cd kinal
chmod +x kinal
sudo mv kinal /usr/local/bin/

# Windows
# After extracting the zip file, add the entire kinal directory to PATH
```

### Verify Installation

```bash
kinal --help
```

If you see the help message, the installation was successful.

## Installing KinalVM (Optional)

KinalVM is Kinal’s bytecode virtual machine, used to run `.knc` files, or to compile `.kn` source files into temporary `.knc` files and execute them via `kinal vm run`.

The `kinalvm` executable is typically distributed alongside the compiler. Place it in the same directory or add it to your `PATH`:

```bash
# Verify
kinalvm --help
```

## Directory Structure (Typical Installation)

```
kinal/
├── kinal          # Main compiler
├── kinalvm        # Bytecode virtual machine
└── locales/       # Localization files for diagnostic messages
    └── zh-CN.json
```

## Diagnostic Language Settings

Kinal supports Chinese diagnostic messages:

```bash
kinal --lang zh hello.kn
```

Alternatively, if the `zh-CN.json` file exists, `--lang zh` will automatically load the Chinese locale file.

## Development Tool Support

### VS Code

Install the **Kinal Language Server** extension (if available) to enable syntax highlighting, code completion, and real-time diagnostics.

The LSP server is located in `apps/kinal-lsp/`; build it locally and configure it within your editor.

## Building from Source

To build Kinal from source:

**Dependencies:**
- CMake 3.20+
- LLVM 14+ (requires `llvm-config` in PATH)
- C compiler (MSVC / GCC / Clang)
- Python 3 (for `infra/` build scripts)

```bash
# Build via command line
python x.py [dev|dist]

# Or use the build GUI to quickly set up the environment and build
python x.py gui
```

See `CMakePresets.json` for specific preset names.

## Next Steps

- [Hello World](hello-world.md) — Write and run your first Kinal program
- [Project Structure](project-structure.md) — Learn how to organize source files
