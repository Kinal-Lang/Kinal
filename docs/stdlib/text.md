# IO.Text — String Operations

`IO.Text` provides utility functions for string querying, transformation, splitting, and more.

## Import

```kinal
Get IO.Text;
```

## Function Reference

### Length and Checking

| Function | Signature | Description |
|----------|-----------|-------------|
| `Length(s)` | `string → int` | String byte length |
| `IsEmpty(s)` | `string → bool` | Whether the string is empty |
| `Contains(s, sub)` | `string, string → bool` | Whether the string contains a substring |
| `StartsWith(s, prefix)` | `string, string → bool` | Whether the string starts with the given prefix |
| `EndsWith(s, suffix)` | `string, string → bool` | Whether the string ends with the given suffix |

```kinal
string s = "Hello, Kinal!";

IO.Text.Length(s);              // 13
IO.Text.IsEmpty("");            // true
IO.Text.Contains(s, "Kinal");  // true
IO.Text.StartsWith(s, "Hello"); // true
IO.Text.EndsWith(s, "!");      // true
```

### Searching

| Function | Signature | Description |
|----------|-----------|-------------|
| `IndexOf(s, sub)` | `string, string → int` | Position of first occurrence, -1 if not found |
| `LastIndexOf(s, sub)` | `string, string → int` | Position of last occurrence |
| `Count(s, sub)` | `string, string → int` | Number of occurrences of the substring |

```kinal
IO.Text.IndexOf("abcabc", "bc");      // 1
IO.Text.LastIndexOf("abcabc", "bc"); // 4
IO.Text.Count("banana", "an");        // 2
```

Search positions count UTF-8 bytes. For example, `IndexOf("雪山雪", "山")`
returns 3. `LastIndexOf("aaaa", "aaa")` returns 1, while `Count("aaaa", "aaa")`
returns 1 because counting consumes each match. An empty pattern gives 0
for `IndexOf` and `Count`, and the text length for `LastIndexOf`.
`Replace` leaves the content unchanged for an empty pattern. `Split` returns a
single field for an empty separator and preserves empty fields around adjacent
or trailing separators.

### Transformation

| Function | Signature | Description |
|----------|-----------|-------------|
| `ToUpper(s)` | `string → string` | Convert to uppercase |
| `ToLower(s)` | `string → string` | Convert to lowercase |
| `Trim(s)` | `string → string` | Remove leading and trailing whitespace |
| `Replace(s, old, new)` | `string, string, string → string` | Replace all matching substrings |
| `Slice(s, start, length)` | `string, int, int → string` | Extract a substring |

```kinal
IO.Text.ToUpper("hello");            // "HELLO"
IO.Text.ToLower("WORLD");            // "world"
IO.Text.Trim("  hello  ");           // "hello"
IO.Text.Replace("aXbXc", "X", "-"); // "a-b-c"
IO.Text.Slice("Hello, World!", 7, 5); // "World"
IO.Text.Slice("a雪z", 1, 3);          // "雪" occupies three UTF-8 bytes
```

`Slice` clamps negative starts to zero and stops at the end of the text. A
nonpositive length returns an empty string. When extracting a fixed-size field
from a long record, pass its byte length directly. The native implementation
reads only as far as the requested range. `StartsWith` also stops as soon as
the prefix matches or a byte differs.

### Splitting and Joining

| Function | Signature | Description |
|----------|-----------|-------------|
| `Split(s, sep)` | `string, string → list` | Split by delimiter, returns a `list` |
| `Join(parts, sep)` | `list, string → string` | Join strings in a list with a delimiter |
| `Lines(s)` | `string → list` | Split by lines |

These three operations are available on Native and KinalVM. `Split` preserves empty fields, including a trailing empty field.

```kinal
list parts = IO.Text.Split("a,b,c", ",");
// parts = ["a", "b", "c"]

string joined = IO.Text.Join(parts, " | ");
// "a | b | c"

list lines = IO.Text.Lines("line1\nline2\nline3");
// ["line1", "line2", "line3"]
```

## Building text incrementally

Use `IO.Text.Builder` on either hosted Native or KinalVM to accumulate text when
the number of pieces is known only at run time. Its buffer grows geometrically.
`Build()` returns an independent string. `Clear()` resets the length and retains
the buffer for reuse.

```kinal
IO.Text.Builder line = New IO.Text.Builder();
line.EnsureCapacity(64);
line.Append("name=");
line.Append("雪山", 3);           // Append the first three UTF-8 bytes.
line.AppendChar('!');
string label = line.Build();     // "name=雪!"
line.Clear();                    // label keeps its contents.
```

The read-only `Capacity` property reports reserved bytes. `EnsureCapacity(minimumBytes)` reserves at least that many bytes and preserves the contents and length. Requests within the existing capacity reuse the buffer. A negative request raises `IO.Error`.

`Length` counts bytes. `Append(value, maxBytes = -1)` stops at the first NUL
or at the byte limit. A negative limit appends the whole string. A null value
appends zero bytes. `AppendChar('\0')` raises `IO.Error`. Choose byte limits at
UTF-8 character boundaries when you need valid UTF-8 output.

## Complete Example

```kinal
Unit App.TextDemo;
Get IO.Console;
Get IO.Text;

Static Function int Main()
{
    string text = "  Hello, Kinal World!  ";

    string trimmed  = IO.Text.Trim(text);
    string upper    = IO.Text.ToUpper(trimmed);
    string replaced = IO.Text.Replace(upper, "WORLD", "DEVELOPER");

    IO.Console.PrintLine(trimmed);   // Hello, Kinal World!
    IO.Console.PrintLine(upper);     // HELLO, KINAL WORLD!
    IO.Console.PrintLine(replaced);  // HELLO, KINAL DEVELOPER!

    list words = IO.Text.Split(trimmed, " ");
    IO.Console.PrintLine(IO.Collection.list.Count(words));  // 3

    string rejoined = IO.Text.Join(words, "-");
    IO.Console.PrintLine(rejoined);  // Hello,-Kinal-World!

    Return 0;
}
```

## See Also

- [String Features](../language/strings.md) — String literal prefixes
- [IO.Console](console.md) — Console output
- [IO.File](filesystem.md) — File text read/write

The 16 functions and `Builder` are supplied by IO.Core. Safe code can call
the Trusted text helpers. Case conversion operates on ASCII letters.
Strings end at NUL. For binary fields containing NUL bytes, use a byte buffer.

The [substring benchmark](../../benchmarks/text_search.kn) covers early hits,
near misses, overlapping prefixes, repeated replacements and trailing fields.
