# IO.System — System Interface

`IO.System` provides low-level system operations including process control, command-line arguments, and dynamic library loading.

> **Safety requirement**: `Exec`, `LoadLibrary`, `GetSymbol`, and `CloseLibrary` require `Trusted` or `Unsafe` safety level.
> `Exit`, `CommandLine`, `FileExists`, and `LastError` can be used in `Safe` contexts.

## Import

```kinal
Get IO.System;
```

---

## Function Reference

### Process

| Function | Signature | Description |
|----------|-----------|-------------|
| `Exit(code)` | `int → void` | Immediately terminate the current process with the specified exit code |
| `CommandLine()` | `→ list` | Returns the command-line argument list (`list` of `string`) |
| `Exec(cmd)` | `string → int` | (**Trusted**) Execute a shell command, returns the exit code |

```kinal
// Get command-line arguments
list args = IO.System.CommandLine();
int argc = IO.Collection.list.Count(args);
If (argc < 2)
{
    IO.Console.PrintLine("An argument is required");
    IO.System.Exit(1);
}

string firstArg = [string](IO.Collection.list.Fetch(args, 1));
IO.Console.PrintLine("Argument: " + firstArg);
```

```kinal
// Execute an external command (Trusted)
Trusted
{
    int code = IO.System.Exec("git status");
    If (code != 0)
    {
        IO.Console.PrintLine("Command execution failed");
    }
}
```

### File Checking

| Function | Signature | Description |
|----------|-----------|-------------|
| `FileExists(path)` | `string → bool` | Check if a file exists (same as `IO.File.Exists`, but without needing to import IO.File) |

```kinal
If (!IO.System.FileExists("config.json"))
{
    IO.Console.PrintLine("Config file not found");
    IO.System.Exit(1);
}
```

### Dynamic Libraries (Trusted)

| Function | Signature | Description |
|----------|-----------|-------------|
| `LoadLibrary(path)` | `string → any` | Load a dynamic library, returns a library handle |
| `GetSymbol(lib, name)` | `any, string → any` | Get a symbol (function pointer) from the library |
| `CloseLibrary(lib)` | `any → void` | Unload the dynamic library |

```kinal
Trusted
{
    any lib = IO.System.LoadLibrary("libmath.so");
    any sqrt = IO.System.GetSymbol(lib, "sqrt");

    // Used together with Delegate
    // Delegate Function float MathSqrt(float x) By C;
    // MathSqrt fn = [MathSqrt](sqrt);
    // float result = fn(16.0);

    IO.System.CloseLibrary(lib);
}
```

> Dynamic library loading is typically used together with FFI/Delegate; see [FFI](../language/ffi.md).

### Error Information

| Function | Signature | Description |
|----------|-----------|-------------|
| `LastError()` | `→ string` | Get a description string of the last system error |

```kinal
Trusted
{
    any lib = IO.System.LoadLibrary("missing.so");
    If (lib == null)
    {
        IO.Console.PrintLine("Load failed: " + IO.System.LastError());
    }
}
```

---

## Complete Example

### CLI Tool Framework

```kinal
Unit App.CLI;
Get IO.Console;
Get IO.System;
Get IO.Collection;
Get IO.Text;

Static Function void PrintUsage()
{
    IO.Console.PrintLine("Usage: myapp <command> [options]");
    IO.Console.PrintLine("Commands:");
    IO.Console.PrintLine("  build   - Build the project");
    IO.Console.PrintLine("  clean   - Clean build artifacts");
    IO.Console.PrintLine("  help    - Show help");
}

Static Function int Main()
{
    list args = IO.System.CommandLine();
    int argc  = IO.Collection.list.Count(args);

    If (argc < 2)
    {
        PrintUsage();
        Return 1;
    }

    string cmd = [string](IO.Collection.list.Fetch(args, 1));

    If (IO.Text.Contains(cmd, "build"))
    {
        IO.Console.PrintLine("Building...");
        Trusted { IO.System.Exec("make all"); }
    }
    Else If (IO.Text.Contains(cmd, "clean"))
    {
        IO.Console.PrintLine("Cleaning...");
        Trusted { IO.System.Exec("make clean"); }
    }
    Else If (IO.Text.Contains(cmd, "help"))
    {
        PrintUsage();
    }
    Else
    {
        IO.Console.PrintLine("Unknown command: " + cmd);
        Return 1;
    }

    Return 0;
}
```

---

## See Also

- [Safety Levels](../language/safety.md) — Using Trusted context
- [FFI](../language/ffi.md) — Working with dynamic library symbols
- [IO.Path](path.md) — Path operations
