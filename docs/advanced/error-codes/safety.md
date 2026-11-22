# Safety-Level Warnings

Warnings related to `Safe` / `Trusted` / `Unsafe` propagation.

This page records the registered templates. Corrected examples show the supported form. A trigger sketch illustrates a rule or parser state and may require surrounding declarations or report an earlier/different diagnostic in the current C compiler.

---

## Diagnostic Code Index

| Code | Title | Default Detail |
|------|------|----------|
| `W-SAF-00001` | Entry Unsafe | Entry point is marked as Unsafe. This disables safety guarantees for the entire program. Consider using Safe Main and isolating Unsafe blocks locally. |

---

## W-SAF-00001 — Entry Unsafe

- Severity: Warning
- Default Detail: Entry point is marked as Unsafe. This disables safety guarantees for the entire program. Consider using Safe Main and isolating Unsafe blocks locally.
- Description: A Native executable entry is marked Unsafe. Calls in other functions still follow their own safety declarations.

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

Reason: an Unsafe entry permits protected operations throughout Main. Compile a Native executable to check this warning; a frontend-only check does not select the entry.

Fix: Prioritize keeping `Main` as `Safe` and only use `Trusted` or `Unsafe Block` in local areas where it is really needed.
