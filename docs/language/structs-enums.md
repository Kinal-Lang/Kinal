# Structs and Enums

## Struct

A Struct is a **value type**: assignment copies its fields, while contained references remain shared. Storage is not restricted to the stack. See [Memory and Lifetimes](../advanced/memory-management.md).

### Declaration

```kinal
Struct Point
{
    int X;
    int Y;
}
```

Struct fields are public by default (slightly different from `Class`). Explicit access control can be added, but structs are typically used as lightweight data containers.

### Usage

```kinal
Point p;
p.X = 3;
p.Y = 4;

int dist = p.X * p.X + p.Y * p.Y;
```

Structs are not created with the `New` keyword; just declare the variable and fields default to zero values:

```kinal
Struct Rectangle
{
    int Width;
    int Height;
}

Rectangle r;
r.Width  = 100;
r.Height = 50;
int area = r.Width * r.Height;
```

### Packed Structs

The `By Packed` attribute causes struct fields to be tightly packed (no padding):

```kinal
Struct Header By Packed
{
    u16 Magic;
    u8  Version;
    u8  Flags;
}
```

### Memory Alignment

`By Align(n)` specifies byte alignment:

```kinal
Struct AlignedData By Align(8)
{
    int a;
    int b;
}
```

Multiple attributes can be combined:

```kinal
Struct Pair By Packed, Align(8)
{
    int a;
    int b;
}
```

### Structs and FFI

Structs are commonly used for interoperability with C `struct`:

```kinal
Struct CScalarHeader
{
    u16 Kind;
    u16 Flags;
    u32 Size;
}
```

See [FFI](ffi.md) for details.

---

## Enum

An enum defines a set of named integer constants.

### Declaration

```kinal
Enum Direction
{
    North,
    South,
    East,
    West
}
```

The default underlying type is `int`, with values auto-incrementing from 0.

### Explicit Values

```kinal
Enum HttpStatus
{
    Ok = 200,
    NotFound = 404,
    ServerError = 500
}
```

### Specifying the Underlying Type

Use `By type` to specify the underlying integer type:

```kinal
Enum Irq By u8
{
    Timer    = 0,
    Keyboard = 1,
    Serial   = 2
}

Enum BigFlag By u64
{
    None  = 0,
    Read  = 1,
    Write = 2,
    Exec  = 4
}
```

Supported underlying types: `i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64`, `int`, `byte`, `char`, `isize`, `usize`.

### Using Enums

```kinal
Direction d = Direction.North;

Switch (d)
{
    Case (Direction.North) { IO.Console.PrintLine("North"); }
    Case (Direction.South) { IO.Console.PrintLine("South"); }
    Case (Direction.East)  { IO.Console.PrintLine("East"); }
    Case (Direction.West)  { IO.Console.PrintLine("West"); }
}
```

Comparison:

```kinal
If (d == Direction.North)
{
    IO.Console.PrintLine("facing north");
}
```

Enum to integer:

```kinal
int val = [int](HttpStatus.Ok);    // 200
u8  irq = [u8](Irq.Timer);        // 0
```

Integer to enum:

```kinal
HttpStatus status = [HttpStatus](404);
```

### Enum Members as Static Constants

Enum members are accessed as constants through their type name:

```kinal
Direction d1 = Direction.North;
```

### Nested Enums (Inside a Class)

```kinal
Class Network
{
    Enum Protocol
    {
        TCP,
        UDP,
        ICMP
    }
}

Network.Protocol p = Network.Protocol.TCP;
```

---

## Complete Example

```kinal
Unit App.HardwareDemo;

Get IO.Console;

// Interrupt number enum
Enum IrqKind By u8
{
    Timer    = 0,
    Keyboard = 1,
    Network  = 2
}

// Interrupt descriptor struct (tightly packed)
Struct IrqDescriptor By Packed
{
    u8  irq;
    u16 handler;
    u8  flags;
}

Static Function void HandleIrq(IrqKind kind)
{
    Switch (kind)
    {
        Case (IrqKind.Timer)
        {
            IO.Console.PrintLine("Timer interrupt");
        }
        Case (IrqKind.Keyboard)
        {
            IO.Console.PrintLine("Keyboard interrupt");
        }
        Case (default)
        {
            IO.Console.PrintLine("Unknown interrupt");
        }
    }
}

Static Function int Main()
{
    IrqDescriptor desc;
    desc.irq     = [u8](IrqKind.Keyboard);
    desc.handler = 0x1000;
    desc.flags   = 0x01;

    HandleIrq([IrqKind](desc.irq));
    Return 0;
}
```

## Next Steps

- [Module System](modules.md)
- [FFI External Function Interface](ffi.md)
- [Safety Levels](safety.md)

Enum/integer conversion is explicit: integer assignment to an enum is rejected, and different enum types cannot be directly compared. Underlying width/signedness governs explicit narrowing. Packed/Align affect Kinal Native layout, not support for passing aggregates by value to C. In particular, fixed-array fields are descriptors rather than inline C arrays; a structure containing `byte[N]` is not automatically a C inline-array declaration. Use measured scalar layouts and pointer wrappers; see [FFI](ffi.md).
