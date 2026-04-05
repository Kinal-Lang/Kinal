# Safety Levels

Kinal has a built-in three-tier safety model that explicitly layers memory operations and dangerous function calls. This is one of Kinal's key design differences from many other languages — you always know whether a piece of code involves potentially dangerous operations.

## The Three Levels

| Level | Keyword | Description |
|-------|---------|-------------|
| Safe | `Safe` | Can only call functions marked with `Safe`; pointer operations are strictly forbidden |
| Trusted | `Trusted` | Can call both `Safe` and `Unsafe` functions; does not directly manipulate raw pointers |
| Unsafe | `Unsafe` | Can perform all operations, including raw pointers and direct memory operations |

When no safety level is specified, the default is unrestricted (equivalent to an uncontrolled environment).

## Safe

A `Safe` function guarantees it will not produce undefined behavior:

```kinal
Public Safe Function int Add(int a, int b)
{
    Return a + b;
}

Safe Function string BuildGreeting(string name)
{
    Return "Hello, " + name + "!";
}
```

Inside a Safe function:
- You cannot call `Trusted` or `Unsafe` functions
- You cannot use raw pointers (`byte*`, `int*`, etc.)
- You cannot use `Extern` FFI functions (unless the function is also annotated as `Safe`)

## Trusted

`Trusted` is the bridge between Safe and Unsafe:

```kinal
Trusted Function int ReadTick()
{
    Return __kn_time_tick();  // calling an external C function
}
```

A Trusted function:
- Can call `Unsafe` functions and `Extern` functions
- Does not directly manipulate raw pointers itself
- Can be called from `Safe` functions (hiding the dangerous details from the caller)

Typical use: wrapping low-level C APIs for use by the safe layer.

## Unsafe

An `Unsafe` function can perform all operations:

```kinal
Unsafe Function void Copy(byte* dst, byte* src, usize len)
{
    For (usize i = 0; i < len; i++)


    {
        dst[i] = src[i];
    }
}

Unsafe Function byte* Allocate(usize size)
{
    Return [byte*](IO.System.Malloc(size));
}
```

Unsafe can:
- Operate on raw pointers (read, write, arithmetic)
- Call `Extern` C functions
- Perform type pointer casts

## Safety Levels in Blocks

`Block` literals also support safety annotations:

```kinal
Unsafe Block UnsafeRegion </
    byte* ptr = null;  // pointer operations are allowed
/>
```

## Safety Level Propagation Rules

A function's safety level determines what it can call:

```
Safe    → can only call Safe
Trusted → can call Safe + Unsafe + Extern
Unsafe  → can call anything
```

This rule is enforced **at compile time**; violations produce compile errors.

## Why Safety Levels?

### The Problem Without Layers

If all code defaults to Unsafe, developers have a hard time distinguishing which code involves memory hazards.

### Kinal's Approach

- **Everyday code** is written in `Safe` functions; the compiler guarantees safety
- **Low-level adapter code** is written in `Trusted` functions, wrapping C calls
- **Extreme low-level code** is written in `Unsafe` functions, with explicit risk labeling

```kinal
// Safe layer: business logic
Safe Function void ProcessFile(string path)
{
    string content = IO.File.ReadAllText(path);  // IO.File functions are Safe
    // process content...
}

// Trusted layer: FFI wrapper
Extern Function void* fopen(string path, string mode) By C;

Trusted Function byte* OpenFileRaw(string path)
{
    Return [byte*](fopen(path, "rb"));  // wraps C function
}

// Unsafe layer: raw operations
Unsafe Function int ReadByte(byte* fp)
{
    // directly call low-level read function
}
```

## Practical Use Cases

### Interop Wrapping (Common Pattern)

```kinal
// 1. Declare C function (Extern has no safety level)
Extern Function int __kn_sys_file_read(byte* fp, byte* buf, int len) By C;

// 2. Trusted bridge layer
Trusted Function int FileRead(byte* fp, byte* buf, int len)
{
    Return __kn_sys_file_read(fp, buf, len);
}

// 3. Safe business layer (callers don't need to know low-level details)
Safe Function string ReadChunk(byte* fp, int len)
{
    // indirectly calls C via Trusted from within a Safe function
    Return BuildStringFromResult(FileRead(fp, null, len));
}
```

### Constraining Dangerous FFI Function Calls

```kinal
Extern Function void* LoadLibrary(string path) By C;

// Calling LoadLibrary directly requires the function to be Trusted or Unsafe
Trusted Function byte* LoadLib(string path)
{
    Return [byte*](LoadLibrary(path));
}
```

## Safety of the Program Entry Point

The `Main` function can be at any level, but annotating `Main` as `Unsafe` will generate a compile warning:

```kinal
// Recommended:
Static Safe Function int Main() { ... }
Static Function int Main() { ... }

// Will warn:
Static Unsafe Function int Main() { ... }
```

## Next Steps

- [FFI External Function Interface](ffi.md)
- [Async Programming](async.md)
- [Bare-Metal and Embedded](../advanced/freestanding.md)
