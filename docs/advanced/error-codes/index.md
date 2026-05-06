# Diagnostic Code Reference

This document summarizes the built-in diagnostic codes shared by the Kinal compiler and language server. It is organized by category and provides a minimal reproducer, an explanation, and a fix for each diagnostic code.

---

## Code Format

- `E-` indicates an error and stops compilation from succeeding.
- `W-` indicates a warning. It does not stop compilation by default, but it can be promoted to an error with `--Werror`.
- The three letters in the middle identify the diagnostic category. For example, `SYN` means syntax, `SEM` means semantics, and `RES` means name resolution.
- The last five digits are the stable identifier. As long as the diagnostic still exists, its public code should remain stable.

## Usage

```bash
kinal build main.kn --lang en
kinal build main.kn --lang en --Werror
```

## Category Index

| Category | Prefix | Count | Document |
|------|------|------|------|
| Argument Diagnostics | `ARG` | 5 | [View](./arguments.md) |
| Control Flow Diagnostics | `CTL` | 10 | [View](./control-flow.md) |
| Entry Point Diagnostics | `ENT` | 5 | [View](./entry-point.md) |
| General and Generic Diagnostics | `GEN` | 19 | [View](./general.md) |
| Code Cleanliness Warnings | `LNT` | 2 | [View](./lint.md) |
| Naming and Duplicate Definition Diagnostics | `NAM` | 18 | [View](./naming.md) |
| Name Resolution Diagnostics | `RES` | 23 | [View](./resolution.md) |
| Safety-Level Warnings | `SAF` | 1 | [View](./safety.md) |
| Semantic Diagnostics | `SEM` | 50 | [View](./semantics.md) |
| Syntax Diagnostics | `SYN` | 109 | [View](./syntax.md) |
| Type System Diagnostics | `TYP` | 12 | [View](./types.md) |

## Writing Notes

- Examples prefer the most common and easiest-to-read trigger rather than trying to cover every internal compiler variant.
- When multiple diagnostic codes differ only by scope or entity kind, the documentation reuses the same minimal code shape and focuses on the trigger condition and the fix strategy.
- Syntax diagnostics usually use local snippets; semantic diagnostics usually lean toward complete declarations or complete functions.

## Related Documents

- [Compilation Pipeline](../compilation-pipeline.md)
- [kinal Compiler](../../cli/compiler.md)
- [Classes and Inheritance](../../language/classes.md)
- [Functions](../../language/functions.md)
- [Type System](../../language/types.md)
- [Blocks](../../language/blocks.md)
