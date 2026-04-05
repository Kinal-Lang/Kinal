# Async Programming

Kinal has built-in native `Async`/`Await` support, giving async code the same writing style as synchronous code.

## Basic Concepts

- `Async Function` — declares an async function that returns `IO.Async.Task`
- `Await expression` — waits for an async task to complete and retrieves its result
- `IO.Async.Run(task)` — synchronously waits for an async task (used in synchronous contexts)

## Declaring Async Functions

```kinal
Async Function int Compute()
{
    Return 42;
}
```

Async functions always return `IO.Async.Task`, but you can write the expected **value type** (e.g., `int`) in the function signature and Kinal will automatically wrap it:

```kinal
Async Function int Step(int value)
{
    Return value + 1;
}
```

## Using Await

Use `Await` inside an `Async` function to wait for another async operation:

```kinal
Static Async Function int Main()
{
    Await IO.Async.Yield();        // yield control
    Await IO.Async.Sleep(100);     // async wait 100ms

    int result = Await Step(41);   // wait and get result
    IO.Console.PrintLine(result);     // 42

    Return 0;
}
```

`Await` is followed by an expression that returns `IO.Async.Task`.

## Calling Async Functions

### In an Async Context: Direct Await

```kinal
Static Async Function int Main()
{
    int a = Await StepOne();
    int b = Await StepTwo(a);
    Return b;
}
```

### In a Synchronous Context: IO.Async.Run

```kinal
Static Function int Main()
{
    int value = IO.Async.Run(Compute());  // synchronously wait for async task result
    IO.Console.PrintLine(value);
    Return 0;
}
```

## Async Chained Calls

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

## IO.Async Standard Library

| Function | Signature | Description |
|----------|-----------|-------------|
| `IO.Async.Yield()` | `→ Task` | Yield execution; async scheduler re-runs |
| `IO.Async.Sleep(ms)` | `int → Task` | Async wait for specified milliseconds |
| `IO.Async.Run(task)` | `Task → any` | Synchronously run an async task and get the result |

### Process Management (Async Processes)

```kinal
Get IO.Async;

Static Function int Main()
{
    byte* proc = IO.Async.Spawn("cmd /c echo hello");

    // Non-blocking check
    IO.Console.PrintLine(IO.Async.IsCompleted(proc));

    // Wait up to 3 seconds
    bool done = IO.Async.Wait(proc, 3000);

    // Get exit code
    int code = IO.Async.ExitCode(proc);

    // Close process handle
    IO.Async.Close(proc);

    Return 0;
}
```

| Function | Signature | Description |
|----------|-----------|-------------|
| `IO.Async.Spawn(cmd)` | `string → byte*` | Start a child process, returns a process handle (`Trusted`) |
| `IO.Async.IsCompleted(h)` | `byte* → bool` | Check if the process has finished (`Trusted`) |
| `IO.Async.Wait(h, ms)` | `byte*, int → bool` | Wait for process; 0 = check immediately, -1 = wait forever (`Trusted`) |
| `IO.Async.ExitCode(h)` | `byte* → int` | Get process exit code (`Trusted`) |
| `IO.Async.Close(h)` | `byte* → bool` | Release the process handle (`Trusted`) |

> `Spawn`, `Wait`, and similar functions have a safety level of `Trusted`; functions that call them must be `Trusted` or `Unsafe`.

## Complete Example

```kinal
Unit App.AsyncDemo;

Get IO.Console;
Get IO.Async;

Static Async Function int Fetch(int id)
{
    Await IO.Async.Sleep(10);  // simulate network latency
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

## Important Notes

1. **`Await` can only be used inside `Async` functions** — using `Await` in a regular function produces a compile error
2. **`IO.Async.Run` is synchronous** — it blocks the calling thread until the task completes
3. **Async functions return Task** — the return type is automatically wrapped; callers receive `IO.Async.Task`
4. **Chained Await** — Await can be chained indefinitely; each step waits for the previous one to complete

## Next Steps

- [Block (Code Block Objects)](blocks.md)
- [Safety Levels](safety.md)
- [IO.Async Standard Library](../stdlib/async.md)
