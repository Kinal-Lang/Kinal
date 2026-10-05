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
  -> NativeBackend -> LlvmBridge -> object -> packaged linker/runtime -> executable
  -> KncBackend -> KncModel -> KNC bytecode (VM builds)
```

Stateful or lifetime-owning components are classes. Stateless public helpers
remain units. Compiler sources use direct imports and fully qualified names
where qualification avoids ambiguity; import aliases are intentionally not
used.

The bridge exposes opaque handles and a small stable C ABI. LLVM objects and
LLVM-C API details do not cross into Kinal source. Generated stages carry the
bridge, LLVM runtime, linker, Kinal runtime, matching KinalVM runner, and VERSION
metadata beside the compiler executable.
Ordinary applications do not link the LLVM bridge or LLVM shared library;
those dependencies are added only when LLVM bridge externs are present.

The compiler banner embeds `VERSION:kinal` through the checked-in generated
`Core/BuildInfo.kn` Unit and is independent of the bootstrap stage number or
runtime working directory. `--version --verbose` prints the compiler version
followed by the embedded `kinalvm` component version, matching stage0 order.
Bootstrap helpers refresh the metadata automatically. After
editing `VERSION`, direct source-build users can run
`python -m infra.scripts.x.compiler_metadata`; `--check` rejects stale metadata.

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
output on x64 or ARM64, plus Windows/MSVC x86, for `--emit ir`, `--emit obj`,
and `--emit asm`. CLI selection overrides
the project profile; `host` selects the compiler's platform. Stage0 aliases
such as `win86`/`win32`/`x86`, `win64`, `linux-arm64`, and `mac64` are accepted alongside supported
LLVM triples. `CompileSession.Target` supplies target constants and output
suffixes; the LLVM machine/data layout is selected before lowering. `IO.Host`
continues to describe the compiler, independently of the selected `IO.Target`.

Windows x86 selects 32-bit pointers and C `isize`/`usize` parameters and results.
Explicit `By System` externs use stdcall there; default and `By C` externs use
C calling conventions. Like stage0, language `int`, `isize`, `usize`, array
counts, and boxed payloads retain 64-bit storage. Binary operations on size
integers use target-width coercions. Aggregate layout, captured alignment,
exception objects, and GC scanning follow the output pointer layout.

Foreign linking selects target-specific runtime assets, platform link attributes,
search directories, and linker settings. It requires a compatible linker and
any target SDK/system libraries; runtime C sources are available as a fallback
when a prebuilt target runtime is absent. Cross-target IR/object support does
not imply that a foreign executable was run or that a complete aggregate-FFI
ABI was validated. Supported scalar FFI and target-layout contracts are checked
against stage0 for Windows x86 and x64.

### Freestanding profiles

`Environment=Freestanding` supports `Runtime=None|Alloc|GC`, including `bare64`
/ `x86_64-unknown-none-elf` and `bare-arm64` / `aarch64-unknown-none-elf`. Direct-source
flags select the same profile as project settings. `IO.Target.Env` follows the
profile; `IO.Host.Env` continues to describe the compiler. `EntrySymbol` selects
a zero-argument or one-pointer entry (default `KMain`), exported through
`__kn_entry`. There is no hosted `main` or injected hosted runtime package.
The driver can link freestanding executable artifacts with an explicit entry
and optional linker script; the target supplies any necessary startup/boot code.

`Runtime=None` emits runtime-free scalar/enum arithmetic, pointers and lvalues,
Struct values, direct/extern calls, explicit generic functions, borrowed array
descriptors, fixed local/global arrays, array literals and scalar globals.
Global initializers execute once in declaration order, including calls, copies,
and nonconstant array elements. The guard is entered before evaluating the
initializers, so a call back into the same module cannot replay startup.
Array bounds resolve as integer constant expressions in Sema. Local backing
storage is stack-owned and reset at each declaration; global backing storage is
static. Partial literals are zero-padded, and descriptor copies retain their
ordinary aliasing semantics. Struct fixed-array fields preserve stage0's
zero-descriptor default and can receive existing array descriptors. Escaping
stack-local array storage is not a supported lifetime extension.

Core string length, equality, concatenation, scalar/primitive-any formatting,
primitive-any tag/type predicates,
string indexing, iteration and Switch use compiler-emitted LLVM helpers.
Like stage0's `Runtime=None` helpers, formatting and concatenation share eight
512-byte scratch slots; concatenation truncates at 511 bytes and float formatting
truncates to an integer. These scratch strings are temporary, not heap-owned.
Volatile accesses lower directly to LLVM volatile instructions;
`Panic=Trap|Loop` lowers to a trap or infinite loop. A bound-HIR gate continues
to reject heap allocation, aggregate boxing, managed callables, collections,
async/exceptions, string parsing and extended-float support-library requirements
in `Runtime=None`. Global initializers are bound in a Safe context, so lowering
cannot bypass pointer/call safety checks.

`Runtime=Alloc|GC` calls the target's `__kn_*` allocation, string, frame/root,
collection, exception and async hooks as needed. As in stage0, Alloc also uses
frame/root hooks. These profiles are not bundled runtimes: applications must
supply compatible hooks and their platform implementation. Core length/equality
and zeroing remain compiler-emitted. Freestanding array backing remains
static/stack-owned for all three profiles.

The profile contract compares stage0 and selfhost host executions using small
custom test runtimes, and inspects bare x64/ARM64 IR, object headers and undefined
hook symbols. It covers core strings, primitive-any formatting, global startup,
class allocation, GC collection hooks and exception handling. The test hooks are
not a production allocator/collector. Foreign runtime execution is not tested.

## Source and project CLI

The native driver accepts direct `.kn`, `.kinal`, and `.fx` sources as well as
`kinal.knproj` projects and legacy JSON project manifests. Directory discovery
checks `kinal.knproj`, `kinal.pkg.json`, then `kinal.pkg`. It uses the same compilation session, package resolver,
semantic analysis, and typed-HIR backend for both paths:

```sh
kinal-selfhost build Main.kn -o app
kinal-selfhost build Main.kn Helper.kn --no-module-discovery --emit obj -o app.o
kinal-selfhost build Main.kn --emit asm
kinal-selfhost build Main.kn --emit check
kinal-selfhost build Library.kn --kind static -o libExample.a
kinal-selfhost build Library.kn --kind shared -o libExample.so
kinal-selfhost build Main.ll --emit obj -o Main.o
kinal-selfhost build Main.o -o app
kinal-selfhost build --project . --profile native --emit ir
kinal-selfhost run Main.kn -- "two words" argument
kinal-selfhost run --project . --profile native -- argument
```

`--emit bin|obj|asm|ir|check|tokens|ast` defaults to `bin`. Stage0's aliases for those
modes are accepted. Source outputs default to the first input's base name in
the invoking directory, with the target-specific executable/object suffix or
`.s`, `.ll`, `.kcheck`, `.ktokens`, or `.kast`. Missing output parent directories are created.
Project output selection keeps its existing profile
behavior. The original positional `build`, `build-object`, and `build-ir`
project commands remain supported for bootstrap consumers.

`check` writes a deterministic frontend-only summary and does not construct an
LLVM module or invoke a linker. Its format is `kinal-selfhost-sema-v1`, matching
`check-source` and `check`, with additional `target`, `pointer_bits`, and
`environment` fields. Selfhost's typed HIR differs from stage0's HIR, so their
summary formats and structural counters are deliberately not interchangeable.
Assembly output uses the same target machine and validation as native objects;
foreign assembly does not require linking or a foreign SDK.

`run` accepts one source or a project, forwards every argument after `--`, and
returns the program's exit status. It claims a temporary directory under
`TMP`, `TEMP`, or `TMPDIR` (in that order), using `/tmp` as the POSIX fallback.
The executable and intermediate object are cleaned up after execution or a
build/link failure. `--keep-temps` retains them and prints the executable's
`output=` path. Ordinary executable builds also remove their intermediate
object unless `--keep-temps` is selected.

`--no-module-discovery` restricts local sources to explicit inputs (or the
project entry); explicit official-package imports and the hosted runtime remain
available. Both commands accept `--pkg-root`, `--stdpkg-root`, `-L`/`--lib-dir`,
`-l`/`--lib`, `--link-file`, `--link-root`, and `--link-arg`. CLI paths are
relative to the invoking directory; manifest paths stay relative to the
project. `build --target` also applies to direct sources. `--freestanding`,
`--env`, `--runtime`, `--panic`, `--entry`, `--link-script`, `--no-crt`, and
`--no-default-libs` select the same behavior as the corresponding project
profile. Invalid hosted/freestanding combinations are diagnosed before lowering.
Like stage0's default executable artifact mode, object, IR, and assembly
emission retain an entry wrapper and require an entry point. `check`, `tokens`,
and `ast` are frontend-only modes; token and syntax dumps retain stage0's stable
schemas, while the semantic summary remains selfhost-specific.

`--kind exe|static|shared` controls linked artifacts. Static/shared libraries do
not require `Main`, use stage0-qualified exported function names, and guard
module global initialization so external calls initialize the module once.
Shared native objects and runtime leaves are compiled as position-independent
code. Freestanding shared libraries are rejected, matching stage0.

Hosted artifacts expose runtime-retained metadata through the stage0-compatible
`__kn_meta_get_module` descriptor ABI. Functions, methods, classes, structs,
enums, interfaces and fields share the same registry for local and loaded-module
`IO.Meta.Query`, `Of`, `Find` and `Has` operations. `Of` returns qualified metadata
names; `Find` supplies tagged arguments and callable owner adapters. Successful
unload unregisters a module, preserves copied metadata strings and invalidates
selfhost's retained callable adapters. Ordinary native symbol pointers still
follow their provider's lifetime rules. The cross-compiler contract loads C and
selfhost shared libraries from both C and selfhost consumers on the host;
foreign metadata/runtime execution is not established by those checks.

LLVM IR and assembly inputs compile forward with LLVM tools; objects can be
copied with `--emit obj` or linked directly. They can accompany Kinal sources in
link mode. No intermediate path reruns the C compiler frontend on Kinal input.
The linker driver supports `--linker lld|zig|msvc`, `--linker-path`, `--crt`,
`--show-link`, and `--show-link-search`; raw CLI link arguments retain their
ordering around archives and other link files. Hosted POSIX links prefer an
installed Zig toolchain and otherwise use Clang/LLD; explicit selection wins.
`--lto[=off|full|thin]` produces LLVM bitcode for linking where the selected
linker/toolchain supports it. Toolchain limitations remain real errors, not
silently ignored options. `--trace` and `--time` report actual compilation work.

Explicit sources can accompany `--project`, with the first source selecting a
project entry. Diagnostic language, color, warning policy, and locale-template
export are described in [FORMATTING.md](FORMATTING.md), together with the native
`fmt` command. Unsupported options report errors. Invalid CLI requests and
missing project paths return 2; source, semantic, emission, and link failures
return 1.

## Package CLI

`pkg build --manifest <file|dir> [-o <file.klib> | --layout <dir>]`,
`pkg info <file.klib>`, and `pkg unpack <file.klib> [-o <dir>]` implement the
ordinary package workflow in Kinal. The existing manifest parser and archive
reader are shared with dependency resolution; streaming file-I/O leaves do not
invoke stage0. Version-1 `KNKLIB1` archives interoperate in both directions.

Build preserves the original embedded manifest and source/native/other assets,
including empty files. It excludes package manifests, `.klib` files, `.git`,
and `.kinal-cache` subtrees, matching stage0. Payloads are ordered
lexicographically and limited to 256 files and a 2 GiB total archive. Layout
emits `<name>/<version>/package.knpkg.json` plus `lib/<name>.klib`, retaining
summary, URL, entry, modules, and dependencies. Missing versions use `0.0.0`
for layout and omit the version suffix for default archive names. These remain
source/asset packages, not precompiled interfaces or a dependency solver.

The package CLI contract checks metadata, binary/empty/UTF-8 payloads,
source-root and explicit-source manifests, legacy manifest names, default
outputs, layouts, deterministic repeat builds, cross-compiler unpack/repack,
and compilation against the generated dependency layouts.

## Initial KinalVM bytecode CLI

The VM commands feed the same source/project loader and typed HIR into the
pure-Kinal KNC emitter. It writes deterministic version-3 `.knc` files and never
invokes stage0, LLVM, or a linker to emit bytecode:

```sh
kinal-selfhost vm build Main.kn -o app.knc
kinal-selfhost vm build --project . --profile vm
kinal-selfhost vm run app.knc
kinal-selfhost vm run Main.kn
kinal-selfhost vm disasm app.knc
kinal-selfhost vm pack app.knc -o app
```

The command accepts one source, or a `kinal.knproj` profile with `Backend = VM`.
An omitted `Backend` follows the command (`Native` for `build`, `VM` for
`vm build`); an explicitly conflicting backend is rejected. `-o`/`--output`,
`--profile`, `--no-module-discovery`, `--pkg-root`, and `--stdpkg-root` are
supported. Default source outputs use the source base name and `.knc` in the
invoking directory. Missing output directories are created.

The emitter covers the registered scalar/control-flow, function, array,
string, object, callable, Block, and runtime cases described in [KNC.md](KNC.md).
Unsupported operations fail with a `KNC:` diagnostic before replacing the
output file. This remains a bounded implementation, not complete KNC language
parity; VM profiles currently require Hosted/GC and the host target.

`vm build --listing file.knasm` writes a readable listing. `--superloop` (the
default) and `--no-superloop` control conservative counted-loop fusion. Source
`vm run` and `vm pack` accept the same superloop switches. The binary and
listing destinations must differ.

`vm run` accepts source, project, or existing `.knc` input; `vm disasm` accepts
an existing `.knc`. They use the adjacent compatible `kinalvm` executable, or an
explicit `--vm-path`. The normal bootstrap builds that runner using the newly
built selfhost compiler, then carries it into later stages. Source runs use a
fresh temporary bytecode file and preserve the program's exit status; temporary
files are removed after success or failure unless `--keep-temps` is selected.
A `Main(string[] args)` entry receives an empty array. Nonempty runtime arguments
are explicitly rejected because argument injection is not part of this VM ABI.

`vm pack` accepts the same source/project or existing-bytecode inputs, and
creates a standalone host executable by appending bytecode and the established
size/`KNCE` footer to the compatible runner. Streaming file I/O is implemented
in Kinal through narrow C file-I/O leaves; packaging never invokes another
compiler. POSIX output permissions are set using `/bin/chmod` or
`/usr/bin/chmod`. `--keep-temps` retains source-generated bytecode and reports it
as `temporary=...`. Foreign executable packaging is rejected. No C compiler
fallback is used when a compatible runner is missing.

`IO.Runtime` constants describe the selected output backend, independently of
`IO.Host` and the process compiling the file. The integer aliases, typed
`IsNative`/`IsVM` booleans, and `Name` are folded in semantic analysis. As in
stage0, `IO.Runtime.Name` is `"Kinal.Native"` or `"Kinal.VM"`. Constant globals
use the same values, and folding a known right-hand operand never discards
side effects in the left operand.

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

Non-assignment scalar binary HIR records the semantic operand coercion separately
from the result type. Mixed floating-point arithmetic promotes both operands to the
wider floating type; numeric comparisons and bitwise operations use the same
width/signedness rules as stage0. Mixed-integer arithmetic retains stage0's
left-operand type rule. Floating remainder (`%`, `%=`), including property
updates, and direct bool/float conversions lower to typed LLVM instructions.
Float-to-bool conversion matches stage0's ordered comparison with zero: NaN
and either signed zero are false, while finite nonzero values are true.
The scalar contract checks these cases against the C compiler and keeps
per-case build/runtime logs.
`f80` is outside the registered selfhost type subset and is rejected explicitly.

`SourceSet.RequireUnit` is validated for every discovered source, including
unreachable files. Unit/import probes retain parser recovery trees without
publishing body diagnostics; selected files are still parsed normally and
report their errors. Canonical-path probes are reused only within one compilation
session, avoiding repeat parsing during validation, reachability and indexing.
The probe contract checks object reuse, session isolation and unreachable-body
diagnostic isolation.

Hosted entry points initialize the native process argument snapshot before
global startup, including programs with a zero-parameter `Main`. This keeps
`IO.System.CommandLine()` available on POSIX; Kinal `ArgumentsFromArgv` still
owns the `string[]` passed to one-parameter entry points. Freestanding entry
points do not acquire hosted argument initialization.

```powershell
python x.py selfhost --test
python x.py selfhost-bootstrap --clean
python tests/check_project_packages.py --compiler out/selfhost/stage1/kinal-selfhost.exe --out-dir out/selfhost/package-checks
python tests/selfhost/check_targets.py --compiler out/selfhost/stage1/kinal-selfhost.exe --stage0 out/selfhost/stage0-host/kinal.exe --out-dir out/selfhost/target-checks
python tests/selfhost/check_freestanding.py --compiler out/selfhost/stage1/kinal-selfhost.exe --stage0 out/stage/host-release/kinal.exe --out-dir out/selfhost/freestanding-checks
python tests/selfhost/check_freestanding_profiles.py --compiler out/selfhost/stage1/kinal-selfhost.exe --stage0 out/stage/host-release/kinal.exe --out-dir out/selfhost/freestanding-profile-checks
python tests/selfhost/check_metadata_artifacts.py --compiler out/selfhost/stage1/kinal-selfhost.exe --stage0 out/stage/host-release/kinal.exe --out-dir out/selfhost/metadata-artifact-checks
python tests/selfhost/check_array_types.py --compiler out/selfhost/stage1/kinal-selfhost.exe --stage0 out/stage/host-release/kinal.exe --out-dir out/selfhost/array-type-checks
python tests/selfhost/check_scalar_types.py --compiler out/selfhost/stage1/kinal-selfhost.exe --stage0 out/stage/host-release/kinal.exe --out-dir out/selfhost/scalar-type-checks
python tests/selfhost/check_package_cli.py --compiler out/selfhost/stage1/kinal-selfhost.exe --stage0 out/stage/host-release/kinal.exe --out-dir out/selfhost/package-cli-checks
python tests/selfhost/check_cli_workflows.py --compiler out/selfhost/stage1/kinal-selfhost.exe --stage0 out/stage/host-release/kinal.exe --out-dir out/selfhost/cli-workflow-checks
python tests/selfhost/check_project_probes.py --compiler out/selfhost/stage1/kinal-selfhost.exe --stage0 out/stage/host-release/kinal.exe --out-dir out/selfhost/project-probe-checks
```

The native and runtime manifest audits run on the current Windows, Linux, or
macOS host, honor manifest platform inclusions/exclusions, and retain strict
host-specific coverage baselines. Current native/runtime case counts are
190/186 on Windows, 188/184 on Linux, and 184/182 on macOS. Negative diagnostic
cases, other-host cases, and compile-only runtime exclusions are reported by
reason; compiler failures are never converted into skips. Reports include
passed counts and case-level build/runtime failures, and `--output` retains
JSON results even when an audit fails. `--jobs` controls build concurrency.

POSIX audits build native-format FFI objects, static archives and shared
libraries with the selected LLVM toolchain. Existing `.obj`/`.dll` fixture
names are retained because source-level `LinkFile` and `LoadLibrary` tests
refer to them; the files contain actual host formats. Static-library cases
remain statically linked and dynamic-library cases exercise shared linking
and symbol loading. Only dynamic FFI executions receive a loader-path update.
Filesystem and HTTPS/web fixtures retain the canonical regression runner's
layout and behavior. Fixture preparation errors are failures, not skipped
compiler coverage.

Run the portable harness contract tests without building a compiler:

```sh
python -m unittest discover -s tests/selfhost -p test_manifest_audits.py
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
