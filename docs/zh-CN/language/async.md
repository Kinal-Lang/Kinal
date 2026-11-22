# 异步编程

本页完整程序面向 **Native**。当前 C KNC 发射器拒绝通用 Await 表达式及 Task Run 内建调用。VM 的 superloop 选项控制循环融合，不是 Async 任务调度。

`Async` 声明产生任务的函数，`Await` 表达对另一任务的依赖。异步语法不会自动并行化工作，也不会把阻塞文件／网络 API 自动变成异步 I/O。

## 声明与等待任务

声明的结果类型是任务完成时产生的值：

```kinal
Async Function int Compute(int value)
{
    Return value + 1;
}
```

调用 `Compute(41)` 产生 IO.Async.Task，Await 后得到声明的 int 结果。Await 用于 Async 函数，包括 Async Main；普通同步函数不能直接使用 Await。

```kinal
Unit Docs.AsyncEntry;
Get IO.Console;
Get IO.Async;

Async Function int Compute(int value)
{
    Await IO.Async.Yield();
    Await IO.Async.Sleep(1);
    Return value + 1;
}

Static Async Function int Main()
{
    int result = Await Compute(41);
    IO.Console.PrintLine(result); // 42
    Return 0;
}
```

完整程序同时导入 IO.Console 和 IO.Async。写限定标准库名称不能替代导入要求。

## 从同步代码调用

使用 `IO.Async.Run(task)` 同步等待，传入异步调用的结果，不是函数名：

```kinal
Unit Docs.AsyncPipeline;
Get IO.Console;
Get IO.Async;

Async Function int Fetch(int id)
{
    Await IO.Async.Sleep(1);
    Return id * 100;
}

Async Function int Aggregate()
{
    int first = Await Fetch(1);
    int second = Await Fetch(2);
    int third = Await Fetch(3);
    Return first + second + third;
}

Static Function int Main()
{
    int total = IO.Async.Run(Aggregate());
    IO.Console.PrintLine(total); // 600
    Return 0;
}
```

以上 Await 是顺序依赖，不是并发 fan-out API。IO.Async.Run 阻塞当前执行路径直至完成；内建声明的擦除结果为 any，受支持的源码调用可在分析时恢复异步函数的结果类型。

## 任务原语

| 函数 | 声明结果 | 用途 |
|------|----------|------|
| `IO.Async.Sleep(int ms)` | `IO.Async.Task` | 定时／等待任务 |
| `IO.Async.Yield()` | `IO.Async.Task` | 协作让出任务 |
| `IO.Async.Run(IO.Async.Task task)` | `any` | 同步完成任务并获得结果 |

这些原语为 Safe。具体调度取决于后端／运行时，不能只从 Async 语法推导线程数、非阻塞系统调用或无限扩展能力。

## Task 与进程句柄不同

`IO.Async.Spawn(string command)` 启动**子进程**并返回 byte*，不会把任意 Kinal 函数作为任务启动。IsCompleted、Wait、ExitCode、Close 操作的是进程句柄。Spawn 传入命令字符串，不是函数对象。参见[IO.Async API](../stdlib/async.md)。

进程句柄是需要 Close 的外部资源；托管 Task 具有不同表示和生命周期，不能互换，也不能把进程句柄传给 Run。

## 后端与运行时要求

VM 的 superloop／no-superloop 控制合适 For／While 模式的专用字节码循环融合，不负责开启或关闭语言 Task 实现。当前 C KNC 没有通用 Await 发射，也未映射 IO.Async.Run，因此以上程序必须使用 Native 路径构建。

Freestanding 的任务／异常／分配支持需要目标运行时 hook，core-only 不提供 hosted 异步服务。参见[后端支持](../advanced/backend-support.md)、[VM CLI](../cli/vm.md)。

## 错误处理与验证

当前 Sema 拒绝 Try／Catch 内的 Await。需要恢复时，可在 Try／Catch 内使用受支持的同步 Run 边界，或将异步工作安排在处理区域之外。Async 声明不会把失败自动变成成功结果。应在声称支持的后端上验证输出和终止行为，语义检查不能代替调度执行。

```bash
kinal run async-demo.kn
```

## 相关文档

- [IO.Async](../stdlib/async.md)
- [函数](functions.md)
- [异常](exceptions.md)
- [Block](blocks.md)
