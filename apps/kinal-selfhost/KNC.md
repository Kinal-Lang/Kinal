# Pure-Kinal KNC backend

`KncBackend` lowers the semantic compiler's typed HIR directly to KNC. It does
not invoke the C compiler, Native backend, LLVM, or an external compiler process.
`KncModel` owns the bytecode tables, wire-format validation, serialization, and
readable compiler listing. The emitted revision is KNC2 version 3, including
signed 64-bit integer constants and numeric opcodes 130–138.

## Pipeline and API

The source/project loader, dependency resolver, and semantic analysis are shared
with Native compilation. Set the selected profile backend before analysis, so
`IO.Runtime` constants and conditional compilation describe the VM environment.
After successful analysis, construct `KncBackend(semantics)` and call
`Emit(outputPath)`. Failure returns `false` and an explanatory `Error`. Unsupported
lowering is detected before the output file is opened. A lowered graph can be
emitted again without appending duplicate functions, constants, or globals.

After successful bytecode emission, `EmitListing(path)` writes the compiler
listing. It returns `false` and reports `Error` on failure. Register names and
lowering comments are listing-only metadata and do not change KNC bytes. A VM
disassembler reads the serialized program independently; it cannot recover those
compiler-only comments.

Functions are lowered through a reachability queue, beginning with the entry
point and required global initializers. Dormant native extern function bodies
therefore do not prevent a VM build. Intrinsics use the existing VM ABI and
stdlib annotations; unsupported native extern calls are diagnosed when reached.

## Supported lowering

The acceptance corpus exercises:

- Signed and unsigned integer widths, f32/f64 arithmetic and coercions, booleans,
  characters, strings, globals, direct calls, recursion, named/default arguments,
  and the VM-compatible variadic calling convention
- If, While, For, Foreach, Switch, short circuit, break/continue, and typed
  conditional/Switch expression joins
- Arrays, indexed updates, ordinary Package-to-array values, structures and their
  copy semantics, classes, inheritance, interfaces, properties and virtual calls
- Checked casts and Is patterns, scalar dynamic values, shared captured cells,
  nested/escaping/Foreach closures, function objects, and dynamic calls
- Local Block/Record/Jump and captured block objects, record labels and
  Run/Jump/RunUntil/JumpAndRunUntil operations
- Try/Throw, handler unwinding, local/global/field/index pointers and pointer
  arithmetic, and the existing supported VM intrinsic calls

Storage coercions are applied deliberately. In particular, an integer converted
to f32 uses the dedicated direct conversion; expression branches are converted
to their semantic join type before merging; compound arithmetic uses the HIR
operand type and narrows only when storing the result. Property receivers,
indices, getters, right-hand expressions, and setters retain their evaluation
order and are evaluated once. Integer-plus-pointer addition retains that source
order even when the pointer is the right-hand operand, and scales the offset by
the pointed-to element type.

`TypeOf()` is folded from the bound static type without evaluating its receiver.
Named types retain unit qualification, including array and pointer descriptors;
the same folded strings work in ordinary global string and string-array storage.

Known-origin local references and local copies of `IO.Type.string.Length`,
`Concat`, `Equals` and `ToChars` use their existing typed builtin signatures.
This does not establish new guarantees for reassigned or type-erased builtin
references, bound instance references, or other builtin families.

`Main()` and `Main(string[])` are accepted. The current VM has no process-argument
injection ABI, so the latter receives a valid empty array. CLI workflows should
reject requested program arguments rather than silently discarding them.

## Conservative loop fusion

`EnableSuperloop` defaults to true. Eligible empty For loops and While loops
whose only effect is a unit update can use the VM's full-loop integer opcodes.
Eligibility requires an uncaptured `int`/`i64` local, signed 64-bit comparison,
and a pure invariant literal or distinct uncaptured local bound. Narrow or
unsigned arithmetic, side-effectful bounds, extra body statements, captures,
and non-unit steps retain ordinary bytecode. Disabling the option preserves the
ordinary lowering. No broader array/reduction/backedge fusion is promised.

## Boundaries

This backend is not universal source-language or Native parity. Current explicit
unsupported or unvalidated families include async functions, delegate storage
and bound instance-method references, general native extern calls, mutable
collection constructors such as `list.Create`, f16/f128, floating remainder, and
some raw-pointer conversions. Block Jump across Try handler scopes is rejected.

Execution acceptance currently covers Linux64. It does not establish 32-bit
`isize`/`usize` semantic parity: pointer-sized numeric planning needs a separate
shared-frontend and backend review before that target can be claimed.

The wire ABI allows four call argument slots (including an instance receiver),
255 live registers, and 16-bit table and branch operands. Oversized or invalid
programs must be diagnosed rather than truncated. Native-only tests are not
counted as KNC regressions merely because their source can be parsed.

## Validation

`tests/selfhost/check_knc_model.py` validates exact serialized bytes, all opcode
widths, numeric bit patterns, metadata bounds, malformed-program rejection,
compiler listings, and listing/bytecode independence.

`tests/selfhost/check_knc_backend.py --suite registered` derives its positive
corpus from the established KNC registrations in `tests/run_tests.py`, plus the
reviewed shared regressions. It compares execution against the C KNC reference,
checks repeatable output, preserves source bytes and project policy, and can run
both the ordinary VM and an independently selfhost-built VM. `--harness` uses the
small frontend/HIR/backend test program; `--compiler` exercises integrated
`vm build` CLI dispatch. Keep provenance hashes with results. Broader supplemental
suites have a separate purpose and do not enlarge the parity claim implicitly.
