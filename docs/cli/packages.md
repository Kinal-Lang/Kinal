# kinal pkg — Package Management

`kinal pkg` is the subcommand group for managing `.klib` package files. Kinal uses a `.knpkg.json`-based package system and the `.klib` binary library format.

---

## Subcommands

| Subcommand | Purpose |
|------------|---------|
| `kinal pkg build` | Build a `.klib` or local package layout from a package manifest |
| `kinal pkg info` | View `.klib` metadata |
| `kinal pkg unpack` | Unpack a `.klib` to a directory |

---

## kinal pkg build

Build a `.klib` library file or local package layout from a package manifest (`.knpkg.json`).

```bash
kinal pkg build --manifest <file|dir> [-o <output>]
kinal pkg build --manifest <file|dir> --layout <dir>
```

| Option | Description |
|--------|-------------|
| `--manifest <file\|dir>` | Package manifest path (required) |
| `-o, --output <file>` | Output `.klib` file path |
| `--layout <dir>` | Output a local package directory structure (mutually exclusive with `-o`) |

```bash
# Build a klib
kinal pkg build --manifest ./mylib/ -o mylib.klib

# Output a local package layout
kinal pkg build --manifest ./mylib/ --layout ./packages/
```

---

## kinal pkg info

Display metadata from a `.klib` file.

```bash
kinal pkg info <file.klib>
```

Example output:

```
Name:      mylib
Version:   1.0.0
Modules:   Mylib.Core
           Mylib.Utils
Exports:   42 symbols
Arch:      x86_64
Platform:  linux
```

---

## kinal pkg unpack

Unpack a `.klib` to a directory, restoring source code and metadata.

```bash
kinal pkg unpack <file.klib> [-o <dir>]
```

| Option | Description |
|--------|-------------|
| `-o, --output <dir>` | Output directory (default: directory named after the `.klib` file) |

```bash
kinal pkg unpack mylib.klib -o ./recovered/
```

---

## Package Manifest: `.knpkg.json`

Each package root contains a `.knpkg.json` file:

```json
{
    "name": "mypackage",
    "version": "1.0.0",
    "description": "My Kinal package",
    "author": "Developer Name",
    "entry": "src/main.kn",
    "dependencies":
    {
        "somelib": "1.0.0"
    }
}
```

### Field Reference

| Field | Required | Description |
|-------|----------|-------------|
| `name` | Yes | Package name (lowercase letters, digits, hyphens) |
| `version` | Yes | Semantic version number (`major.minor.patch`) |
| `description` | No | Short description of the package |
| `author` | No | Author information |
| `entry` | No | Main entry file (default: `src/main.kn`) |
| `dependencies` | No | Map of dependency package names to versions |

---

## `.klib` Library Files

`.klib` is Kinal's pre-compiled library format, containing:

- Compiled object code
- Type metadata (for compiler type checking)
- Exported module interfaces

---

## Using `.klib` in a Build

`.klib` files are added via `kinal build` link options:

```bash
# Link a klib directly
kinal build main.kn --link-file mylib.klib -o app

# Auto-discover via package root
kinal build main.kn --pkg-root ./packages -o app
```

---

## Building a Full Project

```bash
# Use --project to automatically read .knpkg.json
kinal build --project . -o app

# Manually specify entry and package root
kinal build src/main.kn --pkg-root ./packages -o app
```

---

## Recommended Project Structure

```
myproject/
├── .knpkg.json        ← Package manifest
├── src/
│   ├── main.kn        ← Main entry
│   └── lib/
│       └── utils.kn   ← Internal module
├── tests/
│   └── test_utils.kn
└── packages/          ← Third-party .klib files (dependencies)
    └── somelib.klib
```

---

## Versioning

Package versions follow Semantic Versioning (SemVer):

- `1.0.0` — Stable release
- `1.0.1` — Bug fix
- `1.1.0` — Backward-compatible new features
- `2.0.0` — Breaking changes

---

## See Also

- [CLI Overview](compiler.md) — Overview of all subcommands
- [Project Structure](../getting-started/project-structure.md) — Recommended directory layout
- [Module System](../language/modules.md) — `Unit` and `Get` declarations
- [CLI Specification](cli-spec.md) — CLI design specification (developer reference)
