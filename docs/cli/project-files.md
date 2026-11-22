# Project Files: `kinal.knproj`

`kinal.knproj` describes a project's local source graph, build profiles, package roots, link inputs, and editor analysis. It is a dedicated configuration language, not executable Kinal source or JSON. This reference describes the C compiler and shared project-aware LSP.

## Selection and Paths

```bash
kinal build --project .
kinal run --project . --profile native
kinal vm build --project . --profile vm
kinal vm run --project . --profile vm
kinal build --project ./kinal.knproj --emit check -o out/app.kcheck
```

A directory argument prefers `kinal.knproj`; `kinal.pkg.json` and `kinal.pkg` remain legacy fallbacks. A file argument can name another `.knproj` file. `--profile` selects a profile; otherwise `DefaultProfile` is used, falling back to the first declared profile if the field is absent. At least one profile is required. Selecting an unknown profile is an error.

Source paths, output paths, package roots, `LibDirs`, `LinkFiles`, and `LinkRoots` are resolved relative to the project file's directory. `Libs` are library names and `LinkArgs` are raw arguments. `LinkerPath` and `Script` are passed through by the current C driver; use absolute paths for them when invoking a project from another working directory.

Explicit CLI scalar options such as output, target, environment, entry symbol, and linker take precedence over the corresponding profile setting. Package roots and link items are collected together with CLI inputs; they are not a single replacement scalar. Profiles do not inherit from one another.

## A Complete Two-File Project

```text
hello-project/
├── kinal.knproj
└── src/
    ├── Main.kn
    └── Greeter.kn
```

`kinal.knproj`:

```knproj
Project HelloProject
{
    DefaultProfile = "native";
    Workspace { Ignore = ["out/**", ".git/**"]; }
    SourceSet "app"
    {
        Roots = ["src"];
        Include = ["**/*.kn"];
        RequireUnit = true;
    }
    Profile "native"
    {
        Source { Entry = "src/Main.kn"; Sets = ["app"]; Mode = ReachableUnits; }
        Build { Backend = Native; Environment = Hosted; Output = "out/hello"; }
    }
    Profile "vm"
    {
        Source { Entry = "src/Main.kn"; Sets = ["app"]; Mode = ReachableUnits; }
        Build { Backend = VM; Output = "out/hello.knc"; Superloop = true; }
    }
    Lsp { Profile = "native"; StrictProjectScope = true; }
}
```

`src/Main.kn`:

```kinal
Unit Hello.Main;
Get IO.Console;
Get Hello.Greeter;

Static Function int Main()
{
    IO.Console.PrintLine(Hello.Greeter.Greet("Kinal"));
    Return 0;
}
```

`src/Greeter.kn`:

```kinal
Unit Hello.Greeter;

Function string Greet(string name)
{
    Return "Hello, " + name + "!";
}
```

Both run commands above print `Hello, Kinal!`. The declared Unit determines import identity; the file name alone does not create a Unit. An ordinary `Get Hello.Greeter;` requires the qualified call shown here.

## Configuration Syntax

A file contains one `Project <name> { ... }` block. Named blocks use a name followed by braces; fields use `Name = value;`. Quote paths and strings; lists use `["first", "second"]`. Boolean values are `true` and `false`. Enum-style values such as `Native` are identifiers, not imported language symbols. Comments use the source lexer's `//` or `/* ... */` forms.

Unknown fields or enum values are errors; this format does not support arbitrary user-defined configuration properties, environment-variable interpolation, or profile inheritance. Use the spellings below rather than assuming a command-line option has a corresponding field.

## Root and Source Fields

| Location | Field | Meaning |
|----------|-------|---------|
| Project | `DefaultProfile` | Default CLI profile name |
| `Workspace` | `Ignore` | Exclusion patterns applied to local source candidates |
| `SourceSet "name"` | `Roots` | Directories recursively scanned for source files |
| SourceSet | `Files` | Explicit source file paths |
| SourceSet | `Include` | Inclusion patterns; omitted/empty means all candidates |
| SourceSet | `Exclude` | Exclusion patterns; takes precedence over inclusion |
| SourceSet | `RequireUnit` | Require a declared Unit; defaults to true |
| `Profile "name".Source` | `Entry` | Entry source file |
| Source | `Sets` | Active SourceSet names; omitted/empty selects all declared sets |
| Source | `Mode` | How collected local files participate |
| Source | `AutoDiscovery` | Compatibility switch; explicit `Mode` takes precedence |

Patterns support `*` within a path segment, `?` for one non-separator character, and `**` across path segments. The current matcher is case-insensitive and normalizes path separators. For scanned roots it tests both root-relative and project-relative paths. For an explicit `Files` entry it tests the project-relative path. `Workspace.Ignore` and `Exclude` win over `Include`.

Files in overlapping sets are deduplicated. A missing explicit `Files` entry is an error; missing root directories are currently skipped. The active selection must contain the entry and at least one local source file. `RequireUnit = false` relaxes the Unit check; it does not make unsafe-alias files eligible for project compilation.

### Source Participation Modes

