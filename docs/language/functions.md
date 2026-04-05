# Functions

## Function Declaration

Basic syntax:

```kinal
Function ReturnType FunctionName(ParameterList)
{
    // function body
    Return value;
}
```

Examples:

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}

Function string Greet(string name)
{
    Return "Hello, " + name + "!";
}

Function void PrintValue(int n)
{
    Console.PrintLine(n);
    // No Return needed for void functions
}
```

## Static Functions

The `Static` modifier indicates the function does not belong to any class instance:

```kinal
Static Function int Square(int n)


{
    Return n * n;
}
```

Module-level (global) utility functions are typically declared as `Static`. The program entry point `Main` must be `Static`:

```kinal
Static Function int Main()


{
    Return 0;
}

// Version accepting command-line arguments
Static Function int Main(string[] args)


{
    Return 0;
}
```

## Default Parameter Values

Parameters can have default values; parameters with defaults must come after parameters without:

```kinal
Function int Power(int base, int exp = 2)


{
    int result = 1;
    For (int i = 0; i < exp; i++)


    {
        result = result * base;
    }
    Return result;
}

Power(3);       // 9 (exp = 2)
Power(3, 3);    // 27
```

## Named Arguments

When calling a function, arguments can be specified by name, regardless of their order:

```kinal
Function string Format(string text, int indent = 0, bool uppercase = false)
{
    // ...
}

Format(text = "hello", uppercase = true);   // indent uses default value
Format(uppercase = true, text = "world");   // order can be arbitrary
```

The `default` keyword can be used explicitly in named argument calls to use the type's default value:

```kinal
Add(p1 = default, p2 = 5);    // p1 uses default value 1
```

## Generic Functions

Use `<T>` to declare type parameters:

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

Function T First<T>(T a, T b)
{
    Return a;
}

// Specify type at call site
int i = Identity<int>(42);
string s = Identity<string>("hello");
int f = First<int>(3, 7);   // 3
```

Generics support multiple type parameters:

```kinal
Function void Swap<T>(T a, T b)
{
    T temp = a;
    a = b;
    b = temp;
}
```

## Anonymous Functions (Lambda)

Use `Function type(parameterList) { body }` syntax to create anonymous functions:

```kinal
// Assign to a variable
IO.Type.Object.Function fn = Function int(int a, int b)
{
    Return a + b;
};

// Or use Var for inference
Var add = Function int(int a, int b)
{
    Return a + b;
};

// Call
int result = add(3, 4);  // 7
```

Anonymous functions can **capture** outer variables (closures):

```kinal
string msg = "before";
IO.Type.Object.Block cap = Block </
    Console.PrintLine(msg);  // captures msg
/>;
msg = "after";
cap.Run();   // prints "after" (captures by reference)
```

See [Block](blocks.md) and [Async Programming](async.md) for details.

## Function Objects

Functions can be assigned to variables or passed as arguments:

```kinal
IO.Type.Object.Function p = IO.Console.PrintLine;
p("hello");  // call via function object

Var q = p;   // Var inference
q("world");
```

The type of a function object is `IO.Type.Object.Function`, which can be assigned to `IO.Type.Object.Class`:

```kinal
IO.Type.Object.Class c = p;
IO.Console.PrintLine(c.TypeOf());  // prints the type name
```

## Function Pointers (Low-Level Interop)

In `Unsafe` contexts, function objects can be converted to raw pointers:

```kinal
Unsafe Function void Demo()
{
    IO.Type.Object.Function fn = IO.Console.PrintLine;
    byte* ptr = [byte*](fn);   // convert to raw function pointer
}
```

See [FFI](ffi.md) for details.

## `Delegate` Functions (C Callback Bridging)

`Delegate` declarations are used to map a C function pointer to a Kinal-callable symbol:

```kinal
[SymbolName("MyLib.Callback")]
Delegate Function int my_callback(int code) By C;
```

See [FFI](ffi.md) for details.

## Safety Level Modifiers

Functions can declare a safety level (see [Safety Levels](safety.md)):

```kinal
Safe Function void SafeOp() { ... }       // can only call Safe functions
Trusted Function void Bridge() { ... }    // can call Unsafe
Unsafe Function void RawOp() { ... }      // can use raw pointers
```

## Async Functions

```kinal
Async Function int Compute()


{
    int a = Await StepOne();
    int b = Await StepTwo(a);
    Return b;
}
```

See [Async Programming](async.md) for details.

## `Extern` Functions (C Interoperability)

```kinal
Extern Function int printf(string fmt) By C;
Extern Function void* malloc(usize size) By C;
```

See [FFI](ffi.md) for details.

## Recursion

Kinal fully supports recursion, including indirect recursion:

```kinal
Function int Fibonacci(int n)


{
    If (n <= 1)


    {
        Return n;
    }
    Return Fibonacci(n - 1) + Fibonacci(n - 2);
}
```

## Function Overloading

Kinal **does not support** overloading functions with the same name (i.e., two functions with the same name cannot exist within one Unit). Use different names or generics to distinguish functionality.

## Return Values

- `Return expression;` — returns a value
- `Return;` — early return from a `void` function
- If a function reaches the end without a `Return`, the compiler will report a "Missing Return" error (for non-`void` functions)

## Local Functions (Defined Inside a Block)

Local functions, classes, and other declarations can be defined inside Block literals (see [Block](blocks.md)):

```kinal
Block ScopeDecl
</
    Function int LocalAdd(int a, int b)


    {
        Return a + b;
    }
    IO.Console.PrintLine(LocalAdd(3, 4));
/>
ScopeDecl.Run();
```

## Next Steps

- [Classes and Inheritance](classes.md)
- [Generics](generics.md)
- [Async Programming](async.md)
- [Safety Levels](safety.md)
