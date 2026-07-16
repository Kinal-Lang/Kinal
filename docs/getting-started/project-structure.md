# Project Structure

Kinal supports single-file programs, multi-file modules, packages, and project files. This document shows the common layouts and explains how `kinal.knproj` defines the local project graph.

## Single-File Program

The simplest structure is one `.kn` file:

```text
myapp/
└── main.kn
```

Build:

```bash
kinal build main.kn -o myapp
```

## Multi-File Module

Multiple files can participate in one program:

```text
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

If you build by entry file outside project mode, the compiler can still auto-discover nearby files:

```bash
kinal build main.kn -o myapp
```

To disable that legacy directory-based discovery:

```bash
kinal build main.kn utils.kn models.kn -o myapp --no-module-discovery
```

## Multiple Units in One File

A single file can contribute declarations to multiple units through nested blocks, but one unit per file is usually easier to maintain.

## Package Structure

Reusable libraries use package manifests:

```text
MyLib/
├── 1.0.0/
│   ├── package.knpkg.json
│   ├── src/
│   │   └── MyLib/
│   │       ├── Core.kn
│   │       └── Utils.kn
│   └── lib/
│       └── MyLib.klib
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
kinal build main.kn --pkg-root ./packages -o myapp
```

In code:

```kinal
Get MyLib;
Get ML By MyLib;
```

## Project File (`kinal.knproj`)

For larger projects, place `kinal.knproj` at the project root and declare the project boundary there.

```text
myapp/
├── kinal.knproj
├── src/
│   ├── Main.kn
│   └── App/
│       └── Greeter.kn
├── tests/
│   └── Main.kn
└── kpkg/
```

**kinal.knproj**

```kinal
Project MyApp
{
    DefaultProfile = "native";

    Workspace
    {
        Ignore = ["out/**", ".git/**"];
    }

    Packages
    {
        Roots = ["./kpkg"];
    }

    SourceSet "app"
    {
        Roots = ["src"];
        Include = ["**/*.kn"];
        Exclude = ["generated/**"];
        RequireUnit = true;
    }

    SourceSet "tests"
    {
        Roots = ["tests"];
        Include = ["**/*.kn"];
        RequireUnit = true;
    }

    Profile "native"
    {
        Source
        {
            Entry = "src/Main.kn";
            Sets = ["app"];
            Mode = ReachableUnits;
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
            Sets = ["app"];
            Mode = ReachableUnits;
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
        ExtraSets = ["tests"];
        StrictProjectScope = true;
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

When `--project` points to a directory, `kinal` looks for `kinal.knproj` first. Legacy manifests such as `kinal.pkg.json` are still accepted for compatibility.

### How `kinal.knproj` Is Organized

Root form:

```kinal
Project MyApp
{
    DefaultProfile = "native";
    Workspace { ... }
    SourceSet "app" { ... }
    Packages { ... }
    Profile "native" { ... }
    Lsp { ... }
}
```

- `Project <Name>` declares the project name.
- `DefaultProfile` selects the profile used when `--profile` is omitted.
- `Workspace` defines paths or patterns tools should ignore.
- `SourceSet "<name>"` defines which local source files belong to the project.
- `Packages` defines shared package roots.
- `Profile "<name>"` defines one build configuration.
- `Lsp` selects the preferred editor-analysis profile and any extra source sets.

### Top-Level Fields

#### `DefaultProfile`

```kinal
DefaultProfile = "native";
```

Used by commands such as:

```bash
kinal build --project .
kinal run --project .
```

If omitted, the first declared profile is used.

#### `Workspace`

```kinal
Workspace
{
    Ignore = ["out/**", ".git/**"];
}
```

- `Ignore`: patterns CLI/LSP should not treat as normal project content.

#### `SourceSet`

```kinal
SourceSet "app"
{
    Roots = ["src"];
    Files = ["tools/Generate.kn"];
    Include = ["**/*.kn"];
    Exclude = ["scratch/**"];
    RequireUnit = true;
}
```

`SourceSet` controls the local project graph.

- `Roots`: directories to scan recursively.
- `Files`: extra explicit files.
- `Include`: allow-list patterns inside the set.
- `Exclude`: deny-list patterns inside the set.
- `RequireUnit`: whether files in the set are expected to declare `Unit`.

In project mode, local membership comes from the active `SourceSet`s, not from broad folder guessing.

#### `Packages`

```kinal
Packages
{
    Roots = ["./kpkg", "../shared-packages"];
    OfficialRoots = ["../stdpkg"];
}
```

- `Roots`: local package roots, similar to `--pkg-root`.
- `OfficialRoots`: official package roots, similar to `--stdpkg-root`.

These roots apply to every profile unless a profile adds more package roots of its own.

#### `Lsp`

```kinal
Lsp
{
    Profile = "native";
    ExtraSets = ["tests"];
    StrictProjectScope = true;
}
```

- `Profile`: preferred profile for editor analysis.
- `ExtraSets`: additional `SourceSet`s visible in the editor.
- `StrictProjectScope`: keep LSP analysis inside the declared project graph instead of falling back to broad workspace scanning.

LSP profile selection order:

1. `Lsp.Profile`
2. `DefaultProfile`
3. the first declared profile

### `Profile` Blocks

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

- `Source`: entry file and local source-graph mode
- `Build`: backend, environment, output, target, and runtime-related settings
- `Link`: linker and link input settings
- `Packages`: extra package roots for this profile only

### `Source` Section

```kinal
Source
{
    Entry = "src/Main.kn";
    Sets = ["app"];
    Mode = ReachableUnits;
}
```

- `Entry`: entry source file for the profile
- `Sets`: which `SourceSet`s are active for this profile
- `Mode`: how local files are pulled into the build

Supported `Mode` values:

- `FileOnly`: compile only the entry file
- `EntryUnit`: compile the entry file plus other files that declare the same `Unit`
- `ReachableUnits`: compile the entry unit, then pull local units referenced through `Get`
- `AllSources`: compile every file in the active `SourceSet`s

`AutoDiscovery` is still accepted for compatibility:

- `AutoDiscovery = true` behaves like `Mode = ReachableUnits`
- `AutoDiscovery = false` behaves like `Mode = FileOnly`

### `Build` Section

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
- `VM`: build KinalVM bytecode or VM-oriented output

Typical CLI pairing:

- `Native` profiles: `kinal build --project .`, `kinal run --project .`
- `VM` profiles: `kinal vm build --project .`, `kinal vm run --project .`, `kinal vm pack --project .`

#### `Environment`

```kinal
Environment = Hosted;
```

Supported values:

- `Hosted`: normal operating-system process entry, usually `Main`
- `Freestanding`: bare-metal or kernel-style entry, usually `KMain`

#### `Runtime`

```kinal
Runtime = None;
```

Supported values:

- `None`
- `Alloc`
- `GC`

This is mainly relevant for freestanding/native configurations.

#### `Panic`

```kinal
Panic = Trap;
```

Supported values:

- `Trap`
- `Loop`

#### `Target`

```kinal
Target = "x86_64-linux-gnu";
```

Use this to override the target triple for the profile.

#### `Output`

```kinal
Output = "out/myapp";
```

Sets the output path used when the CLI command does not provide `-o`.

#### `EntrySymbol`

```kinal
EntrySymbol = "KernelMain";
```

Overrides the default entry-symbol inference.

Default behavior:

- hosted native builds expect `Main`
- freestanding native builds expect `KMain`
- VM builds expect `Main`

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

Use this when the linker executable is not on `PATH`.

#### `Superloop`

```kinal
Superloop = true;
```

This is for VM-oriented profiles and controls superloop mode in generated bytecode or bundles.

### `Link` Section

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

This is the project-file equivalent of CLI options such as `-L`, `-l`, `--link-file`, `--link-root`, and `--link-arg`.

### Profile-Local `Packages`

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

### Example: Hosted Native + VM

```kinal
Project MyApp
{
    DefaultProfile = "native";

    Packages
    {
        Roots = ["./kpkg"];
    }

    SourceSet "app"
    {
        Roots = ["src"];
        Include = ["**/*.kn"];
    }

    Profile "native"
    {
        Source
        {
            Entry = "src/Main.kn";
            Sets = ["app"];
            Mode = ReachableUnits;
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
            Sets = ["app"];
            Mode = ReachableUnits;
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

### Example: Freestanding Kernel Profile

```kinal
Project KinalOS
{
    DefaultProfile = "kernel";

    SourceSet "kernel"
    {
        Roots = ["src"];
        Include = ["**/*.kn"];
    }

    Profile "kernel"
    {
        Source
        {
            Entry = "src/kernel.kn";
            Sets = ["kernel"];
            Mode = ReachableUnits;
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

### Commands and Profile Compatibility

- `kinal build --project .` expects a `Native` profile
- `kinal run --project .` expects a `Native` hosted profile
- `kinal vm build --project .` expects a `VM` profile
- `kinal vm run --project .` expects a `VM` profile
- `kinal vm pack --project .` expects a `VM` profile

If you have both native and VM profiles, select the correct one with `--profile`.

## Packaging and Distribution

Package source code into a `.klib`:

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
