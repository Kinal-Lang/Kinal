# IO.Meta — 元数据反射

`IO.Meta` 提供运行时访问 Meta 属性的功能，可查询类型或实例上附加的自定义属性。

## 导入

```kinal
Get IO.Meta;
```

---

## 前置知识：Meta 属性系统

Meta 属性使用 `Meta` 关键字定义，使用 `[名称(...)]` 语法应用于类型或成员：

```kinal
Meta Description(string text)
{
    On @type, @function;
    Keep @runtime;
}

[Description("这是一个用户类")]
Class User
{
    [Description("用户名")]
    string Name;
}
```

只有标注了 `Keep @runtime` 的属性才能在运行时通过 `IO.Meta` 查询。

---

## 函数参考

| 函数 | 签名 | 说明 |
|------|------|------|
| `Query(target)` | `any → dict` | 返回目标上所有运行时属性组成的字典 |
| `Of(target, name)` | `any, string → any` | 获取目标上指定名称的属性实例（不存在返回 `null`） |
| `Has(target, name)` | `any, string → bool` | 检查目标是否具有指定属性 |
| `Find(target, name)` | `any, string → list` | 获取目标上指定名称的所有属性（支持 `Repeatable`） |

---

## 使用示例

### 查询单个属性

```kinal
// 检查 User 类是否有 Description 属性
bool hasDesc = IO.Meta.Has(User, "Description");

// 获取属性实例
any attr = IO.Meta.Of(User, "Description");
If (attr != null)


{
    // 访问属性的参数（通过字典键）
    dict d = IO.Meta.Query(User);
    IO.Console.PrintLine(d);
}
```

### 列出所有属性

```kinal
dict allAttrs = IO.Meta.Query(User);
list keys = IO.Collection.dict.Keys(allAttrs);

int i = 0;
While (i < IO.Collection.list.Count(keys))


{
    string attrName = [string](IO.Collection.list.Fetch(keys, i));
    IO.Console.PrintLine("属性: " + attrName);
    i = i + 1;
}
```

### 可重复属性查找

带有 `Repeatable` 修饰的属性可以被多次应用，使用 `Find` 获取所有实例：

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

// 获取所有 Tag 实例
list tags = IO.Meta.Find(ApiController, "Tag");
IO.Console.PrintLine(IO.Collection.list.Count(tags));  // 3
```

---

## 属性生命周期

Meta 属性的 `Keep` 指令决定属性的保留范围：

| 生命周期 | 说明 |
|----------|------|
| `@compile` | 仅编译期可用，不写入可执行文件 |
| `@runtime` | 写入可执行文件，运行时可通过 `IO.Meta` 查询 |

只有 `Keep @runtime` 的属性才能通过 `IO.Meta` 访问。

---

## 完整示例

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

    // 检查方法是否需要认证
    bool needsAuth = IO.Meta.Has(ctrl.GetData, "Auth");
    IO.Console.PrintLine(needsAuth ? "需要认证" : "公开接口");  // 需要认证

    // 获取路由信息
    bool hasRoute = IO.Meta.Has(ctrl.Hello, "Route");
    IO.Console.PrintLine(hasRoute ? "已配置路由" : "无路由");   // 已配置路由

    Return 0;
}
```

---

## 相关

- [Meta 属性](../language/meta.md) — Meta 定义和应用语法
- [IO.Collection](collections.md) — `dict` 和 `list` 操作
