# PR #3–5 local integration

PR heads: #3 `e88543f`, #4 `9fa05d8`, #5 `9e20e95`.
Primary checkout branch: `codex/selfhost-integrated`, incorporating main's ordered global
initialization and complete global/fixed-array root registration. Main and the
GitHub PRs have not been changed or published by this integration.

## Supported use

The candidate retains the hosted Native compiler, HIR-to-KNC backend, VM CLI,
formatter, package CLI and shared-library metadata from the PR. Use the
C-stage0-built **stage1** for targeted trials with ASCII Windows filesystem
paths and process arguments. Stage2/stage3 remain gated on full bootstrap and
batch execution: earlier candidate checks exposed a memory defect, and isolated
successful retries do not establish that its cause is fixed.
Use the matching KinalVM and adjacent toolchain/stdlib bundle, not an arbitrary
older released VM. Standard-library/runtime policy stays in Kinal `.klib`.

The freestanding scalar/pointer/enum/struct/borrowed-array/static-string subset
remains supported with `Runtime=None`. Compiler-embedded LLVM IR runtime helpers
were removed. The PR's extended string/any support and Alloc/GC freestanding
profiles are explicitly rejected until implemented in a Kinal runtime package.
Do not interpret the stage0-only extended-profile tests as selfhost support.

## Repairs

- Klib packing rejects manifest/payload aliases before truncation, including
  filesystem case aliases, hard links and available symlink aliases. Fixed in
  both the C stage0 and selfhost writer.
- Both formatters preserve adjacent operator/comment/number token boundaries.
  C stdin preserves raw newline bytes rather than CRT text-mode conversion.
- Windows shared-library compilation no longer passes unsupported `-fPIC`.
- Metadata stays registered across repeated loads of the same native handle;
  only the final successful close removes it and invalidates selfhost callbacks.
- Windows process argument quoting is implemented in Kinal with proper empty,
  embedded-quote and trailing-backslash handling. The C leaf only starts/waits
  for a process, with no shell or argument policy.
- Test-only freestanding Any hooks use an explicit aggregate-to-scalar ABI
  adapter; a C by-value struct is not the internal Kinal/LLVM aggregate ABI.
- KNC distinguishes a captured lexical `This` from a physical method receiver;
  callable parameter counts, bindings and call arguments use one shared rule.
- Kinal owns the full archive-cache check/extract/publish transaction under a
  native OS file lock. Cold parallel extraction, failed extraction/retry and
  interrupted-owner recovery are tested; the OS leaf does not own cache policy.
- Function symbol dumps collect ordered records and join them once instead of
  repeatedly copying the complete accumulated dump. A 5,000-record exact-output
  test covers empty output, owners, parameters and generic flags.
- Unqualified callable lookup filters imports by the requested name before
  scanning function symbols, avoiding unrelated qualified-name allocations.
  Owner/local/import-order/generic lookup behavior is regression-tested.
- Managed lexical declarations register zeroed slots once at function entry,
  including loop, foreach, pattern and catch bindings. Captured locals retain
  declaration-time heap cells and entry-owned pointer roots. The C stage0 follows
  the same stack-root contract; capture promotion removes its marked old initializer.
- C Typed HIR ignores semantically inactive compile-time branches when lowering
  and counting bindings, rather than reporting their intentionally unresolved syntax.
- Runtime native allocation/copy/fill/compare leaves use fixed `u64` counts to match
  their C contracts on x86 as well as 64-bit targets. Kinal GC/Core policy keeps its
  internal `usize` interface and converts explicitly at the raw boundary.
- Implicit call-argument conversions remain in typed HIR, so newly allocated
  aggregate boxes and character-to-string results are rooted before later arguments
  run. This covers ordinary/named/default/variadic calls, delegates, constructors,
  virtual methods and the physical `any` vector of dynamic Function calls.
- Native callable adapters preserve `any -> any` unchanged in both C stage0 and
  selfhost. The former zero fallback lost the incoming tag/payload and could crash
  dynamic calls with array arguments.
- Direct and known-reference string builtins share typed argument binding, retaining
  allocating character conversions before later arguments. C stage0 also converts
  accepted character arguments and roots converted Concat/Equals values; its KNC
  emitter supplies String rather than Char registers to the existing VM builtins.
