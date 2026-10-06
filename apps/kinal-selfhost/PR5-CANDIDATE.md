# PR #3–5 local integration

PR heads: #3 `e88543f`, #4 `9fa05d8`, #5 `9e20e95`.
Local branch: `codex/selfhost-integration`, incorporating main's ordered global
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
  collection and restoration, three executions each of C-/stage3-built tests.
- Cache transaction: four competing processes on one cold cache, a warm retry,
  extraction failure/recovery and owner termination, on C-/stage1-built tests.
- Function dump: 5,000 exact ordered records, two runs of each C-/stage1-built test.
- Audit harness: 14 unit tests; fake POSIX paths/path separators are independent
  of the OS running the unit tests. Linux/macOS/Windows x86/ARM64 native lock
  object compilation does not establish execution on those hosts.

## Not a release gate

Full PR acceptance is not claimed. Integrated snapshot `138293e` builds stage2
and stage3 with identical executable SHA-256
`eca8f7448b86c7543e0ebae048e27a8acae7bc6f8e944be9a1a985eec3a84866`.
The complete frontend/symbol/LLVM-IR comparison is a separate gate and is not
yet recorded as passed for this snapshot.

Before main's global initialization fixes were integrated, stage2's large
`symbols` command repeatedly exited with `0xC0000005`; stage3's object and
runtime audits passed only 164/190 and 155/186. The first integrated pre-lock
object audit improved to 189/191 but failed one cold-cache transaction and one
compiler invocation with `0xC0000374`. The cache race is fixed and tested; the
heap fault did not recur in eight warm retries or an ASan-instrumented retry,
which does not prove its cause. Fresh complete stage3 audits are still required.
Binary equality alone is not a passing bootstrap gate.

Windows CRT argv is currently ANSI/system
code-page text, while Kinal strings/literals are UTF-8. A direct stage0 probe
with `雪` receives bytes `D1 A9` on CP936, not UTF-8 `E9 9B AA`. The Unicode CLI
argument and diagnostic-path checks remain failing. This needs an end-to-end
UTF-8 CLI/filesystem boundary repair, not a change to test expectations.

Linux/macOS host execution and unrestricted KNC collection support remain
unverified/unsupported respectively. Main's global initialization work is
incorporated. Passing stage1 hosted regressions does
not establish complete C-stage0 behavior parity or unrestricted project support.
