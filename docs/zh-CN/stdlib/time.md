# IO.Time — 时间与日期

`IO.Time` 提供获取当前时间、高精度计时器、休眠以及时间格式化功能。

## 导入

```kinal
Get IO.Time;
```

---

## 函数参考

### 时间获取

| 函数 | 签名 | 说明 |
|------|------|------|
| `Now()` | `→ IO.Time.DateTime` | 返回当前本地时间（DateTime 对象） |
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

```kinal
IO.Time.DateTime now = IO.Time.Now();
string ts = IO.Time.Format(now, "yyyy-MM-dd HH:mm:ss");
IO.Console.PrintLine(ts);  // e.g., "2024-03-09 14:05:07"
```

---

## DateTime 对象

`IO.Time.DateTime` 是只读结构体，包含以下字段：

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
