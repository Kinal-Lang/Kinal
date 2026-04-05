# Meta (Metadata Annotations)

Meta is Kinal's custom attribute system, allowing you to attach metadata to functions, classes, methods, fields, and other declarations, and query that information at compile time or at runtime.

## Defining Meta

Use the `Meta` keyword to define a custom attribute:

```kinal
Meta Version(string text)
{
    On @function;      // can be applied to functions
    Keep @runtime;     // metadata is retained until runtime
}
```

### Syntax

```kinal
Meta Name(ParameterList)
{
    On TargetList;
    Keep Lifecycle;
    [Repeatable;]      // optional: allows applying to the same declaration multiple times
}
```

### On — Targets

Specifies which declarations the Meta can be applied to:

| Target | Description |
|--------|-------------|
| `@function` | Regular functions |
| `@extern_function` | `Extern` functions |
| `@delegate_function` | `Delegate` functions |
| `@class` | Classes |
| `@struct` | Structs |
| `@enum` | Enums |
| `@interface` | Interfaces |
| `@method` | Methods (member methods of classes/structs) |
| `@field` | Fields |

Multiple targets are separated by `|`:

```kinal
Meta Tag(string name)
{
    On @class | @method | @field;
    Keep @runtime;
}
```

### Keep — Lifecycle

| Value | Description |
|-------|-------------|
| `@source` | Visible only at the source level; discarded after compilation |
| `@compile` | Retained until compile time, used for code generation decisions; not queryable at runtime |
| `@runtime` | Retained until runtime; queryable via `IO.Meta` |

### Repeatable

By default, the same Meta cannot be applied to the same declaration more than once. Add `Repeatable;` to allow repetition:

```kinal
Meta Tag(string name)
{
    On @function;
    Keep @runtime;
    Repeatable;
}

[Tag("fast")]
[Tag("pure")]
Function int MyFunc() { Return 0; }
```

## Using Meta (Applying Attributes)

Use the `[Name(arguments)]` syntax to apply Meta to a declaration:

```kinal
[Version("1.0")]
Function void MyApi() { ... }

[Tag("important")]
Class MyService { ... }
```

Multiple attributes can be stacked:

```kinal
[Docs("Computes the sum of two numbers")]
[Deprecated("Please use the Sum function")]
[Since("2.0")]
Static Function int Add(int a, int b) { Return a + b; }
```

## Built-in Meta Attributes

Kinal provides a set of standard library built-in Meta attributes (use directly without defining):

| Attribute | Parameters | Description |
|-----------|------------|-------------|
| `[Docs(text)]` | `string text` | Documentation comment |
| `[Deprecated(message)]` | `string message` | Marks as deprecated |
| `[Since(version)]` | `string version` | Which version it was introduced in |
| `[Example(code)]` | `string code` | Example usage code |
| `[Experimental(message)]` | `string message` | Marks as experimental |
| `[LinkName(name)]` | `string name` | Symbol name at link time (used with `Extern` functions) |
| `[SymbolName(name)]` | `string name` | Symbol name (used with `Delegate` functions) |
| `[LinkLib(lib)]` | `string lib` | Automatically link the specified library |

### Example: Built-in Attributes

```kinal
Unit App;

Get IO.Console;
Get IO.Meta;

[Docs("Main function, demonstrating built-in Meta")]
[Deprecated("Use NewMain")]
[Since("2.1.0")]
[Example("IO.Console.PrintLine(\"demo\");")]
[Experimental("Subject to change")]
Static Function int Main()
{
    dict docs        = IO.Meta.Find(Main, "Docs");
    dict deprecated  = IO.Meta.Find(Main, "Deprecated");
    dict since       = IO.Meta.Find(Main, "Since");
    dict example     = IO.Meta.Find(Main, "Example");
    dict experimental = IO.Meta.Find(Main, "Experimental");

    IO.Console.PrintLine([string](docs.Fetch("arg.text")));
    IO.Console.PrintLine([string](deprecated.Fetch("arg.message")));
    IO.Console.PrintLine([string](since.Fetch("arg.version")));
    IO.Console.PrintLine([string](example.Fetch("arg.code")));
    IO.Console.PrintLine([string](experimental.Fetch("arg.message")));
    Return 0;
}
```

## Runtime Querying: IO.Meta

When a Meta has a lifecycle of `@runtime`, it can be queried at runtime via `IO.Meta`:

```kinal
Get IO.Meta;

// Query a single attribute
dict attr = IO.Meta.Find(MyFunc, "Version");
string version = [string](attr.Fetch("arg.text"));

// Query all matching attributes (returns list)
list allTags = IO.Meta.Of("Tag");

// Check if an attribute exists
bool has = IO.Meta.Has("Version", MyFunc);
```

### IO.Meta Function Reference

| Function | Parameters | Return | Description |
|----------|------------|--------|-------------|
| `IO.Meta.Find(target, name)` | `any, string` | `dict` | Find a named attribute on the specified declaration; returns an attribute dict |
| `IO.Meta.Of(name)` | `string` | `list` | Find all declarations with the specified attribute name |
| `IO.Meta.Query(name)` | `string` | `list` | Same as `Of` |
| `IO.Meta.Has(name, target)` | `string, any` | `bool` | Check whether the specified declaration has the specified attribute |

### Attribute Dictionary Key Format

In the `dict` returned by `IO.Meta.Find`, parameter values use the key format `"arg.parameterName"`:

```kinal
Meta MyAttr(string title, int priority)
{
    On @function;
    Keep @runtime;
}

// When querying:
dict d = IO.Meta.Find(SomeFunc, "MyAttr");
string title = [string](d.Fetch("arg.title"));
int prio = [int](d.Fetch("arg.priority"));
```

## Complete Custom Example

```kinal
Unit App.MetaDemo;

Get IO.Console;
Get IO.Meta;

// Define a route attribute
Meta Route(string path, string method = "GET")
{
    On @function;
    Keep @runtime;
}

// Apply to functions
[Route("/api/users", "GET")]
Function string GetUsers() { Return "[]"; }

[Route("/api/users", "POST")]
Function string CreateUser() { Return "{}"; }

Static Function int Main()
{
    // Query route metadata
    dict getUsersRoute = IO.Meta.Find(GetUsers, "Route");
    string path = [string](getUsersRoute.Fetch("arg.path"));
    string method = [string](getUsersRoute.Fetch("arg.method"));

    IO.Console.PrintLine(path);    // /api/users
    IO.Console.PrintLine(method);  // GET
    Return 0;
}
```

## Compile-Time Attributes (`@compile`)

Attributes with a lifecycle of `@compile` are not retained at runtime; they are primarily used for code generation decisions or documentation tools:

```kinal
Meta Pure()
{
    On @function;
    Keep @compile;
}

[Pure]
Function int Square(int n) { Return n * n; }
```

## Built-in Attributes Related to FFI

These attributes are specifically for FFI (Foreign Function Interface):

```kinal
// Specify the C symbol name for an Extern function
[LinkName("kn_sys_alloc")]
Extern Function byte* SysAlloc(usize size) By C;

// Automatically link a library
[LinkLib("openssl")]
Extern Function int SSL_connect(byte* ssl) By C;

// Symbol name for a Delegate function (used for dynamic library loading)
[SymbolName("MyLib.Calculate")]
Delegate Function int Calculate(int a, int b) By C;
```

See [FFI](ffi.md) for details.

## Next Steps

- [FFI External Function Interface](ffi.md)
- [IO.Meta Standard Library](../stdlib/meta.md)
