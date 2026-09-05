# Packages and .klib archives

Kinal packages are described by `package.knpkg.json` (legacy name:
`package.knpkg`). Projects use a separate `kinal.knproj` manifest.

## Package commands

The C stage0 CLI provides:

```sh
kinal pkg build --manifest ./mylib -o ./mylib.klib
kinal pkg build --manifest ./mylib --layout ./kpkg
kinal pkg info ./mylib.klib
kinal pkg unpack ./mylib.klib -o ./recovered
```

`--manifest` accepts a file or package directory. `--layout` produces a
`<name>/<version>/package.knpkg.json` wrapper and `lib/<name>.klib`;
use it instead of `-o`. `pkg info` reports the archive file, producer
compiler, entry count, and total entry bytes, not a list of compiled exports.

The selfhost compiler can consume these packages during project compilation.
The `pkg build/info/unpack` CLI commands are not yet implemented by selfhost.

## Package manifest

```json
{
  "kind": "library",
  "name": "Acme.Greeter",
  "version": "1.0.0",
  "summary": "Greeting helpers",
  "source_root": "src",
  "modules": ["Acme.Greeter"],
  "dependencies": []
}
```

| Field | Meaning |
|-------|---------|
| `name` | Required package identity. Names beginning with `IO.` are reserved for official roots. |
| `version` | Version-selection text; recommended for every published package. |
| `source_root` | Directory scanned recursively for Kinal source files. |
| `source_files` | Explicit string array of source paths; takes precedence over `source_root`. Each file must declare a Unit. |
| `klib` | Path to an optional archive, preferred when it exists. |
| `summary`, `url` | Optional text metadata. |
| `modules`, `dependencies` | Optional string arrays of metadata. They do not download packages or solve version constraints. |

A library manifest requires a name and at least one of `source_root`,
nonempty `source_files`, or `klib`. Paths resolve relative to the
manifest directory. If the referenced archive is absent, the compiler falls
back to the source fields. If it exists but is invalid, compilation fails.

Manifests use JSON strings, arrays, objects, numbers, booleans, and null.
Unicode escapes and surrogate pairs are decoded as UTF-8. Damaged JSON,
trailing data, invalid escapes, and nesting beyond 64 levels are rejected.
NUL is not allowed in manifest strings because path/name APIs are
NUL-terminated.

## Project dependencies

```text
Project Example
{
    DefaultProfile = "native";

    Packages
    {
        Roots = ["packages"];
        OfficialRoots = ["official-packages"];
    }

    SourceSet "main"
    {
        Roots = ["src"];
        Include = ["**/*.kn"];
    }

    Profile "native"
    {
        Source
        {
            Entry = "src/Main.kn";
            Sets = ["main"];
            Mode = ReachableUnits;
        }
        Build { Backend = Native; Environment = Hosted; }
        Packages { Roots = ["profile-packages"]; }
    }
}
```

Run `kinal build --project .`. Ordinary roots are searched in this order:
project roots, selected-profile roots, and the automatic project `kpkg`
directory. Official roots are project roots, profile roots, and installed
standard packages. Roots contain package directories/manifests, not just
arbitrary loose `.klib` files. Generated `.git`, `.kinal-cache`,
`build`, and `out` subtrees are not package roots.

The highest version of each package name is selected independently in
ordinary and official roots. Dotted numeric segments compare numerically;
other segments compare lexically. Missing numeric segments compare as zero
(`1.0` equals `1.0.0`). Equal versions retain the first root's package.
This is not a full SemVer dependency solver.

Imports resolve by **Unit**, not package name: project Units take precedence
over ordinary packages, which take precedence over official packages.
Unshadowed Units in the same package remain available. `AutoDiscovery = false`
limits local source discovery and ordinary dependencies; explicit official
standard-library imports remain available.

The C CLI also accepts `--pkg-root <dir>` for direct-source builds. Selfhost
currently configures additional package roots through `kinal.knproj`.

## Archive contents and native libraries

The current `KNKLIB1` archive stores an embedded package manifest and
source/native asset payloads. It is **not** precompiled Kinal object code,
typed HIR, or a stable serialized type interface. Kinal sources are compiled
when the consumer imports their Units; native assets retain their own
platform/ABI requirements.

Do not pass a `.klib` to `--link-file`. That option accepts native linker
inputs such as object files and static libraries. Use package roots for
`.klib` discovery and Kinal FFI metadata or project Link options for native
payloads.

Selfhost extracts installed packages into an executable-relative
`stdlib-cache` generation and project archives into content-fingerprinted
`package-cache` directories. Changed archive contents do not reuse stale
source files, including same-size replacements.

## See also

- [CLI overview](compiler.md)
- [Project structure](../getting-started/project-structure.md)
- [Module system](../language/modules.md)