- Assignment destinations keep the exact computed interior pointer in a zeroed,
  entry-registered root before RHS evaluation. A scalar/Struct value copy cannot
  retain its original storage. Both C stage0 and selfhost follow this rule without
  introducing temporary frames. Ordinary compound assignments read/root the old
  value before RHS effects, matching C stage0 evaluation order.
- Dictionary `TryFetch` evaluates the fallback argument before reading the
  collection. Replacement/insertion/removal during that argument is visible to
  lookup, for both instance and namespace calls.

## Local evidence (Windows x64, LLVM 21.1.8)

- C stage0 collection-loop repair, **rebuilt and verified**: collection
  output slots and `ToChars` length storage now use function-entry allocas;
  per-call initialization is preserved, and dictionary Fetch returns its SSA
  aggregate directly. The former lowering emitted fixed allocas at the call
  site, so loop iterations accumulated stack storage until function return.
  `collection_loop_storage.kn` passes at runtime and all seven target IR
  checks require entry-only stack storage. Rebuilt `x.py test --release --full`
  passes 289 enabled manifest cases, 85 Python tests (11 skips), and the
  driver/package/stage checks. Three C-built token-lifecycle reference modes
  formerly failed `0xC00000FD` at 120,000 list items; all now pass unchanged
  inputs for two rounds. The corresponding unchanged-stage3-built programs
  also pass all three modes. The original compiler access/heap faults remain
  unresolved; these focused passes do not replace the failed full batches.
  Evidence: `F:\Kinal-Agent-Temp\gc-entry-root-regression\collection-slots\validation-report.json`.
- Dictionary fallback follow-up: old selfhost returns `10/99/20/40` when eager
  fallback arguments mutate dictionary entries and collect; C stage0 and the
  repaired stage1 return `20/40/99/99`. The extended `collection_runtime` fixture
  also requires exactly four fallback calls. Its complete C/stage1 output and
  existing IR checks for Kinal runtime ownership pass; stage2-/stage3-built
  fixtures each pass three runs. This is an independent semantic fix; no actual
  TryFetch call occurs in compiler implementation code. Fresh stage2/stage3 PE
  SHA-256 is `5bab7ebccec6551aa8060f8aa91d0f8b9507386874ecba98598132e21ff7796c`.
  Both build and pass the large parser input; stage1/2/3 project-AST summaries
  match. Bootstrap then fails in stage2's compiler-project check (`0xC0000374`),
  before stage3 check and symbol/IR comparisons. Native stage2 is 191/193:
  `alias` fails `0xC0000374`, `global_fixed_gc` fails `0xC0000005`, neither emits
  an object. Stage3 build/runtime is 188/189: `struct_value_semantics` fails
  during compilation with `0xC0000005`; all 188 built programs pass their expected
  output/exit checks. These failed runs are retained without retry substitution.
  Four-worker Native auditing overlaps the late bootstrap comparisons.
  Evidence: `F:\Kinal-Agent-Temp\gc-entry-root-regression\dictionary-fallback`.
- Independent legacy-C scanner follow-up: `gc_scan_region` no longer reads a
  complete pointer from a partial tail; unaligned starts use byte-copy loads.
  A plain C unit against the actual runtime fails before the change (size 1
  incorrectly marks an object) and passes afterward for every length from zero
  through three pointer words, with aligned and unaligned starts. Rebuilt
  `x.py test --release --full` passes all 288 enabled manifest cases plus
  driver/package/stage checks; Python discovery runs 96 tests with 85 passing
  and 11 skips. The initial test invocation selected an outdated debug VM and
  failed KNC-v3 checks; the successful run explicitly uses the rebuilt release
  compiler and VM. This does not establish a selfhost stability repair, and the
  bootstrap hashes below still describe the preceding assignment-storage snapshot.
- Assignment-storage follow-up: the old pointer-assignment fixture deterministically
  fails heap-ASan with WRITE8 use-after-free. Repaired pointer/Struct-field direct
  and compound assignments pass the complete GC fixture three times under heap-ASan;
  the RHS-replacement cases yield 24 rather than the former 12. C-/stage1-/stage3-built
  GC fixtures pass three host runs each, and both compilers pass seven-target IR
  checks requiring initialized, exactly-once entry roots for all four destination
  addresses. Current C full manifest passes all 288 enabled Windows cases on its
  first attempt, and Python harness units pass 44/44. No production collector or
  allocation-threshold workaround was added. Cross-target IR is not foreign-host
  execution, and these focused passes do not establish compiler batch stability.
