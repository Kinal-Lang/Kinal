# Type System

Kinal is a statically typed language; the type of every value is determined at compile time. This document describes the type categories and how to use them.

## Primitive Types

### Boolean Type

```kinal
bool flag = true;
bool done = false;
```

Literals: `true` / `false`

### Integer Types

| Type | Bit Width | Signed | Range |
|------|-----------|--------|-------|
| `byte` | 8 | No | 0 ~ 255 |
| `int` | 64 | Yes | Signed 64-bit language integer |
| `i8` | 8 | Yes | -128 ~ 127 |
| `i16` | 16 | Yes | -32768 ~ 32767 |
| `i32` | 32 | Yes | -2147483648 ~ 2147483647 |
| `i64` | 64 | Yes | ±9.2×10¹⁸ |
| `u8` | 8 | No | 0 ~ 255 |
| `u16` | 16 | No | 0 ~ 65535 |
| `u32` | 32 | No | 0 ~ 4294967295 |
| `u64` | 64 | No | 0 ~ 1.8×10¹⁹ |
| `isize` | Platform pointer width | Yes | Platform-dependent |
| `usize` | Platform pointer width | No | Platform-dependent |

```kinal
int n = 42;
i32 small = -100;
u64 big = 18446744073709551615;
byte b = 0xFF;
```

> **Note**: A `byte` value that overflows (> 255) will produce a warning or error at compile time.

### Floating-Point Types

| Type | Standard |
|------|----------|
| `float` | IEEE 754 binary64, like `f64` |
| `f16` | IEEE 754 half precision |
| `f32` | IEEE 754 single precision |
| `f64` | IEEE 754 double precision |
| `f128` | Quadruple precision |

```kinal
float pi = 3.14159;
f32 x = 1.5f;
```

### Character Type

`char` represents a single byte character (ASCII character):

```kinal
char c = 'A';
char newline = '\n';
char quote = '\'';
int code = [int](c);   // 65
```

Escape sequences: `\0`, `\n`, `\r`, `\t`, `\b`, `\\`, `\'`, `\"`

### String Type

`string` is an immutable byte sequence (UTF-8 friendly):

```kinal
string name = "Kinal";
string path = "C:\\Users";
```

Strings can be concatenated directly with `+`:
```kinal
string greeting = "Hello, " + name + "!";
```

In a loop, `text += part;` and `text = text + part;` can reuse the space reserved for a local string in KinalNative. A saved value keeps its original contents. For example, after `string saved = text;`, later appends to `text` leave `saved` intact. Use `IO.Text.Builder.EnsureCapacity` when you know the expected output size in advance.

String prefixes (see [String Features](strings.md)):
```kinal
string raw = [r]"no escapes\n raw string";
string fmt = [f]"value is {name}";     // format string
char[] chars = [c]"abc";               // convert to char array
```

### The `any` Type

`any` can hold a value of any type (boxing mechanism):

```kinal
any value = 42;
any obj = "hello";
any none = null;

value.IsNull();      // false
value.TypeName();    // "int"
value.ToString();    // "42"
value.IsInt();       // true
```

Type checking and conversion:
```kinal
any x = 100;
If (x.IsInt())
{
    int n = [int](x);
}
```

## Arrays

### Fixed-Length Arrays

```kinal
int[3] a = { 1, 2, 3 };
int[5] b = { 1, 2 };    // remaining elements default to zero
```

A local numeric array starts with zeroed elements each time its declaration executes, including inside a loop. KinalNative can reuse the backing storage of large fixed arrays that are only indexed locally. Recursive or concurrent calls have independent active arrays. Returning an array, capturing it in a closure, or returning a pointer to one of its elements preserves the original storage.

### Dynamic Arrays

```kinal
int[] nums = { 1, 2, 3 };
int[] empty = {};
```

### Array Operations

```kinal
int len = nums.Length();
int first = nums[0];
nums[1] = 99;

// Arrays support the Add method (returns a new array)
nums = nums.Add(4);
```

### Multi-Dimensional Arrays

Kinal supports nested array syntax. `T[][]` is an array of `T[]`, and `T[][][]` is an array of `T[][]`.

```kinal
int[][] grid = {
    { 1, 2 },
    { 3 }
};

string[][][] words = {
    {
        { "a", "b" },
        { "c" }
    }
};

any[][][] values = {
    {
        { 1, "two" },
        { true }
    }
};
```

Nested arrays are jagged, so inner arrays can have different lengths. When using `any`, write the array rank explicitly, such as `any[][][]`.

## Packages

`Object.Package` is a compile-time structural value for returning or passing a fixed group of values without declaring a new `struct`.

Package types use angle brackets:

```kinal
Function <int, string> Status()
{
    Return <200, "OK">;
}
```

Repeated element types can be written with a repeat suffix:

```kinal
Function <int(2), string(2)> Repeated()
{
    Return <1, 2, "left", "right">;
}
```

