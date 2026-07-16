# 标准库概览

Kinal 的标准库统一位于 `IO.*` 命名空间下。核心运行时模块可直接使用，`IO.Core`、`IO.Request`、`IO.UI`、`IO.Web` 这类包模块则通过项目包配置引入。

## 模块速览

| 模块 | 说明 | 文档 |
|------|------|------|
| `IO.Console` | 控制台输入输出 | [→](console.md) |
| `IO.Text` | 字符串处理工具 | [→](text.md) |
| `IO.File` | 文件操作 | [→](filesystem.md) |
| `IO.Directory` | 目录操作 | [→](filesystem.md) |
| `IO.Path` | 路径拼接、拆分、规范化 | [→](path.md) |
| `IO.Collection` | `list` / `dict` / `set` | [→](collections.md) |
| `IO.Time` | 时间与计时工具 | [→](time.md) |
| `IO.Async` | 异步任务与进程辅助接口 | [→](async.md) |
| `IO.System` | 进程、命令行与动态库接口 | [→](system.md) |
| `IO.Meta` | 运行时元数据访问 | [→](meta.md) |
| `IO.Json` | 基于 `dict` / `list` 的 JSON 解析与格式化 | [→](json.md) |
| `IO.Request` | 对外 HTTP 客户端 | [→](request.md) |
| `IO.Char` | 字符分类工具 | [→](core.md) |
| `IO.Math` | 整数与浮点数学工具 | [→](core.md) |
| `IO.UI` | 原生桌面 UI 工具包 | [→](ui.md) |
| `IO.Web` | 内嵌 HTTP 服务工具包 | [→](web.md) |
| `IO.Cat` | 示例包，提供 `Rommy` 类 | [→](cat.md) |
| `IO.Type` | 内置类型转换工具 | 内置 |
| `IO.Target` | 编译期平台常量 | 内置 |

## 导入形式

```kinal
Get IO.Console;
IO.Console.PrintLine("hello");

Get PrintLine By IO.Console.PrintLine;
PrintLine("hello");
```

## 内置值方法

所有值都直接具备 `IO.Type.any` 提供的辅助方法，无需额外导入：

```kinal
42.ToString();       // "42"
42.TypeName();       // "int"
42.Equals(42);       // true
42.IsNull();         // false
```

`any` 值还可以做运行时类型判断：

```kinal
any value = 3.14;
value.IsFloat();     // true
value.IsNumber();    // true
value.IsString();    // false
value.Tag();         // 内部运行时标签
```

## 常见入口

```kinal
Get IO.Console;

IO.Console.PrintLine("Hello");
IO.Console.Print("不换行");
string input = IO.Console.ReadLine();
```

```kinal
Get IO.Text;

IO.Text.Contains("kinal", "na");
IO.Text.ToUpper("hello");
IO.Text.Split("a,b,c", ",");
```

## 包模块

### IO.Core

`IO.Core` 提供 `IO.Char` 和 `IO.Math`。

```kinal
Get IO;

IO.Char.IsDigit('5');
IO.Math.Abs(-5);
IO.Math.Sqrt(16.0);
```

### IO.Request

`IO.Request` 是同步的对外 HTTP 客户端包。

```kinal
Get IO.Request;

IO.Request.Response response = IO.Request.Fetch("http://127.0.0.1:8000/");
```

### IO.Json

`IO.Json` 会把 JSON 对象解析成 `dict`，把数组解析成 `list`，同时提供一层属性风格包装。

```kinal
Get IO.Json;

dict root = IO.Json.Parse("{\"name\":\"kinal\"}");
```

### IO.UI

`IO.UI` 提供原生窗口与控件层。

```kinal
Get IO.UI;

IO.UI.Window window = New IO.UI.Window("Demo", 640, 480);
window.Add(New IO.UI.Button("OK", 20, 20, 100, 28));
```

### IO.Web

`IO.Web` 提供内嵌 HTTP 服务、路由元数据和静态目录挂载能力。

```kinal
Get IO.Web;

IO.Web.Server server = New IO.Web.Server(8080);
server.UseStatic("/static", "public").UseRoutes();
```

## 相关

- [IO.Console](console.md)
- [IO.Text](text.md)
- [IO.Collection](collections.md)
- [IO.File / IO.Directory](filesystem.md)
- [IO.Path](path.md)
- [IO.Core](core.md)
- [IO.Json](json.md)
- [IO.Request](request.md)
- [IO.UI](ui.md)
- [IO.Web](web.md)
