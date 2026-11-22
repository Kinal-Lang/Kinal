# IO.Meta: Runtime Metadata

Import `Get IO.Meta;`. Runtime metadata describes declarations with `Keep @runtime` attributes; it is not arbitrary reflection over an instance's current field values. The examples here describe the C Native runtime.

## Complete Query API

| Function | Result | Meaning |
|----------|--------|---------|
| `Query(string attributeName)` | `list` | All retained records matching an attribute name |
| `Of(string owner)` | `list` | Retained attribute records belonging to one declaration |
| `Has(string owner, string attributeName)` | `bool` | Whether that declaration has a matching attribute |
| `Find(string owner, string attributeName)` | `dict` | One matching attribute record |

Of/Has/Find also accept supported declaration references such as `Main`; the compiler resolves them to an owner name. Has takes owner **first**, attribute name second. Query searches by attribute; Of searches by owner. They are not aliases.

For a method, a qualified owner string such as `App.Controller.Hello` avoids unsupported instance-method references like `instance.Hello`. Applying an attribute to a method requires `On @method`, not `On @function`. There is no `@type` shorthand: choose `@class`, `@struct`, `@enum` or `@interface`, separated by commas when needed.

## Attribute Records

Records are dict values, not custom attribute-class instances:

| Key | Meaning |
|-----|---------|
| `owner` | Qualified declaration owner name |
| `module` | Declaring Unit |
| `meta` / `name` | Metadata identity/name fields |
| `arg.<parameterName>` | Applied argument value, including declared defaults |

For example, `Meta Route(string path)` produces `arg.path`. Only request keys appropriate to the metadata definition. Guard optional records with Has or inspect the result before assuming a parameter exists.

Callable owners can additionally expose `owner_ptr` and `owner_param_count` in the Native metadata path. These relate to Kinal callable adapters, not an arbitrary C function-pointer ABI. Unsafe invocation and loaded-library lifetime rules still apply.

## Complete Repeatable Metadata Example

```kinal
Unit Docs.Metadata;
Get IO.Console;
Get IO.Meta;

Meta Route(string path, string method = "GET")
{
    On @function;
    Keep @runtime;
}

Meta Tag(string value)
{
    On @function;
    Keep @runtime;
    Repeatable;
}

[Route("/hello")]
[Tag("api")]
[Tag("public")]
Function string Hello()
{
    Return "hello";
}

Static Function int Main()
{
    dict route = IO.Meta.Find(Hello, "Route");
    IO.Console.PrintLine(IO.Meta.Has(Hello, "Route"));
    IO.Console.PrintLine([string](route.Fetch("arg.path")));
    IO.Console.PrintLine([string](route.Fetch("arg.method")));
    IO.Console.PrintLine(IO.Meta.Of(Hello).Count());
    IO.Console.PrintLine(IO.Meta.Query("Tag").Count());
    Return 0;
}
```

```text
true
/hello
GET
3
2
```

Find returns a single dict, not every repeated application. Use Of(owner) and filter its records, or Query(attributeName) and filter by owner, when collecting repeated metadata. Query results can include declarations other than the one you are currently inspecting.

## Retention and Library Lifetime

`Keep @source` and `Keep @compile` do not make an attribute runtime-queryable. Repeatable permits multiple applications; it does not change retention. Custom compile-only metadata does not automatically add compiler macros or optimization behavior.

Native shared libraries can register retained metadata when loaded. Keep the library loaded while using its callable metadata; final unload invalidates that module's owners/adapters. A record obtained earlier is not permission to invoke unloaded code. See [IO.System](system.md).

## See Also

- [Meta Syntax](../language/meta.md)
- [Collections](collections.md)
- [FFI](../language/ffi.md)
- [IO.Web](web.md)
