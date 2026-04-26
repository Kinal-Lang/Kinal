# kinal CLI Overview

`kinal` is the unified command-line tool for the Kinal language. All operations are distinguished by **subcommands**; each subcommand has its own independent set of options.

## Subcommand Reference

| Subcommand | Purpose |
|------------|---------|
| [`build`](#kinal-build) | Compile source code to native executables, object files, assembly, or IR |
| [`run`](#kinal-run) | Compile a source file to a temporary native executable and run it immediately |
| [`fmt`](fmt.md) | Format `.kn` source files |
| [`vm`](vm.md) | Bytecode operations (build `.knc`, run, pack vm-exe) |
| [`pkg`](packages.md) | Package management (build `.klib`, view info, unpack) |

```
kinal [options] <subcommand> [options] [args...]
kinal build [options] <input file> [file2 ...]
kinal run [options] <file.kn> [-- program-args...]
```

---

## Global Options

### Version

```bash
kinal --version            # Show compiler version
kinal -V                   # Same (short form)
kinal --version --verbose  # Also show KinalVM version
```

### Diagnostic Language and Localization

`--lang` and `--locale-file` can be placed **before** (global) or **after** (local) a subcommand with the same effect:

```bash
kinal --lang zh build main.kn    # Global
kinal build --lang zh main.kn    # Equivalent: local
```

| Option | Default | Description |
|--------|---------|-------------|
| `--lang <en\|zh>` | `en` | Diagnostic output language |
| `--locale-file <file>` | — | Custom locale JSON (overrides the built-in language pack) |

> **Tip:** When `--lang zh` is specified, the tool automatically looks for `locales/zh-CN.json` in the same directory as the compiler.  Usually no manual `--locale-file` is needed.

---

## Quick Reference

```bash
# Compile to executable
kinal build main.kn -o main

# Compile to object file
kinal build main.kn --emit obj -o main.o

# Output LLVM IR
kinal build main.kn --emit ir

# Quick run (native compile, auto-cleanup after execution)
kinal run main.kn

# Build bytecode
kinal vm build main.kn -o main.knc

# Build from a project file
kinal build --project .

# Run via VM
kinal vm run main.kn

# Format code
kinal fmt main.kn

# View .klib info
kinal pkg info mylib.klib
```

---

## kinal build

Compile `.kn` source files to native outputs.

```
kinal build [options] <file.kn> [file2.kn ...]
```

### Output Control

| Option | Description |
|--------|-------------|
| `-o, --output <file>` | Specify the output file path |
| `--emit <mode>` | Output format (see table below), default `bin` |
| `--kind <type>` | Artifact type: `exe` (default), `static`, `shared` |

#### `--emit` Modes

| Mode | Description |
|------|-------------|
| `bin` | Link to an executable (default) |
| `obj` | Generate an object file (`.o` / `.obj`) |
| `asm` | Generate assembly code |
| `ir` | Generate LLVM IR (`.ll`) |

### Target Platform

| Option | Description |
|--------|-------------|
| `--target <triple>` | Target platform triple (e.g. `x86_64-linux-gnu`) |
| `--freestanding` | Bare-metal/embedded mode |
| `--env <env>` | Execution environment: `hosted` (default) or `freestanding` |
| `--runtime <type>` | Runtime: `none`, `alloc`, `gc` |
| `--panic <strategy>` | Panic strategy: `trap`, `loop` |
| `--entry <symbol>` | Override entry symbol (default `Main`/`KMain`) |

#### Built-in Platform Aliases

| Alias | Equivalent Triple |
|-------|------------------|
| `win86` | `i686-windows-msvc` |
| `win64` | `x86_64-windows-msvc` |
| `linux64` | `x86_64-linux-gnu` |
| `linux-arm64` | `aarch64-linux-gnu` |
| `bare64` | `x86_64-unknown-none` |
| `bare-arm64` | `aarch64-unknown-none` |

### Linking Options

| Option | Description |
|--------|-------------|
| `-L, --lib-dir <dir>` | Add a library search directory |
| `-l, --lib <name>` | Link the specified library |
| `--link-file <file>` | Link a specific object file |
| `--link-root <dir>` | Add a link root directory |
| `--link-arg <arg>` | Pass extra arguments to the linker |
| `--link-script <script>` | Specify a linker script (`.ld`) |
| `--linker <backend>` | Linker backend: `lld` (default), `zig`, `msvc` |
| `--linker-path <path>` | Specify linker executable path |
| `--lto[=off\|full\|thin]` | Link-time optimization (default `off`) |
| `--crt <auto\|dynamic\|static>` | CRT selection (default `auto`) |
| `--no-crt` | Do not link the C runtime |
| `--no-default-libs` | Do not link default libraries |
| `--show-link` | Display the link command |
| `--show-link-search` | Display library search paths |

### Project and Packages

| Option | Description |
|--------|-------------|
| `--project <file\|dir>` | Build from `kinal.knproj` or a legacy project manifest |
| `--profile <name>` | Select a profile from `kinal.knproj` |
| `--pkg-root <dir>` | Add a package search root directory |
| `--stdpkg-root <dir>` | Override the standard package root directory |

If `--project` points to a directory, `kinal` looks for `kinal.knproj` first. If `--profile` is omitted, the project file uses `DefaultProfile`.

### Frontend Options

| Option | Description |
|--------|-------------|
| `--no-module-discovery` | Disable automatic module discovery |
| `--keep-temps` | Retain intermediate files |

### Diagnostics

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help |
| `--color <auto\|always\|never>` | Color output mode |
| `--lang <en\|zh>` | Diagnostic language |
| `--locale-file <file>` | Custom locale file |
| `--Werror` | Treat warnings as errors |
| `--warn-level <level>` | Warning level (`0` disables warnings) |

### Developer Options

| Option | Description |
|--------|-------------|
| `--trace` | Print compile phase trace |
| `--time` | Print compile phase timing |
| `--dump-locale-en <file>` | Export English locale template JSON (for translation use) |

---

## Input File Types

`kinal build` accepts multiple input formats, allowing entry at different stages of the compilation pipeline:

| Extension | Description |
|-----------|-------------|
| `.kn` / `.kinal` / `.fx` | Full frontend compilation |
| `.ll` / `.s` / `.asm` | Continue compilation from an intermediate representation |
| `.o` / `.obj` | Continue from object file and link |
| `.knc` | Bytecode; use `kinal vm run` or `kinal vm pack` |

---

## kinal run

Compile a source file to a native executable and run it immediately; the temporary file is deleted automatically after execution. Ideal for rapid iteration — similar to `go run`.

```
kinal run [options] <file.kn> [-- program-args...]
```

| Option | Description |
|--------|-------------|
| `--no-module-discovery` | Disable source file/module auto-discovery |
| `--project <file\|dir>` | Run from `kinal.knproj` or a legacy project manifest |
| `--profile <name>` | Select a profile from `kinal.knproj` |
| `--pkg-root <dir>` | Add a local package root directory |
| `--stdpkg-root <dir>` | Override the official stdpkg root directory |
| `--keep-temps` | Retain the temporary executable after execution |
| `-L, --lib-dir <dir>` | Add a library search directory |
| `-l, --lib <name>` | Link the specified library |
| `--link-file <file>` | Link a specific object/archive file |
| `--link-root <dir>` | Add a link root directory |
| `--link-arg <arg>` | Pass extra arguments to the linker |
| `--lang <en\|zh>` | Diagnostic language |
| `--locale-file <file>` | Custom locale file |
| `--color <auto\|always\|never>` | Diagnostic color mode |
| `--Werror` | Treat warnings as errors |
| `--warn-level <n>` | Warning level (0 disables warnings) |

`kinal run` compiles the source file to a temporary native executable via the LLVM backend and cleans it up after execution. All arguments after `--` are passed as-is to the executed program.

> To run `.knc` bytecode, use `kinal vm run` instead.

```bash
# Quick run
kinal run main.kn

# Run from the default project profile
kinal run --project .

# Pass program arguments
kinal run main.kn -- arg1 arg2

# Keep the temporary executable for debugging
kinal run --keep-temps main.kn
```

---

## Common Workflows

### Development and Debugging

```bash
# Quick compile and run (native, similar to go run)
kinal run main.kn

# View LLVM IR
kinal build main.kn --emit ir --keep-temps
```

### Production Builds

```bash
# Optimized build with LTO
kinal build main.kn -o app --lto

# Static library
kinal build mylib.kn --kind static -o libmylib.a

# Shared library
kinal build mylib.kn --kind shared -o libmylib.so
```

### Cross-Compilation

```bash
# Compile a Windows executable from Linux
kinal build --target win64 main.kn -o main.exe --linker zig

# Embedded bare-metal target
kinal build --target bare-arm64 \
      --freestanding \
      --runtime none \
      --panic trap \
      main.kn -o firmware.elf
```

### Chinese Diagnostics

```bash
kinal build --lang zh main.kn
```

---

## VM / Package Management Quick Reference

```bash
# Bytecode operations
kinal vm build main.kn -o main.knc
kinal vm build --project . --profile vm
kinal vm run main.kn                       # or main.knc
kinal vm pack main.kn -o myapp             # Standalone executable

# Package management
kinal pkg build --manifest mylib/ -o mylib.klib
kinal pkg info mylib.klib
kinal pkg unpack mylib.klib -o output_dir/
```

> See [kinal vm](vm.md) and [kinal pkg](packages.md) for detailed options.

---

## See Also

- [kinal fmt](fmt.md) — Code formatting
- [kinal vm](vm.md) — Bytecode VM operations
- [kinal pkg](packages.md) — Package management
- [Cross-Compilation](../advanced/cross-compilation.md) — Cross-platform build details
- [Linking](../advanced/linking.md) — Linker configuration
- [Freestanding](../advanced/freestanding.md) — Embedded/OS-less environments
- [CLI Specification](cli-spec.md) — CLI design specification (developer reference)
