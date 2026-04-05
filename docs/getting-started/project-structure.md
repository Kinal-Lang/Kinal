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

## Project Manifest File

For large projects, use a JSON manifest file:

**manifest.json** (example)
```json
{
  "entry": "main.kn",
  "output": "myapp",
  "pkg-roots": ["./packages"]
}
```

Build:
```bash
kinal build --project manifest.json
```

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
| `.knc` | Compiled bytecode files (KNC format) |
| `.klib` | Packaged archive |
| `.knpkg.json` | Package manifest file |

## Next Steps

- [Language Overview](../language/overview.md)
- [kinal Compiler CLI](../cli/compiler.md)
- [Package System](../cli/packages.md)
