# Linking Configuration

This reference describes native linking in the C compiler. `--emit check`, `tokens`, `ast`, `ir`, `asm` and `obj` stop before final linking. KNC execution uses a separate [VM path](../cli/vm.md).

## Backend and Executable Selection

| Option | Meaning |
|--------|---------|
| `--linker lld` | LLD integration; default for Windows and freestanding |
| `--linker zig` | Zig driver; hosted Linux/macOS default, with a warning and LLD fallback when Zig is unavailable |
| `--linker msvc` | MSVC linker, Windows targets only |
| `--linker-path <file>` | Override the chosen tool executable |
| `--show-link` | Print the command actually executed |
| `--show-link-search` | Print resolved library search directories |

Selecting a backend does not install it. The complete distribution carries tools and runtime assets; source builds additionally need the target SDK/libraries. Prefer an absolute `--linker-path` when invoking from different directories.

```bash
kinal build main.kn -o app --show-link --show-link-search
kinal build main.kn -o app --linker zig
kinal build main.kn -o app --linker lld --linker-path /usr/local/bin/ld.lld
```

Tool syntax depends on the target: Windows LLD uses the COFF linker interface, while Linux uses ELF conventions. Copy flags from the actual tool's interface rather than assuming all backends accept the same flags.

## Libraries, Objects and Search Roots

| Option | Meaning |
|--------|---------|
| `--link-file <file>` | Pass a native object/archive/library input |
| `-L <dir>` / `--lib-dir <dir>` | Add a library search directory |
| `-l <name>` / `--lib <name>` | Request a named library |
| `--link-root <dir>` | Add existing `lib/<triple>`, `lib/<platform-alias>`, `lib`, and root directories as search directories |
| `--link-arg <arg>` | Add one linker argument; repeat for multiple arguments |

`--link-root` does not recursively link every object it finds. Select the required library with `-l` or the required file with `--link-file`.

```bash
kinal build main.kn --link-file build/helper.o -L vendor/lib -l helper -o app
kinal build main.kn --link-root vendor -l helper -o app
```

These examples assume compatible files already exist. Match architecture, object format, calling convention and CRT. On POSIX targets, a C math dependency may additionally need `-l m`.

A `.klib` stores Kinal source and native assets with a package manifest. Configure `--pkg-root packages` or project package roots; do not pass `.klib` to `--link-file`. Imported package FFI metadata can select native assets and system libraries. See [Packages](../cli/packages.md).

## Output Kinds

| Kind | Typical output | Notes |
|------|----------------|-------|
| `--kind exe` | `.exe` or an extensionless program | Default; hosted entry is `Static Function int Main(...)` |
| `--kind static` | `.lib` / `.a` | Archive of native objects; consumers still need compatible dependencies |
| `--kind shared` | `.dll` / `.so` / `.dylib` | Dynamic library; ABI/export policy remains relevant |

```bash
kinal build library.kn --kind static -o library.lib
kinal build library.kn --kind shared -o library.dll
```

Choose target-appropriate filenames on other platforms. A filename extension alone does not select the artifact kind. For exported C functions, use the supported `[Cdecl]` / `[SymbolName(...)]` declarations described in [FFI](../language/ffi.md). A Kinal `string`, managed object or array descriptor is not automatically a portable C data layout.

## LTO

`--lto` means full LTO. Explicit modes are `--lto=off` (default), `--lto=full`, and `--lto=thin`.

```bash
kinal build main.kn --lto=thin -o app
```

LTO requires compatible compiler/linker bitcode support. Native machine-code libraries remain machine-code inputs; LTO does not reconstruct their source or guarantee a performance improvement. Compare the resulting size and workload performance. Keep the matching LLVM tools with the compiler bundle when diagnosing bitcode-version errors.

## Hosted CRT

`--crt auto|dynamic|static` selects the supported hosted CRT policy, especially the Windows CRT library family. It is not a filename option. Supply custom objects using `--link-file`.

`--no-default-libs` suppresses automatic default libraries. An application using console, allocation, GC or other host services must then provide their dependencies explicitly. `--crt static` does not make all external libraries or the whole program static. Selecting Zig alone does not promise a static musl executable.

## Freestanding Linking

`--no-crt` and `--link-script` are freestanding-only C driver options. Hosted builds reject them. Supply a matching startup environment, linker script and runtime hooks:

```bash
kinal build kernel.kn --target bare-arm64 \
    --freestanding --runtime none --panic trap \
    --no-crt --link-script board.ld -o kernel.elf --show-link
```

This command assumes `kernel.kn` and `board.ld` already exist. The default language entry is `KMain`; the compiler emits `__kn_entry`. Set up the stack, sections and boot protocol before calling it. See [Freestanding](freestanding.md) for complete source and runtime requirements.

## Diagnose Failures

| Symptom | Check |
|---------|-------|
| Tool not found | Full bundle layout, executable path and selected backend |
| Undefined symbol | Actual link command, correct `-l` / file, symbol spelling, exports, ABI and transitive dependencies |
| Incompatible object | Target architecture, ELF/COFF/Mach-O format and bitcode/tool versions |
| Missing CRT/SDK | Target runtime assets and platform SDK; target selection does not install an SDK |
| Program cannot load a library | Runtime search paths and deployment of shared dependencies, separate from link-time search |
| `.klib` rejected | Use package discovery rather than native linking |

Use `--keep-temps` to retain intermediates. First verify `--emit obj`; then inspect `--show-link` to separate code generation from linking. See [Cross-Compilation](cross-compilation.md) and [Project Files](../cli/project-files.md) for target and path rules.
