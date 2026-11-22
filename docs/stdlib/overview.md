# Standard Library Overview

This reference follows the current C compiler and standard-library source. `IO.*` is a unit naming scheme, not a claim that all APIs are compiler builtins or available on VM/freestanding.

## Packages, Units and Runtime

| Source | Surface |
|--------|---------|
| Compiler builtins | IO.Type, IO.Collection, IO.Error, IO.GC, IO.Meta, IO.Volatile, IO.Panic and Target/Host/Runtime/Version constants |
| IO.Core package | IO.Console, Text, File, Directory, Path, Time, System, Memory, Char, Math |
| Separate official packages | IO.Json, IO.Request, IO.Web, IO.UI, IO.Cat |
| IO.Async builtin interface | Task, Async/Await support and child-process operations; corresponding runtime support is still required |

Official bundles preinstall the packages. Imports use declared Unit names; the package name IO.Core is not a Unit that imports every member. Use `Get IO.Console;` followed by `IO.Console.PrintLine`, or import a symbol alias explicitly.

## Unit Navigation

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
| `IO.Json` | JSON parsing and formatting with `dict` / `list` values | [→](json.md) |
| `IO.Request` | Outbound HTTP client | [→](request.md) |
| `IO.Char` | Character classification helpers | [→](core.md) |
| `IO.Math` | Integer and floating-point math helpers | [→](core.md) |
| `IO.UI` | Native desktop UI toolkit | [→](ui.md) |
| `IO.Web` | Embedded HTTP server toolkit | [→](web.md) |
| `IO.Cat` | Sample package with the `Rommy` class | [→](cat.md) |
| `IO.Type` | Conversion and value helpers | [→](types.md) |
| `IO.Target` | Compile-time platform constants | Built-in |


## Import and Call Example

```kinal
Get PrintLine By IO.Console.PrintLine;
Get IO.Text;

Static Function int Main()
{
    PrintLine(IO.Text.ToUpper("kinal"));
    any value = 42;
    PrintLine(value.IsInt(), value.ToString());
    Return 0;
}
```

## Execution Boundaries

- Safe code may call Trusted standard-library bridges. Trusted does not automatically validate raw pointers or resource ownership.
- Native and KinalVM both support collections, JSON parsing and formatting, text splitting and joining, line I/O and directory enumeration. General Await and `IO.Async.Run` currently require Native.
- IO.Request/Web require matching native assets; HTTPS also requires OpenSSL 3. IO.UI 2.0 draws Windows controls with Direct3D 11, Direct2D and DirectWrite on Native and the desktop VM.
- Freestanding none/alloc/gc profiles restrict available operations; arbitrary hosted packages are unavailable.
- Returning bool, null, empty text, an error status, and throwing an exception are distinct contracts. Consult each API page instead of assuming every failure throws.

See [Backend Support](../advanced/backend-support.md), [Packages](../cli/packages.md), [Memory and Lifetimes](../advanced/memory-management.md), [Type Helpers](types.md), and [Exceptions](../language/exceptions.md).
