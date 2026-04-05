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
```

### Splitting and Joining

| Function | Signature | Description |
|----------|-----------|-------------|
| `Split(s, sep)` | `string, string → list` | Split by delimiter, returns a `list` |
| `Join(parts, sep)` | `list, string → string` | Join strings in a list with a delimiter |
| `Lines(s)` | `string → list` | Split by lines |

```kinal
list parts = IO.Text.Split("a,b,c", ",");
// parts = ["a", "b", "c"]

string joined = IO.Text.Join(parts, " | ");
// "a | b | c"

list lines = IO.Text.Lines("line1\nline2\nline3");
// ["line1", "line2", "line3"]
```

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
