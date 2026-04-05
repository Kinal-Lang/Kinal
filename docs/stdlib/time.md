# IO.Time — Time and Date

`IO.Time` provides functionality for retrieving the current time, high-precision timers, sleeping, and time formatting.

## Import

```kinal
Get IO.Time;
```

---

## Function Reference

### Getting Time

| Function | Signature | Description |
|----------|-----------|-------------|
| `Now()` | `→ IO.Time.DateTime` | Returns the current local time (DateTime object) |
| `GetTick()` | `→ int` | Returns a millisecond-precision timer (relative value, used for measurements) |
| `GetNanoTick()` | `→ int` | Returns a nanosecond-precision timer (high precision) |

```kinal
IO.Time.DateTime now = IO.Time.Now();
int t0 = IO.Time.GetTick();
// ... perform operations ...
int elapsed = IO.Time.GetTick() - t0;
IO.Console.PrintLine("Elapsed: " + [string](elapsed) + " ms");
```

### Sleeping

| Function | Signature | Description |
|----------|-----------|-------------|
| `Sleep(ms)` | `int → void` | Block the current thread for the specified number of milliseconds |

```kinal
IO.Time.Sleep(1000);  // Sleep for 1 second (blocking)
```

> In async functions, `IO.Async.Sleep` (non-blocking) is recommended. `IO.Time.Sleep` blocks the current thread.

### Formatting

| Function | Signature | Description |
|----------|-----------|-------------|
| `Format(dt, fmt)` | `IO.Time.DateTime, string → string` | Format a DateTime as a string |

Common format specifiers:

| Specifier | Description | Example |
|-----------|-------------|---------|
| `yyyy` | Four-digit year | `2024` |
| `MM` | Two-digit month | `03` |
| `dd` | Two-digit day | `09` |
| `HH` | 24-hour hour | `14` |
| `mm` | Minutes | `05` |
| `ss` | Seconds | `07` |

```kinal
IO.Time.DateTime now = IO.Time.Now();
string ts = IO.Time.Format(now, "yyyy-MM-dd HH:mm:ss");
IO.Console.PrintLine(ts);  // e.g., "2024-03-09 14:05:07"
```

---

## DateTime Object

`IO.Time.DateTime` is a read-only struct with the following fields:

| Field | Type | Description |
|-------|------|-------------|
| `Ticks` | `int` | Tick count since epoch |
| `Year` | `int` | Year (e.g. 2024) |
| `Month` | `int` | Month (1–12) |
| `Day` | `int` | Day (1–31) |
| `Hour` | `int` | Hour (0–23) |
| `Minute` | `int` | Minute (0–59) |
| `Second` | `int` | Second (0–59) |
| `Millisecond` | `int` | Millisecond (0–999) |

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

## Complete Examples

### Performance Benchmarking

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

    IO.Console.PrintLine("Result: " + [string](sum));
    IO.Console.PrintLine("Elapsed: " + [string](ns) + " ns");
    IO.Console.PrintLine("Elapsed: " + [string](ns / 1000000) + " ms");
}

Static Function int Main()
{
    RunBenchmark();
    Return 0;
}
```

### Timestamped Logging

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

## See Also

- [IO.Async.Sleep](async.md) — Non-blocking sleep
- [IO.File](filesystem.md) — Persistent timestamp logging
