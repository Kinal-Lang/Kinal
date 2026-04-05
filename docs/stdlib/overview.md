# Standard Library Overview

Kinal's standard library is unified under the `IO.*` namespace, grouped by functional module. All standard library modules can be imported and used via `Get`.

## Module Quick Reference

| Module | Description | Docs |
|--------|-------------|------|
| `IO.Console` | Console input/output | [→](console.md) |
| `IO.Text` | String manipulation utilities | [→](text.md) |
| `IO.File` | File read/write operations | [→](filesystem.md) |
| `IO.Directory` | Directory operations | [→](filesystem.md) |
| `IO.Path` | Path joining and parsing | [→](path.md) |
| `IO.Collection` | list / dict / set | [→](collections.md) |
| `IO.Time` | Time and timers | [→](time.md) |
| `IO.Async` | Coroutine tasks and process management | [→](async.md) |
| `IO.System` | System calls and dynamic library loading | [→](system.md) |
| `IO.Meta` | Runtime metadata queries | [→](meta.md) |
| `IO.Type` | Type conversion functions | Built-in, no separate import needed |
| `IO.Target` | Compile-time platform constants | Built-in |
| `IO.Math` | Mathematical functions | Via IO.Core package |
| `IO.Char` | Character utility functions | Via IO.Core package |
| `IO.UI` | Graphical interface (UI components) | Via IO.UI package |
| `IO.Web` | Web services (Civet) | Via IO.Web package |

## Import Methods

```kinal
// Open entire module (requires fully qualified names when used)
Get IO.Console;
IO.Console.PrintLine("hello");

// Alias import
Get IO.Console;
IO.Console.PrintLine("hello");

// Symbol import
Get PrintLine By IO.Console.PrintLine;
PrintLine("hello");
```

## Built-in Type Methods

The following methods are available on **all values** through the `IO.Type.any` interface, with no import required:

```kinal
42.ToString();       // "42"
42.TypeName();       // "int"
42.Equals(42);       // true
42.IsNull();         // false
true.ToString();     // "true"
```

Type detection for `any` values:

```kinal
any v = 3.14;
v.IsNull();     // false
v.IsFloat();    // true
v.IsInt();      // false
v.IsString();   // false
v.IsNumber();   // true
v.IsObject();   // false
v.IsPointer();  // false
v.IsBool();     // false
v.IsChar();     // false
v.Tag();        // internal type tag (int)
```

## Standard Output Entry Point

The two most commonly used standard library functions:

```kinal
Get IO.Console;

IO.Console.PrintLine("Hello!");           // Print with newline
IO.Console.Print("Print without newline");
string input = IO.Console.ReadLine();     // Read one line of input
```

Supports multiple variadic arguments (`varargs`):

```kinal
IO.Console.PrintLine("a=", 1, ", b=", true);  // Print all arguments
```

## Type System Supplement: IO.Type

`IO.Type` provides type conversion functions (equivalent to the `[type](expr)` syntax):

```kinal
IO.Type.int(any_value);      // Convert to int
IO.Type.string(any_value);   // Convert to string
IO.Type.bool(any_value);     // Convert to bool
IO.Type.float(any_value);    // Convert to float
// ...
```

## IO.Target — Compile-Time Constants

```kinal
// Current compilation target (used with Const If)
IO.Target           // Target platform value
IO.Target.OS        // Operating system enum
IO.Target.OS.Windows
IO.Target.OS.Linux
IO.Target.OS.MacOS
```

Used for compile-time conditional branches (see [Control Flow → Const If](../language/control-flow.md#const-if-compile-time-condition)).

## IO.Core Package (Official Core Package)

`IO.Core` includes foundational tools such as `IO.Char` and `IO.Math`:

```kinal
// Requires IO.Core package to be configured in your project first
Get IO;   // Import IO namespace

IO.Char.IsDigit('5');   // true
IO.Char.IsLetter('A');  // true
IO.Char.ToLower('Z');   // 'z'
IO.Char.ToUpper('a');   // 'A'

IO.Math.Abs(-5);        // 5.0
IO.Math.Sqrt(16.0);     // 4.0
IO.Math.Max(3, 7);      // 7
```

## IO.UI Package (Graphical Interface)

```kinal
Get IO.UI;

IO.UI.Window window = New IO.UI.Window("My App", 800, 600);
IO.UI.Button btn = New IO.UI.Button("OK", 10, 10, 80, 30);
window.Add(btn);
Return window.Run();
```

## IO.Web Package (Web Services)

Provides HTTP service capabilities through the Civet framework (platform-specific feature).

## Next Steps

- [IO.Console](console.md)
- [IO.Text](text.md)
- [IO.Collections](collections.md)
- [IO.File / IO.Directory](filesystem.md)
