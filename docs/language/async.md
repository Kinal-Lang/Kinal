# Async Programming

The complete programs on this page target **Native**. Current C KNC emission rejects general Await expressions and the Task Run builtin. VM `--superloop` controls loop fusion, not Async task scheduling.

`Async` declares a task-producing function and `Await` expresses a dependency on another task. Async syntax does not automatically make work parallel or turn a blocking file/network API into asynchronous I/O.

## Declaring and Awaiting Tasks

The declared result is the value produced when the task finishes:

```kinal
Async Function int Compute(int value)
{
    Return value + 1;
}
```

Calling `Compute(41)` produces an `IO.Async.Task`; awaiting it obtains the declared int result. Use Await inside an Async function, including an Async Main. A non-async function cannot use Await directly.

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

Both IO.Console and IO.Async are imported in this complete program. A qualified standard-library name does not remove its import requirement.

## Calling from Synchronous Code

Use `IO.Async.Run(task)` to wait synchronously. Pass the result of an async call, not a function name:

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

These Await operations are sequential dependencies. They are not a concurrent fan-out API. IO.Async.Run blocks the calling execution path until completion; the builtin's erased result is any, with supported source calls recovering the async function's result type during analysis.

## Task Primitives

| Function | Declared result | Purpose |
|----------|-----------------|---------|
| `IO.Async.Sleep(int ms)` | `IO.Async.Task` | Timer/wait task |
| `IO.Async.Yield()` | `IO.Async.Task` | Cooperative yield task |
| `IO.Async.Run(IO.Async.Task task)` | `any` | Synchronously complete a task and obtain its result |

These primitives are Safe. Scheduler behavior depends on backend/runtime. Do not infer thread counts, nonblocking system calls or unlimited scalability from Async syntax alone.

## Task and Process Handles Are Different

`IO.Async.Spawn(string command)` starts a **child process** and returns `byte*`; it does not start an arbitrary Kinal function as a task. `IsCompleted`, `Wait`, `ExitCode` and `Close` operate on that process handle. Pass a command string, not a function object. See [IO.Async API](../stdlib/async.md).

A process handle is an external resource requiring Close. A managed Task has a different representation and lifecycle; do not interchange them or pass a process handle to Run.

## Backend and Runtime Requirements

VM `--superloop`/`--no-superloop` toggles specialized bytecode loop fusion for eligible For/While patterns. It neither enables nor disables the language Task implementation. Current C KNC has no general Await emission and does not map IO.Async.Run, so the programs above must be built through the Native path.

Freestanding task/exception/allocation support requires target runtime hooks; core-only mode does not supply hosted async services. See [Backend Support](../advanced/backend-support.md) and [VM CLI](../cli/vm.md).

## Error Handling and Validation

The current Sema rejects Await inside Try/Catch. When recovery is needed, use a supported synchronous Run boundary inside Try/Catch, or structure async work outside the handler region. An Async declaration does not convert failure into a successful result. Validate output and termination on the claimed backend; semantic checking alone does not exercise task scheduling.

```bash
kinal run async-demo.kn
```

## See Also

- [IO.Async](../stdlib/async.md)
- [Functions](functions.md)
- [Exceptions](exceptions.md)
- [Blocks](blocks.md)
