# Variables and Constants

## Variable Declaration

Kinal variable declarations come in two forms: **explicit type** and **type inference**.

### Explicit Type Declaration

```kinal
int count = 0;
string name = "Kinal";
bool flag = true;
float pi = 3.14159;
```

Format: `type variableName = initialValue;`

### Type Inference (Var)

Use the `Var` keyword to let the compiler infer the type from the initial value:

```kinal
Var count = 0;       // inferred as int
Var name = "Kinal";  // inferred as string
Var flag = true;     // inferred as bool
Var obj = New MyClass();  // inferred as MyClass
```

`Var` is only usable when the initial value's type is unambiguous; you cannot declare an uninitialized `Var` variable.

### Declaration Without Initialization

Explicit type declarations can omit the initial value:

```kinal
int x;          // x is uninitialized, to be assigned later
string text;
```

> **Warning**: An uninitialized variable must be assigned before it is used, otherwise a compile error will occur.

### Array-Style Declaration

```kinal
int nums[3];           // fixed-length array, uninitialized
int vals[] = { 1, 2, 3 }; // dynamic array
```

## Constants

Use `Const` to declare compile-time constants:

```kinal
Const int MaxSize = 100;
Const string Version = "1.0.0";
Const bool Debug = true;
```

Constants must be assigned at declaration, and the value must be a compile-time evaluable literal. Constants cannot be reassigned.

## Variable Assignment

```kinal
count = 10;
name = "World";
```

Compound assignment operators:

```kinal
count += 5;    // count = count + 5
count -= 2;    // count = count - 2
count *= 3;    // count = count * 3
count /= 2;    // count = count / 2
count %= 7;    // count = count % 7
```

Increment/decrement:

```kinal
count++;   // post-increment
count--;   // post-decrement
```

> Kinal currently only supports **postfix** increment/decrement (`i++`), not prefix (`++i`).

## Scope

Variables follow block scoping rules:

```kinal
Static Function int Main()
{
    int x = 1;          // function scope

    If (x > 0)


    {
        int y = 2;      // If block scope
        x = y;          // valid: x is visible in the outer scope
    }
    // y is not accessible here

    For (int i = 0; i < 3; i++)


    {
        IO.Console.PrintLine(i);  // i is only visible inside the for loop
    }

    Return x;
}
```

## The `default` Value

`default` represents the zero value of a type, commonly used in function calls with default arguments:

```kinal
Function int Add(int a = 1, int b = 2)
{
    Return a + b;
}

Add(default, default);  // uses both defaults: 1 + 2 = 3
Add(10, default);       // 10 + 2 = 12
Add(default, 5);        // 1 + 5 = 6
```

`default` can also be assigned to a variable to represent the zero value of that type:
```kinal
int n = default;     // 0
string s = default;  // "" (empty string)
bool b = default;    // false
```

## The `null` Value

Reference types (class instances, string, any, list, dict, etc.) can be assigned `null`:

```kinal
string s = null;
MyClass obj = null;
any a = null;

If (s == null)


{
    IO.Console.PrintLine("null string reference");
}
```

Primitive value types (`int`, `bool`, `char`, etc.) **cannot** be `null`.

## Global Variables

Variables declared outside of functions are global variables:

```kinal
Unit App;

int GlobalCounter = 0;   // global variable

Static Function void Increment()
{
    GlobalCounter++;
}
```

Global reference types can default to `null` and be initialized before use:

```kinal
list GlobalList = null;

Static Function void Init()
{
    GlobalList = list.Create();
}
```

## Variable Shadowing

A variable can be declared in an inner scope with the same name as one in an outer scope (shadowing), though this is generally discouraged:

```kinal
int x = 1;
If (true)


{
    int x = 2;  // shadows outer x; x = 2 within this block
    IO.Console.PrintLine(x);  // 2
}
IO.Console.PrintLine(x);  // 1 (outer x was not modified)
```

## Next Steps

- [Operators](operators.md)
- [Control Flow](control-flow.md)
- [Functions](functions.md)
