# Generics

Kinal supports function-level generics, using type parameters to implement reusable algorithms.

## Generic Functions

Use `<T>` to declare type parameters:

```kinal
Function T Identity<T>(T value)
{
    Return value;
}
```

Specify the concrete type at the call site:

```kinal
int i = Identity<int>(42);
string s = Identity<string>("hello");
```

The caller resolves explicit type arguments using its own imports. The
function's declared parameter and return types use the imports where the
function is defined. For example, a library can declare
`Function Model.Box Select<T>(T[] tags, Model.Box[] boxes)` with its own
`Model` alias. A caller that imports the same module as `Data` can pass a
`Data.Box[]` and receive a `Data.Box`. This also works when the library's
functions are split across several files in one unit.

A class method or property accessor can call a generic function directly.
For example, this getter specializes `ReadCount` for `Counter`. The compiler
checks that `Counter.Count` exists and that its type matches the return type.

```kinal
Function int ReadCount<T>(T item) { Return item.Count; }

Class Counter
{
    Public int Count;
    Public Property int Total { Get { Return ReadCount<Counter>(This); } }
}
```

## Multiple Type Parameters

```kinal
Function T First<T>(T a, T b)
{
    Return a;
}

Function void Swap<T>(T a, T b)
{
    T tmp = a;
    a = b;
    b = tmp;
}
```

Multiple type parameters are separated by commas:

```kinal
Function void Pair<A, B>(A first, B second)
{
    IO.Console.PrintLine([string](first));
    IO.Console.PrintLine([string](second));
}

Pair<int, string>(42, "hello");
```

## Generics and Type Safety

Type parameters are concretized at compile time; a separate implementation is generated for each type combination, maintaining type safety:

```kinal
Function bool AreEqual<T>(T a, T b)
{
    Return a == b;
}

AreEqual<int>(1, 1);          // true
AreEqual<string>("a", "b");   // false
AreEqual<bool>(true, false);  // false
```

## Value Semantics and Composition

`Swap<T>(T a, T b)` only swaps its parameter copies; it does not update caller scalar variables. Use a returned Package or an intentional mutable reference when caller state must change. Each instantiated body is checked normally: `AreEqual<int[]>(...)`, for example, is invalid because arrays do not support direct aggregate equality.

The current parser rejects some nested explicit generic-call spellings, including `Print<string>(Describe<bool>(true))`. Store the inner result in a typed local before passing it to the outer call, as in the complete example. This is an implementation boundary, not a rule that generics implicitly infer all nested type arguments.

## Current Limitations

Kinal currently supports generics **at the function level only**. Not yet supported:
- Generic classes (`Class Container<T>`)
- Generic interfaces
- Type constraints (e.g., `where T : IShape`)

These features may be added as the language evolves.

## Complete Example

```kinal
Unit App.Generics;

Get IO.Console;

// Generic maximum
Function T Max<T>(T a, T b)
{
    If (a > b)
    {
        Return a;
    }
    Return b;
}

// Generic print
Function void Print<T>(T value)
{
    IO.Console.PrintLine([string](value));
}

// Generic container wrapper
Function string Describe<T>(T value)
{
    Return "value: " + [string](value);
}

Static Function int Main()
{
    int maxInt = Max<int>(3, 7);
    float maxFloat = Max<float>(1.5, 2.7);

    Print<int>(maxInt);
    Print<float>(maxFloat);
    string description = Describe<bool>(true);
    Print<string>(description);

    Return 0;
}
```

Output:
```
7
2.7000000000000002
value: true
```

## Next Steps

- [Functions](functions.md)
- [Classes and Inheritance](classes.md)

Generic body support depends on the backend; see [Backend Support](../advanced/backend-support.md).

To adapt a generic function to a function object, call Identity<int>(value) inside an anonymous function with concrete parameter/return types. The current parser does not accept a standalone Identity<int> reference expression.
