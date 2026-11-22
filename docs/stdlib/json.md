# IO.Json

`IO.Json` provides JSON parsing and formatting on top of Kinal's built-in `dict`, `list`, and scalar values.

## Import

```kinal
Get IO.Json;
```

## Quick Start

```kinal
Get Console By IO.Console;
Get IO.Json;

Static Function int Main()
{
    dict root = IO.Json.Parse("{\"name\":\"kinal\",\"version\":1}");
    Console.PrintLine([string](root.Fetch("name")));
    Console.PrintLine([int](root.Fetch("version")));
    Return 0;
}
```

## Core Shape

`IO.Json` keeps JSON objects as `dict` and arrays as `list`. That is the primary surface. `IO.Json.Object` and `IO.Json.Value` are optional convenience wrappers on top of those raw values.

- `Parse(text)` returns `dict`
- `ParseValue(text)` returns `any`
- `Format(value, pretty = true)` accepts `dict`, `list`, strings, numbers, booleans, or `null`
- `Minify(text)` and `FormatText(text, pretty = true)` work on raw JSON text

`Format` and `FormatValue` write a Kinal `int` as exact decimal digits in both compact and pretty output. On a 64-bit target this includes values such as `9007199254740993`. Parsing uses cJSON's numeric representation. `ParseValue` returns an `int` for whole numbers in the signed 32-bit range and a floating-point value for other numbers. Store wide identifiers as JSON strings when they must survive parsing with every digit intact.

```kinal
dict data = IO.Json.Parse("{\"ok\":true}");
Console.PrintLine([bool](data.Fetch("ok")));

list items = IO.Json.AsList(IO.Json.ParseValue("[1,2,3]"));
Console.PrintLine(items.Count());
```

For an array of records, parse the document once and read each row through
`AsDict`. Arrays preserve input order. If an object repeats a key, the last
value is stored in the dictionary.

```kinal
list rows = IO.Json.AsList(IO.Json.ParseValue("[{\"name\":\"Ada\"},{\"name\":\"Lin\"}]"));
For (int i = 0; i < rows.Count(); i++)
{
    dict row = IO.Json.AsDict(rows.Fetch(i));
    IO.Console.PrintLine([string](row.Fetch("name")));
}
```

Parsing materializes the complete document in memory. The native reader
indexes array elements and object members once, then locates each child by
position in constant time.

## Property-Style Wrappers

`IO.Json.Object` and `IO.Json.Value` provide a property-style surface on top of raw `dict` / `list` / scalar values.

### IO.Json.Object

`IO.Json.Object` is a convenience wrapper when you want property-style access, but the underlying JSON object is still a `dict`.

Properties and helpers:

- `Data`
- `Count`
- `Keys`
- `Value(key)`
- `ValueOr(key, fallback)`
- `Put(key, value)`
- `Remove(key)`
- `Has(key)`

```kinal
dict data = IO.Json.Parse("{\"meta\":{\"stable\":true}}");
IO.Json.Object root = IO.Json.Wrap(data);
Console.PrintLine(root.Has("meta"));
Console.PrintLine(root.Value("meta").Dict.Fetch("stable"));

root.Put("name", "kinal");
Console.PrintLine(IO.Json.Format(data));
```

### IO.Json.Value

Properties:

- `Raw`
- `Dict`
- `List`
- `Text`
- `Int`
- `Float`
- `Bool`
- `IsNull`
- `IsDict`
- `IsList`

```kinal
IO.Json.Value value = root.Value("name");
Console.PrintLine(value.Text);
Console.PrintLine(value.IsNull);
```

## Custom Cast Pattern

When you want JSON data to become a real class instance, define a `[Cast]` method on the target class and cast from `dict` directly:

```kinal
Get IO.Json;

Class User
{
    Public string Name;
    Public int Age;

    [Cast]
    Public Static Function User From(dict value)
    {
        User user = New User();
        user.Name = [string](value.TryFetch("name", ""));
        user.Age = [int](value.TryFetch("age", 0));
        Return user;
    }
}

IO.Json.Object root = IO.Json.ParseObject("{\"name\":\"kinal\",\"age\":9}");
User user = [User](root.Data);
```

## Notes

