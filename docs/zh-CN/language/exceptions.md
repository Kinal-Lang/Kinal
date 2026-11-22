# 异常处理

当前 C 语言前端只接受 `string`、`IO.Error` 类型的 Throw 值，不支持抛出任意整数、对象或 any。Hosted 代码可使用异常处理；freestanding core 禁止 Try／Catch／Throw。

## Try、Catch 与 Throw

一个 Try 对应一个 Catch：

```kinal
Try
{
    Throw "operation failed";
}
Catch (string message)
{
    // message 保存抛出的文本
}
```

还支持 `Catch (IO.Error error)` 和无参数 `Catch { ... }`。当前语法没有连续多个 Catch，也没有 `Finally`。需要多个恢复边界时嵌套 Try。

`Throw expression;` 必须有表达式，没有省略值的重新抛出语法；传播异常时显式 Throw 捕获值。Catch 变量只在其处理块内可见。

## `IO.Error`

构造函数使用**两个**参数：

```kinal
IO.Error error = New IO.Error("File Error", "File was not found");
```

| 字段 | 类型 | 含义 |
|------|------|------|
| `Title` | `string` | 简短分类／标题 |
| `Message` | `string` | 错误细节 |
| `Inner` | `IO.Error` | 关联原因，可以为 null |
| `Trace` | `string` | 运行时记录的追踪文本 |

内建构造函数只接受 title／message，没有 `(title, message, inner)` 三参数重载。需要确定的原因链时显式设置 `Inner`。运行时也可在抛出处理中附加记录的原因／追踪；链结构重要时不应只依赖隐式行为。

## 完整恢复与异常链示例

```kinal
Unit Docs.Exceptions;
Get IO.Console;
Get IO.Error;

Function int Divide(int left, int right)
{
    If (right == 0)
    {
        Throw New IO.Error("Division Error", "Divisor cannot be zero");
    }
    Return left / right;
}

Function void Calculate()
{
    Try
    {
        IO.Console.PrintLine(Divide(10, 0));
    }
    Catch (IO.Error cause)
    {
        IO.Error outer = New IO.Error("Calculation Error", "Unable to calculate");
        outer.Inner = cause;
        Throw outer;
    }
}

Static Function int Main()
{
    Try
    {
        Calculate();
    }
    Catch (IO.Error error)
    {
        IO.Console.PrintLine(error.Title);
        IO.Console.PrintLine(error.Message);
        If (error.Inner != null)
        {
            IO.Console.PrintLine(error.Inner.Title);
        }
    }
    Return 0;
}
```

Native 与 VM 预期输出：

```text
Calculation Error
Unable to calculate
Division Error
```

示例处理错误后返回成功。应用无法恢复时，应返回合适的非零退出码，而非静默吞掉失败。

## 捕获表示与后端边界

`Catch (string message)` 可以取得错误消息文本，包括 IO.Error 抛出的 Message；它不是通用任意类型模式匹配。可移植代码中，抛出的字符串按 string 捕获，IO.Error 按 IO.Error 捕获。

将原始字符串按 IO.Error 捕获存在已知后端差异：Native 将其规范化为错误对象，当前 VM 路径可能仍保留待处理异常并非零退出。不要依赖这种混用；需要结构化恢复时显式构造 IO.Error。当前也没有通用用户类异常继承分派契约。

未捕获异常会终止执行。诊断颜色和追踪拼写是运行时／展示细节；测试应断言关键错误信息和退出状态，不应假定追踪中的文件路径跨平台完全相同。

## 运行时查询辅助函数

调用以下辅助函数时导入 `Get IO.Error;`：

| 辅助函数 | 结果 | 用途 |
|----------|------|------|
| `IO.Error.LastTrace()` | `string` | 查询运行时记录的追踪 |
| `IO.Error.HasException()` | `bool` | 查询待处理异常状态 |
| `IO.Error.Current()` | `IO.Error` | 查询记录的异常对象 |
| `IO.Error.Clear()` | `void` | 清除待处理异常状态 |

已记录错误信息与待处理状态是不同概念。普通恢复优先使用 Catch 的显式绑定。清除状态不负责资源清理，也不能替代应执行的错误处理路径。

## 资源处理

Try／Catch 不引入确定性析构，也没有 Finally 块。拥有外部资源时，应在成功和恢复路径显式调用 close／release。GC 不会自动关闭任意外部资源。参见[内存与生命周期](../advanced/memory-management.md)。

## 相关文档

- [函数](functions.md)
- [安全](safety.md)
- [后端支持](../advanced/backend-support.md)
- [Freestanding](../advanced/freestanding.md)
