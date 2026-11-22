# IO.System：进程与动态库

导入 `Get IO.System;`。这些 hosted 操作提供命令行文本、进程执行及原生动态库加载。源码包装属于 IO.Core，GetSymbol 另外由编译器处理委托元数据。

## 完整 API

| 函数 | 结果 | 含义 |
|------|------|------|
| `CommandLine()` | `string` | 命令行文本，不是列表／argv 解析器 |
| `Exit(int code)` | `void` | 以指定状态终止进程 |
| `FileExists(string path)` | `bool` | 文件存在性检查 |
| `Exec(string commandLine)` | `int` | 启动并等待主机命令，返回状态或失败 |
| `LoadLibrary(string path)` | `byte*` | 原生库句柄，失败为 null |
| `GetSymbol(byte* library, string symbol)` | `byte*` | 原生符号地址，失败为 null |
| `CloseLibrary(byte* library)` | `bool` | 释放原生加载器引用 |
| `LastError()` | `string` | 最近主机／加载器错误文本 |

当前编译器还接受兼容 C Delegate 引用作为 GetSymbol 第二参数，解析其 SymbolName 元数据。Delegate 是绑定槽，不是把任意 any 变量转换成函数的类型。参见[FFI](../language/ffi.md)。

公开 Trusted 包装可被 Safe 调用；原始指针操作和 Unsafe 外部调用另受检查。没有独立 `Trusted { ... }` 语句，应使用 Trusted 函数或受支持的显式 Block 语法。

## 结构化参数使用 Main

```kinal
Unit Docs.Arguments;
Get IO.Console;
Get IO.System;

Static Function int Main(string[] args)
{
    IO.Console.PrintLine(IO.System.CommandLine().Length() > 0);
    If (args.Length() == 0)
    {
        IO.Console.PrintLine("Usage: app <name>");
        Return 1;
    }
    IO.Console.PrintLine(args[0]);
    Return 0;
}
```

```bash
kinal run arguments.kn -- Kinal
```

Main 的 args 不包含可执行文件名。不要按空格拆 CommandLine 替代它：扁平文本不能可靠恢复引号／空参数。Windows 进程参数和原生文件系统边界目前不保证通用 UTF-8。

## Exec 取决于主机

Windows 使用带命令行的进程创建，shell 内建命令需要显式 `cmd /c ...`；POSIX 路径使用系统 shell。应使用固定命令或明确的主机引用规则，此接口不是 argv 数组启动器。确认返回状态后再认为成功，也不会自动选择工程工作目录。

进程查询、超时和句柄所有权参见[IO.Async](async.md)；其中进程句柄与语言 Task 不同。

## 原生动态库生命周期

选择目标 OS／架构／ABI 兼容的库。LoadLibrary 可按支持情况注册 Kinal 元数据；GetSymbol 不会验证原始地址是否具有预期 C 签名，应通过匹配的 Delegate By C 绑定。

每次成功加载都持有一个加载器引用。所有调用／回调停止后再关闭；最终关闭使库内符号／元数据失效，不能继续调用保留委托。重复加载可能返回相同句柄值，但持有独立引用。

动态加载属于 Native 主机设施，不是任意可移植 KNC FFI。完整的委托／加载／错误／关闭程序参见[FFI](../language/ffi.md)。LastError 文本依赖主机，不应要求所有平台的措辞完全相同。

## 相关文档

- [文件与目录](filesystem.md)
- [FFI](../language/ffi.md)
- [IO.Async](async.md)
- [内存与生命周期](../advanced/memory-management.md)