- String-builtin follow-up (preceding snapshot): old selfhost lowering deterministically fails heap-ASan
  with use-after-free when a later argument collects; repaired output prints `xx`
  and `true`. Four direct/reference GC forms pass three C-/stage1-/stage2-built host runs;
  seven-target IR requires each conversion to be rooted before collection.
  The ordinary nine-output fixture passes C/selfhost Native and KNC. Fresh C full
  manifest passes 288 enabled cases; the first attempt stopped on `function_objects`
  with `0xC0000043`, then that case and the fresh full retry passed. This retry is
  not a diagnosis of the initial process failure. Current stage1 diagnostics pass
  95/95, registered KNC passes 56/56, and Python harness units pass 37/37.
- Fresh C stage0 and stage1 built; stage1 compiled the matching KinalVM.
- Integrated stage1 manifest Native object compilation: 191/191 positive cases.
- Integrated stage1 manifest executable/runtime checks: 187/187 cases, including hosted
  standard-library and native FFI fixtures. Negative diagnostics and other
  host platforms are separate gates.
- Complete stage0/stage1 negative diagnostic differential: 95/95 cases.
- Formatter: 61 fixtures (including 41 compiler sources), 282 invocations,
  13 before/after lexer-stream comparisons and executable semantic checks.
- Package CLI: 27 cases/40 commands, cross-compiler package consumption, and
  16 non-destructive input/output alias rejections.
- Native metadata: four stage0/selfhost producer-consumer combinations,
  repeated-load lifetime, final-close invalidation, enum metadata.
- Runtime-free subset: nine target object/IR inspections, host consumer,
  16 negative cases. Foreign target binaries were not executed.
- Integrated KNC CLI + matching VM: 55/55 registered differential cases. The
  broader 65-case suite passes 64 and fails `any_object_casts`: `list.Create`
  has no C/selfhost VM mapping. The failing expectation remains intact.
- KNC workflow: listings, superloop toggle, 19 loop-semantic checks,
  eight fused opcodes, writer/listing failure handling.
- Process argument regression: spaces in executable/output paths, empty args,
  quotes, tabs, backslashes and shell metacharacters, without a shell.
- Kinal GC frame growth: 257 native-owned root slots, nested frames, explicit
  collection and restoration, 128 loop iterations with nested/same-name/pattern/catch
  declarations, three executions each of C-/stage3-built tests. Both compilers'
  seven-target IR has entry-only initialized roots and exact native-memory prototypes.
- Cache transaction: four competing processes on one cold cache, a warm retry,
  extraction failure/recovery and owner termination, on C-/stage1-built tests.
- Function dump: 5,000 exact ordered records, two runs of each C-/stage1-built test.
  The same model checks 32 unrelated imports and 64 missing-name lookups, plus
  owner/local/import priority and generic qualified targets.
- Selfhost Python harness: 36 unit tests; fake POSIX paths/path separators are independent
  of the OS running the unit tests. Linux/macOS/Windows x86/ARM64 native lock
  object compilation does not establish execution on those hosts.
- Current C stage0: all 287 enabled main-manifest cases pass after the Any adapter
  fix. Driver/package, complete stage/HIR and seven-target capture-storage checks
  passed on the preceding C snapshot.
- Converted-argument GC regression: C-/stage1-/stage3-built executables each pass
  three runs; seven-target selfhost IR checks all eight aggregate boxes and the
  allocated character string. The old compiler deterministically produces an ASan
  use-after-free on this fixture; the repaired output passes the same ASan check.
  Only the fixture temporarily clears/restores conservative stack retention; no
  production collector workaround or allocation-threshold change was introduced.
- Current argument-root/Any-identity snapshot: stage2 Native objects 192/192;
  stage3 Native objects 191/192 (`package_contextual_default`, compiler
  `0xC0000374`); stage3 executable/runtime 187/188 (`stdlib_file_text_roundtrip`,
  compiler `0xC0000005` before linking). Current stage3 diagnostics remain exact
  95/95; C-built stage1 registered KNC remains 55/55. These are failed overall
  stage3 batches, not unsupported/skipped cases.
