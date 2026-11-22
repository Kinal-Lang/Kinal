# Diagnostic Code Reference

This document summarizes the built-in diagnostic codes shared by the Kinal compiler and language server. It is organized by category and provides a trigger explanation and a fix for each registered template. Examples distinguish copyable failures from context-dependent trigger sketches.

---

## Code Format

- `E-` indicates an error and stops compilation from succeeding.
- `W-` indicates a warning. It does not stop compilation by default, but it can be promoted to an error with `--Werror`.
- The three letters in the middle identify the diagnostic category. For example, `SYN` means syntax, `SEM` means semantics, and `RES` means name resolution.
- The last five digits identify a registered template; see the stability boundary below for dynamically assigned codes.

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
| Syntax Diagnostics | `SYN` | 110 | [View](./syntax.md) |
| Type System Diagnostics | `TYP` | 13 | [View](./types.md) |

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

## Coverage and Code Stability

This catalog follows the preregistered templates in the C compiler's kn_diag_registry.c. New unregistered diagnostics receive group-local sequential IDs in encounter order, so additional GEN/WRN or other diagnostics may appear. Automatically assigned IDs are not a cross-version stable API. Preserve phase, title, detail, location and compiler version when reporting a failure.

Examples are local fragments: supply imports and the enclosing type/function required by the rule. Deliberately incorrect fragments are not runnable programs. Use --emit check for frontend diagnosis; backend/linker failures may not carry the same language diagnostic format. Fix the first root cause before investigating subsequent errors.

Compiler --lang zh and editor diagnosticsLanguage select their diagnostic language independently. A custom --locale-file changes template text, not semantics; do not identify diagnostics solely by translated prose.

Use a full Native build to check entry diagnostics; --emit check does not require an executable entry. Unused-variable and unreachable-code warnings are level 2; enable them with --warn-level 2. Invalid top-level Return/Throw can first report E-SYN-00106. Parser recovery, overload selection and template normalization can change the observed code.
