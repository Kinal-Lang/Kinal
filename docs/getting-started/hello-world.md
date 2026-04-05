# Hello World

This guide walks you through your first Kinal program — from writing to running.

## Your First Program

Create a new file `hello.kn` and enter:

```kinal
Unit App;
Get IO.Console;

Static Function int Main()
{
    IO.Console.PrintLine("Hello, Kinal!");
    Return 0;
}
```

## Line-by-Line Breakdown

```kinal
Unit App;
```
Declares the **unit** that the current source file belongs to — similar to a module or namespace in other languages.

```kinal
Get IO.Console;
```
Imports the standard library module `IO.Console`, making `IO.Console.PrintLine(...)` available for console output.

```kinal
Static Function int Main()
{
    ...
}
```
The program entry point. `Static` means this is a static function (no class instance needed); the return type is `int`; returning `0` signals success.

```kinal
IO.Console.PrintLine("Hello, Kinal!");
```
Outputs a line of text to the console.

```kinal
Return 0;
```
Returns exit code 0.

## Compiling and Running

### Option 1: Compile to a native executable

```bash
kinal build hello.kn -o hello
./hello         # Linux/macOS
hello.exe       # Windows
```

Output:
```
Hello, Kinal!
```

### Option 2: Run directly via KinalVM (no LLVM required)

```bash
kinal vm run hello.kn
```

The compiler compiles `.kn` to `.knc` bytecode, which KinalVM immediately executes.

### Option 3: Generate bytecode only

```bash
kinal vm build hello.kn -o hello.knc
kinalvm hello.knc
```

## Entry Point with Command-Line Arguments

`Main` can also accept command-line arguments:

```kinal
Unit App;
Get IO.Console;

Static Function int Main(string[] args)
{
    If (args.Length() > 0)
    {
        IO.Console.PrintLine("First argument: " + args[0]);
    }
    Return 0;
}
```

## Async Entry Point

If the program needs top-level `Await`, declare `Main` as async:

```kinal
Unit App;
Get IO.Console;
Get IO.Async;

Static Async Function int Main()
{
    Await IO.Async.Sleep(100);
    IO.Console.PrintLine("Async Hello!");
    Return 0;
}
```

## Common Errors

| Error Message | Cause | Fix |
|---------------|-------|-----|
| `Missing ';' after Unit` | Semicolon missing at end of Unit line | Add `;` |
| `Expected Semicolon` | Statement is missing a trailing semicolon | Add `;` |
| `kinalvm: command not found` | KinalVM not installed | See [Installation Guide](installation.md) |

## Next Steps

- [Project Structure](project-structure.md) — Multi-file and package organization
- [Language Overview](../language/overview.md) — Dive deeper into Kinal's core concepts
