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
| `int` | 64 (platform-dependent) | Yes | Platform-native integer |
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
| `float` | Platform default float (typically 64-bit) |
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

Escape sequences: `\0`, `\n`, `\r`, `\t`, `\\`, `\'`, `\"`

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
int a[3] = { 1, 2, 3 };
int b[5] = { 1, 2 };    // remaining elements default to zero
```

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

Kinal currently supports one-dimensional arrays; multi-dimensional structures can be implemented via class encapsulation.

## Pointers

Pointers are an `Unsafe` feature and must be used in an `Unsafe` context:

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

`struct` is value-semantic (stack-allocated); fields are accessed directly:

```kinal
Struct Point
{
    int X;
    int Y;
}
```

See [Structs and Enums](structs-enums.md) for details.
