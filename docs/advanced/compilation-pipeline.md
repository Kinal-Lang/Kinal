# Compilation Pipeline

This page describes the C compiler. The Native and KNC backends share frontend analysis; KNC is generated from the typed program, not translated from LLVM IR.

## Pipeline and Stop Points

```text
CLI / project / package source selection
                  |
             Lexer + Parser
                  |
             Sema / typed AST
                  |
       Resolved Call + Binary HIR slices
                /     \
       Native LLVM     KNC writer
            |              |
      IR / asm / object    .knc
            |              |
      Native linker     KinalVM interpreter
            |              |
      executable/library   optional packed VM executable
```

| Command / emit mode | Last relevant stage |
|---------------------|---------------------|
| `--emit tokens` | Lexical token summary |
| `--emit ast` | Parser AST summary |
| `--emit check` | Successful Sema/resolved-plan summary |
| `--emit ir` | Native LLVM IR |
| `--emit asm` / `obj` | Native target emission |
| `build` with default `bin` | Native link |
| `vm build` | KNC bytecode write |
| `run` / `vm run` | Compilation as needed, then execution |

Frontend summaries are deterministic test/inspection artifacts, not executable code or a stable package interface. A successful check does not validate backend support, linker inputs, runtime dependencies or program behavior.

## Source Selection and Imports

The driver loads the selected project/profile, collects source candidates and resolves imported Units/packages while preparing the compilation. Explicit SourceSets constrain local membership; imports determine visibility, not unrestricted filesystem participation. Packages can provide source and native assets through manifests/archives.

For entry-file discovery, the driver indexes the entry directory and adds participating same-Unit/imported-Unit files. Unsafe-alias files are local-only and cannot be imported as project Units. See [Project Files](../cli/project-files.md) and [Packages](../cli/packages.md).

## Lexer and Parser

The lexer recognizes source bytes, identifiers, keywords, literals, punctuation and comments, retaining source locations. Unicode identifiers are accepted; source byte columns must be converted at the LSP UTF-16 protocol boundary.

The parser builds declarations/statements/expressions and applies the file-local unsafe-alias token prepass. The C parser's included implementation files are:

| File under `apps/kinal/src/parser/` | Responsibility |
|------------------------------------|----------------|
| `kn_parser_decl.inc` | Declarations, modifiers, attributes and Meta |
| `kn_parser_type_expr.inc` | Types, expressions, calls, casts and string-prefix handling |
| `kn_parser_stmt.inc` | Statements and literal escape helpers |
| `kn_parser_float.inc` | Floating literal parsing helpers |

A lexically valid token stream can still fail parsing. Inactive compile-time branches must remain syntactically valid. A declaration-only AST summary does not prove that names and types resolve.

## Sema and Resolved Plans

Sema validates types, scopes, imports, defaults, generics, overloads, safety, entry points and other language rules. The driver supplies the participating program; Sema does not fetch missing packages from a registry.

After successful analysis, every bound direct/member call has a `KnResolvedCall`; each binary expression has a `KnResolvedBinary` describing selected operation kind, coercions, pointer side, unsignedness and target-aware integer width. Native and KNC consume those decisions rather than independently choosing call meaning or numeric coercion.

These are **incremental HIR slices**, not a complete backend-independent statement/expression IR. Other families still use typed AST data. Compile-time conditions select their active semantic branch; inactive platform syntax can intentionally remain unbound and must not inflate unresolved-plan counts.

`--emit check` writes `format=kinal-sema-v1`, `hir_format=kinal-call-hir-v1` and `binary_hir_format=kinal-binary-hir-v1` with structural counts. [Developer Architecture](../../dev/architecture.md) explains the boundary in more detail.

## Native LLVM and Linking

Native lowering emits target-specific LLVM types, functions, globals, runtime calls and GC roots. LLVM produces IR, assembly or an object as requested. The linker combines objects, startup/CRT files, runtime helpers and compatible package libraries.

The current CLI exposes LTO controls; internal optimization passes do not imply public `-O0`/`-O1`/`-O2`/`-O3` options. Use documented options and inspect the actual link command rather than assuming an optimization or library policy.

Objects/IR for a foreign target are not runnable host programs. Extern ABI declarations and runtime hooks must match that target. See [Linking](linking.md), [Cross-Compilation](cross-compilation.md) and [FFI](../language/ffi.md).

## KNC and Runtime Boundaries

The KNC writer directly consumes the semantically bound program and emits supported bytecode operations. Unsupported features fail during emission, even when shared Sema succeeds. Superloop is recognized-loop fusion, not a task scheduler.

The current writer emits KNC format 3; the magic remains KNC2, and current loaders also read version 2. The VM has its own values/registers and interpreter. Packed execution combines a compatible VM template with bytecode; it does not turn the program into LLVM native code. See [VM CLI](../cli/vm.md) and [Backend Support](backend-support.md).

## Builtins and Standard Packages

`kn_builtins.c` injects compiler-owned object/error/collection/task support types. `kn_std.c` owns builtin identities, lowering ownership, profile availability and sparse KNC builtin IDs. This registry is not the implementation of every public standard-library function.

Versioned IO.Core/IO.Json/IO.Request/IO.UI/IO.Web and other packages supply ordinary Kinal declarations and narrow native bridges. C compiler use is compatible with Kinal-written standard packages; that is independent of compiler selfhosting. Package ABI/lifetime boundaries must be respected.

## Diagnostics and Inspection

Diagnostics carry stage/code/location information and can be localized. Fix the earliest relevant stage first: an undefined Unit is not solved by adding an arbitrary linker library, and a missing native symbol is not generally a parser error.

```bash
kinal build main.kn --emit tokens -o out/main.tokens
kinal build main.kn --emit ast -o out/main.ast
kinal build main.kn --emit check -o out/main.kcheck
kinal build main.kn --emit ir -o out/main.ll --trace --time
kinal build main.kn -o out/main --show-link --show-link-search
```

`--keep-temps` retains applicable intermediates for inspection. Temporary source/package extraction is not a general incremental-build cache or a public serialized compiler state.
