# IO.Version (Compiler and VM Version Constants)

`IO.Version.*` and `IO.Version.VM.*` are compile-time constants that expose the toolchain version used when the current artifact was compiled.

Like `IO.Target.*`, `IO.Host.*`, and `IO.Runtime.*`, these symbols are folded directly to literal values during the semantic phase, producing no runtime overhead.

> **Note:** `IO.Version` describes the compiler/toolchain version, not your application's own version number. For application versioning, keep a project-level constant or read it from your package metadata.

---

## Constants at a Glance

### Compiler Version

| Symbol | Type | Description |
|--------|------|-------------|
| `IO.Version` | `string` | Compiler version string, equivalent to `IO.Version.String` |
| `IO.Version.String` | `string` | Same as above |
| `IO.Version.Major` | `int` | Compiler major version |
| `IO.Version.Minor` | `int` | Compiler minor version |
| `IO.Version.Patch` | `int` | Compiler patch version |

### VM Version

| Symbol | Type | Description |
|--------|------|-------------|
| `IO.Version.VM` | `string` | VM version string, equivalent to `IO.Version.VM.String` |
| `IO.Version.VM.String` | `string` | Same as above |
| `IO.Version.VM.Major` | `int` | VM major version |
| `IO.Version.VM.Minor` | `int` | VM minor version |
| `IO.Version.VM.Patch` | `int` | VM patch version |

When the artifact targets the native backend, VM string constants are empty and VM numeric constants are `0`.

---

## Usage Example

```kinal
Unit MyLib;

Get IO.Console;

Static Function void PrintVersions()
{
    IO.Console.PrintLine("Compiler: " + IO.Version);
    IO.Console.PrintLine("Compiler minor: " + [string](IO.Version.Minor));

    If (IO.Runtime.IsVM)
    {
        IO.Console.PrintLine("VM: " + IO.Version.VM);
    }
}
```

You can also branch on numeric members directly:

```kinal
If (IO.Version.Major == 0 && IO.Version.Minor >= 8)
{
    // ...
}
```

---

## Values by Command

| Command | `IO.Version` | `IO.Version.VM` |
|---------|--------------|-----------------|
| `kinal build` | Compiler version | `""` |
| `kinal run` | Compiler version | `""` |
| `kinal vm build` | Compiler version | VM version |
| `kinal vm pack` | Compiler version | VM version |
| `kinal vm run <file.kn>` | Compiler version | VM version |
| `--emit obj` / `--emit asm` / `--emit llvm-ir` | Compiler version | `""` |
| `--emit knc` | Compiler version | VM version |

`IO.Version.Major` / `Minor` / `Patch` always describe the compiler build that produced the artifact.

`IO.Version.VM.*` describes the VM version only for VM-targeted artifacts. In native artifacts these members fold to empty string or zero.

Constants are written into the artifact at compile time and do not change afterward. A `.knc` produced by `kinal vm build` keeps the compiler and VM version constants that were present when the bytecode was generated.

---

## Relationship with `kinal --version`

The CLI command:

```bash
kinal --version
kinal --version --verbose
```

prints the same version families that are available in source code through `IO.Version` and `IO.Version.VM`.

---

## See Also

- [IO.Runtime Constants](runtime-environment-constants.md) — Detecting whether the current artifact targets native code or KinalVM
- [Compiler CLI](../cli/compiler.md) — `kinal --version` and other global options
- [KinalVM](../cli/vm.md) — VM-oriented build and run commands
- [Compilation Pipeline](compilation-pipeline.md) — When compile-time constants are folded