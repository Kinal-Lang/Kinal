# Native and KinalVM Support

This page describes Native and KNC support in the Kinal toolchain. Kinal 1.0.0 ships the self-hosted compiler. The tables call out behavior specific to the legacy C compiler or to a platform-specific VM.

## Choose an Execution Path

| Path | Build / run | Required components |
|------|-------------|---------------------|
| Native hosted | `kinal build`, `kinal run` | Compiler, LLVM/linker toolchain, target runtime and package native assets |
| KNC source build | `kinal vm build`, `kinal vm run source.kn` | Compiler frontend/KNC emitter and matching VM |
| Existing bytecode | `kinalvm program.knc`, `kinal vm run program.knc` | Compatible VM; the latter also invokes the compiler CLI |
| Packed VM executable | `kinal vm pack` | Compatible VM template and bytecode; target-host dependencies still apply |
| Native freestanding | `kinal build --env freestanding` | Target linker/startup code; custom runtime hooks for Alloc/GC profiles |

Keep the complete SDK layout, including its LLVM files, for source builds. To run an existing `.knc` with the matching standalone `kinalvm`, invoke the interpreter directly and skip the source compilation and linking steps.

KinalVM currently interprets bytecode. It is not documented as a JIT. Startup and throughput depend on the program and environment; native linking occurs at build time, not every time a previously built native executable starts.

The Windows x64 selfhost SDK builds its bundled interpreter with ThinLTO. Existing `.knc` files can run with the updated `kinalvm.exe`. Repack standalone VM executables to embed the updated interpreter.

## Language and Numeric Behavior

| Feature | Native | KNC / VM |
|---------|--------|----------|
| Basic control flow, functions, strings, arrays | Supported | Supported within emitter/runtime limits |
| IO.UI 2.0 windows | Windows DX11 surface | Build with the selfhost compiler and the updated IO.UI 2.0 package. The Windows desktop VM executes forms, events, custom controls and mesh projection as bytecode. |
| `IO.Math` | Supported | Both compilers support the full public API, including trigonometry, rounding, classification, and `Hypot`, with the matching IO.Core package and VM. |
| `IO.Time` | Supported | Both compilers support timers, sleep, UTC calendar snapshots, and formatting. |
| Scalar `IO.Text` operations | Supported | The matching VM supports Length, IsEmpty, Contains, StartsWith, EndsWith, IndexOf, LastIndexOf, Count, Trim, Slice, Replace, ToUpper and ToLower. Positions and lengths use UTF-8 bytes. |
| `IO.Text.Builder` | Supported | Both compilers support length and capacity queries, reservation, appending, reuse after clearing, and string snapshots. |
| Filesystem paths and directories | Full `IO.Path` and `IO.Directory` APIs | All path and directory operations, including recursive file enumeration. |
| `Async` / `Await`, Task Run | Native task lowering | General Await emission and Run builtin are currently unsupported |
| Collections and JSON | list, dict, set and IO.Json | All 14 list, 10 dictionary and 8 set operations. Parse and format JSON, including nested collections, scalar roots and typed errors. |
| Generic functions | Instantiated by shared Sema | Same frontend instantiation; generated body must be VM-supported |
| Closures and mutable captures | Managed capture storage | VM cells; common closure forms supported |
| Classes, structs, Package values | Native layouts | VM representations; some method/cast paths remain restricted |
| Interface receiver calls | Native dispatch | Dispatches methods, including inherited and overloaded implementations, and property accessors through an interface reference. |
| Integer constants | 64-bit language integers | KNC v3 and v4 have 64-bit constants; unsigned operations carry width information |
| `f32` | LLVM binary32 operations | Binary64 registers with explicit binary32 rounding/conversion instructions |
| `float`, `f64` | Binary64 | Binary64 |
| `f16`, `f128` | LLVM types; target lowering/helper availability matters | Float registers remain binary64; no general half/quad precision parity |
| Dynamic `any` to scalar cast | Tagged conversion | Both emitters convert boxed integers, floats, booleans, characters and strings according to their runtime tag. |
| User C ABI calls and delegates | Explicit ABI marshalling | Arbitrary foreign-call targets are not a portable KNC capability |

`--emit check` stops after Sema. It does not prove Native emission/linking, KNC emission, VM execution, or availability of a package's native library. A backend-specific `Unsupported KNC Feature` diagnostic can follow successful semantic checking.

### Known Representation Differences

The table compares default values, array lengths and reflection results across the two backends.

