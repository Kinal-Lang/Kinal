# Project Structure

Kinal supports single-file programs, multi-file modules, and a full package system. This document covers all three approaches.

## Single-File Program

The simplest structure: one `.kn` file is a complete program.

```
myapp/
└── main.kn
```

Build:
```bash
kinal build main.kn -o myapp
```

## Multi-File Module (Shared Unit)

Multiple `.kn` files can share the same `Unit` name; the compiler merges them into a single module.

```
myapp/
├── main.kn        # Unit App.Main;
├── utils.kn       # Unit App.Utils;
└── models.kn      # Unit App.Models;
```

**main.kn**
```kinal
Unit App.Main;

Get App.Utils;
Get App.Models;
Get IO.Console;

Static Function int Main()
{
    Var msg = App.Utils.Greet("world");
    IO.Console.PrintLine(msg);
    Return 0;
}
```

**utils.kn**
```kinal
Unit App.Utils;

Function string Greet(string name)
{
    Return "Hello, " + name + "!";
}
```

When building, specify only the entry file; the compiler automatically discovers other `.kn` files in the same directory:
```bash
kinal build main.kn -o myapp
```

To disable auto-discovery:
```bash
kinal build main.kn utils.kn models.kn -o myapp --no-module-discovery
```

## Multiple Units in a Single File

A single file can contribute declarations to multiple units (via nested blocks), but one-unit-per-file is generally recommended.

## Package Structure

For reusable libraries, use the package format:

```
MyLib/
├── 1.0.0/
│   ├── package.knpkg.json
│   ├── src/
│   │   └── MyLib/
│   │       ├── Core.kn
│   │       └── Utils.kn
│   └── lib/
│       └── MyLib.klib    # Pre-compiled package (optional)
```

**package.knpkg.json**
```json
{
  "kind": "package",
  "name": "MyLib",
  "version": "1.0.0",
  "summary": "A brief description of my library",
  "source_root": "src",
  "entry": "src/MyLib/Core.kn"
}
```

Using the package:
```bash
# Specify the package root directory
kinal build main.kn --pkg-root ./packages -o myapp
```

In code:
```kinal
Get MyLib;       // Open the entire MyLib module
Get ML By MyLib; // Access via alias ML
```

## Project File (`kinal.knproj`)

For larger projects, place `kinal.knproj` at the project root and keep build settings there.

```
myapp/
├── kinal.knproj
├── src/
│   └── Main.kn
└── kpkg/
```

**kinal.knproj**
```kinal
Project MyApp
{
    DefaultProfile = "native";

    Packages
    {
        Roots = ["./kpkg"];
    }

    Profile "native"
    {
        Source
        {
            Entry = "src/Main.kn";
            AutoDiscovery = true;
        }

        Build
        {
            Backend = Native;
            Environment = Hosted;
            Output = "out/myapp";
        }
    }

    Profile "vm"
    {
        Source
        {
            Entry = "src/Main.kn";
            AutoDiscovery = true;
        }

        Build
        {
            Backend = VM;
            Output = "out/myapp.knc";
            Superloop = true;
        }
    }

    Lsp
    {
        Profile = "native";
    }
}
```

Build with the default profile:
```bash
kinal build --project .
```

Select another profile explicitly:
```bash
kinal vm build --project . --profile vm
```

When `--project` points to a directory, `kinal` looks for `kinal.knproj` first. Legacy project manifests such as `kinal.pkg.json` are still accepted for compatibility.

### How `kinal.knproj` is organized

The root form is:

```kinal
Project MyApp
{
    DefaultProfile = "native";
    Packages { ... }
    Profile "native" { ... }
    Lsp { ... }
}
```

- `Project <Name>` declares the project name.
- `DefaultProfile` selects the profile used when `--profile` is omitted.
- `Packages` defines package roots shared by all profiles.
- `Profile "<name>"` defines one build configuration.
- `Lsp` tells the language server which profile to use for editor analysis.

### Top-level fields

