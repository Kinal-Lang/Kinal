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

`Format` 和 `FormatValue` 在紧凑和缩进模式下都能精确写出 Kinal `int` 的十进制数字。在 64 位目标上，`9007199254740993` 也会原样输出。读取时使用 cJSON 的数值表示。`ParseValue` 将有符号 32 位范围内的整数读为 `int`，其余数值读为浮点数。需要在读回后保留每一位的长编号时，可将其保存为 JSON 字符串。

```kinal
dict data = IO.Json.Parse("{\"ok\":true}");
Console.PrintLine([bool](data.Fetch("ok")));

list items = IO.Json.AsList(IO.Json.ParseValue("[1,2,3]"));
Console.PrintLine(items.Count());
```

读取一组记录时，可以一次解析整个数组，再用 `AsDict` 逐条取值。数组保留输入顺序。同一对象中出现重复键时，字典保留最后一个值。

```kinal
list rows = IO.Json.AsList(IO.Json.ParseValue("[{\"name\":\"Ada\"},{\"name\":\"Lin\"}]"));
For (int i = 0; i < rows.Count(); i++)
{
    dict row = IO.Json.AsDict(rows.Fetch(i));
    IO.Console.PrintLine([string](row.Fetch("name")));
}
```

解析时整个文档会进入内存。原生读取器为数组元素和对象成员一次建立索引，随后按位置查找每个子节点的时间为常数。

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

## 解析、包装与错误

`ParseValue` 接受任意 JSON 根值；`Parse`／`ParseObject` 要求对象根。`AsDict`／`AsList` 检查动态类型，不匹配时抛出 IO.Error。损坏 JSON 也会抛出异常；需要恢复时使用 `Catch (IO.Error error)`。

`Wrap(data)` 和 Object.Data 引用同一 dict，不会深复制；修改包装对象会修改原始集合。`Value` 各类型属性提供转换视图，使用前先检查类型。序列化传 raw 值、Data 或 Raw；嵌套包装对象不会自动展开。在 KinalVM 中，解析得到的对象和数组是普通的 VM 字典和列表。配套运行器包含与 Native 相同的 cJSON 解析和格式化实现。无效输入会抛出可以捕获的 `IO.Error`，标题为 `IO.Json`。

## 公开源码签名

下表逐项列出当前包源码的公开字段、构造函数、属性和方法；继承成员在基类下列出。Get/Set 表示属性访问器，函数行的参数包含声明默认值。原生句柄、Host／Win32 等低层接口需要相应 ABI 与生命周期知识，普通应用优先使用包的高级对象。

### IO.Json

| 成员 | 声明 | 访问器 |
|------|------|--------|
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

| 成员 | 声明 | 访问器 |
|------|------|--------|
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

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(dict data = null)` | — |
| `Data` | `Public Property dict Data` | Get/Set |
| `Count` | `Public Trusted Property int Count` | Get |
| `Keys` | `Public Trusted Property list Keys` | Get |
| `Value` | `Public Trusted Function IO.Json.Value Value(string key)` | — |
| `ValueOr` | `Public Trusted Function IO.Json.Value ValueOr(string key, any fallback)` | — |
| `Put` | `Public Trusted Function IO.Json.Object Put(string key, any value)` | — |
| `Remove` | `Public Trusted Function IO.Json.Object Remove(string key)` | — |
| `Has` | `Public Trusted Function bool Has(string key)` | — |
