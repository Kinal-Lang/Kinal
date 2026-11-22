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
int x;          // numeric storage starts at zero; explicit initialization is clearer
string text;
```

The current compiler accepts these declarations and zero-initializes numeric/fixed-array storage. There is no general definite-assignment error as described by older documentation. Initialize references explicitly: default string representation currently differs between Native and VM (see [Backend Support](../advanced/backend-support.md)).

### Array-Style Declaration

```kinal
int[3] nums;             // fixed array with zeroed elements
int[] vals = { 1, 2, 3 }; // dynamic array
```

In hosted programs, `int[2][4] grid;` creates four independent rows of two integers. The rightmost bound is the outer length. With `int[2][4] grid = {{7}};`, the first row is `{7, 0}` and each remaining row is `{0, 0}`. With `int[][4] grid;`, each row starts empty. Placing an existing array variable in the initializer shares its backing with that row. Padding applies at declaration. A literal supplied by a later assignment, a function argument or a return expression keeps its actual row lengths.

In hosted programs, a default struct gets storage for each of its fixed-array fields. Given `Struct Row { int[3] Cells; }`, declaring `Row[2] rows;` creates two independent three-element arrays. Writing to `rows[0].Cells[1]` leaves `rows[1].Cells[1]` untouched. Copying an existing row copies its array descriptor, so the copy shares that row's cells.

The same rule applies to local variables, globals, static fields and fields of a new object. Constructors can use these arrays immediately. A global initializer can write to a later declaration's default array. If that later declaration has an explicit initializer, it still runs in declaration order.

In a hosted program, a typed Package initializes each element according to its declared type. For example, after `<int[2], int> pair;`, `pair[0].Length()` is 2 and `pair[1]` is 0. Assigning `default` to a fixed array, struct or typed Package creates fresh storage for its fixed-array members. Existing aliases still refer to the previous arrays.

A recursive structure can use a dynamic array such as `Struct Node { Node[] Children; }`. A positive fixed bound on that field would require infinitely many default elements and is rejected when the compiler constructs the default value.

## Constants

Use `Const` to declare a binding that cannot be reassigned:

```kinal
Const int MaxSize = 100;
Const string Version = "1.0.0";
Const bool Debug = true;
```

A `Const` declaration requires an initializer and rejects reassignment. Its initializer can be a runtime expression, including a function call; `Const` alone does not force compile-time evaluation. Contexts that require an integer constant expression, such as an array bound, still require an evaluable expression.

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

Both prefix (`++i`, `--i`) and postfix (`i++`, `i--`) forms are supported. Prefix returns the updated value; postfix returns the old value.

## Scope

Variables follow block scoping rules:

```kinal
Get IO.Console;

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
string s = "";       // explicitly request an empty string
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