- JSON objects use `dict`.
- JSON arrays use `list`.
- Prefer `Parse(...)`, raw `dict`, and raw `list` for normal code. Use `ParseObject(...)` or `Wrap(...)` only when property-style access reads better.
- The wrapper types are convenience surfaces; formatting and serialization still operate on raw values such as `dict`, `list`, `root.Data`, or `value.Raw`.
- Because `Get` and `Set` are language keywords in Kinal, the wrapper API uses names such as `Value(...)` and `Put(...)` instead.
- JSON object keys must be strings.
- `NaN` and `Infinity` are rejected during serialization.

## See Also

- [IO.Text](text.md) — String helpers often used around JSON payloads
- [IO.Request](request.md) — HTTP client package that commonly exchanges JSON text

## Parsing, Wrapping and Errors

`ParseValue` accepts any JSON root; `Parse`/`ParseObject` require an object root. `AsDict`/`AsList` check dynamic type and throw IO.Error on mismatch. Malformed JSON also throws; use `Catch (IO.Error error)` when recovery is required.

`Wrap(data)` and Object.Data share the same dict rather than deep-copying it. Mutating the wrapper changes the original collection. Value properties provide conversion views; check the type before selecting one. Serialize raw values, Data or Raw; nested wrapper objects are not automatically unwrapped. On KinalVM, parsed objects and arrays are ordinary guest dictionaries and lists. The matching runner includes the same cJSON reader and formatter as Native. Invalid input throws a catchable `IO.Error` with the title `IO.Json`.

## Public Source Signatures

The tables list public fields, constructors, properties and methods from the current package source. Inherited members are listed under their declaring base type. Get/Set identify property accessors; signatures include declared defaults. Native handles and Host/Win32 helpers require their ABI/lifetime contracts; ordinary applications should start with the package high-level objects.

### IO.Json

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `IsDict` | `Trusted Function bool IsDict(any value)` | — |
| `IsList` | `Trusted Function bool IsList(any value)` | — |
| `AsDict` | `Trusted Function dict AsDict(any value)` | — |
| `AsList` | `Trusted Function list AsList(any value)` | — |
| `ParseValue` | `Trusted Function any ParseValue(string text)` | — |
| `Parse` | `Trusted Function dict Parse(string text)` | — |
| `ParseObject` | `Trusted Function IO.Json.Object ParseObject(string text)` | — |
| `Wrap` | `Trusted Function IO.Json.Object Wrap(dict value)` | — |
| `FormatText` | `Trusted Function string FormatText(string text, bool pretty = true)` | — |
| `Minify` | `Trusted Function string Minify(string text)` | — |
| `FormatValue` | `Trusted Function string FormatValue(any value, bool pretty = true)` | — |
| `Format` | `Trusted Function string Format(any value, bool pretty = true)` | — |

### IO.Json.Value

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(any value = null)` | — |
| `Raw` | `Public Property any Raw` | Get/Set |
| `Dict` | `Public Trusted Property dict Dict` | Get/Set |
| `List` | `Public Trusted Property list List` | Get/Set |
| `Text` | `Public Property string Text` | Get/Set |
| `Int` | `Public Property int Int` | Get/Set |
| `Float` | `Public Property float Float` | Get/Set |
| `Bool` | `Public Property bool Bool` | Get/Set |
| `IsNull` | `Public Trusted Property bool IsNull` | Get |
| `IsDict` | `Public Trusted Property bool IsDict` | Get |
| `IsList` | `Public Trusted Property bool IsList` | Get |

### IO.Json.Object

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(dict data = null)` | — |
| `Data` | `Public Property dict Data` | Get/Set |
| `Count` | `Public Trusted Property int Count` | Get |
| `Keys` | `Public Trusted Property list Keys` | Get |
| `Value` | `Public Trusted Function IO.Json.Value Value(string key)` | — |
| `ValueOr` | `Public Trusted Function IO.Json.Value ValueOr(string key, any fallback)` | — |
| `Put` | `Public Trusted Function IO.Json.Object Put(string key, any value)` | — |
| `Remove` | `Public Trusted Function IO.Json.Object Remove(string key)` | — |
| `Has` | `Public Trusted Function bool Has(string key)` | — |
