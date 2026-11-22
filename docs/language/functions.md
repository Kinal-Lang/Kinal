# Functions

Functions declare a result type, name, parameters and body. Examples below are independent unless identified as fragments; a file has one program entry point.

## Declarations and Entry Points

```kinal
Function int Add(int left, int right)
{
    Return left + right;
}
```

Use `void` for no result and `Return;` for an early void return. A non-void function needs a return value on every required path; returning a value from void is invalid. Ordinary top-level functions and `Static Function` utilities are supported. Within a class, Static distinguishes class methods from instance methods.

Choose one hosted entry signature, not both in the same file:

```kinal
Static Function int Main()
{
    Return 0;
}
```

Alternatively use `Static Function int Main(string[] args)`. `kinal run file.kn -- first second` forwards program arguments. Freestanding normally uses `KMain` or an explicit entry symbol. See [Compiler CLI](../cli/compiler.md).

## Defaults and Named Arguments

```kinal
Unit Docs.Arguments;
Get IO.Console;

Function int Add(int left = 1, int right = 2)
{
    Return left + right;
}

Static Function int Main()
{
    IO.Console.PrintLine(Add());                       // 3
    IO.Console.PrintLine(Add(10, default));            // 12
    IO.Console.PrintLine(Add(right = 5, left = 10));    // 15
    IO.Console.PrintLine(Add(left = default, right = 5)); // 6
    Return 0;
}
```

Defaults follow required parameters. Omitted defaulted parameters and explicit `default` arguments use the declared parameter default; `default` is not a request to ignore that default and always pass a zero value. A parameter without a default cannot be filled with `default`. Named arguments bind to declared parameter names; unknown names, duplicate bindings, missing required arguments and incompatible types are errors.

Function arguments have ordinary value/reference semantics. Passing two scalar parameters to a `Swap<T>(T a, T b)` function only swaps its local copies. To change caller state, use an intentional mutable reference, return updated values (for example a Package), or use a supported low-level pointer API. See [Memory and Lifetimes](../advanced/memory-management.md).

## Generics and Recursion

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

Function int Fibonacci(int n)
{
    If (n <= 1) { Return n; }
    Return Fibonacci(n - 1) + Fibonacci(n - 2);
}
```

Call a generic with explicit types, such as `Identity<int>(42)`. The instantiated body must be valid for those types and the output backend. Recursion uses runtime stack/storage; no unlimited recursion or tail-call guarantee is implied. See [Generics](generics.md).

## Anonymous Functions and Closures

```kinal
Unit Docs.Callables;
Get IO.Console;

Function IO.Type.Object.Function MakeCounter(int start)
{
    int count = start;
    Return Function int()
    {
        count++;
        Return count;
    };
}

Static Function int Main()
{
    Var add = Function int(int left, int right)
    {
        Return left + right;
    };
    IO.Console.PrintLine(add(3, 4)); // 7

    IO.Type.Object.Function counter = MakeCounter(10);
    IO.Console.PrintLine(counter()); // 11
    IO.Console.PrintLine(counter()); // 12
    Return 0;
}
```

Function literals use `Function ResultType(parameters) { ... }`. Captured variables share their declaration's managed storage, so an escaping callable can retain and update its state after the creator returns. Copying a callable reference shares that state. Block objects have different control-flow operations; see [Blocks](blocks.md).

A function reference can also be stored in `IO.Type.Object.Function`, for example `IO.Console.PrintLine`, after importing `IO.Console`. A callable is a managed Kinal object, not automatically an arbitrary C function pointer. Unsafe representation casts do not replace the C delegate adapter ABI.

## Explicit Overloads

Every overload participant needs both attributes and a distinct parameter-type signature:

```kinal
[Overload]
[SymbolName("DescribeInt")]
Static Function string Describe(int value) { Return "int"; }

[Overload]
[SymbolName("DescribeText")]
Static Function string Describe(string value) { Return "string"; }
```

Calls use `Describe(...)`; `SymbolName` identifies the compiled symbol. Return type alone does not distinguish overloads. Multiple assignable candidates produce an ambiguity diagnostic. Generic functions cannot use `[Overload]` in the current implementation. The same explicit attributes apply to supported class/interface method overload sets.

Splitting a unit across files keeps its top-level functions in one namespace. An ordinary function can be defined once in that unit. Each overload needs a distinct parameter-type list and a compiled name that is free in the unit. For example, `Box` and `Models.Box` denote the same parameter type inside `Unit Models`. A separate unit can define its own function of the same name.

## Multiple Results

```kinal
Unit Docs.Results;
Get IO.Console;

Function <int, string> Status()
{
    Return <200, "OK">;
}

Static Function int Main()
{
    Object.Package status<code, message> = Status();
    IO.Console.PrintLine(status.code);
    IO.Console.PrintLine(status.message);
    Return 0;
}
```

Package aliases belong to the declaration, not the type. Use constant indexes or declared aliases; Package v1 has no destructuring. See [Types](types.md#packages).

## Safety, Async, Extern and Delegate

| Form | Purpose |
|------|---------|
| `Safe Function` | Safe implementation; can call Safe and Trusted functions |
| `Trusted Function` | Audited bridge allowed to call Unsafe operations |
| `Unsafe Function` | Explicitly unsafe implementation, including raw pointer operations |
| `Async Function` | Async task-producing function; declared result is the awaited value type |
| `Extern ... By C` | Foreign symbol bound by the native linker |
| `Delegate ... By C` | Runtime-bindable C ABI callable slot |
| `Delegate ... By Kinal` | Runtime-bindable Kinal callable slot |

Extern and C delegate signatures use explicit C ABI marshalling. Delegates are not generic or async. `Async` return wrapping and invocation differ from ordinary functions; see [Async](async.md), [Safety](safety.md) and [FFI](ffi.md).

Local declarations inside Block literals are described in [Blocks](blocks.md). Cross-unit functions must be imported and called with a visible qualified name or explicit symbol alias; an ordinary `Get` does not inject every function into bare-name scope.
