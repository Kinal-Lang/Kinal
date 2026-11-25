# 标准库总览

本参考介绍 Kinal 工具链使用的标准库。Kinal 1.0.0 随 SDK 交付自举编译器。`IO.*` 是单元命名体系。编译器内建、KinalVM 和 freestanding 的支持范围，请查看各 API 的包和后端要求。

## 包、单元与运行时

| 来源 | 提供内容 |
|------|----------|
| 编译器内建 | IO.Type、IO.Collection、IO.Error、IO.GC、IO.Meta、IO.Volatile、IO.Panic，以及 Target／Host／Runtime／Version 常量 |
| IO.Core 包 | IO.Console、Text、File、Directory、Path、Time、System、Memory、Char、Math |
| 独立官方包 | IO.Json、IO.Request、IO.Web、IO.UI、IO.Cat |
| IO.Async 内建接口 | Task、Async／Await 支持及子进程操作；底层仍依赖相应运行时 |

发行包预装官方包。`Get` 使用声明的 Unit 名称；例如包名 IO.Core 本身不是导入所有内容的 Unit。`Get IO.Console;` 后使用 `IO.Console.PrintLine`，或显式导入符号别名。

## 单元导航

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
| `IO.Type` | 类型转换和值检查 | [→](types.md) |
| `IO.Target` | 编译期平台常量 | 内置 |


## 导入与调用示例

```kinal
Get PrintLine By IO.Console.PrintLine;
Get IO.Text;

Static Function int Main()
{
    PrintLine(IO.Text.ToUpper("kinal"));
    any value = 42;
    PrintLine(value.IsInt(), value.ToString());
    Return 0;
}
```

## 使用边界

- 标准库中 Safe 函数可以调用 Trusted 桥接接口；Trusted 不表示自动校验指针和资源所有权。
- Native 与 KinalVM 均支持集合、JSON 解析与格式化、文本拆分与拼接、按行读写和目录枚举。通用 Await 和 `IO.Async.Run` 目前需要 Native。
- IO.Request／Web 需要相应原生资产；HTTPS 还需要 OpenSSL 3。IO.UI 2.0 在 Windows Native 和桌面 VM 上通过 Direct3D 11、Direct2D 和 DirectWrite 绘制控件。
- freestanding 按 none／alloc／gc profile 限制可用功能，不能导入任意 hosted 包。
- 返回 bool、null、空文本、错误状态和抛出异常是不同契约；各页分别说明，不应统一假定“失败总会抛异常”。

更多信息：[后端支持](../advanced/backend-support.md)、[包管理](../cli/packages.md)、[内存与生命周期](../advanced/memory-management.md)、[类型辅助](types.md)、[异常](../language/exceptions.md)。
