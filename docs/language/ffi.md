# FFI — Foreign Function Interface

Kinal provides native C ABI integration through `Extern` declarations and runtime-bindable `Delegate` slots. The supported boundary includes explicit scalar/pointer marshalling; it does not cover arbitrary C types, varargs or every aggregate ABI. See [Backend Support](../advanced/backend-support.md) for VM limits.

## Extern Functions

`Extern` declares an external C function; the calling convention is specified with `By`:

```kinal
Extern Function int puts(string text) By C;
Extern Function void* malloc(usize size) By C;
Extern Function void  free(void* ptr)   By C;
```

### Calling Conventions

| Keyword | Description |
|---------|-------------|
| `By C` | C calling convention (`cdecl`) — default |
| `By System` | System calling convention (`stdcall` on Windows) |

### C ABI Scalar Mapping

`By C`, `By System`, and `Delegate ... By C` use an explicit boundary ABI. Kinal's
internal representation does not change; arguments and return values are marshalled
at the call boundary.

| Kinal type | C ABI representation |
|------------|----------------------|
| `bool` | 32-bit C truth value / Win32 `BOOL`; zero is false, non-zero is true |
| `int` | 32-bit signed C `int` |
| `i8`…`i64`, `u8`…`u64` | Matching fixed-width integer |
| `isize`, `usize` | Target pointer-width signed/unsigned integer |
| `f32`, `f64`, `float` | C `float`, C `double`, C `double` |
| `string` | Null-terminated `const char*` |
| `T*` | C pointer |

Kinal `int` remains a 64-bit language value. Passing it to C truncates it to a
32-bit C `int`; a C `int` result is sign-extended back to Kinal `int`. Prefer
fixed-width types when the foreign declaration is not literally a C `int`.
By-value arrays, classes, packages, and structs are not part of FFI v1; pass them
through pointers. C `_Bool`, C `long`, variadic calls, and platform-specific
aggregate ABIs require an explicit wrapper when their representation differs from
the table above.

```kinal
Extern Function int MessageBoxA(byte* hWnd, string text, string caption, int type) By System;
```

### The LinkName Attribute

If the C function name differs from the Kinal declaration name, use `[LinkName]` to specify the actual symbol name at link time:

```kinal
[LinkName("__kn_time_tick")]
Extern Function u64 GetTimeTick() By C;
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
Extern Function u64 __kn_time_tick() By C;

Trusted Function u64 GetTick()
{
    Return __kn_time_tick();   // calling Extern inside a Trusted function
}

Safe Function void Demo()
{
    u64 t = GetTick();  // Safe functions can call Trusted
}
```

Calling directly from an `Unsafe` function:

```kinal
Unsafe Function void DirectCall()
{
    u64 t = __kn_time_tick();  // direct call
}
```

## Delegate Functions (Function Pointer Types)

`Delegate` is used to declare a **function pointer variable that can be bound at runtime**:

```kinal
[SymbolName("Add")]
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

[SymbolName("Add")]
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

Kinal callable representation to pointer (not a substitute for a C callback adapter):

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
Extern Function usize strlen(string s) By C;

Trusted Function usize GetLen(string s)
{
    Return strlen(s);
}
```

## Structs and C Structs

Match the C field widths, offsets, alignment and packing explicitly. `By Packed` is appropriate only if the C declaration is also packed; it is not a universal C-layout switch:

```kinal
Struct CPoint
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
    draw_point(&pt);
}
```

## Extern System Calling Convention (Windows)

`By System` selects the platform system convention. On 32-bit Windows this includes stdcall; 64-bit Windows uses its unified ABI. Match the target declaration:

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
Extern Function f64 sqrt(f64 x)   By C;

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


## Array Parameters, Ownership and Deployment

C ABI array parameters decay to their data pointer. A C pointer returned with a Kinal `T[]` result has unknown length zero; indexing still requires caller-provided bounds knowledge. A Kinal fixed array field is a descriptor/backing representation, not an inline C array field. Do not mirror C array-containing structs by merely writing `T[N]` fields.

Borrowed strings/arrays and callbacks need a valid lifetime for every foreign use. Release foreign allocations with their matching allocator and close libraries only after all calls/callbacks have stopped. Raw function-object representation casts do not create a C ABI callback; use `Delegate ... By C` and its supported adapter path.

The complete `sqrt` example uses `f64`, not the nonexistent language type `double`. On Linux link the system math library, for example `kinal build ffi.kn -l m -o ffi`; platform CRT/linker selection must provide `puts`/`abs`/`sqrt`. Dynamic-loader examples additionally need a target-compatible library exporting `Add`; pass its path after `--` when using `kinal run`.

Source strings use byte-oriented UTF-8 text, but Windows argv/filesystem/ANSI API boundaries are not a general UTF-8 guarantee. Choose the foreign API's encoding explicitly. See [Memory and Lifetimes](../advanced/memory-management.md).

## Build the Dynamic-Loader C Fixture

Save this as add.c. On Windows use clang -shared add.c -o add.dll; on Linux use clang -shared -fPIC add.c -o libadd.so; on macOS use clang -dynamiclib add.c -o libadd.dylib. Run kinal run loader.kn -- /absolute/path/to/library. The consumer above should print 42 and close the library after its final call.

```c
#if defined(_WIN32)
__declspec(dllexport)
#endif
int Add(int a, int b)
{
    return a + b;
}
```

C int matches the Delegate int boundary conversion here. By System differs from By C in 32-bit Windows stack cleanup; x64/ARM64 follow their target ABI rather than the 32-bit stdcall stack rules.
