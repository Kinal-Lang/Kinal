# Language Overview

This document introduces Kinal's design philosophy and core features at a high level, helping you quickly build an overall understanding of the language.

## Design Philosophy

Kinal's core design goals:

1. **Clarity and Readability** — Keywords are capitalized, semantics are explicit, reducing ambiguity
2. **Layered Safety** — Memory operations and pointer access have clearly defined safety boundaries
3. **Zero-Cost Abstractions** — Object-oriented features compile to efficient native code
4. **Dual-Backend Flexibility** — The same code can be compiled to native binaries or interpreted in a VM
5. **Modular Standard Library** — The `IO.*` namespace provides standard functionality, importable as needed

## Language Style at a Glance

```kinal
Unit App.Demo;

Get IO.Console;

// Interface definition
Interface IShape
{
    Function float Area();
}

// Class definition, implementing an interface
Class Circle By IShape
{
    Private float radius;

    Public Constructor(float radius)
    {
        This.radius = radius;
    }

    Public Function float Area()
    {
        Return 3.14159 * This.radius * This.radius;
    }
}

// Generic function
Function T Max<T>(T a, T b)
{
    Return a > b ? a : b;  // Note: Kinal has no ternary operator, this is illustrative only
}

// Async function
Async Function string FetchName()
{
    Await IO.Async.Sleep(10);
    Return "Kinal";
}

// Program entry point
Static Function int Main()
{
    IShape s = New Circle(5.0);
    IO.Console.PrintLine([string](s.Area()));

    int bigger = Max<int>(3, 7);
    IO.Console.PrintLine(bigger);

    Return 0;
}
```

## Core Features Overview

### Type System

- **Primitive types**: `bool`, `byte`, `char`, `int`, `float`, `string`, `any`
- **Precise numerics**: `i8`/`u8` through `i64`/`u64`, `f16`/`f32`/`f64`/`f128`, `isize`/`usize`
- **Reference types**: `class` instances, built-in collections (`list`, `dict`, `set`)
- **Value types**: `struct` (value-semantic structs), `enum` (enumerations)
- **Pointers**: `byte*`, `T*` (only usable in `Unsafe` contexts)
- **Arrays**: `int[3]` (fixed-length), `int[]` (dynamic)
- **any type**: can hold any value, similar to a boxing mechanism

### Module System

```kinal
Unit MyApp;                      // Declares the unit name for the current file
Get IO.Console;                  // Opens the entire module
Get IO.Console;       // Import module with an alias
Get PrintLine By IO.Console.PrintLine; // Import a single symbol
```

### Safety Levels

```kinal
Safe Function void SafeOp() { ... }       // Can only call Safe functions
Trusted Function void BridgeOp() { ... }  // Can call Unsafe functions
Unsafe Function void DangerOp() { ... }   // Can perform pointer operations
```

See [Safety Levels](safety.md) for details.

### Object-Oriented

```kinal
Class Animal
{
    Public Virtual Function string Sound() { Return "..."; }
}

Class Dog By Animal
{
    Public Override Function string Sound() { Return "Woof!"; }
}
```

- Single inheritance (`By`)
- Multiple interface implementation
- `Virtual` / `Override` / `Abstract` / `Sealed`
- Access control: `Public` / `Private` / `Protected` / `Internal`

### Async/Concurrency

```kinal
Static Async Function int Main()
{
    int result = Await Compute();
    Return result;
}
```

- `Async Function` returns `IO.Async.Task`
- `Await` waits for a task to complete
- `IO.Async.Run(task)` synchronously waits for an async task's result

### Block (Code Block Objects)

Kinal's unique Block mechanism — encapsulates a section of code as an object, supporting deferred execution and label-based jumping:

```kinal
IO.Type.Object.Block flow = Block FlowControl </
    IO.Console.PrintLine("Step A");
    Record StepB
    IO.Console.PrintLine("Step B");
    Record StepC
    IO.Console.PrintLine("Step C");
/>;

flow.Run();                        // Execute all
flow.RunUntil(flow.StepB);        // Execute until StepB
flow.JumpAndRunUntil(flow.StepB, flow.StepC); // Run from StepB to StepC
```

See [Block](blocks.md) for details.

### Meta (Metadata Annotations)

```kinal
Meta Version(string text)
{
    On @function;
    Keep @compile;
}

[Version("1.0")]
Function void MyFunc() { ... }
```

- Custom attributes can be defined
- Supports `@source`, `@compile`, and `@runtime` lifecycles
- Can be queried at runtime via `IO.Meta`

### FFI (C Interoperability)

```kinal
Extern Function int printf(string fmt) By C;

Trusted Function void Demo()
{
    printf("Hello from C!\n");
}
```

See [FFI](ffi.md) for details.

## Compilation Pipeline Overview

```
.kn source files
    ↓ Lexical analysis (Lexer)
Token stream
    ↓ Syntax analysis (Parser)
AST (Abstract Syntax Tree)
    ↓ Semantic analysis (Sema)
Typed AST
    ↓ Code generation (Codegen via LLVM)
LLVM IR
    ↓ LLVM optimization + machine code generation
Object files .obj / .o
    ↓ Linking (LLD / MSVC / zig)
Executable / Library
```
