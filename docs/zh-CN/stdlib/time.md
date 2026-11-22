# IO.Time — 时间与日期

`IO.Time` 提供获取当前时间、高精度计时器、休眠以及时间格式化功能。

原生后端和配套的 KinalVM 都支持本页的五个函数。每次调用 `Now()` 都会返回独立的 UTC 快照，`Ticks` 是从 Unix 纪元起算的毫秒数。用 `GetTick()` 和 `GetNanoTick()` 测量经过的时间。

## 导入

```kinal
Get IO.Time;
```

---

## 函数参考

五个函数的安全级别均为 `Trusted`。

### 时间获取

| 函数 | 签名 | 说明 |
|------|------|------|
| `Now()` | `→ IO.Time.DateTime` | 返回当前 UTC 日期字段（DateTime 对象） |
| `GetTick()` | `→ int` | 返回毫秒级计时器（相对值，用于计时） |
| `GetNanoTick()` | `→ int` | 返回纳秒级计时器（高精度） |

```kinal
IO.Time.DateTime now = IO.Time.Now();
int t0 = IO.Time.GetTick();
// ... 执行操作 ...
int elapsed = IO.Time.GetTick() - t0;
IO.Console.PrintLine("耗时 " + [string](elapsed) + " ms");
```

### 休眠

| 函数 | 签名 | 说明 |
|------|------|------|
| `Sleep(ms)` | `int → void` | 阻塞当前线程指定毫秒数 |

```kinal
IO.Time.Sleep(1000);  // 休眠 1 秒（阻塞）
```

> 在异步函数中推荐使用 `IO.Async.Sleep`（非阻塞），`IO.Time.Sleep` 会阻塞当前线程。

### 格式化

| 函数 | 签名 | 说明 |
|------|------|------|
| `Format(dt, fmt)` | `IO.Time.DateTime, string → string` | 将 DateTime 格式化为字符串 |

常用格式符：

| 格式符 | 说明 | 示例 |
|--------|------|------|
| `yyyy` | 四位年份 | `2024` |
| `MM` | 两位月份 | `03` |
| `dd` | 两位日期 | `09` |
| `HH` | 24 小时制小时 | `14` |
| `mm` | 分钟 | `05` |
| `ss` | 秒 | `07` |
| `fff` | 毫秒 | `123` |

格式化读取当前的年月日、时分秒和毫秒字段，因此修改字段会影响结果。`null` 日期按全零字段处理，空字符串或 `null` 格式使用 `yyyy-MM-dd HH:mm:ss`。完整标记以外的文本原样保留，包括 UTF-8 字符。例如，`yyyy年MM月dd日` 可生成 `2024年02月29日`。

```kinal
IO.Time.DateTime now = IO.Time.Now();
string ts = IO.Time.Format(now, "yyyy-MM-dd HH:mm:ss");
IO.Console.PrintLine(ts);  // e.g., "2024-03-09 14:05:07"
```

---

## DateTime 对象

`IO.Time.DateTime` 是具有公开整数成员的可变类：

| 字段 | 类型 | 说明 |
|------|------|------|
| `Ticks` | `int` | 自纪元以来的 tick 数 |
| `Year` | `int` | 年份（如 2024） |
| `Month` | `int` | 月份（1–12） |
| `Day` | `int` | 日（1–31） |
| `Hour` | `int` | 小时（0–23） |
| `Minute` | `int` | 分钟（0–59） |
| `Second` | `int` | 秒（0–59） |
| `Millisecond` | `int` | 毫秒（0–999） |

```kinal
IO.Time.DateTime now = IO.Time.Now();

IO.Console.PrintLine(now.Year);        // 2024
IO.Console.PrintLine(now.Month);       // 3
IO.Console.PrintLine(now.Day);         // 9
IO.Console.PrintLine(now.Hour);        // 14
IO.Console.PrintLine(now.Minute);      // 5
IO.Console.PrintLine(now.Second);      // 7
IO.Console.PrintLine(now.Millisecond); // 123
```

---

## 完整示例

### 性能基准计时

```kinal
Unit App.Benchmark;
Get IO.Console;
Get IO.Time;

Static Function void RunBenchmark()
{
    int start = IO.Time.GetNanoTick();

    int sum = 0;
    int i = 0;
    While (i < 1000000)
    {
        sum = sum + i;
        i = i + 1;
    }

    int end = IO.Time.GetNanoTick();
    int ns = end - start;

    IO.Console.PrintLine("结果: " + [string](sum));
    IO.Console.PrintLine("耗时: " + [string](ns) + " ns");
    IO.Console.PrintLine("耗时: " + [string](ns / 1000000) + " ms");
}

Static Function int Main()
{
    RunBenchmark();
    Return 0;
}
```

### 带时间戳的日志

```kinal
Unit App.TimedLog;

Get IO.Time;
Get IO.File;

Static Function void Log(string level, string msg)
{
    IO.Time.DateTime now = IO.Time.Now();
    string ts  = IO.Time.Format(now, "HH:mm:ss");
    string line = "[" + ts + "] [" + level + "] " + msg;
    IO.File.AppendLine("app.log", line);
}
```

---

## 相关

- [IO.Async.Sleep](async.md) — 非阻塞休眠
- [IO.File](filesystem.md) — 持久化时间戳日志
- [IO.Core](core.md) — 计时代码里常见的数学辅助函数
- [后端支持](../advanced/backend-support.md)
