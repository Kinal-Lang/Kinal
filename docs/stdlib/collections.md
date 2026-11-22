# IO.Collection: list, dict and set

Native and KinalVM support all the operations below for `list`, `dict` and `set`, including dictionary literals and key/value snapshots. See [Backend Support](../advanced/backend-support.md).

These built-in reference types are available as `list`, `dict` and `set`. Assigning a collection shares it; it does not clone its contents. Create a collection before using it. `Get IO.Collection;` may be written explicitly, but the builtin aliases do not need a separate package import.

## Call Forms and Creation

```kinal
list names = list.Create();
dict settings = dict.Create();
set tags = set.Create();
```

Receiver and namespace forms are equivalent: `names.Add("Kinal")` and `IO.Collection.list.Add(names, "Kinal")`. The namespace form has the receiver as its first argument. The tables below show receiver-form parameters.

Arrays use `{ ... }` literals and are distinct from list. `list numbers = [1, 2, 3];` is not supported list-literal syntax. Use Create/Add. Dict literals use string-key/value pairs:

```kinal
dict options = { "name": "Kinal", "enabled": true };
```

## list: Ordered any Values

| Method | Result | Behavior |
|--------|--------|----------|
| `Add(any value)` | `void` | Append |
| `Fetch(int index)` | `any` | Read an element. An out-of-range index yields null. |
| `TryFetch(int index, any fallback)` | `any` | Read or return the explicit fallback |
| `Set(int index, any value)` | `void` | Replace an existing element |
| `Insert(int index, any value)` | `void` | Insert at an index |
| `RemoveAt(int index)` | `any` | Remove and return an element |
| `Pop()` | `any` | Remove and return the last element |
| `Peek()` | `any` | Read the last element without removal |
| `Contains(any value)` | `bool` | Membership using collection value equality |
| `IndexOf(any value)` | `int` | First matching index, or -1 |
| `Count()` | `int` | Element count |
| `IsEmpty()` | `bool` | Whether count is zero |
| `Clear()` | `void` | Remove all elements |

Indexes are zero-based. `Fetch` and `RemoveAt` return null for an absent index, as do `Pop` and `Peek` on an empty list. `TryFetch(index, fallback)` returns the fallback only for an absent index and preserves a stored null. `Set` updates an existing element. Use `Add` or `Insert` to grow the list.

`Clear`, `Pop` and `RemoveAt` release the list's references to discarded elements.
An object can then be collected once its other references, including any returned
value held by the caller, are gone. Native lists keep their capacity for reuse
after `Clear`.

On KinalVM, appending to a list takes amortized constant time. Inserting or removing
an element in the middle shifts subsequent elements. Growing a dictionary also
reserves space for later entries. These improvements apply to existing bytecode
when it runs on the updated VM. Repack standalone VM executables to include the new runner.

## dict: String Keys, any Values

| Method | Result | Behavior |
|--------|--------|----------|
| `Set(string key, any value)` | `void` | Insert or replace |
| `Fetch(string key)` | `any` | Read the value. A missing key yields null. |
| `TryFetch(string key, any fallback)` | `any` | Read or return the explicit fallback |
| `Contains(string key)` | `bool` | Test key presence |
| `Remove(string key)` | `bool` | Whether a key was removed |
| `Count()` | `int` | Entry count |
| `IsEmpty()` | `bool` | Whether count is zero |
| `Keys()` | `list` | Materialize key values |
| `Values()` | `list` | Materialize stored values |

Keys are strings, not arbitrary any values. `TryFetch` requires the fallback argument: `settings.TryFetch("missing", "default")`. A stored null is distinct from an absent key; use Contains when that distinction matters. Key/value enumeration order is not a public sorting guarantee.

## set: Unique Strings

| Method | Result | Behavior |
|--------|--------|----------|
| `Add(string value)` | `void` | Add; duplicate strings do not increase count |
| `Contains(string value)` | `bool` | Membership |
| `Remove(string value)` | `bool` | Whether an element was removed |
| `Count()` | `int` | Element count |
| `IsEmpty()` | `bool` | Whether count is zero |
| `Clear()` | `void` | Remove all elements |
| `Values()` | `list` | Materialize the strings |

Set elements are strings; this is not a generic mathematical set over every Kinal type. Do not depend on Values ordering. Sorting or deep semantic comparison is an application-level operation.

On Native and KinalVM, removing a dict key or set member keeps the existing table
and its capacity. Removal examines the neighboring entries that may share its
probe sequence. For fixed-length keys, the expected cost is constant with
well-distributed hashes and can reach linear time when many keys collide.
Updating an existing dict key or adding an existing set member also reuses the
current table.

## Complete Example

```kinal
Unit Docs.Collections;
Get IO.Console;

Static Function int Main()
{
    list names = list.Create();
    names.Add("Kinal");
    names.Insert(0, "Hello");
    IO.Console.PrintLine(names.Fetch(0));
    IO.Console.PrintLine(names.TryFetch(10, "fallback"));
    IO.Console.PrintLine(names.Pop());
    IO.Console.PrintLine(names.Count());

    dict settings = { "name": "Kinal" };
    settings.Set("mode", "native");
    IO.Console.PrintLine(settings.TryFetch("mode", "unknown"));
    IO.Console.PrintLine(settings.TryFetch("missing", "default"));
    IO.Console.PrintLine(settings.Remove("missing"));

    set tags = set.Create();
    tags.Add("language");
    tags.Add("language");
    IO.Console.PrintLine(tags.Count());
    IO.Console.PrintLine(tags.Contains("language"));
    Return 0;
}
```

```text
Hello
fallback
Kinal
1
native
default
false
1
true
```

This example runs on Native and KinalVM. Both `list.Fetch` and `dict.Fetch` return `any`. Read a stored integer with `[int](items.Fetch(index))`.

Foreach currently targets arrays/strings rather than a general collection iterator protocol. Iterate a list by Count/Fetch; materialize dict Keys or set Values when needed. Avoid mutating the traversal collection unless the algorithm defines the resulting index behavior.

## See Also

- [Types](../language/types.md)
- [JSON](json.md)
- [Memory and Lifetimes](../advanced/memory-management.md)
- [Backend Support](../advanced/backend-support.md)
