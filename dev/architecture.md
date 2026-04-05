# Project Structure

```
Kinal/
├── VERSION                  # Kinal version number (single source of truth)
├── CMakeLists.txt           # Root CMake configuration
├── x.py                     # Build entry script
├── apps/
│   ├── kinal/               # Kinal compiler (C)
│   │   ├── include/kn/      # Public headers (including version.h)
│   │   └── src/             # Compiler source code
│   ├── kinal-lsp/           # LSP language server (C++)
│   │   └── server/src/
│   └── kinalvm/             # KinalVM virtual machine (written in Kinal)
│       ├── VERSION           # VM version number
│       └── src/
├── libs/
│   ├── runtime/             # Runtime library (C)
│   ├── selfhost/            # Self-hosting support
│   └── std/                 # Standard library packages (stdpkg)
├── infra/
│   ├── assets/              # Asset files (locales, etc.)
│   ├── cmake/               # CMake modules
│   ├── scripts/             # Python build scripts
│   └── toolchains/          # CMake toolchain files
├── tests/                   # Compiler test cases
├── docs/                    # User documentation
└── dev/                     # Developer specification documents (this directory)
```

## Core Components

- **Kinal Compiler** (`apps/kinal/`): Implemented in C, uses LLVM backend to generate native code or VM bytecode
- **KinalVM** (`apps/kinalvm/`): A bytecode virtual machine written in Kinal itself
- **LSP Server** (`apps/kinal-lsp/`): C++ implementation providing IDE language support
- **Runtime** (`libs/runtime/`): Target platform runtime library (C)
- **Standard Library** (`libs/std/`): Standard library packages distributed as stdpkg
