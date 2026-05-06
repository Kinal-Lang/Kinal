# Control Flow Diagnostics

Diagnostics related to control-flow rules such as `Return`, `Throw`, `Break`, and `Continue`.

The examples in this document are minimal reproductions of the most common way each diagnostic code is triggered. A few diagnostics that are more implementation-oriented use the closest surface-level form that user code can express.

---

## Diagnostic Code Index

| Code | Title | Default Detail |
|------|------|----------|
| `E-CTL-00001` | Invalid Control Flow | break/continue not in loop |
| `E-CTL-00002` | Invalid Return | Return is not allowed inside Block |
| `E-CTL-00003` | Invalid Throw | Throw expression must be string or IO.Error |
| `E-CTL-00004` | Invalid Throw | Throw requires an expression |
| `E-CTL-00005` | Missing Return | Non-void function must return a value on all paths |
| `E-CTL-00006` | Missing Return | Non-void method must return a value on all paths |
| `E-CTL-00007` | Return Outside Function | Return not allowed here |
| `E-CTL-00008` | Return Type | Non-void function must return a value |
| `E-CTL-00009` | Return Type | Void function cannot return a value |
| `E-CTL-00010` | Throw Outside Function | Throw not allowed here |

---

## E-CTL-00001 — Invalid Control Flow

- Severity: Error
- Default Detail: break/continue not in loop
- Description: `break` or `continue` can only appear inside a loop.

Incorrect Example:

```kinal
Static Function int Main()
{
    Break;
    Return 0;
}
```

Correct Example:

```kinal
Static Function int Main()
{
    While (true)
    {
        Break;
    }
    Return 0;
}
```

Reason: `break` / `continue` rely on the latest loop context; they have no legal jump target after leaving the loop body.

Fix: Move the statement into a loop body such as `While` / `For`; if the goal is to leave the function early, use `Return` instead.

---

## E-CTL-00002 — Invalid Return

- Severity: Error
- Default Detail: Return is not allowed inside Block
- Description: `Block` is not allowed to use `Return` internally to exit the outer function directly.

Incorrect Example:

```kinal
Block Demo</
    Return 1;
/>
```

Correct Example:

```kinal
Block Demo</
    Jump Done;
    Record Done;
/>
```

Reason: `Block` is a pauseable object, not an ordinary function body; `Return` will destroy its jump model.

Fix: Use the `Record` / `Jump` / `RunUntil` set of control flow APIs in Block instead of directly `Return`.

---

## E-CTL-00003 — Invalid Throw

- Severity: Error
- Default Detail: Throw expression must be string or IO.Error
- Description: `Throw` can only throw `string` or `IO.Error`.

Incorrect Example:

```kinal
Throw 42;
```

Correct Example:

```kinal
Throw "invalid state";
```

Reason: Kinal currently only provides stable exception models for string exceptions and `IO.Error`.

Fix: If you just want to throw textual reasons, use `string`; if you want to carry structured information, construct `IO.Error`.

---

## E-CTL-00004 — Invalid Throw

- Severity: Error
- Default Detail: Throw requires an expression
- Description: The `Throw` keyword must be followed by an exception expression.

Incorrect Example:

```kinal
Throw;
```

Correct Example:

```kinal
Throw New IO.Error("Config", "missing key");
```

Reason: A separate `Throw` does not contain an exception object, and the runtime cannot construct an error value to propagate.

Fix: Provide a string or `IO.Error` expression after `Throw`.

---

## E-CTL-00005 — Missing Return

- Severity: Error
- Default Detail: Non-void function must return a value on all paths
- Description: Non-`void` functions must return values on all paths.

Incorrect Example:

```kinal
Function int Score(bool ok)
{
    If (ok)
    {
        Return 1;
    }
}
```

Correct Example:

```kinal
Function int Score(bool ok)
{
    If (ok)
    {
        Return 1;
    }
    Return 0;
}
```

Reason: Control flow analysis found that at least one path leads to the end of a function that declares a non-`void` return type.

Fix: Complete `Return` for each branch, or change the function return type to `void`.

---

## E-CTL-00006 — Missing Return

- Severity: Error
- Default Detail: Non-void method must return a value on all paths
- Description: Non-`void` methods must also return values on all paths.

Incorrect Example:

```kinal
Class Counter
{
    Public Function int Value(bool ok)
    {
        If (ok)
        {
            Return 1;
        }
    }
}
```

Correct Example:

```kinal
Class Counter
{
    Public Function int Value(bool ok)
    {
        If (ok)
        {
            Return 1;
        }
        Return 0;
    }
}
```

Reason: Methods share the same return path checking rules as ordinary functions.

Fix: Make all branches return explicitly, or change the method to `void`.

---

## E-CTL-00007 — Return Outside Function

- Severity: Error
- Default Detail: Return not allowed here
- Description: `Return` cannot be written directly outside a function or method.

Incorrect Example:

```kinal
Return 0;
```

Correct Example:

```kinal
Static Function int Main()
{
    Return 0;
}
```

Reason: `Return` can only end the current function execution; there is no returnable function context in the top-level, class body, or other declaration area.

Fix: Put `Return` inside a function, method, or constructor body.

---

## E-CTL-00008 — Return Type

- Severity: Error
- Default Detail: Non-void function must return a value
- Description: `Return` in non-`void` functions must carry a value.

Incorrect Example:

```kinal
Function int Add(int a, int b)
{
    Return;
}
```

Correct Example:

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}
```

Reason: The return statement did not provide an expression matching the function's return type.

Fix: Give the value of the corresponding type after `Return`; if there is no need to return a value, change the return type to `void`.

---

## E-CTL-00009 — Return Type

- Severity: Error
- Default Detail: Void function cannot return a value
- Description: The `void` function cannot return a specific value.

Incorrect Example:

```kinal
Function void Log()
{
    Return 1;
}
```

Correct Example:

```kinal
Function void Log()
{
    Return;
}
```

Reason: `void` means that the call site does not produce a return value and the return expression has no legal receiving position.

Fix: Remove the return value, or change the function signature to the specific type required.

---

## E-CTL-00010 — Throw Outside Function

- Severity: Error
- Default Detail: Throw not allowed here
- Description: `Throw` cannot be written directly outside a function or method.

Incorrect Example:

```kinal
Throw "boom";
```

Correct Example:

```kinal
Static Function int Main()
{
    Throw "boom";
}
```

Reason: Exception propagation also requires an execution context; exceptions cannot be thrown directly from the top-level declaration area.

Fix: Place `Throw` in the execution statement area allowed by a function, method, or Block.
