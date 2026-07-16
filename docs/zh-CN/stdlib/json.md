# IO.Json

`IO.Json` 提供 JSON 解析与格式化能力，底层直接使用 Kinal 内建的 `dict`、`list` 和标量值。

## 导入

```kinal
Get IO.Json;
```

## 快速开始

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

## 核心形态

`IO.Json` 会把 JSON 对象保留成 `dict`，把数组保留成 `list`。这也是它的主表面接口；`IO.Json.Object` 和 `IO.Json.Value` 只是建立在原始值之上的可选便捷包装。

- `Parse(text)` 返回 `dict`
- `ParseValue(text)` 返回 `any`
- `Format(value, pretty = true)` 接受 `dict`、`list`、字符串、数字、布尔或 `null`
- `Minify(text)` 和 `FormatText(text, pretty = true)` 直接处理原始 JSON 文本

```kinal
dict data = IO.Json.Parse("{\"ok\":true}");
Console.PrintLine([bool](data.Fetch("ok")));

list items = IO.Json.AsList(IO.Json.ParseValue("[1,2,3]"));
Console.PrintLine(items.Count());
```

## 属性风格包装

`IO.Json.Object` 和 `IO.Json.Value` 在原始 `dict` / `list` / 标量值之上提供了一层属性风格接口。

### IO.Json.Object

`IO.Json.Object` 适合想要属性式访问时使用，但它底下仍然只是一个 `dict`。

属性和方法：

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

属性：

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

## 自定义转换用法

如果你希望把 JSON 数据直接变成真实类实例，可以在目标类上定义一个 `[Cast]` 方法，然后从 `dict` 直接强转：

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

## 说明

- JSON 对象统一使用 `dict`。
- JSON 数组统一使用 `list`。
- 正常代码优先使用 `Parse(...)`、原始 `dict` 和原始 `list`。只有在属性式访问更顺手时再用 `ParseObject(...)` 或 `Wrap(...)`。
- 这两个包装类主要是便捷表层；格式化和序列化仍然直接吃原始值，比如 `dict`、`list`、`root.Data` 或 `value.Raw`。
- 因为 `Get` 和 `Set` 在 Kinal 里是语言关键字，所以这里用了 `Value(...)`、`Put(...)` 这样的名字。
- JSON 对象键必须是字符串。
- 序列化时不接受 `NaN` 和 `Infinity`。

## 相关

- [IO.Text](text.md) — 经常和 JSON 文本一起配合使用
- [IO.Request](request.md) — 经常用于传输 JSON 文本的 HTTP 客户端包
