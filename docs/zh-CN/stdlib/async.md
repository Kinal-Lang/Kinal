# IO.Async — 异步并发

`IO.Async` 提供任务派生、等待、状态查询等异步原语，配合语言级 `Async`/`Await` 关键字使用。

> **安全要求**：大多数 `IO.Async` 函数（`Spawn`、`Wait`、`IsCompleted` 等）需要在 `Trusted` 或 `Unsafe` 安全块中调用。`Run`、`Sleep`、`Yield` 可在 `Safe` 上下文中使用。

## 导入

```kinal
Get IO.Async;
```

---

## 语言级关键字

在学习 `IO.Async` 之前，先了解语言层面的异步支持：

### `Async Function`

声明一个异步函数，返回类似 future 的任务句柄：

```kinal
Async Function string FetchData()
{
    Await IO.Async.Sleep(100);
    Return "result";
}
```

### `Await`

等待一个异步操作完成并获取其结果：

```kinal
string data = Await FetchData();
```

---

## 函数参考

### 任务管理（需要 Trusted 上下文）

| 函数 | 签名 | 说明 |
|------|------|------|
| `Spawn(fn)` | `Function → any` | 派生一个异步任务，返回任务句柄 |
| `IsCompleted(task)` | `any → bool` | 查询任务是否已完成 |
| `Wait(task)` | `any → any` | 阻塞等待任务完成并返回结果 |
| `ExitCode(task)` | `any → int` | 获取任务退出码 |
| `Close(task)` | `any → void` | 释放任务资源 |

```kinal
Trusted
{
    any task = IO.Async.Spawn(MyAsyncFunc);

    While (!IO.Async.IsCompleted(task))


    {
        IO.Async.Yield();
    }

    any result = IO.Async.Wait(task);
    IO.Async.Close(task);
}
```

### 调度原语（可在 Safe 上下文使用）

| 函数 | 签名 | 说明 |
|------|------|------|
| `Run(fn)` | `Function → void` | 在当前调度器上运行一个任务 |
| `Sleep(ms)` | `int → void` | 异步休眠指定毫秒数 |
| `Yield()` | `→ void` | 让出 CPU 给其他任务 |

```kinal
// 异步休眠（不阻塞主线程）
Await IO.Async.Sleep(500);

// 运行任务
IO.Async.Run(BackgroundWorker);
```

---

## 使用模式

### 模式一：Async/Await（推荐）

语言层面的 `Async`/`Await` 是编写异步代码最简洁的方式：

```kinal
Unit App.AsyncDemo;

Get IO.Console;
Get IO.Async;

Async Function string LoadConfig(string path)
{
    Await IO.Async.Sleep(10);  // 模拟 I/O 延迟
    Return IO.File.ReadAllText(path);
}

Async Function int Main()
{
    IO.Console.PrintLine("开始加载...");
    string cfg = Await LoadConfig("config.json");
    IO.Console.PrintLine("加载完成: " + cfg);
    Return 0;
}
```

### 模式二：手动任务派生（Trusted）

适合需要更精细控制的场景：

```kinal
Trusted Function void ParallelWork()
{
    any t1 = IO.Async.Spawn(WorkA);
    any t2 = IO.Async.Spawn(WorkB);

    IO.Async.Wait(t1);
    IO.Async.Wait(t2);

    IO.Async.Close(t1);
    IO.Async.Close(t2);

    IO.Console.PrintLine("两个任务均已完成");
}
```

### 模式三：轮询完成状态

```kinal
Trusted Function void PollTask()
{
    any task = IO.Async.Spawn(LongRunningJob);

    While (!IO.Async.IsCompleted(task))


    {
        IO.Console.Print(".");
        IO.Async.Sleep(100);
        IO.Async.Yield();
    }

    IO.Console.PrintLine(" 完成!");
    IO.Async.Close(task);
}
```

---

## 超级循环（Superloop）

KinalVM 的异步调度器基于"超级循环"（superloop）模型。默认情况下，`IO.Async.Run` 会将任务加入事件循环并自动推进。

编译器选项：
- `--superloop`：启用超级循环模式（默认）
- `--no-superloop`：禁用超级循环（用于嵌入式等场景）

---

## 相关

- [异步编程](../language/async.md) — Async / Await 语言特性详解
- [安全级别](../language/safety.md) — Trusted / Safe 上下文
- [IO.System](system.md) — `Exec` 等进程级操作
