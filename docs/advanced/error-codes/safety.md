# Safety-Level Warnings

Warnings related to `Safe` / `Trusted` / `Unsafe` propagation.

The examples in this document are minimal reproductions of the most common way each diagnostic code is triggered. A few diagnostics that are more implementation-oriented use the closest surface-level form that user code can express.

---

## Diagnostic Code Index

| Code | Title | Default Detail |
|------|------|----------|
| `W-SAF-00001` | Entry Unsafe | Entry point is marked as Unsafe. This disables safety guarantees for the entire program. Consider using Safe Main and isolating Unsafe blocks locally. |

---

## W-SAF-00001 — Entry Unsafe

- Severity: Warning
- Default Detail: Entry point is marked as Unsafe. This disables safety guarantees for the entire program. Consider using Safe Main and isolating Unsafe blocks locally.
- Description: The entry function is marked as `Unsafe`, which means that the entire program entry no longer enjoys the static guarantee of the security layer.

Incorrect Example:

```kinal
Static Unsafe Function int Main()
{
    Return 0;
}
```

Correct Example:

```kinal
Static Safe Function int Main()
{
    Unsafe Block LowLevel</
        byte* ptr = null;
    />
    Return 0;
}
```

Reason: Once `Main` is `Unsafe`, the boundary of dangerous operations will spread from the local to the entire process entry.

Fix: Prioritize keeping `Main` as `Safe` and only use `Trusted` or `Unsafe Block` in local areas where it is really needed.
