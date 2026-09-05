# Kinal self-hosting compiler

This application is the pure-Kinal bootstrap compiler. The C compiler in
`apps/kinal` builds stage1; stage1 and every later stage use this compiler's
own frontend, typed HIR, LLVM backend, and linker path.

## Architecture

```text
Driver
  -> CompileSession
  -> ProjectLoader + PackageResolver + SourceManager
  -> DependencyResolver (project / ordinary / official Unit maps)
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

Package manifests are parsed and validated by the Kinal `PackageManifest`
component. `PackageResolver` selects versions and materializes source/klib
inputs; `DependencyResolver` indexes their Units once and follows imports.
Runtime policy and public standard-library behavior remain Kinal package code,
not compiler-owned builtin mappings or a new C JSON/package runtime.

Project and profile `Packages.Roots` / `OfficialRoots` are supported, together
with the project-local `kpkg` convention. Hosted `LinkRoots` expands existing
directories in target-triple, platform-alias, `lib`, root order.
`NoDefaultLibs` suppresses automatically added platform libraries; it does not
disable the Kinal runtime or imply `NoCRT`. Explicit target overrides, freestanding
output, custom linker scripts, and `NoCRT` remain explicitly unsupported here.

## Validation

```powershell
python x.py selfhost --test
python x.py selfhost-bootstrap --clean
python tests/check_project_packages.py --compiler out/selfhost/stage1/kinal-selfhost.exe --out-dir out/selfhost/package-checks
```

The bootstrap check requires stage1 to build stage2 and stage2 to build stage3
without a host-compiler callback. It compares frontend summaries and emitted
LLVM IR across stages, and requires stage2 and stage3 executables to be
byte-identical. Generated files and the JSON report live under `out/selfhost`.

## Scope

The current implementation covers the language subset used by its own source
and the dedicated backend fixtures. Explicit top-level generic functions use
typed substitutions and compile-time monomorphization, including nested and
cross-unit instantiation. It proves genuine self-hosting, but it is
not yet a drop-in replacement for every C stage0 feature, standard-library
package, target, diagnostic, KNC/VM backend, or the planned complete generic
system. Those surfaces should be migrated behind the typed HIR boundary rather
than copied from the C implementation.
