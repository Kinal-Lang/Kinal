# Standard Library Overview

Kinal's standard library is organized under the `IO.*` namespace. Core runtime modules are available directly, while package modules such as `IO.Core`, `IO.Request`, `IO.UI`, and `IO.Web` are brought in through project package configuration.

## Module Quick Reference

| Module | Description | Docs |
|--------|-------------|------|
| `IO.Console` | Console input/output | [→](console.md) |
| `IO.Text` | String processing helpers | [→](text.md) |
| `IO.File` | File operations | [→](filesystem.md) |
| `IO.Directory` | Directory operations | [→](filesystem.md) |
| `IO.Path` | Path join / split / normalize helpers | [→](path.md) |
| `IO.Collection` | `list` / `dict` / `set` | [→](collections.md) |
| `IO.Time` | Time helpers and timers | [→](time.md) |
| `IO.Async` | Async tasks and process helpers | [→](async.md) |
| `IO.System` | Process, command line, and dynamic library APIs | [→](system.md) |
| `IO.Meta` | Runtime metadata access | [→](meta.md) |
| `IO.Request` | Outbound HTTP client | [→](request.md) |
| `IO.Char` | Character classification helpers | [→](core.md) |
| `IO.Math` | Integer and floating-point math helpers | [→](core.md) |
| `IO.UI` | Native desktop UI toolkit | [→](ui.md) |
| `IO.Web` | Embedded HTTP server toolkit | [→](web.md) |
| `IO.Cat` | Sample package with the `Rommy` class | [→](cat.md) |
| `IO.Type` | Built-in conversion helpers | Built-in |
| `IO.Target` | Compile-time platform constants | Built-in |

## Import Forms

```kinal
Get IO.Console;
IO.Console.PrintLine("hello");

Get PrintLine By IO.Console.PrintLine;
PrintLine("hello");
```

## Built-in Value Helpers

All values expose the `IO.Type.any` helper surface without an extra import:

```kinal
42.ToString();       // "42"
42.TypeName();       // "int"
42.Equals(42);       // true
42.IsNull();         // false
```

The same surface is available on `any` values for runtime type checks:

```kinal
any value = 3.14;
value.IsFloat();     // true
value.IsNumber();    // true
value.IsString();    // false
value.Tag();         // internal runtime tag
```

## Common Entry Points

```kinal
Get IO.Console;

IO.Console.PrintLine("Hello");
IO.Console.Print("Without newline");
string input = IO.Console.ReadLine();
```

```kinal
Get IO.Text;

IO.Text.Contains("kinal", "na");
IO.Text.ToUpper("hello");
IO.Text.Split("a,b,c", ",");
```

## Package Modules

### IO.Core

`IO.Core` provides `IO.Char` and `IO.Math`.

```kinal
Get IO;

IO.Char.IsDigit('5');
IO.Math.Abs(-5);
IO.Math.Sqrt(16.0);
```

### IO.Request

`IO.Request` is the synchronous outbound HTTP client package.

```kinal
Get IO.Request;

IO.Request.Response response = IO.Request.Fetch("http://127.0.0.1:8000/");
```

### IO.UI

`IO.UI` provides a native window / control layer for desktop apps.

```kinal
Get IO.UI;

IO.UI.Window window = New IO.UI.Window("Demo", 640, 480);
window.Add(New IO.UI.Button("OK", 20, 20, 100, 28));
```

### IO.Web

`IO.Web` provides an embedded HTTP server with route metadata and static mounts.

```kinal
Get IO.Web;

IO.Web.Server server = New IO.Web.Server(8080);
server.UseStatic("/static", "public").UseRoutes();
```

## See Also

- [IO.Console](console.md)
- [IO.Text](text.md)
- [IO.Collection](collections.md)
- [IO.File / IO.Directory](filesystem.md)
- [IO.Path](path.md)
- [IO.Core](core.md)
- [IO.Request](request.md)
- [IO.UI](ui.md)
- [IO.Web](web.md)
