# IO.Type: Conversions and Value Inspection

These helpers are compiler builtins; no extra package installation is needed. See [Types](../language/types.md) for declarations and numeric widths, and [Backend Support](../advanced/backend-support.md) for Native/VM differences.

## Explicit Conversions

Prefer `[type](expression)`; the old `int(value)` form is not supported. `IO.Type.<name>(any value)` is the builtin function form. Each name below takes one `any` argument, returns the corresponding type, and is Safe:

| Names | Return types |
|------|------|
| `bool`, `byte`, `char`, `int`, `float` | Same as each name |
| `i8`, `i16`, `i32`, `i64` | Same as each name |
| `u8`, `u16`, `u32`, `u64` | Same as each name |
| `isize`, `usize` | Same as each name |
| `f16`, `f32`, `f64`, `f128` | Same as each name |
| `string`, `any`, `dict`, `list` | Same as each name |

`any` is a dynamic carrier, not a guarantee that arbitrary conversions succeed. Validate external text and ranges before conversion. Narrow integers, signed/unsigned conversions and floating-point conversions follow target representation rules; the VM does not promise f16/f128 precision parity. Converting to dict/list does not parse arbitrary objects into collections; use `IO.Json` for JSON validation and parsing.

## any Member Helpers

Signatures below omit the `any value` receiver. For example, `value.IsInt()` corresponds to `IO.Type.any.IsInt(value)`.

| Member signature | Meaning |
|------|------|
| `bool IsNull()` | Null predicate |
| `string ToString()` | Display representation |
| `bool Equals(any other)` | Value equality |
| `int Tag()` | Internal representation tag; do not persist as a public type ID |
| `string TypeName()` | Runtime representation name |
| `bool IsNumber()` | Numeric representation predicate |
| `bool IsPointer()` | Raw pointer predicate |
| `bool IsInt()` | Integer predicate |
| `bool IsFloat()` | Floating-point predicate |
| `bool IsString()` | String predicate |
| `bool IsBool()` | Boolean predicate |
| `bool IsChar()` | Character predicate |
| `bool IsObject()` | Object predicate |

These predicates inspect runtime representation, not complete source-level type reflection. `value.TypeOf()` describes the compile-time type; `value.TypeName()` describes the backend representation. A fixed array can report `int[]` on Native and `array` on VM. Do not use tag numbers or display names as serialization schemas, FFI layout descriptions or fixed-array bounds.

## string Member Helpers

| Member signature | Meaning |
|------|------|
| `int Length()` | Byte length |
| `string Concat(string other)` | Concatenate text |
| `bool Equals(string other)` | Text equality |
| `char[] ToChars()` | Convert to a character array |

All are Safe. Strings are NUL-terminated byte text; length, indexing and character arrays do not implement Unicode grapheme processing. See [IO.Text](text.md) for searching, slicing and replacement. Arrays have their own language operations such as `Length()` and `Add()`; do not confuse them with list methods.

## Complete Example

```kinal
Get IO.Console;

Static Function int Main()
{
    any value = 42;
    IO.Console.PrintLine(value.IsInt(), value.IsNumber());
    IO.Console.PrintLine([string](value));
    string text = "Kinal";
    IO.Console.PrintLine(text.Length(), text.Equals("Kinal"));
    char[] chars = text.ToChars();
    IO.Console.PrintLine(chars.Length());
    Return 0;
}
```

```text
true true
42
5 true
5
```

See [Exceptions](../language/exceptions.md) for `IO.Error`, [Memory and Lifetimes](../advanced/memory-management.md) for `IO.GC`/`IO.Memory`, and [Freestanding](../advanced/freestanding.md) for `IO.Volatile`/`IO.Panic`.
