# IO.System: Process and Dynamic Libraries

Import `Get IO.System;`. These hosted operations expose command-line text, process execution and native library loading. Source wrappers are part of IO.Core; GetSymbol additionally has compiler handling for delegate metadata.

## Complete API

| Function | Result | Meaning |
|----------|--------|---------|
| `CommandLine()` | `string` | Command-line text; not a list/argv parser |
| `Exit(int code)` | `void` | Terminate the process with the selected status |
| `FileExists(string path)` | `bool` | File existence check |
| `Exec(string commandLine)` | `int` | Launch/wait for a host command, return status or failure |
| `LoadLibrary(string path)` | `byte*` | Native library handle, null on failure |
| `GetSymbol(byte* library, string symbol)` | `byte*` | Native symbol address, null on failure |
| `CloseLibrary(byte* library)` | `bool` | Release a native loader reference |
| `LastError()` | `string` | Last host/loader error text |

The current compiler also accepts a compatible C Delegate reference as GetSymbol's second argument, resolving its SymbolName metadata. A Delegate is a binding slot, not a type used to cast an arbitrary any variable. See [FFI](../language/ffi.md).

Public Trusted wrappers may be called from Safe code. Raw pointer operations and Unsafe foreign calls remain separately checked. There is no standalone `Trusted { ... }` statement form: use a Trusted function or supported explicit Block syntax.

## Use Main for Structured Arguments

```kinal
Unit Docs.Arguments;
Get IO.Console;
Get IO.System;

Static Function int Main(string[] args)
{
    IO.Console.PrintLine(IO.System.CommandLine().Length() > 0);
    If (args.Length() == 0)
    {
        IO.Console.PrintLine("Usage: app <name>");
        Return 1;
    }
    IO.Console.PrintLine(args[0]);
    Return 0;
}
```

```bash
kinal run arguments.kn -- Kinal
```

`Main(string[] args)` excludes the executable name. Use it instead of splitting CommandLine on spaces: a flattened command-line string cannot reconstruct quoted/empty arguments reliably. Windows process arguments and native filesystem boundaries currently do not provide a universal UTF-8 guarantee.

## Exec Is Host-Specific

Windows uses process creation with a command line; shell builtins require an explicit shell such as `cmd /c ...`. The POSIX host path uses the system shell. Use fixed commands or a deliberate host-specific quoting policy; this API is not an argv-array launcher. Check the returned status before treating the operation as successful. No automatic working-directory/project selection is implied.

For process polling/timeouts and explicit handle ownership, see [IO.Async](async.md); those process handles are distinct from language Tasks.

## Native Library Lifetime

Choose a library built for the target OS/architecture/ABI. `LoadLibrary` registers available Kinal metadata when supported; GetSymbol itself does not validate that a raw address has your intended C signature. Bind with a matching `Delegate ... By C`.

Every successful load owns a loader reference. Close it when no calls/callbacks can use it; the final close invalidates symbols/metadata owned by that library. Do not invoke a retained delegate after final unload. Repeated loads can return the same handle value with separate reference ownership.

Dynamic loading is a Native host facility, not arbitrary portable KNC FFI. For a complete delegate/load/error/close program see [FFI](../language/ffi.md). `LastError` is host-dependent text, so avoid tests that require identical wording on every platform.

## See Also

- [File and Directory](filesystem.md)
- [FFI](../language/ffi.md)
- [IO.Async](async.md)
- [Memory and Lifetimes](../advanced/memory-management.md)
