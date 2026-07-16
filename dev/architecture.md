# Project Structure

```
Kinal/
├── VERSION                  # Kinal version number (single source of truth)
├── CMakeLists.txt           # Root CMake configuration
├── x.py                     # Build entry script
├── apps/
│   ├── kinal/               # Kinal compiler (C)
│   │   ├── include/kn/      # Public headers (including version.h)
│   │   └── src/             # Compiler source code
│   ├── kinal-lsp/           # LSP language server (C++)
│   │   └── server/src/
│   └── kinalvm/             # KinalVM virtual machine (written in Kinal)
│       ├── VERSION           # VM version number
│       └── src/
├── libs/
│   ├── runtime/             # Runtime library (C)
│   └── std/                 # Standard library packages (stdpkg)
├── infra/
│   ├── assets/              # Asset files (locales, etc.)
│   ├── cmake/               # CMake modules
│   ├── scripts/             # Python build scripts
│   └── toolchains/          # CMake toolchain files
├── tests/                   # Compiler test cases
├── docs/                    # User documentation
└── dev/                     # Developer specification documents (this directory)
```

## Core Components

- **Kinal Compiler** (`apps/kinal/`): Implemented in C, uses LLVM backend to generate native code or VM bytecode
- **KinalVM** (`apps/kinalvm/`): A bytecode virtual machine written in Kinal itself
- **LSP Server** (`apps/kinal-lsp/`): C++ implementation providing IDE language support
- **Runtime** (`libs/runtime/`): Target platform runtime library (C)
- **Standard Library** (`libs/std/`): Standard library packages distributed as stdpkg

## Compiler Pipeline

```text
Lexer -> Parser AST -> Sema -> Typed HIR slices (Call v1 + Binary v1) -> Native LLVM / KNC
```

The HIR boundary is being introduced incrementally. Sema normalizes every
direct and member call into `KnResolvedCall`, including the selected
builtin/function/delegate or static/direct/virtual/interface method target.
Native and KNC dispatch from that record; they must not infer the call kind
again from parser-facing names and flags.

Typed Binary HIR v1 applies the same rule to every `EXPR_BINARY`. Sema stores a
backend-independent `KnResolvedBinary` plan with:

- `kind`: numeric arithmetic, string concatenation, pointer arithmetic,
  bitwise, string/reference/scalar equality, numeric comparison, or logical
  short-circuit lowering
- `lhs_coercion` and `rhs_coercion`: the already-selected operand types
- `pointer_side`: which operand is the pointer for pointer arithmetic
- `is_unsigned` and `integer_bits`: signedness and the resolved 8/16/32/64-bit
  width; `isize` and `usize` follow the target pointer width

Both Native LLVM and KNC consume this plan directly. Backend code may select a
mechanical instruction for the recorded plan, but must not repeat Sema's type
classification or choose different coercions. Equality on aggregate or `any`
values is not part of Binary HIR v1 and is rejected by Sema before either
backend runs.

This is not yet a complete Typed HIR. Other expression families do not yet
have a backend-independent resolved plan and still rely on typed-AST semantics.
They will move behind the same boundary incrementally. `kinal build --emit
check` exposes deterministic
`kinal-call-hir-v1` and `kinal-binary-hir-v1` structural counts for stage
differential tests.

## KNC Unsigned Integer ABI

Unsigned integer operations that differ from their signed equivalents use
stable KNC opcodes appended after the existing opcode space:

| ID | Opcode |
|---:|---|
| 130 | `UDivInt` |
| 131 | `URemInt` |
| 132 | `ULtInt` |
| 133 | `ULeInt` |
| 134 | `UGtInt` |
| 135 | `UGeInt` |
| 136 | `LShrInt` |

Each instruction is five bytes: `[opcode, dst, lhs, rhs, width]`. `width` is
the Binary HIR `integer_bits` value and must be one of 8, 16, 32, or 64. These
IDs and the width byte are bytecode ABI and must not be reordered or omitted.

The existing KNC condition/backedge/superloop comparison shortcuts are
signed-only. An unsigned comparison skips those shortcuts, emits the matching
unsigned opcode, and uses the general expression-plus-branch path. This keeps
optimization from changing unsigned ordering semantics.

## Builtin Registry Boundary

`KnBuiltinKind` is the stable compiler-wide builtin identity space and ends in
`KN_BUILTIN_COUNT`. Backend responsibility groups and the sparse KNC/VM ABI ID
mapping live in `kn_std.c`; KNC consumes that mapping instead of maintaining a
second switch. VM IDs are stable ABI values, so unimplemented reserved IDs stay
as gaps rather than being renumbered.

Native lowering dispatches through small responsibility helpers (platform,
collections, system, text, filesystem, and dynamic operations), with collection
lowering split again by string/dict/list/set/math/conversion. Adding a builtin
must update the centralized registry and its actual Native helper. The
`tests/check_builtin_registry.py` structural check enforces:

~~~text
all builtin kinds == exactly one Native lowering
KNC emitted builtin IDs ⊆ VM handlers ⊆ Bytecode.BuiltinId declarations
~~~

An unavailable VM builtin must fail during KNC compilation; emitting bytecode
that can only reach `Unregistered builtin` at runtime is not allowed.

## Large-Function Boundaries

Sema expression analysis keeps the public recursive dispatcher small by moving
leaf expression families, direct calls, and member-builtin families into
single-responsibility helpers. Member builtin probes use an explicit boolean
“handled” result; an unknown type is a valid error result and must never be used
as the not-handled sentinel.

Hosted runtime IR construction is coordinated by `build_runtime_hosted`, which
calls ordered helpers for declarations, console, string operations, conversions,
time, and entry generation. `HostedPlatformApi` carries the small set of
Win32 LLVM handles shared by console and entry lowering. Helper extraction must
preserve LLVM declaration/body creation order because generated IR is part of
the compiler's differential-test surface.

The O0+ASan build uses `-Wframe-larger-than=16384` as a structural guard.
The Sema dispatcher, builtin dispatcher, and hosted runtime coordinator are
required to stay below that threshold.
