# IO.Async：任务与子进程

本文是 Native API 参考。当前 C KNC 不提供通用 Await／Run lowering；superloop 融合是独立的字节码优化。

导入 `Get IO.Async;`。该命名空间包含托管语言 Task 和主机子进程句柄两类 API，进程句柄不是 IO.Async.Task。

## 完整 API

| 函数 | 结果 | 安全等级／含义 |
|------|------|---------------|
| `Run(IO.Async.Task task)` | `any` | Safe，同步获得任务结果 |
| `Sleep(int ms)` | `IO.Async.Task` | Safe，等待任务 |
| `Yield()` | `IO.Async.Task` | Safe，协作让出任务 |
| `Spawn(string command)` | `byte*` | Trusted，启动子进程 |
| `IsCompleted(byte* handle)` | `bool` | Trusted，查询完成 |
| `Wait(byte* handle, int ms)` | `bool` | Trusted，带超时等待 |
| `ExitCode(byte* handle)` | `int` | Trusted，查询进程退出状态 |
| `Close(byte* handle)` | `bool` | Trusted，释放进程句柄 |

语言调用规则允许 Safe 调用 Trusted，原始指针操作另受限制。进程包装通常写为 Trusted 函数，将主机资源处理集中在一处。

## 任务

Async 函数调用产生 Task；Run 传 `Compute()` 而不是 `Compute`。Await 只在 Async 函数内有效，Sleep／Yield 返回 Task，不是 void。完整程序参见[异步编程](../language/async.md)。

该 API 没有 `Spawn(Function)`、通用线程创建、任务取消、任务组合器家族或自动并行 map。顺序 Await 不代表重叠执行，阻塞库调用也不会自动异步化。

## 进程生命周期

以下 Native 主机示例等待小型子命令，打印退出码并关闭句柄：

```kinal
Unit Docs.Processes;
Get IO.Console;
Get IO.Async;

Trusted Function int RunChild(string command)
{
    byte* handle = IO.Async.Spawn(command);
    If (handle == null) { Return -1; }
    bool completed = IO.Async.Wait(handle, -1);
    If (!completed)
    {
        IO.Async.Close(handle);
        Return -1;
    }
    int code = IO.Async.ExitCode(handle);
    IO.Async.Close(handle);
    Return code;
}

Static Function int Main()
{
    string command = "sh -c 'exit 7'";
    If (IO.Target.OS == IO.Target.OS.Windows)
    {
        command = "cmd /c exit 7";
    }
    IO.Console.PrintLine(RunChild(command)); // 7
    Return 0;
}
```

`Wait(handle, 0)` 立即查询，正数为毫秒超时，-1 无限等待。完成后再读取 ExitCode。Close 释放句柄，不是通用子进程终止 API。

命令字符串遵守目标主机的进程／shell 约定，不是 argv 数组接口；不要盲目拼接不可信输入。以上示例使用固定命令，并在查询结果前确认完成。

## 运行时与平台边界

进程操作需要 hosted 操作系统服务，存在声明不代表 freestanding 镜像或所有 VM 都具备这些服务。运行时／VM 支持和任务 superloop 选项参见[后端支持](../advanced/backend-support.md)、[VM CLI](../cli/vm.md)。

外部进程资源需要显式 Close，Kinal GC 不替代句柄所有权。参见[内存与生命周期](../advanced/memory-management.md)。