#### `DefaultProfile`

```kinal
DefaultProfile = "native";
```

The profile named here is used by commands such as:

```bash
kinal build --project .
kinal run --project .
```

If `DefaultProfile` is omitted, the first declared profile is used.

#### `Packages`

```kinal
Packages
{
    Roots = ["./kpkg", "../shared-packages"];
    OfficialRoots = ["../stdpkg"];
}
```

- `Roots`: local package roots, similar to passing `--pkg-root`.
- `OfficialRoots`: official package roots, similar to `--stdpkg-root`.

These roots apply to every profile unless a profile adds more package roots of its own.

#### `Lsp`

```kinal
Lsp
{
    Profile = "native";
}
```

This is only for editor analysis. It does not force the CLI to build with that profile.

Profile selection order in the LSP is:

1. `Lsp.Profile`
2. `DefaultProfile`
3. the first declared profile

### `Profile` blocks

A project can contain multiple profiles, for example native, VM, debug, release, or freestanding builds.

```kinal
Profile "native"
{
    Source { ... }
    Build { ... }
    Link { ... }
    Packages { ... }
}
```

Each profile may contain these sections:

- `Source`: source entry and source discovery behavior
- `Build`: backend, environment, output, target, and runtime-related settings
- `Link`: linker and link input settings
- `Packages`: extra package roots for this profile only

### `Source` section

```kinal
Source
{
    Entry = "src/Main.kn";
    AutoDiscovery = true;
}
```

- `Entry`: entry source file for the profile
- `AutoDiscovery`: whether `kinal` should automatically discover additional `.kn` files near the entry

Typical values:

- `true`: convenient for most apps and libraries
- `false`: use when you want explicit file control

### `Build` section

```kinal
Build
{
    Backend = Native;
    Environment = Hosted;
    Output = "out/myapp";
}
```

Supported fields:

- `Backend`
- `Environment`
- `Runtime`
- `Panic`
- `Target`
- `Output`
- `EntrySymbol`
- `Linker`
- `LinkerPath`
- `Superloop`

#### `Backend`

```kinal
Backend = Native;
```

Supported values:

- `Native`: build a native executable or native output
- `VM`: build KinalVM bytecode / VM-oriented output

Use with the matching CLI flow:

- `Native` profiles: `kinal build --project .`, `kinal run --project .`
- `VM` profiles: `kinal vm build --project .`, `kinal vm run --project .`, `kinal vm pack --project .`

#### `Environment`

```kinal
Environment = Hosted;
```

Supported values:

- `Hosted`: normal operating-system process entry, usually `Main`
- `Freestanding`: bare-metal or kernel-style entry, usually `KMain`

Use `Freestanding` when building kernels, boot-stage code, or other non-hosted targets.

#### `Runtime`

```kinal
Runtime = None;
```

Supported values:

- `None`
- `Alloc`
- `GC`

This is mainly relevant for freestanding/native configurations where you want to choose how much runtime support is linked in.

#### `Panic`

```kinal
Panic = Trap;
```

Supported values:

- `Trap`
- `Loop`

This controls panic behavior for freestanding-style builds.

#### `Target`

```kinal
Target = "x86_64-linux-gnu";
```

Use this to override the target triple for the profile.

#### `Output`

```kinal
Output = "out/myapp";
```

Sets the output path used when the CLI command does not provide `-o` or `--output`.

#### `EntrySymbol`

```kinal
EntrySymbol = "KernelMain";
```

Overrides the default entry symbol inference.

Default behavior:

- hosted native analysis/builds expect `Main`
- freestanding native analysis/builds expect `KMain`
- VM analysis/builds expect `Main`

#### `Linker`

```kinal
Linker = LLD;
```

Supported values:

- `LLD`
- `Zig`
- `MSVC`

#### `LinkerPath`

```kinal
LinkerPath = "C:/toolchains/lld-link.exe";
```

Use this when the linker executable is not on `PATH` or you want a specific toolchain binary.

