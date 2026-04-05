# IO.Async — Asynchronous Concurrency

`IO.Async` provides async primitives such as task spawning, waiting, and status querying, used together with the language-level `Async`/`Await` keywords.

> **Safety requirement**: Most `IO.Async` functions (`Spawn`, `Wait`, `IsCompleted`, etc.) require a `Trusted` or `Unsafe` safety block. `Run`, `Sleep`, and `Yield` can be used in `Safe` contexts.

## Import

```kinal
Get IO.Async;
```

---

## Language-Level Keywords

Before exploring `IO.Async`, understand the language-level async support:

### `Async Function`

Declares an asynchronous function that returns a future-like task handle:

```kinal
Async Function string FetchData()
{
    Await IO.Async.Sleep(100);
    Return "result";
}
```

### `Await`

Waits for an asynchronous operation to complete and retrieves its result:

```kinal
string data = Await FetchData();
```

---

## Function Reference

### Task Management (requires Trusted context)

| Function | Signature | Description |
|----------|-----------|-------------|
| `Spawn(fn)` | `Function → any` | Spawn an async task, returns a task handle |
| `IsCompleted(task)` | `any → bool` | Query whether the task has completed |
| `Wait(task)` | `any → any` | Block until the task completes and return the result |
| `ExitCode(task)` | `any → int` | Get the task exit code |
| `Close(task)` | `any → void` | Release task resources |

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

### Scheduling Primitives (usable in Safe context)

| Function | Signature | Description |
|----------|-----------|-------------|
| `Run(fn)` | `Function → void` | Run a task on the current scheduler |
| `Sleep(ms)` | `int → void` | Async sleep for the specified number of milliseconds |
| `Yield()` | `→ void` | Yield the CPU to other tasks |

```kinal
// Async sleep (does not block the main thread)
Await IO.Async.Sleep(500);

// Run a task
IO.Async.Run(BackgroundWorker);
```

---

## Usage Patterns

### Pattern 1: Async/Await (Recommended)

The language-level `Async`/`Await` is the most concise way to write async code:

```kinal
Unit App.AsyncDemo;

Get IO.Console;
Get IO.Async;

Async Function string LoadConfig(string path)
{
    Await IO.Async.Sleep(10);  // Simulate I/O delay
    Return IO.File.ReadAllText(path);
}

Async Function int Main()
{
    IO.Console.PrintLine("Loading...");
    string cfg = Await LoadConfig("config.json");
    IO.Console.PrintLine("Load complete: " + cfg);
    Return 0;
}
```

### Pattern 2: Manual Task Spawning (Trusted)

Suitable for scenarios requiring finer control:

```kinal
Trusted Function void ParallelWork()
{
    any t1 = IO.Async.Spawn(WorkA);
    any t2 = IO.Async.Spawn(WorkB);

    IO.Async.Wait(t1);
    IO.Async.Wait(t2);

    IO.Async.Close(t1);
    IO.Async.Close(t2);

    IO.Console.PrintLine("Both tasks have completed");
}
```

### Pattern 3: Polling Completion Status

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

    IO.Console.PrintLine(" Done!");
    IO.Async.Close(task);
}
```

---

## Superloop

KinalVM's async scheduler is based on a "superloop" model. By default, `IO.Async.Run` adds tasks to the event loop and automatically advances them.

Compiler options:
- `--superloop`: Enable superloop mode (default)
- `--no-superloop`: Disable superloop (for embedded and similar scenarios)

---

## See Also

- [Async Programming](../language/async.md) — Async / Await language features in detail
- [Safety Levels](../language/safety.md) — Trusted / Safe contexts
- [IO.System](system.md) — Process-level operations such as `Exec`
