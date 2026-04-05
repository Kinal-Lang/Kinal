# Module System

Kinal manages code namespaces and visibility through `Unit` (module declaration) and `Get` (module import).

## Unit — Declaring a Module

The first line (optional) of each `.kn` file declares the unit it belongs to:

```kinal
Unit App.Services;
```

- The `Unit` declaration must be at the top of the file, before all `Get` declarations and other declarations
- Unit names use dot notation to form a hierarchy (e.g., `IO.Console`, `App.Utils`)
- Files without a `Unit` declaration belong to an anonymous module; their symbols can still be accessed by files in the same directory

Functions, classes, interfaces, etc. declared in this Unit have a fully qualified name of `UnitName.DeclarationName`, for example:

```kinal
Unit App.Services;

Function string GetVersion() { Return "1.0"; }
// Fully qualified name: App.Services.GetVersion
```

## Get — Importing Modules

### 1. Open Import (Namespace Style)

```kinal
Get IO.Console;
```

After importing, all public symbols in `IO.Console` can be accessed as `IO.Console.SymbolName`, or with the prefix partially omitted when unambiguous.

### 2. Alias Import

```kinal
Get Console By IO.Console;
```

Binds the module `IO.Console` to a local alias `Console`, allowing `Console.PrintLine(...)` instead of `IO.Console.PrintLine(...)`.

```kinal
Get FS By IO.File;
Get Dir By IO.Directory;

FS.ReadAllText("data.txt");
Dir.Exists("logs/");
```

### 3. Symbol Import

Import a single symbol from a module:

```kinal
Get PrintLine By IO.Console.PrintLine;

// Can now call PrintLine(...) directly
PrintLine("Hello!");
```

### 4. Bulk Symbol Import

```kinal
Get IO.Utils { ReadFile By ReadAllText, WriteFile By WriteAllText };
```

Used to selectively extract and rename multiple symbols from a module.

### Rules for Get Placement

`Get` declarations must appear after `Unit` and before all other declarations (functions, classes, etc.):

```kinal
Unit App.Main;          // ← Unit first

Get IO.Console;         // ← then all Get declarations
Get FS By IO.File;

Static Function int Main() { ... }  // ← then other declarations
```

## Automatic Cross-File Module Discovery

At compile time, Kinal by default automatically discovers `.kn` files in the same directory and subdirectories, including them in the same compilation unit.

As long as two files are part of the same compilation (regardless of whether their Unit names match), their symbols can reference each other.

To disable this:
```bash
kinal build main.kn --no-module-discovery
```

## Qualified Name Calls

Regardless of whether a module is imported, you can always use the fully qualified name directly:

```kinal
// Even without `Get IO.Console;`, you can write:
IO.Console.PrintLine("Hello");
```

## Built-in IO.* Standard Library Modules

All standard library modules are in the `IO.*` namespace:

```kinal
Get IO.Console;       // console
Get IO.File;          // file operations
Get IO.Directory;     // directory operations
Get IO.Path;          // path utilities
Get IO.Text;          // string utilities
Get IO.Time;          // time
Get IO.Async;         // async
Get IO.System;        // system calls
Get IO.Meta;          // metadata reflection
Get IO.Target;        // compilation target info (platform constants)
```

See [Standard Library Overview](../stdlib/overview.md) for details.

## IO.Target — Compile-Time Platform Constants

`IO.Target` provides compile-time platform detection, enabling conditional compilation combined with `If`:

```kinal
If (IO.Target.OS == IO.Target.OS.Windows)
{
    Console.PrintLine("Running on Windows");
}
Else If (IO.Target.OS == IO.Target.OS.Linux)
{
    Console.PrintLine("Running on Linux");
}
```

Common constants:
- `IO.Target.OS.Windows` / `IO.Target.OS.Linux` / `IO.Target.OS.MacOS`
- `IO.Target` — current target (participates in `Const If` evaluation)

## Complete Example

**math.kn**
```kinal
Unit App.Math;

Function int GCD(int a, int b)
{
    While (b != 0)
    {
        int t = b;
        b = a % b;
        a = t;
    }
    Return a;
}

Function int LCM(int a, int b)
{
    Return a / GCD(a, b) * b;
}
```

**main.kn**
```kinal
Unit App.Main;

Get Console By IO.Console;
Get App.Math;              // open App.Math module

Static Function int Main()
{
    int g = App.Math.GCD(48, 18);    // 6
    int l = App.Math.LCM(4, 6);     // 12
    Console.PrintLine([string](g));
    Console.PrintLine([string](l));
    Return 0;
}
```

## Next Steps

- [Generics](generics.md)
- [Standard Library Overview](../stdlib/overview.md)
- [Package System](../cli/packages.md)
