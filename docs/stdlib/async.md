# IO.Async: Tasks and Child Processes

This is the Native API surface. General Await/Run lowering is not available in the current C KNC backend. Superloop fusion is a separate bytecode optimization.

Import `Get IO.Async;`. The namespace contains two distinct APIs: managed language Tasks, and host child-process handles. A process handle is not an IO.Async.Task.

## Complete API

| Function | Result | Safety / meaning |
|----------|--------|------------------|
| `Run(IO.Async.Task task)` | `any` | Safe; synchronously obtain a task result |
| `Sleep(int ms)` | `IO.Async.Task` | Safe; wait task |
| `Yield()` | `IO.Async.Task` | Safe; cooperative yield task |
| `Spawn(string command)` | `byte*` | Trusted; start child process |
| `IsCompleted(byte* handle)` | `bool` | Trusted; poll completion |
| `Wait(byte* handle, int ms)` | `bool` | Trusted; wait with timeout |
| `ExitCode(byte* handle)` | `int` | Trusted; inspect process exit status |
| `Close(byte* handle)` | `bool` | Trusted; release process handle |

Trusted APIs can be called from Safe code under the language call rules; raw pointer operations have their own restrictions. Process wrappers commonly use Trusted functions to keep host-resource handling in one place.

## Tasks

An Async function call creates a task. Pass `Compute()` to Run, not `Compute`. Await is only valid inside Async functions; Sleep/Yield return Tasks, not void. See the complete programs in [Async Programming](../language/async.md).

The API does not define `Spawn(Function)`, general thread creation, task cancellation, a task-combinator family or an automatic parallel map. Sequential Await does not prove overlapping execution. A blocking library call remains blocking unless that API defines otherwise.

## Process Lifecycle

The following Native-host example waits for a small child command, prints its exit code and closes the handle:

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

`Wait(handle, 0)` polls immediately; a positive value is a millisecond timeout; `-1` waits indefinitely. Query ExitCode after completion. Close releases the handle; it is not documented as a general child-termination API.

Command strings follow the target host's process/shell conventions. This API is not an argv-array interface: do not construct commands by blindly concatenating untrusted input. The example above uses fixed commands and validates completion before reading the result.

## Runtime and Platform Boundaries

Process operations require hosted OS services. The fact that their declarations exist does not provide those services to a freestanding image or every VM build. Runtime/VM support and task superloop options are covered by [Backend Support](../advanced/backend-support.md) and [VM CLI](../cli/vm.md).

Close external process resources explicitly; Kinal GC does not replace handle ownership. See [Memory and Lifetimes](../advanced/memory-management.md).
