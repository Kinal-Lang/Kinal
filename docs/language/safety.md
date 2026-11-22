# Safety Levels

Safety annotations control which operations and calls the compiler permits. They are a language boundary, not a proof that foreign code, raw addresses, resource ownership or every runtime behavior is correct.

## Call Rules and Defaults

| Caller | Safe callee | Trusted callee | Unsafe callee | Checked raw pointer operations |
|--------|-------------|----------------|---------------|--------------------------------|
| `Safe` | Allowed | Allowed | Rejected | Rejected |
| `Trusted` | Allowed | Allowed | Allowed | Allowed |
| `Unsafe` | Allowed | Allowed | Allowed | Allowed |

An ordinary unannotated function defaults to **Safe**, not unrestricted execution. Extern declarations default to Unsafe when no explicit stronger boundary is provided by the supported declaration rules. Calls through delegates and callable wrappers also need the appropriate safety context.

`Trusted` deliberately exposes a callable interface to Safe callers while allowing its implementation to perform unsafe work. The author is responsible for validating inputs and maintaining that interface's contract. The current C compiler does not prohibit pointer manipulation merely because the body is Trusted.

## Safe Code

```kinal
Unit Docs.SafeArithmetic;
Get IO.Console;

Safe Function int Add(int left, int right)
{
    Return left + right;
}

Static Safe Function int Main()
{
    IO.Console.PrintLine(Add(3, 4));
    Return 0;
}
```

Safe functions may use ordinary language values, managed allocation, control flow and permitted library calls. They cannot directly call an Unsafe function or perform the protected raw pointer dereference/arithmetic and pointer/integer conversion operations. A name such as `ReadAllText` does not determine safety: use the actual API declaration.

## Trusted FFI Bridge

This is a Native example; arbitrary C calls are not a portable KNC feature:

```kinal
Unit Docs.TrustedBridge;
Get IO.Console;

Extern Unsafe Function int abs(int value) By C;

Trusted Function int Absolute(int value)
{
    Return abs(value);
}

Static Safe Function int Main()
{
    IO.Console.PrintLine(Absolute(-7)); // 7
    Return 0;
}
```

The Safe caller trusts `Absolute`'s contract. Passing an invalid foreign pointer, using the wrong ABI width or unloading a library too early is not repaired by marking a wrapper Trusted. See [FFI](ffi.md).

## Raw Pointer Implementations

Trusted and Unsafe implementations can perform raw memory operations. The choice communicates whether callers are meant to rely on an audited safe interface or must opt into an unsafe call boundary.

```kinal
Unsafe Function byte ReadByte(byte* address)
{
    Return address[0];
}
```

The caller must supply a valid accessible address. Pointer syntax alone carries no allocation size, lifetime or ownership proof. `IO.Memory` provides Trusted byte-copy/fill/compare helpers with similarly explicit buffer obligations; see [Memory and Lifetimes](../advanced/memory-management.md).

## Blocks and Entry Points

Block literals can carry safety annotations:

```kinal
Unsafe Block RawRegion </
    byte* address = null;
/>;
```

An explicitly Unsafe Block is an intentional boundary; it does not mark all code in the enclosing file as audited. `Unsafe Alias` is a separate file-local token-preprocessing feature, not another spelling for an ordinary unsafe memory block. See [Blocks](blocks.md) and [Modules](modules.md).

A hosted Main can be Safe, Trusted or Unsafe, but Unsafe Main produces a warning. That warning is not a linker failure. With `--Werror`, a warning can become a build failure. Prefer a narrow bridge or explicit unsafe region when only a small operation requires it.

## What Annotations Do Not Promise

- Safe code can still throw exceptions, fail allocations, exhaust resources or call an incorrectly implemented Trusted wrapper.
- A Trusted declaration does not automatically sanitize paths, check lengths, secure network transport or release handles.
- Unsafe permits operations; it does not make invalid operations valid.
- Native and VM share frontend safety checks, but backend/runtime support remains a separate requirement.

## See Also

- [Functions](functions.md)
- [FFI](ffi.md)
- [Memory and Lifetimes](../advanced/memory-management.md)
- [Backend Support](../advanced/backend-support.md)
