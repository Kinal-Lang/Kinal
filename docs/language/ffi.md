# FFI — Foreign Function Interface

Kinal provides complete C interoperability through `Extern` (declaring C functions) and `Delegate` (declaring function pointer types), enabling zero-overhead integration with C libraries.

## Extern Functions

`Extern` declares an external C function; the calling convention is specified with `By`:

```kinal
Extern Function int printf(string fmt) By C;
Extern Function void* malloc(usize size) By C;
Extern Function void  free(void* ptr)   By C;
```

### Calling Conventions

| Keyword | Description |
|---------|-------------|
| `By C` | C calling convention (`cdecl`) — default |
| `By System` | System calling convention (`stdcall` on Windows) |

```kinal
Extern Function int MessageBoxA(byte* hWnd, string text, string caption, int type) By System;
```

### The LinkName Attribute

If the C function name differs from the Kinal declaration name, use `[LinkName]` to specify the actual symbol name at link time:

```kinal
[LinkName("__kn_time_tick")]
Extern Function int GetTimeTick() By C;
```

### The LinkLib Attribute

Automatically link the specified library:

```kinal
[LinkLib("sqlite3")]
Extern Function int sqlite3_open(string filename, byte** db) By C;
```

This is equivalent to passing `-l sqlite3` at compile time, but more intuitive and co-located with the code.

### Calling Extern Functions

`Extern` function calls require a `Trusted` or `Unsafe` context:

```kinal
Extern Function int __kn_time_tick() By C;

Trusted Function int GetTick()
{
    Return __kn_time_tick();   // calling Extern inside a Trusted function
}

Safe Function void Demo()
{
    int t = GetTick();  // Safe functions can call Trusted
}
```

Calling directly from an `Unsafe` function:

```kinal
Unsafe Function void DirectCall()
{
    int t = __kn_time_tick();  // direct call
}
```

## Delegate Functions (Function Pointer Types)

`Delegate` is used to declare a **function pointer variable that can be bound at runtime**:

```kinal
[SymbolName("MyLib.Add")]
Delegate Function int dyn_add(int a, int b) By C;
```

After declaration, `dyn_add` is a globally assignable variable. After dynamically loading a library via `IO.System`, assign the function pointer to it:

```kinal
byte* lib = IO.System.LoadLibrary("mylib.so");
byte* ptr = IO.System.GetSymbol(lib, dyn_add);  // look up symbol by Delegate name
dyn_add = ptr;  // bind

// Call like a regular function thereafter
int result = dyn_add(3, 4);
```

Complete dynamic library loading example:

```kinal
Unit App.DynLoader;

Get IO.Console;
Get IO.System;

[SymbolName("MyLib.Add")]
Delegate Function int dyn_add(int a, int b) By C;

Unsafe Static Function int Main(string[] args)
{
    If (args.Length() == 0)


    {
        IO.Console.PrintLine("Missing library path argument");
        Return 1;
    }

    byte* lib = IO.System.LoadLibrary(args[0]);
    If (lib == null)


    {
        IO.Console.PrintLine(IO.System.LastError());
        Return 2;
    }

    byte* addPtr = IO.System.GetSymbol(lib, dyn_add);
    If (addPtr == null)


    {
        IO.Console.PrintLine(IO.System.LastError());
        IO.System.CloseLibrary(lib);
        Return 3;
    }

    dyn_add = addPtr;
    IO.Console.PrintLine(dyn_add(20, 22));  // 42

    IO.System.CloseLibrary(lib);
    Return 0;
}
```

## Pointer Operations (Unsafe Feature)

Raw pointers can be used in `Unsafe` contexts:

```kinal
Unsafe Function void MemCopy(byte* dst, byte* src, usize len)
{
    For (usize i = 0; i < len; i++)


    {
        dst[i] = src[i];
    }
}
```

Pointer arithmetic:

```kinal
Unsafe Function void PrintBytes(byte* ptr, int count)
{
    For (int i = 0; i < count; i++)


    {
        IO.Console.PrintLine([int](ptr[i]));
    }
}
```

Multi-level pointers:

```kinal
Unsafe Function byte** GetAllocTable()
{
    // ...
}
```

## Pointer Type Casting

In Unsafe contexts, use `[type*](expression)` for pointer type casting:

```kinal
Unsafe Function void Demo(byte* raw)
{
    int* ip = [int*](raw);   // byte* to int*
    *ip = 42;
}
```

Function object to pointer:

```kinal
Unsafe Function void GetFuncPtr()
{
    IO.Type.Object.Function fn = MyFunc;
    byte* ptr = [byte*](fn);
}
```

## String and C String Interoperability

Kinal's `string` is internally a null-terminated byte sequence and can be passed directly to C functions expecting `const char*`:

```kinal
Extern Function int strlen(string s) By C;

Trusted Function int GetLen(string s)
{
    Return strlen(s);
}
```

## Structs and C Structs

Use `Struct By Packed` to ensure layout matches C:

```kinal
Struct CPoint By Packed
{
    i32 x;
    i32 y;
}

Extern Function void draw_point(CPoint* p) By C;

Unsafe Function void Demo()
{
    CPoint pt;
    pt.x = 10;
    pt.y = 20;
    draw_point([CPoint*]([byte*](pt)));
}
```

## Extern System Calling Convention (Windows)

Windows APIs (such as `CreateFile`, `VirtualAlloc`, etc.) use the `stdcall` convention:

```kinal
[LinkName("VirtualAlloc")]
Extern Function byte* VirtualAlloc(
    byte* lpAddress,
    usize dwSize,
    u32 flAllocationType,
    u32 flProtect
) By System;
```

## Linking Considerations

Manually linking external libraries:
```bash
kinal build main.kn -l mylib -L ./lib/ -o app
```

Or declare in code with the `[LinkLib]` attribute (recommended):
```kinal
[LinkLib("mylib")]
Extern Function void mylib_init() By C;
```

## Complete C Interoperability Example

```kinal
Unit App.FFIDemo;

Get IO.Console;

// Declare C standard library functions
Extern Function int    puts(string s)    By C;
Extern Function int    abs(int n)        By C;
Extern Function double sqrt(double x)   By C;

Trusted Function void Demo()
{
    puts("Hello from puts!");
    IO.Console.PrintLine([int](abs(-99)));
    IO.Console.PrintLine(sqrt(2.0));
}

Static Function int Main()
{
    Demo();
    Return 0;
}
```

## Next Steps

- [Safety Levels](safety.md)
- [Meta Metadata](meta.md)
- [Bare-Metal and Embedded](../advanced/freestanding.md)
