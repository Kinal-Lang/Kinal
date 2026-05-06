# Code Cleanliness Warnings

Diagnostics that do not block compilation, but should still be cleaned up soon.

The examples in this document are minimal reproductions of the most common way each diagnostic code is triggered. A few diagnostics that are more implementation-oriented use the closest surface-level form that user code can express.

---

## Diagnostic Code Index

| Code | Title | Default Detail |
|------|------|----------|
| `W-LNT-00001` | Unused Variable | Variable is never used |
| `W-LNT-00002` | Unreachable Code | Unreachable code |

---

## W-LNT-00001 — Unused Variable

- Severity: Warning
- Default Detail: Variable is never used
- Description: The variable is never read after it is defined.

Incorrect Example:

```kinal
Static Function int Main()
{
    int unused = 42;
    Return 0;
}
```

Correct Example:

```kinal
Static Function int Main()
{
    int used = 42;
    Return used;
}
```

Reason: Unused variables usually mean redundant code, missing logic, or typos.

Fix: Remove the useless variable, or actually read it and incorporate it into the business logic.

---

## W-LNT-00002 — Unreachable Code

- Severity: Warning
- Default Detail: Unreachable code
- Description: A certain piece of code can never be executed.

Incorrect Example:

```kinal
Static Function int Main()
{
    Return 0;
    IO.Console.PrintLine("never");
}
```

Correct Example:

```kinal
Static Function int Main()
{
    IO.Console.PrintLine("once");
    Return 0;
}
```

Reason: The previous `Return`, `Throw`, infinite loop or constant true/constant false branch has cut off the subsequent execution path.

Fix: Delete unreachable statements or move them to a location where they are still executable.
