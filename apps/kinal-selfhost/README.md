# Kinal self-hosting compiler

This application is the pure-Kinal bootstrap compiler. The C compiler in
`apps/kinal` builds stage1; stage1 and every later stage use this compiler's
own frontend, typed HIR, LLVM backend, and linker path.

## Architecture

```text
Driver
  -> CompileSession
  -> ProjectLoader + SourceManager
  -> Lexer -> Parser
  -> SemanticCompilation -> typed HIR
  -> NativeBackend -> LlvmBridge
  -> object -> packaged linker/runtime -> executable
```

Stateful or lifetime-owning components are classes. Stateless public helpers
remain units. Compiler sources use direct imports and fully qualified names
where qualification avoids ambiguity; import aliases are intentionally not
used.

The bridge exposes opaque handles and a small stable C ABI. LLVM objects and
LLVM-C API details do not cross into Kinal source. Generated stages carry the
bridge, LLVM runtime, linker, and Kinal runtime beside the compiler executable.

## Validation

```powershell
python x.py selfhost --test
python x.py selfhost-bootstrap --clean
```

The bootstrap check requires stage1 to build stage2 and stage2 to build stage3
without a host-compiler callback. It compares frontend summaries and emitted
LLVM IR across stages, and requires stage2 and stage3 executables to be
byte-identical. Generated files and the JSON report live under `out/selfhost`.

## Scope

The current implementation covers the language subset used by its own source
and the dedicated backend fixtures. It proves genuine self-hosting, but it is
not yet a drop-in replacement for every C stage0 feature, standard-library
package, target, diagnostic, KNC/VM backend, or the planned complete generic
system. Those surfaces should be migrated behind the typed HIR boundary rather
than copied from the C implementation.