| Mode | Local files compiled |
|------|----------------------|
| `FileOnly` | Only the entry file |
| `EntryUnit` | Entry plus active files declaring the same Unit |
| `ReachableUnits` | Entry Unit plus active local Units reachable through `Get` |
| `AllSources` | Every local file in the active sets |

These modes choose participation from the collected candidates; they do not bypass the active SourceSet boundary. `AllSources` also diagnoses errors in otherwise unreferenced local files. Package and standard-library resolution is separate from local participation.

With source sets but no explicit mode, the default is `ReachableUnits`. Otherwise `AutoDiscovery = true` maps to `ReachableUnits` and `false` to `FileOnly`. A project without source-set rules retains legacy entry-directory discovery. For explicit source graphs, `--no-module-discovery` selects local `FileOnly` while keeping package/standard-library lookup available. Use declared source sets with explicit modes rather than relying on legacy scanning.

## Build Fields

| Field | Values / meaning |
|-------|------------------|
| `Backend` | `Native` or `VM` |
| `Environment` | `Hosted` or `Freestanding` for native builds |
| `Runtime` | `None`, `Alloc`, `GC`; native freestanding memory profile |
| `Panic` | `Trap` or `Loop`; freestanding panic policy |
| `Target` | Target triple or supported alias |
| `Output` | Output artifact path |
| `EntrySymbol` | Entry symbol override; normal entries are `Main` or freestanding `KMain` |
| `Linker` | `LLD`, `Zig`, `MSVC` |
| `LinkerPath` | Linker executable selection; specify `Linker` as well |
| `Superloop` | VM superloop boolean |
| `HideConsole` | Boolean, default `false`. On Windows, `true` selects the GUI subsystem for native and VM-packed executables. |

`Source.Entry` starts source discovery. `Build.EntrySymbol` selects the function to execute among the active sources. If two units define `Main`, set e.g. `EntrySymbol = "App.Desktop.Main";` to select one. Native build/run and VM build/run/pack use this same selection. A semantic check can include multiple units with their own entry functions.

An omitted field leaves the command's normal default in effect. `kinal build`/`run` require a Native-compatible profile; `kinal vm build`/`run`/`pack` require a VM-compatible profile. The command does not silently switch backends because a profile requests another backend.

`Runtime` and `Panic` are freestanding options, not hosted GC tuning. `Backend = VM` does not provide a bare-metal VM. There are no `Emit`, `LTO`, `CRT`, optimization-level, or warning fields in the current C `.knproj` grammar; use supported CLI options for such controls.

## Packages and Link Fields

`Packages` can appear at project level and inside a profile:

| Field | Meaning |
|-------|---------|
| `Roots` | Local package search roots |
| `OfficialRoots` | Official standard-package search roots |
| `Pins` | Exact package versions, e.g. `["IO.UI@1.0.0"]`. The active profile can override a project pin. |

Project and profile roots are combined; a project's `kpkg/` is also added by the driver. Package search roots are not SourceSets and are not the project output directory. See [Packages](packages.md) for manifests and `.klib` archives.

The profile's `Link` block accepts:

| Field | Meaning |
|-------|---------|
| `Script` | Freestanding linker script |
| `NoCRT` | Disable CRT startup |
| `NoDefaultLibs` | Disable injected platform default libraries |
| `LibDirs` | Library search-directory list |
| `Libs` | Library-name list |
| `LinkFiles` | Exact object/static-library input list |
| `LinkArgs` | Raw linker-argument list; one item per argument |
| `LinkRoots` | Dependency roots expanded into target-specific search paths |

A package root does not automatically replace a missing native linker input. Choose target-compatible objects/libraries and inspect `--show-link`/`--show-link-search` when resolving symbols. See [Linking](../advanced/linking.md).

## LSP Fields

| Field | Meaning |
|-------|---------|
| `Profile` | Analysis profile; otherwise `DefaultProfile`, then first profile |
| `ExtraSets` | Additional source sets visible to editor analysis |
| `StrictProjectScope` | Disable broad workspace fallback outside the active local source graph |

The LSP finds the nearest `kinal.knproj`. Its extra sets do not change CLI build inputs. Keep test/helper sets separate from build sets when they should only be visible in the editor. Backend/environment/entry inference follows the selected profile; no separate editor `analysisEnvironment` setting is needed.

## Troubleshooting

| Symptom | Check |
|---------|-------|
| Profile not found | `--profile`, `DefaultProfile`, declared names |
| Entry excluded / empty graph | `Source.Sets`, roots, patterns, and `Source.Entry` |
| Missing Unit / unsafe alias | Use ordinary Unit source files inside projects |
| Unknown imported Unit | Exact declared Unit, active mode/sets, package roots |
| `Scoped Symbol Required` | Qualify the imported call or use an explicit symbol alias |
| CLI succeeds, editor disagrees | Nearest project, LSP profile, ExtraSets, strict scope |
| Wrong backend | Pair Native profiles with native commands, VM profiles with `vm` commands |

## See Also

- [Project Structure](../getting-started/project-structure.md)
- [Compiler CLI](compiler.md)
- [Native and VM Support](../advanced/backend-support.md)