- After the lexical-root repair, stage3 Native objects pass 191/191 and executable
  runtime checks pass 187/187. A separate stage2 object audit passes 190/191,
  with `multidim_arrays` failing inside the compiler with `0xC0000374`.
  A second complete stage3 object audit passes 190/191; `stdlib_request_compile`
  fails inside the compiler with `0xC0000005`, before producing an object.

## Not a release gate

Full PR acceptance is not claimed. The assignment-storage follow-up builds stage2
and stage3 with identical executable SHA-256
`19fcc5a5c3daa93c3b3ff8eee712b7509c18a04f523dfbdcffc2045b82f1d47e`.
Full bootstrap now passes project-AST/check/symbol summaries and stage1/2/3 IR
comparisons (IR SHA-256
`5765f653c3faf70f0c69be3d8513347f6823d0b5102a3185c76c92491f1013a0`).
This establishes convergence only. Its stage2
Native-object batch fails overall at 185/193: `array_type_contract`, `autolink`,
`ffi_attr_file`, `knc_f32_rounding`, `pointer_depth`, `recursive_method_static`,
and `type_modules` fail inside the compiler with `0xC0000005`;
`unsafe_alias_unicode_keyword` fails with `0xC0000374`. None produces an object.
Its stage3 runtime batch fails overall at 185/189: `attributes`, `stdlib_more`
and `type_modules` fail during compilation with `0xC0000005`, while
`builtin_function_ref` fails during compilation with `0xC0000374`. The 185
successfully built programs all pass their expected runtime output/exit checks;
the four compiler failures remain failed cases, not skips.
The independently reproduced assignment-storage bug is fixed, but is not the
complete explanation for the remaining compiler instability.

Ordinary CLI follow-up on the unchanged stage2 binary passes 32/32 `check` and
32/32 `build --emit check` invocations (four processes, eight previously failing
projects, four rounds). A read-only CFG audit of the saved stage2 IR checks
1,446 functions with a GC frame and 14,871 reachable return blocks without
finding an unmatched pop, live-frame return, or invalid frame registration.
This checks control-flow bookkeeping only; it neither validates every memory
access nor replaces the failed Native/runtime batches.
An additional unchanged-stage2 ordinary `build-ir --trace` batch fails 1/32
(`pointer_depth`, `0xC0000005`); its log stops at `compile:frontend`, before
`compile:sema`, and no IR file exists. This reproduces a frontend failure without
requiring object generation or linking. The other 31 passes do not erase it.

An ordinary standalone workload built by the latest stage3 reproduces
`0xC0000005` while only loading and lexing the 41 compiler source files
(1,390,061 characters; 244,290 tokens). One of four lexer-mode processes fails;
the other three match the C reference. File-only and parser modes each pass
their four processes. That failed mode does not call the parser, project/package
resolver, Sema, or LLVM backend. It narrows the exercised path but does not prove
that this failure and the compiler batches share the same first invalid write.
Reports: `F:\Kinal-Agent-Temp\gc-entry-root-regression\frontend-corpus\compiler-corpus\report.json`.
An additional ordinary lexer-only project logs each source file and checks
retained Token kinds, positions and text lengths after collection. Its four
stage3-built processes each complete five passes of the same corpus and match
the C reference checksum. This is a different test executable; its success
does not erase the original lexer-mode failure or locate its first cause.
Report: `F:\Kinal-Agent-Temp\gc-entry-root-regression\lexer-corpus\report.json`.
A separate read-only audit of the saved assignment-stage2 IR checks 35,992
root registrations in 1,381 functions: direct alloca provenance, matching
storage size, entry placement, prior stores and unique registration all pass.
These syntactic checks do not prove dynamic memory correctness.

