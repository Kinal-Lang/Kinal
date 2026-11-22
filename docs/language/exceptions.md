# Exception Handling

The current C language frontend accepts `Throw` values of type `string` or `IO.Error`. It does not support throwing arbitrary integers, objects, or `any` values. Exception handling is available in hosted code; freestanding core forbids Try/Catch/Throw.

## Try, Catch, and Throw

A Try has one Catch handler:

```kinal
Try
{
    Throw "operation failed";
}
Catch (string message)
{
    // message contains the thrown text
}
```

Other supported handler spellings are `Catch (IO.Error error)` and parameterless `Catch { ... }`. Multiple consecutive Catch clauses and `Finally` are not in the current grammar. Nest Try blocks when separate recovery boundaries are needed.

`Throw expression;` requires an expression. There is no bare rethrow keyword form; explicitly throw the caught value when propagating it. The catch variable is scoped to its handler.

## `IO.Error`

Construct an error with **two** arguments:

```kinal
IO.Error error = New IO.Error("File Error", "File was not found");
```

| Field | Type | Meaning |
|-------|------|---------|
| `Title` | `string` | Short error classification/title |
| `Message` | `string` | Error details |
| `Inner` | `IO.Error` | Linked cause; can be null |
| `Trace` | `string` | Runtime-recorded trace text |

The built-in constructor takes title/message. There is no three-argument `(title, message, inner)` overload. Set `Inner` explicitly when constructing a deterministic cause chain. The runtime can also attach recorded causes/traces during throw processing; avoid relying on implicit chaining when the exact structure matters.

## Complete Recovery and Chaining Example

```kinal
Unit Docs.Exceptions;
Get IO.Console;
Get IO.Error;

Function int Divide(int left, int right)
{
    If (right == 0)
    {
        Throw New IO.Error("Division Error", "Divisor cannot be zero");
    }
    Return left / right;
}

Function void Calculate()
{
    Try
    {
        IO.Console.PrintLine(Divide(10, 0));
    }
    Catch (IO.Error cause)
    {
        IO.Error outer = New IO.Error("Calculation Error", "Unable to calculate");
        outer.Inner = cause;
        Throw outer;
    }
}

Static Function int Main()
{
    Try
    {
        Calculate();
    }
    Catch (IO.Error error)
    {
        IO.Console.PrintLine(error.Title);
        IO.Console.PrintLine(error.Message);
        If (error.Inner != null)
        {
            IO.Console.PrintLine(error.Inner.Title);
        }
    }
    Return 0;
}
```

Expected output on Native and VM:

```text
Calculation Error
Unable to calculate
Division Error
```

The recovery function returns success after handling the error. For an unrecoverable application failure, choose an appropriate nonzero exit status instead of swallowing it silently.

## Catch Representation and Backend Boundaries

`Catch (string message)` can expose an error's message text, including for an `IO.Error` throw. It is not a general arbitrary-type pattern-matching handler. For portable code, catch a thrown string as string and a thrown `IO.Error` as `IO.Error`.

There is a known cross-backend boundary when catching a raw string as `IO.Error`: Native normalizes it to an error object, while the current VM path can leave the exception pending and exit nonzero. Do not depend on that mixed spelling. Use explicit `IO.Error` construction when structured recovery is required. There is no general user-class exception hierarchy dispatch contract.

An uncaught exception terminates execution. Exact diagnostic colors and trace spelling are runtime/presentation details; tests should assert the relevant error information and exit status rather than assuming trace paths are portable.

## Runtime Inspection Helpers

Import `Get IO.Error;` when calling these helpers:

| Helper | Result | Purpose |
|--------|--------|---------|
| `IO.Error.LastTrace()` | `string` | Inspect the runtime's recorded trace |
| `IO.Error.HasException()` | `bool` | Query pending exception status |
| `IO.Error.Current()` | `IO.Error` | Inspect the recorded exception object |
| `IO.Error.Clear()` | `void` | Clear pending exception state |

Recorded last-error information and pending status are distinct. Prefer the explicit Catch binding for ordinary recovery. Clearing pending status is not resource cleanup and is not a substitute for executing the appropriate error path.

## Resource Handling

Try/Catch does not introduce deterministic destructors or a Finally block. Use explicit close/release calls on both success and recovery paths when owning an external resource. GC collection does not close arbitrary foreign resources. See [Memory and Lifetimes](../advanced/memory-management.md).

## See Also

- [Functions](functions.md)
- [Safety](safety.md)
- [Backend Support](../advanced/backend-support.md)
- [Freestanding](../advanced/freestanding.md)
