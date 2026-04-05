# IO.Meta — Metadata Reflection

`IO.Meta` provides runtime access to Meta attributes, allowing you to query custom attributes attached to types or instances.

## Import

```kinal
Get IO.Meta;
```

---

## Prerequisites: The Meta Attribute System

Meta attributes are defined using the `Meta` keyword and applied with `[Name(...)]` syntax to types or members:

```kinal
Meta Description(string text)
{
    On @type, @function;
    Keep @runtime;
}

[Description("This is a user class")]
Class User
{
    [Description("Username")]
    string Name;
}
```

Only attributes marked with `Keep @runtime` can be queried at runtime through `IO.Meta`.

---

## Function Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `Query(target)` | `any → dict` | Returns a dictionary of all runtime attributes on the target |
| `Of(target, name)` | `any, string → any` | Gets the attribute instance with the specified name on the target (returns `null` if not found) |
| `Has(target, name)` | `any, string → bool` | Checks whether the target has the specified attribute |
| `Find(target, name)` | `any, string → list` | Gets all attribute instances with the specified name on the target (supports `Repeatable`) |

---

## Usage Examples

### Query a Single Attribute

```kinal
// Check if the User class has a Description attribute
bool hasDesc = IO.Meta.Has(User, "Description");

// Get the attribute instance
any attr = IO.Meta.Of(User, "Description");
If (attr != null)
{
    // Access the attribute's parameters (via dictionary keys)
    dict d = IO.Meta.Query(User);
    IO.Console.PrintLine(d);
}
```

### List All Attributes

```kinal
dict allAttrs = IO.Meta.Query(User);
list keys = IO.Collection.dict.Keys(allAttrs);

int i = 0;
While (i < IO.Collection.list.Count(keys))
{
    string attrName = [string](IO.Collection.list.Fetch(keys, i));
    IO.Console.PrintLine("Attribute: " + attrName);
    i = i + 1;
}
```

### Finding Repeatable Attributes

Attributes with the `Repeatable` modifier can be applied multiple times; use `Find` to retrieve all instances:

```kinal
Meta Tag(string value)
{
    On @type;
    Keep @runtime;
    Repeatable;
}

[Tag("web")]
[Tag("api")]
[Tag("v2")]
Class ApiController { }

// Get all Tag instances
list tags = IO.Meta.Find(ApiController, "Tag");
IO.Console.PrintLine(IO.Collection.list.Count(tags));  // 3
```

---

## Attribute Lifecycle

The `Keep` directive on a Meta attribute determines where the attribute is retained:

| Lifecycle | Description |
|-----------|-------------|
| `@compile` | Available at compile time only; not written into the executable |
| `@runtime` | Written into the executable; queryable at runtime via `IO.Meta` |

Only attributes with `Keep @runtime` can be accessed through `IO.Meta`.

---

## Complete Example

```kinal
Unit App.MetaDemo;

Get IO.Console;
Get IO.Meta;
Get IO.Collection;

Meta Route(string path)
{
    On @function;
    Keep @runtime;
}

Meta Auth()
{
    On @function;
    Keep @runtime;
}

Class Controller
{
    [Route("/api/hello")]
    Function string Hello()
    {
        Return "Hello!";
    }

    [Route("/api/data")]
    [Auth]
    Function string GetData()
    {
        Return "secret";
    }
}

Static Function int Main()
{
    Controller ctrl = New Controller();

    // Check whether a method requires authentication
    bool needsAuth = IO.Meta.Has(ctrl.GetData, "Auth");
    IO.Console.PrintLine(needsAuth ? "Authentication required" : "Public endpoint");  // Authentication required

    // Get routing information
    bool hasRoute = IO.Meta.Has(ctrl.Hello, "Route");
    IO.Console.PrintLine(hasRoute ? "Route configured" : "No route");   // Route configured

    Return 0;
}
```

---

## See Also

- [Meta Attributes](../language/meta.md) — Meta definition and application syntax
- [IO.Collection](collections.md) — `dict` and `list` operations
