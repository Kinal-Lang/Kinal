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
Ordinary applications do not link the LLVM bridge or LLVM shared library;
those dependencies are added only when LLVM bridge externs are present.

Package manifests are parsed and validated by the Kinal `PackageManifest`
component. `PackageResolver` selects versions and materializes source/klib
inputs; `DependencyResolver` indexes their Units once and follows imports.
Runtime policy and public standard-library behavior remain Kinal package code,
not compiler-owned builtin mappings or a new C JSON/package runtime.

Project and profile `Packages.Roots` / `OfficialRoots` are supported, together
with the project-local `kpkg` convention. Hosted `LinkRoots` expands existing
directories in target-triple, platform-alias, `lib`, root order.
`NoDefaultLibs` suppresses automatically added platform libraries; it does not
disable the Kinal runtime or imply `NoCRT`.

`Build.Target` and `build --target` select Windows/MSVC, Linux/GNU, or macOS
output on x64 or ARM64 for `--emit ir` and `--emit obj`. CLI selection overrides
the project profile; `host` selects the compiler's platform. Stage0 aliases
such as `win64`, `linux-arm64`, and `mac64` are accepted alongside supported
LLVM triples. `CompileSession.Target` supplies target constants and output
suffixes; the LLVM machine/data layout is selected before lowering. `IO.Host`
continues to describe the compiler, independently of the selected `IO.Target`.

Foreign executable linking is deliberately rejected before emission: the
packaged runtime, LLVM bridge, SDK libraries, and linker configuration remain
host-specific. Cross-target IR/object support is not a claim of foreign runtime
or complete aggregate-FFI ABI validation. 32-bit/other ABIs, custom linker
scripts, and hosted `NoCRT` remain explicitly unsupported.

### Initial freestanding core

`Environment=Freestanding; Runtime=None;` now emits runtime-free IR/objects,
including `bare64` / `x86_64-unknown-none` and `bare-arm64` /
`aarch64-unknown-none`. `IO.Target.Env` follows the profile; `IO.Host.Env`
continues to describe the compiler. `NoCRT=true` is accepted in this mode.
`EntrySymbol` selects a zero-argument or one-pointer entry (default `KMain`),
exported through `__kn_entry`. There is no hosted `main`, GC setup, automatic
exception checking, or injected Kinal runtime package. Link the resulting
object using an explicit platform toolchain; selfhost freestanding executable
linking is not implemented yet.

Supported core operations include scalar/enum arithmetic, pointers and lvalues,
Struct values, direct/extern calls, explicit generic functions, borrowed array
descriptors, fixed local/global arrays, local array literals, constant global
array literals, static string data, and scalar globals. Array bounds retain their
syntax trees and resolve as integer constant expressions in Sema. Local backing
storage is stack-owned and reset at each declaration; global backing storage is
static. Partial literals are zero-padded, and array descriptor copies retain
their ordinary aliasing semantics. Escaping stack-local array storage is not a
supported lifetime extension. Volatile accesses lower
directly to LLVM volatile instructions; `Panic=Trap|Loop` lowers to a trap or
infinite loop. Intrinsic imports do not discover unrelated parent `Unit IO`
library sources. Runtime policy and public library implementations are not
duplicated in this path.

A bound-HIR validation pass rejects managed allocation, collections, closures,
async/exceptions, runtime string operations and extended-float support-library
requirements. Fixed-array struct fields and global-copy startup initialization
remain explicitly rejected; global array elements currently require constant
values. Global initializers are now bound in a Safe context, so constant lowering
cannot bypass pointer/call safety checks. `Runtime=Alloc|GC` freestanding modes
remain unsupported. This is an
initial core, not complete parity with stage0's freestanding implementation.

## Validation

Array types preserve their complete syntax structure, including each nested
bound expression. Bound lengths participate in type identity, generic instance
keys, and recursive array/Package assignability. An unsized destination accepts
a compatible fixed array; a fixed destination rejects unsized sources and
larger fixed bounds. Copies still alias the original descriptor and backing
storage: widening a declared bound does not resize a copied array.
`TypeOf()` retains static bounds (for example `int[2]`), while `TypeName()`
reports the runtime array representation (`int[]`). The contract tests cover
both operations independently of generic specialization keys.

```powershell
python x.py selfhost --test
python x.py selfhost-bootstrap --clean
python tests/check_project_packages.py --compiler out/selfhost/stage1/kinal-selfhost.exe --out-dir out/selfhost/package-checks
python tests/selfhost/check_targets.py --compiler out/selfhost/stage1/kinal-selfhost.exe --stage0 out/selfhost/stage0-host/kinal.exe --out-dir out/selfhost/target-checks
python tests/selfhost/check_freestanding.py --compiler out/selfhost/stage1/kinal-selfhost.exe --stage0 out/stage/host-release/kinal.exe --out-dir out/selfhost/freestanding-checks
python tests/selfhost/check_array_types.py --compiler out/selfhost/stage1/kinal-selfhost.exe --stage0 out/stage/host-release/kinal.exe --out-dir out/selfhost/array-type-checks
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
