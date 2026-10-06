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

## Local evidence (Windows x64, LLVM 21.1.8)

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

Full PR acceptance is not claimed. The current argument-root/Any-identity snapshot
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
