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
    Print<string>(Describe<bool>(true));

    Return 0;
}
```

Output:
```
7
2.7
value: true
```

## Next Steps

- [Functions](functions.md)
- [Classes and Inheritance](classes.md)