| Expression / declaration | Native | VM |
|--------------------------|--------|----|
| `string text;` then `text.IsNull()` | `true` | `false` (empty string) |
| Fixed array `values.TypeOf()` | Static bounds, e.g. `int[3]` | Same static bound spelling |
| Fixed array `values.TypeName()` | Bound-erased name, e.g. `int[]` | Runtime tag name `array` |
| Initial length of `int[3] Cells` in a default struct local, global, static field, object field or struct array element | 3 in hosted programs | 3 |
| Lengths for a hosted declaration `int[2][4] grid;` | 4 rows of 2 | 4 rows of 2 |
| Dimensions after `int[2][4] grid = default;` or `grid = default;` | Four fresh two-element rows in hosted programs | Four fresh two-element rows |
| Length of the first element after `<int[2], int> pair;` | 2 in hosted programs | 2 |

Use `string text = "";` or an explicit intended value instead of relying on implicit reference defaults. Use `TypeOf()` when you need static type spelling; do not treat runtime display names as a portable serialization or type-identity format. See [Types](../language/types.md).

## Standard Library and Native Dependencies

KinalVM reuses call frames and traces reachable guest objects, arrays, pointers and captured variables. Allocation pressure triggers collection during execution. Calling `IO.GC.Collect()` runs a collection immediately. Surviving references keep their handles and unreachable arrays release their primitive buffers.

String and boxed-value arrays allocate storage as elements are written. Short final pages hold 4 or 16 slots, and full pages hold 64. `Array.Add` can share populated pages with the original array. Writing to either array copies the affected shared page so their elements can change independently.

The current builtin registry gives each Native builtin an implementation owner. KNC exposes a smaller stable builtin-ID surface; reserved IDs need not have VM handlers. Unsupported builtins must fail during bytecode compilation rather than produce an `Unregistered builtin` runtime failure.

A package can combine ordinary Kinal code, runtime leaves, and target-native objects. Therefore:

- An import resolving successfully is not proof that every function in that package is VM-compatible.
- Native-backed UI, HTTP, dynamic-library and other host APIs require their documented target assets; arbitrary C libraries cannot be copied into bytecode.
- Pure source helpers can work on both backends when their transitive calls are supported.
- Native executables can still depend on system libraries, CRT, OpenSSL, package libraries, or runtime files. Inspect the target and package requirements before distribution.
- Bytecode portability requires compatible VM versions, supported operations, and compatible runtime/platform assumptions. Compile-time `IO.Target` values are fixed in the artifact.

Use the [standard-library overview](../stdlib/overview.md) and each package's platform/dependency notes alongside this page.

## KNC Format Compatibility

A function can address registers 0 through 254. The compiler listing shows its
frame size as `registers=`. Successive `If` checks can reuse the registers for
their conditions and for expressions passed to `Return` or `Throw`.

The writer emits format **3**, retaining the magic `KNC2`. Version 3 stores integer constants as signed 64-bit little-endian entries. Current loaders also accept version 2's signed 32-bit entries. Old version-2-only VMs reject version 3; compiler and VM updates should be deployed together.

`FloatToF32` (137) and `IntToF32` (138) perform binary32 conversions. Selfhost emits `CallRange` (139) for calls exceeding four physical arguments. The C emitter also uses it for constructors and direct, virtual and interface instance calls with four or more explicit arguments. The receiver occupies the first argument register. Both compilers emit `UIntToFloat` (140) to convert unsigned 64-bit values directly to binary64. Distributing these programs requires a matching VM. A loader accepting a header is not sufficient if its interpreter lacks the instructions used by a program. Component versions (`kinal`, `kinalvm`) and bytecode format versions are separate contracts.

`--listing` produces a compiler-time listing with local names; disassembly of an existing `.knc` cannot recover local source names that were not stored in the bytecode. See [VM CLI](../cli/vm.md).

## Conditional Code and Validation

Use `IO.Runtime.IsNative` / `IO.Runtime.IsVM` to select source paths for the output backend. Use `IO.Target.*` for the target platform and `IO.Host.*` for the compiler host; these are distinct from `--runtime none/alloc/gc` memory profiles.

```bash
kinal build main.kn --emit check -o out/main.kcheck
kinal run main.kn
kinal vm run main.kn
kinal vm build main.kn --no-superloop -o out/main-no-superloop.knc
kinalvm out/main-no-superloop.knc
```

Run only the paths the application claims to support. Compare output/exit behavior, not merely compilation. For cross-platform claims, execute on each claimed host; producing foreign-target IR/object files is a separate, narrower check.

## See Also

- [Compiler CLI](../cli/compiler.md)
- [Project Files](../cli/project-files.md)
- [Memory and Lifetimes](memory-management.md)
- [Runtime Constants](runtime-environment-constants.md)
- [Freestanding](freestanding.md)