Further localization on unchanged `003d708` reproduces three production faults
in 24 targeted compilations. LLDB with the Windows debug heap disabled captures
faults in `GarbageCollector.AddRoot` and `PopFrame` during compiler frontend
work; a separate run captures a null-root-table write and saves a minidump.
Diagnostic IR instrumentation independently traps a null root table before the
write. One later header snapshot differs from the initial loads; another
preserves the corrupt count/capacity. The first invalid transition is still
unidentified. Do not replace the
collector or add a null-table fallback on that evidence alone.
An ASan build without the extra IR O2 pipeline reports a heap-buffer-overflow
in `GarbageCollector.ScanRegion` while tracing reachable blocks; other cases
time out under instrumentation. Different instrumented/debugger variants pass
their repeats, and a 200,000-iteration standalone frame-lifetime stress passes.
A subsequent diagnostic lifecycle trace fails once in 80 compilations: a
`ListHeader` frame is valid after `PushFrame` and at its first `AddRoot`
entry, then corrupt at the immediately following `PopFrame` entry. The
registration interval is narrowed; the offending write remains unidentified.
A later sequential production subset passes 24/24, while instrumented variants
also pass repeats; none supersede the failed parallel production batches.
These are localization results, not a repaired production stability gate.
Reproduction scripts, stacks, minidump and a structured evidence index are in
`F:\Kinal-Agent-Temp\gc-entry-root-regression\stability-investigation.json`.

The preceding string-builtin follow-up produces stage2
SHA-256 `f1a819d5f5038133d8e8533963834ec40b081022023897abdf3567bd37dff7a6`,
but stage2 exits with `0xC0000005` while building stage3. Its complete Native-object
batch passes 189/193: `any_cast` and `overload_functions` fail inside the compiler
with `0xC0000374`; `any_mixed_kind_equals` and `string_builtin_conversions` fail
with `0xC0000005`. None produces an object. These are failed compilation gates,
not unsupported features or skipped expectations. The same stage2 separately
builds the GC fixture, whose executable passes three times; that does not erase
the failed full batch or establish a fix for the remaining compiler instability.

The preceding argument-root/Any-identity snapshot
builds stage2 and stage3 with identical executable SHA-256
`218e7d3d18997fd7b0c978dd46cc8d8a215ad41db32ae7db7be8ad29fd3dc4ad`.
Both pass the large parser input and the stage1/2/3 project-AST comparisons.
However, stage2 exits with `0xC0000005` while checking the compiler project itself,
so this bootstrap run fails before symbol/IR comparisons. Do not substitute the
matching executables or the earlier green bootstrap for that failed gate.

The preceding lexical-root snapshot's bootstrap passed
large parser input, frontend/project summaries, check/symbol summaries and
stage1/2/3 LLVM-IR comparisons. Stage2/stage3 executable SHA-256 is
`4c6bba0dab691b20143efc7d4271d34854a44ad9c2ba32616589eef9d336d67e`;
all three stages' LLVM-IR SHA-256 is
`9b1fa27df921e0510d5b7aa2b2d95102078708677ec93a5f966c6cd953f4bcf0`.
The separate bootstrap consuming the fixed-width runtime package also passes
all summaries and stage1/2/3 IR comparisons, with the same IR and stage2/stage3
executable hashes.

Earlier integrated batches failed repeatedly with compiler `0xC0000005` and
`0xC0000374` exits. A stage3 object/runtime batch is green, but separate stage2
and stage3 object batches each record one compiler memory fault. A heap-ASan build
passes `multidim_arrays`; neither that retry nor one green batch establishes a
root-cause fix. The archive cache race and repeated loop-root registration are independently
fixed and tested, not explanations for every historical heap failure. Repeatable
batch stability remains a release/main-promotion gate. Bootstrap `100%` means
stage convergence, not complete language/backend/platform behavior parity.
The converted-argument use-after-free now has a deterministic regression and
verified repair; the later compiler-project check failure confirms that it was
not the only remaining defect.
Diagnostic-only O1 heap-ASan with a reduced GC threshold also passes
`multidim_arrays`, `binary_hir_native_semantics`, `any_object_casts`,
`capture_control_flow`, `global_fixed_gc` and `stdlib_request_compile` object
compilation. These are isolated retries with a different allocator/optimization
environment, not passing production stability evidence; no diagnostic threshold
or sanitizer setting is part of the shipped runtime.

Windows CRT argv is currently ANSI/system
code-page text, while Kinal strings/literals are UTF-8. A direct stage0 probe
with `雪` receives bytes `D1 A9` on CP936, not UTF-8 `E9 9B AA`. The Unicode CLI
argument and diagnostic-path checks remain failing. This needs an end-to-end
UTF-8 CLI/filesystem boundary repair, not a change to test expectations.

Linux/macOS host execution and unrestricted KNC collection support remain
unverified/unsupported respectively. Main's global initialization work is
incorporated. Passing stage1 hosted regressions does
not establish complete C-stage0 behavior parity or unrestricted project support.
