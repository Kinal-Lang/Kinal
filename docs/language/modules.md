# Module System

Kinal manages code namespaces and visibility through `Unit` (module declaration) and `Get` (module import). `Get` controls visibility; project membership is determined by the current build inputs or by active `SourceSet`s in `kinal.knproj`.

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

## Alias — Local Symbol Alias

`Alias` creates a local alias for a symbol that is already visible in the current file:

```kinal
Get IO.Console;

Alias Print By IO.Console.PrintLine;

Static Function int Main()
{
    Print("Hello");
    Return 0;
}
```

Rules:
- `Alias <Name> By <QualifiedSymbol>;`
- `Alias` does not import modules by itself
- The target symbol is validated when the file is compiled; it must already be visible through the current file's `Get` declarations or current compilation scope
- `Alias` only aliases symbols; it does not rewrite keywords, literals, operators, or punctuation
- `Alias` must appear in the same front section as `Get`, before normal declarations

`Alias` can point at ordinary visible symbols such as types, functions, enum members, and static members:

```kinal
Unit App.AliasValues;

Get IO.Console;

Alias Print By IO.Console.PrintLine;
Alias Say By App.AliasValues.Box.Say;
Alias Ready By App.AliasValues.Mode.Ready;
```

In editors that use the Kinal LSP, local/imported aliases are semantically colored by the resolved target kind. For example, an alias to a module keeps module-style coloring, and an alias to a function keeps function-style coloring.

## Unsafe Alias

`Unsafe Alias` is a file-local token alias feature for keywords and literals:

```kinal
Unsafe Alias fn By Function;
Unsafe Alias yes By true;
Unsafe Alias Take By Get;
Unsafe Alias 真 By true;

Take IO.Console;

Static fn int Main()
{
    If (yes)
        IO.Console.PrintLine("ok");
    Return 0;
}
```

Rules:
- `Unsafe Alias <NameOrLiteral> By <KeywordOrLiteral>;`
- It only affects the current source file
- It must appear in the file prologue
- Files that use `Unsafe Alias` cannot declare `Unit`
- Files that use `Unsafe Alias` cannot be compiled through `kinal.knproj` / `--project`
- Because those files cannot declare `Unit`, they are also excluded from normal same-directory module discovery / `Get` participation
- Plain `Unsafe Alias` cannot rewrite punctuation or operator tokens
- Alias names on the left can be ordinary UTF-8 identifiers such as `真` or `返回`

`Unsafe Alias` only rewrites tokens. It does not automatically turn a new spelling into a module or symbol import by itself. If you want localized names for modules or symbols, use normal `Get` / `Alias` after the token alias:

```kinal
Unsafe Alias 获取 By Get;
Unsafe Alias 来自 By By;
Unsafe Alias 别名 By Alias;
Unsafe Alias 静态 By Static;
Unsafe Alias 函数 By Function;
Unsafe Alias 整数 By int;
Unsafe Alias 返回 By Return;

获取 控制台 来自 IO.Console;
获取 打印原 来自 IO.Console.PrintLine;
别名 打印行 来自 打印原;

静态 函数 整数 Main()
{
    控制台.PrintLine("module");
    打印行("symbol");
    返回 0;
}
```

With the Kinal LSP active, unsafe-alias spellings are semantically colored by the rewritten token kind, so a localized `Get`/`Return`/`int` alias is highlighted like the original keyword or type.

## Unsafe Unsafe Unsafe Alias

`Unsafe Unsafe Unsafe Alias` is the full token-rewrite form:

```kinal
Unsafe Unsafe Unsafe Alias != By ==;
Unsafe Unsafe Unsafe Alias !! By 1;
Unsafe Unsafe Unsafe Alias ========= By ==;
Unsafe Unsafe Unsafe Alias 1011 By ;;
```

It includes all `Unsafe Alias` behavior, but it can also rewrite operator and punctuation tokens. It keeps the same restrictions:
- current-file only
- prologue only
- no `Unit`
- not allowed in project / `knproj` builds
- Triple-unsafe aliases match whole token sequences after lexing, so forms like `!!` and long operator runs such as `=========` are valid alias sources

When the target itself is `;`, write one extra semicolon for the directive terminator:

```kinal
Unsafe Unsafe Unsafe Alias 1011 By ;;
```

That rewrites `1011` to a single `;`.

## Automatic Cross-File Module Discovery

Outside project mode, Kinal can automatically discover `.kn` files in the same directory tree and include them in the same compilation.

In `kinal.knproj` builds, local membership comes from the active `SourceSet`s instead. `Get` then resolves against:

- local units inside the active `SourceSet`s
- package roots
- standard-library roots

To disable the legacy directory-based discovery:
```bash
kinal build main.kn --no-module-discovery
```

## Qualified Name Calls

Regardless of whether a module is imported, you can still write a fully qualified name directly:

```kinal
// Even without `Get IO.Console;`, you can write:
IO.Console.PrintLine("Hello");
```

However, in project mode that does not expand the local project boundary by itself. The target unit still has to be part of the active `SourceSet`s or come from a package / stdlib root.

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
