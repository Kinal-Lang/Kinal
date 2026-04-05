# Linking Configuration

This document covers Kinal's linker integration, including linker backend selection, library dependencies, LTO, and static/shared library builds.

---

## Linker Backends

Kinal supports three linker backends:

| Backend | Option | Description |
|---------|--------|-------------|
| **lld** | `--linker lld` (default) | LLVM built-in linker, fastest, cross-platform |
| **zig** | `--linker zig` | Uses zig toolchain, best cross-compilation support |
| **msvc** | `--linker msvc` | Windows MSVC linker, requires Visual Studio |

```bash
# Default (lld)
kinal build main.kn -o app

# zig linker (recommended for cross-compilation)
kinal build main.kn -o app --linker zig

# Specify linker path
kinal build main.kn -o app --linker-path /usr/local/bin/ld.lld
```

---

## Library Dependencies

### Linking System Libraries

```bash
# Link pthread and math libraries
kinal build main.kn -o app -l pthread -l m

# Add library search directories
kinal build main.kn -o app -L /usr/local/lib -l mylib
```

### Linking Object Files

```bash
# Link pre-compiled object files
kinal build main.kn -o app --link-file mylib.o --link-file utils.o

# Specify link root directory (searches for .o and .a files)
kinal build main.kn -o app --link-root ./build/lib
```

### Linking `.klib`

```bash
kinal build main.kn -o app --link-file mylib.klib
```

---

## Output Types

| Type | Option | Description |
|------|--------|-------------|
| Executable | `--kind exe` (default) | System executable |
| Static library | `--kind static` | `.a` / `.lib` static library |
| Shared library | `--kind shared` | `.so` / `.dll` shared library |

```bash
# Static library
kinal build mylib.kn --kind static -o libmylib.a

# Shared library
kinal build mylib.kn --kind shared -o libmylib.so
# Windows:
kinal build mylib.kn --kind shared -o mylib.dll
```

---

## Link-Time Optimization (LTO)

LTO allows the linker to perform optimizations across compilation units, typically significantly reducing binary size and improving runtime performance:

```bash
kinal build main.kn -o app --lto
```

LTO will:
- Perform whole-program analysis across all object files
- Remove unused functions and data (dead code elimination)
- Inline cross-module function calls
- Optimize cross-module references more aggressively

> LTO increases compilation time; it is recommended to enable it only for release builds.

---

## C Runtime (CRT)

### Default Behavior

By default, Kinal links the platform's standard CRT:
- **Linux**: `crt0.o`, `crti.o`, `crtn.o` (from glibc/musl)
- **Windows**: MSVC CRT or MinGW CRT

### Disabling CRT

```bash
# Do not link CRT (bare-metal/embedded)
kinal build main.kn -o app --no-crt

# Do not link default libraries
kinal build main.kn -o app --no-default-libs

# Specify custom CRT
kinal build main.kn -o app --crt mycrt.o
```

---

## Linker Scripts

Embedded targets typically require custom memory layouts:

```bash
kinal build main.kn -o firmware.elf \
      --target bare-arm64 \
      --link-script memory_layout.ld \
      --no-crt
```

---

## Extra Linker Arguments

Use `--link-arg` to pass arbitrary arguments to the linker:

```bash
# Set entry point
kinal build main.kn -o app --link-arg -e --link-arg _start

# Add rpath
kinal build main.kn -o app --link-arg -rpath --link-arg /usr/local/lib
```

---

## Debugging the Link Step

View the actual link command being executed:

```bash
kinal build main.kn -o app --show-link
```

Example output:

```
link: ld.lld -o app main.o -lc -lm --dynamic-linker /lib64/ld-linux-x86-64.so.2
```

---

## Typical Build Configurations

### Development Build

```bash
kinal build main.kn -o app
```

### Release Build (with LTO)

```bash
kinal build main.kn -o app --lto
```

### Statically Linked Release (no external dependencies)

```bash
kinal build main.kn -o app \
      --linker zig \
      --lto
```

> The zig linker automatically performs static musl libc linking (Linux), producing a fully static executable.

### Embedded Firmware

```bash
kinal build main.kn \
      --target bare-arm64 \
      --freestanding \
      --runtime none \
      --panic trap \
      --no-crt \
      --link-script board.ld \
      -o firmware.elf
```
