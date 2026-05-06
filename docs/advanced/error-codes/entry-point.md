# Entry Point Diagnostics

Diagnostics related to `Main` / `KMain` entry-point signatures and uniqueness.

The examples in this document are minimal reproductions of the most common way each diagnostic code is triggered. A few diagnostics that are more implementation-oriented use the closest surface-level form that user code can express.

---

## Diagnostic Code Index

| Code | Title | Default Detail |
|------|------|----------|
| `E-ENT-00001` | Entry Ambiguous | Multiple Main functions found |
| `E-ENT-00002` | Entry Signature | Main must be Static |
| `E-ENT-00003` | Entry Signature | Main must return int |
| `E-ENT-00004` | Entry Signature | Main must take zero or one parameter |
| `E-ENT-00005` | Entry Signature | Main parameter must be string[] |

---

## E-ENT-00001 — Entry Ambiguous

- Severity: Error
- Default Detail: Multiple Main functions found
- Description: There can only be one `Main` for program entry.

Incorrect Example:

```kinal
Static Function int Main()
{
    Return 0;
}

Static Function int Main(string[] args)
{
    Return 0;
}
```

Correct Example:

```kinal
Static Function int Main(string[] args)
{
    Return 0;
}
```

Reason: The compiler cannot automatically pick the final launch point among multiple entries with the same name.

Fix: Only one `Main` that conforms to the specification is retained, and the remaining entries are renamed to ordinary auxiliary functions.

---

## E-ENT-00002 — Entry Signature

- Severity: Error
- Default Detail: Main must be Static
- Description: The program entry must be declared as `Static`.

Incorrect Example:

```kinal
Function int Main()
{
    Return 0;
}
```

Correct Example:

```kinal
Static Function int Main()
{
    Return 0;
}
```

Reason: The launcher will not construct a class instance first and then call the entry point; the entry point must be statically callable directly.

Fix: Add `Static` before `Main`.

---

## E-ENT-00003 — Entry Signature

- Severity: Error
- Default Detail: Main must return int
- Description: The program entry return type must be `int`.

Incorrect Example:

```kinal
Static Function void Main()
{
}
```

Correct Example:

```kinal
Static Function int Main()
{
    Return 0;
}
```

Reason: The entry return code maps to the process exit code, so the signature must be stable to `int`.

Fix: Change the return type to `int` and explicitly return the exit code.

---

## E-ENT-00004 — Entry Signature

- Severity: Error
- Default Detail: Main must take zero or one parameter
- Description: `Main` can only take no parameters or only one parameter.

Incorrect Example:

```kinal
Static Function int Main(string[] args, int mode)
{
    Return 0;
}
```

Correct Example:

```kinal
Static Function int Main(string[] args)
{
    Return 0;
}
```

Reason: The entrance parameter layout is a fixed convention and cannot be expanded into multiple independent parameters at will.

Fix: Convergence of entry arguments to zero arguments, or a single `string[] args`.

---

## E-ENT-00005 — Entry Signature

- Severity: Error
- Default Detail: Main parameter must be string[]
- Description: The entry parameter can only be the command line parameter array `string[]`.

Incorrect Example:

```kinal
Static Function int Main(int[] args)
{
    Return 0;
}
```

Correct Example:

```kinal
Static Function int Main(string[] args)
{
    Return 0;
}
```

Reason: The host environment passes in an array of command line strings, not any type.

Fix: Change the entry parameter type to `string[]`.