#### `Superloop`

```kinal
Superloop = true;
```

This is for VM-oriented profiles and controls superloop mode in generated bytecode/bundles.

### `Link` section

```kinal
Link
{
    Script = "link/kernel.ld";
    NoCRT = true;
    NoDefaultLibs = true;
    LibDirs = ["./libs"];
    Libs = ["mylib"];
    LinkFiles = ["./prebuilt/startup.obj"];
    LinkArgs = ["--gc-sections"];
    LinkRoots = ["./deps"];
}
```

Supported fields:

- `Script`: linker script path
- `NoCRT`: disable CRT startup objects
- `NoDefaultLibs`: disable default system libraries
- `LibDirs`: library search directories
- `Libs`: libraries by name
- `LinkFiles`: exact link inputs such as `.obj`, `.lib`, `.a`
- `LinkArgs`: raw linker arguments
- `LinkRoots`: dependency roots expanded into target-specific search locations

This section is the project-file equivalent of CLI options such as `-L`, `-l`, `--link-file`, `--link-root`, and `--link-arg`.

### Profile-local `Packages`

Profiles can add package roots on top of the global `Packages` block:

```kinal
Profile "native"
{
    Packages
    {
        Roots = ["./native-packages"];
    }
}
```

Use this when one profile needs extra packages that other profiles do not use.

### Example: hosted native + VM

```kinal
Project MyApp
{
    DefaultProfile = "native";

    Packages
    {
        Roots = ["./kpkg"];
    }

    Profile "native"
    {
        Source
        {
            Entry = "src/Main.kn";
            AutoDiscovery = true;
        }

        Build
        {
            Backend = Native;
            Environment = Hosted;
            Output = "out/myapp";
        }
    }

    Profile "vm"
    {
        Source
        {
            Entry = "src/Main.kn";
            AutoDiscovery = true;
        }

        Build
        {
            Backend = VM;
            Output = "out/myapp.knc";
            Superloop = true;
        }
    }

    Lsp
    {
        Profile = "native";
    }
}
```

### Example: freestanding kernel profile

```kinal
Project KinalOS
{
    DefaultProfile = "kernel";

    Profile "kernel"
    {
        Source
        {
            Entry = "src/kernel.kn";
            AutoDiscovery = true;
        }

        Build
        {
            Backend = Native;
            Environment = Freestanding;
            Runtime = None;
            Panic = Loop;
            Target = "x86_64-unknown-none";
            Output = "out/kernel.elf";
            EntrySymbol = "KMain";
            Linker = LLD;
        }

        Link
        {
            Script = "link/kernel.ld";
            NoCRT = true;
            NoDefaultLibs = true;
        }
    }

    Lsp
    {
        Profile = "kernel";
    }
}
```

This kind of profile is appropriate for kernels and other bare-metal targets.

### Commands and profile compatibility

- `kinal build --project .` expects a `Native` profile
- `kinal run --project .` expects a `Native` hosted profile
- `kinal vm build --project .` expects a `VM` profile
- `kinal vm run --project .` expects a `VM` profile
- `kinal vm pack --project .` expects a `VM` profile

If you have both native and VM profiles, select the correct one with `--profile`.

## Packaging and Distribution

Package source code into a `.klib` (a compressed archive containing source code and metadata):

```bash
kinal pkg build --manifest ./MyLib/1.0.0/ -o output/MyLib.klib
```

Restore source from a `.klib`:
```bash
kinal pkg unpack MyLib.klib -o ./recovered/
```

## File Extensions

| Extension | Description |
|-----------|-------------|
| `.kn` / `.kinal` | Kinal source files |
| `.knproj` | Project file |
| `.knc` | Compiled bytecode files (KNC format) |
| `.klib` | Packaged archive |
| `.knpkg.json` | Package manifest file |

## Next Steps

- [Language Overview](../language/overview.md)
- [kinal Compiler CLI](../cli/compiler.md)
- [Package System](../cli/packages.md)
