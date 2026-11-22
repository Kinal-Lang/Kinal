# IO.Meta：运行时元数据

导入 `Get IO.Meta;`。运行时元数据描述带 Keep @runtime 属性的声明，不是对任意实例当前字段值的反射。本文示例对应 C Native 运行时。

## 完整查询 API

| 函数 | 结果 | 含义 |
|------|------|------|
| `Query(string attributeName)` | `list` | 所有匹配属性名的保留记录 |
| `Of(string owner)` | `list` | 一个声明所属的保留属性记录 |
| `Has(string owner, string attributeName)` | `bool` | 该声明是否有匹配属性 |
| `Find(string owner, string attributeName)` | `dict` | 一条匹配属性记录 |

Of／Has／Find 还接受 Main 等受支持声明引用，由编译器解析为 owner 名。Has 的首参数是 owner，第二参数是属性名。Query 按属性查，Of 按 owner 查，二者不是别名。

方法可使用 `App.Controller.Hello` 等限定 owner 字符串，避免 `instance.Hello` 这类不支持的实例方法引用。方法属性需要 On @method，不是 On @function。没有 @type 缩写，应选 @class／@struct／@enum／@interface，多个目标用逗号连接。

## 属性记录

记录是 dict，不是自定义属性类实例：

| 键 | 含义 |
|----|------|
| `owner` | 声明 owner 限定名 |
| `module` | 声明所属 Unit |
| `meta`／`name` | 元数据身份／名称字段 |
| `arg.<参数名>` | 应用参数值，包括声明默认值 |

例如 Meta Route(string path) 对应 arg.path。只读取与定义相符的键；可选属性应先用 Has 或检查结果，不能假定某个参数必然存在。

Native 可调用 owner 路径还可暴露 owner_ptr、owner_param_count，它们关联 Kinal 可调用适配器，不是任意 C 函数指针 ABI。Unsafe 调用和动态库生命周期约束仍有效。

## 完整可重复元数据示例

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

Find 返回单个 dict，不返回全部重复应用。收集重复元数据时，可取 Of(owner) 后筛选，或取 Query(attributeName) 后按 owner 筛选；Query 也可能包含其他声明。

## 保留期与动态库生命周期

Keep @source、@compile 不提供运行时查询。Repeatable 只允许重复应用，不改变保留期；自定义 compile-only 元数据也不会自动产生编译器宏或优化行为。

原生共享库可在加载时注册保留元数据。使用可调用元数据时应保留对应动态库，最终卸载会使模块 owner／适配器失效。以前拿到记录不代表可以调用已卸载代码。参见[IO.System](system.md)。

## 相关文档

- [Meta 语法](../language/meta.md)
- [集合](collections.md)
- [FFI](../language/ffi.md)
- [IO.Web](web.md)
