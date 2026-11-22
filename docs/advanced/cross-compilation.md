# Cross-Compilation

Native cross-compilation emits code for another OS/architecture. Selecting a triple is one part of a complete target toolchain; it does not automatically provide a target SDK, every runtime library, or host execution of the result.

## Select Target and Output Stage

```bash
kinal build main.kn --target linux-arm64 --emit ir -o out/main-aarch64.ll
kinal build main.kn --target linux-arm64 --emit obj -o out/main-aarch64.o
```

IR/object emission is useful before target linking is configured. To create a runnable executable, additionally provide a compatible linker, target startup/CRT/system libraries and package native assets. The command begins with `kinal build`; placing build options directly after kinal is not the documented subcommand interface.

## Main Target Aliases

| Alias | Triple |
|-------|--------|
| `win86` | `i686-pc-windows-msvc` |
| `win64` | `x86_64-pc-windows-msvc` |
| `win-arm64` | `aarch64-pc-windows-msvc` |
| `linux64` | `x86_64-pc-linux-gnu` |
| `linux-arm64` | `aarch64-unknown-linux-gnu` |
| `mac64` | `x86_64-apple-darwin` |
| `macos-arm64` | `aarch64-apple-darwin` |
| `bare64` | `x86_64-unknown-none-elf` |
| `bare-arm64` | `aarch64-unknown-none-elf` |

A supported full triple can also be passed. Aliases normalize platform spelling; they do not change available target libraries. Artifact suffixes and ABI follow the target, even when compiling on another host.

## Linking a Foreign Target

```bash
kinal build main.kn --target linux-arm64 --linker zig -o out/app-linux-arm64
kinal build main.kn --target win64 --linker lld -o out/app-windows.exe
```

These commands require the respective target dependencies. Zig can supply parts of a cross toolchain, but cannot guarantee every vendor SDK, Windows MSVC library, Apple SDK or third-party native dependency. LLD/MSVC also require suitable target inputs. Hosted Linux/macOS defaults prefer an available Zig and otherwise warn/fall back to LLD; explicitly selecting a backend overrides that default.

Kinal currently has no general `--sysroot` CLI option. Supply target library search paths with `-L`/project Link fields and use backend-appropriate `--link-arg` options when required. Do not assume `-L` alone supplies headers, startup files or a complete SDK. Foreign FFI libraries must match architecture and ABI, not only a filename extension.

## Target, Host and Runtime Constants

| Constant family | Meaning |
|-----------------|---------|
| `IO.Target.OS`, `.Arch`, `.Env` | Compiled artifact's OS, architecture and hosted/freestanding environment |
| `IO.Host.OS`, `.Arch`, `.Env` | Compiler host's corresponding properties |
| `IO.Target.PointerBits` / `IO.Host.PointerBits` | Pointer width of the relevant machine |
| `.PathSeparator`, `.NewLine`, `.ExeSuffix`, `.ObjectSuffix`, `.DynamicLibrarySuffix`, `.StaticLibrarySuffix` | Platform spelling for the chosen family |
| `IO.Runtime.*` | Native versus VM output backend, independent of the above environment/profile |

Target OS constants include Windows/Linux/MacOS/Unknown; Arch includes X86/X64/Arm64/Unknown; Env includes Hosted/Freestanding/Unknown. Compare constants instead of hard-coding their integer values.

```kinal
Unit Docs.Platform;
Get IO.Console;

Static Function int Main()
{
    If (IO.Target.OS == IO.Target.OS.Windows)
    {
        IO.Console.PrintLine("Windows target");
    }
    Else
    {
        IO.Console.PrintLine("non-Windows target");
    }
    IO.Console.PrintLine(IO.Target.PointerBits);
    IO.Console.PrintLine(IO.Target.DynamicLibrarySuffix);
    Return 0;
}
```

Compile-time inactive branches remain syntax-checked but are not semantically bound. Use target properties for program path/ABI policy; the compiler host is not the deployed program's platform. Cross-compiling does not fix Windows native argv/filesystem encoding boundaries.

## Freestanding Targets

```bash
kinal build kernel.kn --target bare-arm64 --freestanding --runtime none \
    --panic trap --no-crt --link-script board.ld -o out/kernel.elf
```

The target must supply its boot/startup contract and a real linker script. Alloc/GC profiles additionally require target runtime hooks. Producing an ELF is not evidence that a board boots it. See [Freestanding](freestanding.md).

## Static Linking and Deployment

Choosing Zig or enabling LTO does not automatically make a GNU Linux target fully static or remove package/system dependencies. `--kind static` creates a static library; it is not a switch for a fully static executable. `--crt static` selects a supported CRT mode, not an arbitrary CRT file or a complete dependency-free deployment policy.

Inspect the emitted binary and actual linked libraries on the target platform. Confirm runtime assets, library search paths and OS compatibility using the final distribution layout. See [Linking](linking.md).

## Verification

A useful order is semantic checks, target IR/object emission, target link, then execution on the target host. Keep those results distinct. The hosted CI matrix and seven-target stage emission checks establish different forms of evidence; neither substitutes for testing your application's native dependencies and behavior.

For a target matrix, build each output into a separate directory/name so one target does not overwrite another. Project profiles can encode common target/link/source choices; see [Project Files](../cli/project-files.md).
