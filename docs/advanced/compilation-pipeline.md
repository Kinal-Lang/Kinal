# Compilation Pipeline

This document describes the complete compilation pipeline from Kinal source files to executables.

---

## Overview

```
.kn source file
    │
    ▼
┌─────────────┐
│   Lexer     │  kn_lexer.c
│             │  Token stream
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   Parser    │  kn_parser.c + parser/*.inc
│             │  Abstract Syntax Tree (AST)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   Sema      │  kn_sema.c
│             │  Type checking, name resolution, safety checks
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   Codegen   │  LLVM-based
│             │  LLVM IR
└──────┬──────┘
       │
       ├──────────────────────────────────┐
       │                                  │
       ▼                                  ▼
┌─────────────┐                   ┌──────────────┐
│  LLVM Back  │                   │  KNC Bytecode│
│  Opt+Target │                   │  Generator   │
│  Codegen    │                   └──────┬───────┘
└──────┬──────┘                          │
       │                                 ▼
       ▼                          ┌──────────────┐
 Object file (.o)                 │  .knc bytecode│
       │                          └──────┬───────┘
       ▼                                 │
┌─────────────┐                          ▼
│   Linker    │                   ┌──────────────┐
│ lld/zig/   │                   │  KinalVM     │
│ msvc       │                   │  Execution   │
└──────┬──────┘                  └──────────────┘
       │
       ▼
 Executable/Library
```

---

## Phase Details

### 1. Lexer

**Source file**: `apps/kinal/src/kn_lexer.c`

The lexer converts the source file byte stream into a token sequence.

Key tasks:
- Recognize keywords (`If`, `While`, `Function`, `Class`, etc., all PascalCase)
- Recognize identifiers, numeric literals, and string literals
- Handle string prefixes: `[r]"`, `[f]"`, `[c]"`
- Skip comments (`//` and `/* */`)
- Track source positions (line and column numbers) for error reporting

Generated token types are defined in `apps/kinal/include/kn/lexer.h`.

### 2. Parser

**Source file**: `apps/kinal/src/kn_parser.c` + `apps/kinal/src/parser/*.inc`

The parser converts the token sequence into an Abstract Syntax Tree (AST).

The parser is split into several segments (`.inc` files) by functionality:

| File | Responsibility |
|------|----------------|
| `parse_decl.inc` | Top-level declarations (Unit, Function, Class, Struct, Enum, Meta) |
| `parse_expr.inc` | Expressions (operators, function calls, member access, type casts) |
| `parse_stmt.inc` | Statements (If, While, For, Return, Try, Throw, Block) |
| `parse_type.inc` | Type expressions (primitive types, arrays, pointers, generics) |

Key syntax characteristics:
- Type cast syntax `[type](expr)` is handled specially by the parser
- String prefixes `[raw]"..."` / `[f]"..."` / `[c]"..."` are handled cooperatively between the lexer and parser
- Generic function `Function T Name<T>` type parameters are parsed at the declaration stage

AST node types are defined in `apps/kinal/include/kn/ast.h`.

### 3. Semantic Analysis (Sema)

**Source file**: `apps/kinal/src/kn_sema.c`

The semantic analyzer performs type checking and semantic validation on the AST.

Key tasks:
- **Name resolution**: Bind each identifier to its declaration
- **Type inference**: Infer the type of `Var` declarations
- **Type checking**: Validate type compatibility for expressions, assignments, and function calls
- **Safety checking**: Ensure `Unsafe` APIs are only called in `Trusted`/`Unsafe` contexts
- **Module resolution**: Process `Get` imports, locate standard library and user modules
- **Built-in type injection**: `kn_builtins.c` injects `IO.Type.Object.Class`, `IO.Error`, etc. into the symbol table
- **Standard library binding**: `kn_std.c` binds all standard library functions as callable built-ins

### 4. Code Generation (Codegen)

After semantic validation, the code generator traverses the type-annotated AST to produce backend code.

Kinal supports two backend paths:

#### LLVM Path

1. **LLVM IR generation**: Convert the AST to LLVM Intermediate Representation
2. **LLVM optimization**: Apply LLVM optimization passes (`-O0`/`-O1`/`-O2`/`-O3`)
3. **Target code generation**: LLVM backend generates target-platform assembly/object files
4. **Linking**: lld/zig/msvc linker produces the final executable or library

#### KNC Path

1. **Bytecode generation**: Convert the AST to `.knc` bytecode
2. **Optional packaging**: `kinal vm pack` bundles bytecode with KinalVM

---

## Built-in Type Injection

Before semantic analysis begins, `kn_builtins.c` injects the following built-in types into the symbol table:

| Type | Description |
|------|-------------|
| `IO.Type.Object.Class` | Root base class for all `Class` types, provides `ToString()` and other methods |
| `IO.Type.Object.Function` | Function object type (contains `Invoke` and `Env` fields) |
| `IO.Type.Object.Block` | Block object type (inherits from Function) |
| `IO.Error` | Standard exception class (`Title`, `Message`, `Inner`, `Trace` fields) |
| `IO.Time.DateTime` | Time struct (returned by `IO.Time.Now()`) |
| `IO.Collection.list` | Built-in `list` type |
| `IO.Collection.dict` | Built-in `dict` type |
| `IO.Collection.set` | Built-in `set` type |

---

## Standard Library Function Bindings

`kn_std.c` contains a registry of all standard library functions (`g_*_funcs[]` arrays). Each entry contains:

- Fully qualified function name (e.g., `IO.Console.PrintLine`)
- Parameter type signature
- Return type
- Safety level requirement (`SAFE`/`TRUSTED`/`UNSAFE`)
- Reference to the built-in function implementation

The compiler uses this registry during semantic analysis to resolve standard library calls.

---

## Error Reporting

The Kinal compiler's diagnostic system supports:

- Precise line/column location
- Colored terminal output (`--color auto/always/never`)
- Bilingual diagnostics in Chinese and English (`--lang zh/en`)
- Custom locale files (`--locale-file`)

---

## Compilation Cache and Intermediate Files

Use `--keep-temps` to retain intermediate files generated during compilation:

```bash
kinal build main.kn --emit ir --keep-temps
```

Retained files:
- `main.ll` — LLVM IR
- `main.s` — Assembly code
- `main.o` — Object file

---

## See Also

- [Compiler CLI](../cli/compiler.md) — Complete compiler options
- [KinalVM](../cli/vm.md) — Bytecode execution path
- [Cross-Compilation](cross-compilation.md) — Multi-platform compilation
- [Linking](linking.md) — Linker configuration
- [IO.Runtime Constants](runtime-environment-constants.md) — Distinguishing native and VM backends in source code
