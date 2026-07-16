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

```kinal
dict data = IO.Json.Parse("{\"ok\":true}");
Console.PrintLine([bool](data.Fetch("ok")));

list items = IO.Json.AsList(IO.Json.ParseValue("[1,2,3]"));
Console.PrintLine(items.Count());
```

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
