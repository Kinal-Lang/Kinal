# Structs and Enums

## Struct

A struct is a **value type**, allocated on the stack. Assignment copies the value (unlike the reference semantics of classes).

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
Struct Pair By Align(8)
{
    int a;
    int b;
}
```

### Structs and FFI

Structs are commonly used for interoperability with C `struct`:

```kinal
Struct sockaddr_in By Packed
{
    u16 sin_family;
    u16 sin_port;
    u32 sin_addr;
    byte[8] sin_zero;
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

Supported underlying types: `i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64`, `int`, `byte`.

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

Enum members can be accessed via the type name; static member call syntax is also supported:

```kinal
// The following two forms are equivalent
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
