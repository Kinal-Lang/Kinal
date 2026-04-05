# 异步编程

Kinal 内置了 `Async`/`Await` 原生异步支持，使异步代码与同步代码具有相同的书写风格。

## 基本概念

- `Async Function` — 声明一个异步函数，返回 `IO.Async.Task`
- `Await 表达式` — 等待一个异步任务完成并获取结果
- `IO.Async.Run(task)` — 同步等待一个异步任务（在同步上下文中使用）

## 声明异步函数

```kinal
Async Function int Compute()
{
    Return 42;
}
```

异步函数总是返回 `IO.Async.Task`，但你可以在函数签名中写期望的**值类型**（如 `int`），Kinal 会自动包装：

```kinal
Async Function int Step(int value)
{
    Return value + 1;
}
```

## 使用 Await

在 `Async` 函数中使用 `Await` 等待另一个异步操作：

```kinal
Static Async Function int Main()
{
    Await IO.Async.Yield();        // 让出控制权
    Await IO.Async.Sleep(100);     // 异步等待 100ms

    int result = Await Step(41);   // 等待并获取结果
    IO.Console.PrintLine(result);     // 42

    Return 0;
}
```

`Await` 后跟返回 `IO.Async.Task` 的表达式。

## 调用异步函数

### 在异步上下文中：直接 Await

```kinal
Static Async Function int Main()
{
    int a = Await StepOne();
    int b = Await StepTwo(a);
    Return b;
}
```

### 在同步上下文中：IO.Async.Run

```kinal
Static Function int Main()
{
    int value = IO.Async.Run(Compute());  // 同步等待异步任务结果
    IO.Console.PrintLine(value);
    Return 0;
}
```

## 异步链式调用

```kinal
Async Function int StepOne()
{
    Return 40;
}

Async Function int StepTwo(int prev)
{
    Return prev + 2;
}

Async Function int Pipeline()
{
    int a = Await StepOne();
    int b = Await StepTwo(a);
    Return b;
}

Static Function int Main()
{
    int result = IO.Async.Run(Pipeline());
    IO.Console.PrintLine(result);  // 42
    Return 0;
}
```

## IO.Async 标准库

| 函数 | 签名 | 说明 |
|------|------|------|
| `IO.Async.Yield()` | `→ Task` | 让出执行权，异步调度器重执行 |
| `IO.Async.Sleep(ms)` | `int → Task` | 异步等待指定毫秒 |
| `IO.Async.Run(task)` | `Task → any` | 同步运行异步任务，获取结果 |

### 进程管理（异步进程）

```kinal
Get IO.Async;

Static Function int Main()
{
    byte* proc = IO.Async.Spawn("cmd /c echo hello");

    // 非阻塞检查
    IO.Console.PrintLine(IO.Async.IsCompleted(proc));

    // 等待最多 3 秒
    bool done = IO.Async.Wait(proc, 3000);

    // 获取退出码
    int code = IO.Async.ExitCode(proc);

    // 关闭进程句柄
    IO.Async.Close(proc);

    Return 0;
}
```

| 函数 | 签名 | 说明 |
|------|------|------|
| `IO.Async.Spawn(cmd)` | `string → byte*` | 启动子进程，返回进程句柄（`Trusted`） |
| `IO.Async.IsCompleted(h)` | `byte* → bool` | 检查进程是否已结束（`Trusted`） |
| `IO.Async.Wait(h, ms)` | `byte*, int → bool` | 等待进程，0 = 立即检查，-1 = 永久等待（`Trusted`） |
| `IO.Async.ExitCode(h)` | `byte* → int` | 获取进程退出码（`Trusted`） |
| `IO.Async.Close(h)` | `byte* → bool` | 释放进程句柄（`Trusted`） |

> `Spawn`、`Wait` 等函数安全级别为 `Trusted`，调用它们的函数需要是 `Trusted` 或 `Unsafe`。

## 完整示例

```kinal
Unit App.AsyncDemo;

Get IO.Console;
Get IO.Async;

Static Async Function int Fetch(int id)
{
    Await IO.Async.Sleep(10);  // 模拟网络延迟
    Return id * 100;
}

Static Async Function int Aggregate()
{
    int a = Await Fetch(1);
    int b = Await Fetch(2);
    int c = Await Fetch(3);
    Return a + b + c;
}

Static Function int Main()
{
    int total = IO.Async.Run(Aggregate());
    IO.Console.PrintLine(total);  // 600
    Return 0;
}
```

## 注意事项

1. **`Await` 只能在 `Async` 函数中使用** — 在普通函数中使用 `Await` 会产生编译错误
2. **`IO.Async.Run` 是同步的** — 会阻塞调用线程直到任务完成
3. **Async 函数返回 Task** — 其返回类型会被自动包装，调用方接收 `IO.Async.Task`
4. **链式 Await** — 可以无限链式 Await，每次都等待前一个完成

## 下一步

- [Block（代码块对象）](blocks.md)
- [安全等级](safety.md)
- [IO.Async 标准库](../stdlib/async.md)
