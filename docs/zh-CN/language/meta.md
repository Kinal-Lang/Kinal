# Meta（元数据注解）

Meta 是 Kinal 的自定义属性系统，允许你为函数、类、方法、字段等声明附加元数据，并在编译期或运行时查询这些信息。

## 定义 Meta

使用 `Meta` 关键字定义一个自定义属性：

```kinal
Meta Version(string text)
{
    On @function;      // 可以应用于函数
    Keep @runtime;     // 元数据保留到运行时
}
```

### 语法

```kinal
Meta 名称(参数列表)
{
    On 目标列表;
    Keep 生命周期;
    [Repeatable;]      // 可选：允许多次应用于同一声明
}
```

### On — 目标

指定该 Meta 可以被应用到哪些声明上：

| 目标 | 说明 |
|------|------|
| `@function` | 普通函数 |
| `@extern_function` | `Extern` 函数 |
| `@delegate_function` | `Delegate` 函数 |
| `@class` | 类 |
| `@struct` | 结构体 |
| `@enum` | 枚举 |
| `@interface` | 接口 |
| `@method` | 方法（类/结构体的成员方法） |
| `@field` | 字段 |

多个目标用 `|` 分隔：

```kinal
Meta Tag(string name)
{
    On @class | @method | @field;
    Keep @runtime;
}
```

### Keep — 生命周期

| 值 | 说明 |
|----|------|
| `@source` | 仅在源码级可见，编译后丢弃 |
| `@compile` | 保留到编译期，用于代码生成决策，运行时不可查询 |
| `@runtime` | 保留到运行时，可通过 `IO.Meta` 查询 |

### Repeatable

默认情况下，同一 Meta 不能在同一声明上使用多次。添加 `Repeatable;` 允许重复：

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

## 使用 Meta（应用属性）

用 `[名称(参数)]` 语法将 Meta 应用到声明上：

```kinal
[Version("1.0")]
Function void MyApi() { ... }

[Tag("important")]
Class MyService { ... }
```

多个属性可连续或组合：

```kinal
[Docs("计算两数之和")]
[Deprecated("请使用 Sum 函数")]
[Since("2.0")]
Static Function int Add(int a, int b) { Return a + b; }
```

## 内置 Meta 属性

Kinal 提供了一組标准库内置的 Meta 属性（无需定义，直接使用）：

| 属性 | 参数 | 说明 |
|------|------|------|
| `[Docs(text)]` | `string text` | 文档注释 |
| `[Deprecated(message)]` | `string message` | 标记为已废弃 |
| `[Since(version)]` | `string version` | 从哪个版本引入 |
| `[Example(code)]` | `string code` | 使用示例代码 |
| `[Experimental(message)]` | `string message` | 标记为实验性 |
| `[LinkName(name)]` | `string name` | 链接时的符号名（用于 `Extern` 函数） |
| `[SymbolName(name)]` | `string name` | 符号名（用于 `Delegate` 函数） |
| `[LinkLib(lib)]` | `string lib` | 自动链接指定库 |

### 示例：内置属性

```kinal
Unit App;

Get IO.Console;
Get IO.Meta;

[Docs("主函数，演示内置 Meta")]
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

## 运行时查询：IO.Meta

当 Meta 生命周期为 `@runtime` 时，可以在运行时通过 `IO.Meta` 查询：

```kinal
Get IO.Meta;

// 查询单个属性
dict attr = IO.Meta.Find(MyFunc, "Version");
string version = [string](attr.Fetch("arg.text"));

// 查询所有匹配属性（返回 list）
list allTags = IO.Meta.Of("Tag");

// 检查是否存在某属性
bool has = IO.Meta.Has("Version", MyFunc);
```

### IO.Meta 函数参考

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `IO.Meta.Find(target, name)` | `any, string` | `dict` | 查找指定声明上的指定属性，返回属性字典 |
| `IO.Meta.Of(name)` | `string` | `list` | 查找所有带指定属性名的声明 |
| `IO.Meta.Query(name)` | `string` | `list` | 同 `Of` |
| `IO.Meta.Has(name, target)` | `string, any` | `bool` | 检查指定声明是否拥有指定属性 |

### 属性字典键格式

`IO.Meta.Find` 返回的 `dict` 中，参数值以 `"arg.参数名"` 为键：

```kinal
Meta MyAttr(string title, int priority)
{
    On @function;
    Keep @runtime;
}

// 查询时：
dict d = IO.Meta.Find(SomeFunc, "MyAttr");
string title = [string](d.Fetch("arg.title"));
int prio = [int](d.Fetch("arg.priority"));
```

## 完整自定义示例

```kinal
Unit App.MetaDemo;

Get IO.Console;
Get IO.Meta;

// 定义路由属性
Meta Route(string path, string method = "GET")
{
    On @function;
    Keep @runtime;
}

// 应用到函数
[Route("/api/users", "GET")]
Function string GetUsers() { Return "[]"; }

[Route("/api/users", "POST")]
Function string CreateUser() { Return "{}"; }

Static Function int Main()
{
    // 查询路由元数据
    dict getUsersRoute = IO.Meta.Find(GetUsers, "Route");
    string path = [string](getUsersRoute.Fetch("arg.path"));
    string method = [string](getUsersRoute.Fetch("arg.method"));

    IO.Console.PrintLine(path);    // /api/users
    IO.Console.PrintLine(method);  // GET
    Return 0;
}
```

## 编译期属性（@compile）

生命周期为 `@compile` 的属性不保留到运行时，主要用于代码生成决策或文档工具：

```kinal
Meta Pure()
{
    On @function;
    Keep @compile;
}

[Pure]
Function int Square(int n) { Return n * n; }
```

## 与 FFI 相关的内置属性

这些属性专用于 FFI（外部函数接口）：

```kinal
// 指定 Extern 函数的 C 符号名
[LinkName("kn_sys_alloc")]
Extern Function byte* SysAlloc(usize size) By C;

// 自动链接库
[LinkLib("openssl")]
Extern Function int SSL_connect(byte* ssl) By C;

// Delegate 函数的符号名（用于动态库加载）
[SymbolName("MyLib.Calculate")]
Delegate Function int Calculate(int a, int b) By C;
```

详见 [FFI](ffi.md)。

## 下一步

- [FFI 外部函数接口](ffi.md)
- [IO.Meta 标准库](../stdlib/meta.md)
