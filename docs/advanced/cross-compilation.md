# Cross-Compilation

Kinal is built on the LLVM backend and natively supports cross-compilation — compiling executables for another platform on one machine.

---

## Core Options

```bash
kinal --target <triple> [other options] <file.kn> -o <output>
```

---

## Built-in Platform Aliases

Kinal provides a set of platform shorthand aliases so you don't need to memorize full LLVM triples:

| Alias | Equivalent Triple | Description |
|-------|------------------|-------------|
| `win86` | `i686-windows-msvc` | Windows 32-bit |
| `win64` | `x86_64-windows-msvc` | Windows 64-bit |
| `linux64` | `x86_64-linux-gnu` | Linux x86_64 |
| `linux-arm64` | `aarch64-linux-gnu` | Linux ARM64 |
| `bare64` | `x86_64-unknown-none` | Bare-metal x86_64 |
| `bare-arm64` | `aarch64-unknown-none` | Bare-metal ARM64 |

---

## Common Cross-Compilation Scenarios

### Compiling a Windows Program on Linux

```bash
kinal --target win64 main.kn -o main.exe --linker zig
```

> Using `--linker zig` is recommended for Windows cross-compilation, as the zig toolchain includes C runtime headers for Windows targets.

### Compiling a Linux Program on Windows

```bash
kinal --target linux64 main.kn -o main --linker zig
```

### Compiling for ARM64 Linux

```bash
kinal --target linux-arm64 main.kn -o main
```

---

## Linker Selection

| Linker | Option | Use Case |
|--------|--------|----------|
| `lld` | `--linker lld` (default) | Native compilation, fastest |
| `zig` | `--linker zig` | Cross-compilation, includes cross toolchain |
| `msvc` | `--linker msvc` | Windows native, requires MSVC installation |

The zig toolchain (`zig cc`) is the most recommended cross-compilation option because it has built-in sysroots for multiple platforms, eliminating the need to install separate cross-compilation toolchains:

```bash
# Install zig (used as clang frontend + cross linker)
# Then use directly:
kinal --target linux-arm64 main.kn -o myapp --linker zig
```

---

## Sysroot

When compilation requires system headers and libraries, you may need to specify a sysroot:

```bash
kinal --target linux-arm64 \
      -L /path/to/arm64-sysroot/lib \
      main.kn -o myapp
```

---

## Static Linking (Recommended for Release)

Static linking is recommended for cross-compilation to avoid dynamic library version issues on the target platform:

```bash
kinal --target linux64 \
      --linker zig \
      -o myapp-linux \
      main.kn
```

---

## Build Matrix Example

A script to build for multiple platforms simultaneously:

```bash
#!/bin/bash
TARGETS=(
    "win64:main-windows.exe"
    "linux64:main-linux"
    "linux-arm64:main-linux-arm64"
)

for entry in "${TARGETS[@]}"; do
    target="${entry%%:*}"
    output="${entry##*:}"
    echo "Compiling $target -> $output ..."
    kinal --target "$target" --linker zig main.kn -o "dist/$output"
done
```

---

## Notes

### Platform-Specific Code

Use `IO.Target.OS` conditions to handle platform differences:
```kinal
Static Function string GetConfigPath()
{
    If (IO.Target.OS == IO.Target.OS.Windows)
    {
        Return IO.Path.Combine(IO.System.GetEnv("APPDATA"), "myapp");
    }
    Return IO.Path.Combine(IO.System.GetEnv("HOME"), ".config/myapp");
}

```

### Availability of C Standard Library Functions

When cross-compiling, C functions called via `Extern` FFI must exist on the target platform. When using `--linker zig`, zig is responsible for providing the C runtime for the target platform.

---

## See Also

- [Compiler CLI](../cli/compiler.md) — `--target`, `--linker` options
- [Linking](linking.md) — Linker backend details
- [Freestanding Development](freestanding.md) — No-OS targets
- [FFI](../language/ffi.md) — Platform-specific C interfaces