Use `Object.Package` when you want the variable type inferred from the initializer:

```kinal
Object.Package status = Status();
IO.Console.PrintLine(status[0]);  // 200
IO.Console.PrintLine(status[1]);  // OK
```

Package indexes must be compile-time constants.

You can attach field aliases to one variable declaration. Aliases are local to that variable and are not part of the Package type:

```kinal
Object.Package status<code, message> = Status();
IO.Console.PrintLine(status.code);
IO.Console.PrintLine(status.message);
```

Package values can be explicitly materialized into arrays:

```kinal
any[] items = [any[]](status);
int[] nums = [int[]](<1, 2, 3>);
```

Empty packages are not supported. Package v1 does not support destructuring, named package literals, or named package types.

## Pointers

Raw pointer operations require a Trusted or Unsafe context; an Unsafe function makes the caller boundary explicit:

```kinal
Unsafe Function void Demo()
{
    byte* ptr = null;
    int* ip = null;
}
```

Deep pointers: `byte**` (pointer to pointer).

Pointers are primarily used for FFI interoperability and low-level memory operations; they should be avoided in everyday development.

See [Safety Levels](safety.md) and [FFI](ffi.md) for details.

## Reference Types

### class

`class` instances are reference types (heap-allocated), created with `New`:

```kinal
Class Point
{
    Public int X;
    Public int Y;

    Public Constructor(int x, int y)
    {
        This.X = x;
        This.Y = y;
    }
}

Point p = New Point(3, 4);
p.X = 10;
```

Class variables can be assigned `null`:
```kinal
Point q = null;
If (q != null)
{
    ...
}
```

### Type Checking: `Is`

```kinal
Animal a = New Dog();
IO.Console.PrintLine(a Is Dog);     // true
IO.Console.PrintLine(a Is Animal);  // true
IO.Console.PrintLine(a Is Cat);     // false
```

Pattern matching (combined with `If`):
```kinal
If (a Is Dog dog)
{
    // dog is a type-checked Dog instance
    dog.Bark();
}
```

### list (Dynamic List)

```kinal
list l = list.Create();
l.Add("apple");
l.Add("banana");
int count = l.Count();            // 2
any val = l.Fetch(0);             // "apple"
string s = [string](l.Peek());    // last item
```

### dict (Dictionary)

```kinal
dict d = dict.Create();
d.Set("name", "Kinal");
d.Set("version", 1);
string name = [string](d.Fetch("name"));
bool has = d.Contains("version"); // true
```

### set (Set)

```kinal
set s = set.Create();
s.Add("a");
s.Add("b");
bool has = s.Contains("a"); // true
```

## Value Types

### struct

`Struct` has value semantics: assignments copy its fields. Its storage can be local, global, inside another value, or retained by a managed capture; value semantics do not promise stack-only allocation. Fields are accessed directly:

```kinal
Struct Point
{
    int X;
    int Y;
}
```

See [Structs and Enums](structs-enums.md) for details.


## Representation, Aliasing and Backend Boundaries

Language int is 64-bit, while the By C boundary maps it to 32-bit C int. Native target support for f16/f128 is separate from VM precision. Array assignment copies a descriptor and shares elements; Add allocates new outer storage. Fixed-length declaration initializers may zero-pad, while later assignment, argument and return-length checks do not mean every conversion pads. See [Memory and Lifetimes](../advanced/memory-management.md) for complete examples.

any uses a dynamic representation rather than full source-level reflection; TypeOf() and TypeName() differ. See [IO.Type](../stdlib/types.md) for all conversion/value helpers, and [Collections](../stdlib/collections.md) for list/dict/set creation and key/value constraints. Both KNC emitters support these collections.

## Explicit Conversion and Builtin Object Types

Use `[type](value)` for conversions. Both KNC compilers can unbox scalar values, for example `[int](items.Fetch(0))`. Use `Is` to check a dynamic type when the caller requires a particular kind of value.

| Type | Purpose |
|------|---------|
| `IO.Type.Object.Class` | Builtin class-object root; strings/arrays do not thereby share its layout |
| `IO.Type.Object.Function` | Function objects and supported callable references |
| `IO.Type.Object.Block` | Block objects with explicit execution APIs |
| `IO.Type.Object.Package` / `Object.Package` | Placeholder used to infer a concrete structural Package from its initializer |
| `IO.Error` | Standard error class with Title, Message, Inner and Trace |

```kinal
Get IO.Console;

IO.Type.Object.Function fn = Function int(int value) { Return value + 1; };
int result = fn(41);
IO.Type.Object.Class obj = fn;
IO.Console.PrintLine(result);        // 42
IO.Console.PrintLine(obj.TypeOf());  // static type spelling
```

The example is a fragment; put the local declarations and calls inside Main when running it. See [Functions](functions.md), [Packages](functions.md), [Blocks](blocks.md), [Exceptions](exceptions.md), and [IO.Type](../stdlib/types.md) for their contracts.
