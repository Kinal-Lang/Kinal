# Memory and Lifetimes

This page connects language value semantics, the hosted collector, raw memory helpers, and C interop. It describes the C compiler; Native memory layouts and VM value representations are not interchangeable ABIs.

## Values, References, and Aliasing

| Value | Assignment behavior |
|-------|---------------------|
| Numbers, bool, char, enum | Copy the value |
| Struct and Package | Copy the value/fields; contained references still refer to shared storage |
| Class instance | Copy an object reference; fields remain shared |
| Array | Copy a descriptor/reference to backing elements; ordinary assignment is not a deep clone |
| list, dict, set | Copy a collection reference; mutations are shared |
| string | Copy an immutable text reference; text operations can create new strings |

A fixed array bound is part of its static type. It does not change descriptor-copy assignment into an element copy or a resize:

```kinal
Unit Docs.ArrayStorage;
Get IO.Console;

Static Function int Main()
{
    int[3] first = { 1, 2 }; // declaration literal allocates three elements
    int[5] alias = first;   // aliases existing backing; actual length remains three
    alias[0] = 9;
    IO.Console.PrintLine(first[0]);       // 9
    IO.Console.PrintLine(alias.Length()); // 3

    int[] extended = first.Add(4); // new outer backing, length four
    extended[0] = 7;
    IO.Console.PrintLine(first[0]); // still 9
    Return 0;
}
```

Nested references in copied structs, Package values or arrays remain shared. `.Add` copies the outer array backing; it does not recursively clone referenced inner arrays/objects. Declaration initialization from a shorter literal zero-pads fixed storage; later assignment, argument passing and returning an array do not generally pad it.

## Hosted Garbage Collection

The selfhost hosted runtime stores a 16-byte header for each managed allocation.
The process footprint also depends on the allocator's size classes, live payloads,
temporary mark/sort storage, loaded code and stacks. A collection can free objects
while the system allocator keeps pages available for reuse. Windows measurements
for the compiler, a generated native program and KinalVM are in the
[heap benchmark](../../benchmarks/README.md#compiler-and-program-memory-on-windows).

When the self-hosted runtime collects a large heap, it sorts allocation addresses using the mark stack as temporary storage. A registered root can hold an interior pointer, such as the address of an array element. The collector locates and retains the allocation containing that address. [Collector measurements](../../benchmarks/README.md#hosted-collector-address-index)

Hosted Native output uses the supplied managed runtime. The C implementation tracks managed allocations and scans registered roots/interior pointers and the active native stack conservatively. Global/static roots and escaping closure storage are retained through the runtime's root mechanisms.

```kinal
Unit Docs.Collection;
Get IO.Console;
Get IO.GC;

Static Function int Main()
{
    string[] values = { "kept", "alive" };
    IO.GC.Collect();
    IO.Console.PrintLine(values[0]); // kept
    Return 0;
}
```

`IO.GC.Collect()` requests collection; it is not deterministic destruction. Conservative reachability can retain values longer than their last source-level use. Scope exit does not promise immediate memory release, and allocation count alone is not a guarantee of collection timing.

GC manages Kinal-managed storage, not every operating-system resource. Close dynamic libraries, files/handles, sockets, server instances and foreign allocations using their owning API. Do not assume a collected object automatically releases arbitrary external resources.

## KinalVM garbage collection

KinalVM runs a collection automatically as guest allocations accumulate. Calling `IO.GC.Collect()` runs a collection before the call returns. In KinalVM, collection starts from active frames, globals, exceptions and guest values retained by the host. It follows object fields, arrays, closure environments and interior pointers. Unreachable cycles can be collected. In a custom VM intrinsic, retain a guest value across callbacks with `int token = ctx.Heap.Pin(value)` and release it with `ctx.Heap.Unpin(token)` when its owner closes. The Windows desktop runner uses this mechanism for open surfaces.

## Closures and Captured Variables

An escaping callable can keep its captured storage alive. Captures of one declaration share its cell, so changes are visible to sibling callables. A variable declared inside a loop has a distinct declaration instance per iteration; a variable declared outside the loop remains shared. Copying a callable reference does not clone its captured state.

Captured references retain their referenced objects; capturing a struct does not recursively deep-copy the references stored in it. See [Functions](../language/functions.md) and [Blocks](../language/blocks.md) for callable syntax and control flow.

## `IO.Memory` Raw Operations

`IO.Memory` belongs to `IO.Core` and is implemented using self-contained byte loops, so importing it does not require the hosted GC. Import `Get IO.Memory;`. The helpers are declared Trusted and can be called from Safe code; raw pointer operations themselves remain subject to the safety model.

| Function | Signature | Behavior |
|----------|-----------|----------|
| `Copy` | `void Copy(byte* destination, byte* source, usize count)` | Forward byte copy; null source/destination is a no-op |
| `Set` | `void Set(byte* destination, byte value, usize count)` | Fill bytes; null destination is a no-op |
| `Zero` | `void Zero(byte* destination, usize count)` | Fill zero bytes; null destination is a no-op |
| `Compare` | `int Compare(byte* left, byte* right, usize count)` | Lexicographic byte comparison, returning -1/0/1 |

`Compare` returns 0 for identical pointers or zero count; otherwise null sorts before a non-null pointer. These helpers neither allocate nor verify the actual buffer size. `Copy` is a forward copy, not an overlap-safe `memmove`; use non-overlapping regions. Counts are bytes, not typed element counts.

## FFI Ownership and Lifetime

- Match the C declaration exactly. Kinal `int` is internally i64 but marshals as C `int`/i32; fixed-width integers and pointer-sized values have their documented ABI.
- Passing a string/array pointer borrows its backing for the call. A foreign API retaining it needs a lifetime/ownership strategy; a borrowed pointer alone is not a universal GC-retention contract.
- Foreign heap memory must be released through the matching foreign allocator, never an unrelated Kinal allocation path.
- Array parameters decay to data pointers. A foreign pointer returned as `T[]` has unknown Kinal length zero; a pointer being indexable does not prove the access is in bounds.
- Keep callbacks and their captures alive while C can invoke them. Do not call a symbol/delegate after its library is finally unloaded.
- Struct/Package by-value C ABI and arbitrary varargs are outside the supported boundary; use explicit C wrappers.

See [FFI](../language/ffi.md) for declarations, delegates, link metadata and array marshalling. Safety annotations regulate allowed operations; they do not validate foreign addresses, buffer lengths, ownership or callback lifetime.

## Freestanding Profiles

| Profile | Responsibility |
|---------|----------------|
| `none` | No general managed heap; use supported core operations and target-owned storage |
| `alloc` | Supply allocator/string hooks plus frame/root hooks; non-collecting implementations can make root hooks no-ops |
| `gc` | Supply allocator, collector and root/lifetime hooks |

`--runtime alloc` and `gc` do not bundle an embedded allocator or collector. `libs/runtime/include/kn/freestanding.h` declares core hook ABI, including `__kn_gc_alloc`, frame/root functions, string concatenation and collection. Collection, exception and async operations need their corresponding additional hooks when used.

A freestanding object compiling successfully does not establish that its final executable has all runtime symbols, startup initialization, memory mapping and platform services. Link and execute the actual target image as a separate validation step.

## See Also

- [Types](../language/types.md)
- [Variables](../language/variables.md)
- [Safety](../language/safety.md)
- [Collections](../stdlib/collections.md)
- [Backend Support](backend-support.md)
- [Freestanding](freestanding.md)
