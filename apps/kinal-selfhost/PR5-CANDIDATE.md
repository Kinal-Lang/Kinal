# PR #5 local candidate

Base: PR #5 head `d38f2880f6bc190f493f478fc6e7a5658003007f`.
Local branch: `codex/pr5-usable`. This is not merged into main or published.

## Supported use

The candidate retains the hosted Native compiler, HIR-to-KNC backend, VM CLI,
formatter, package CLI and shared-library metadata from the PR. It is suitable
for targeted trials with ASCII Windows filesystem paths and process arguments.
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

## Local evidence (Windows x64, LLVM 21.1.8)

- Fresh C stage0 and stage1 built; stage1 compiled the matching KinalVM.
- Formatter: 61 fixtures (including 41 compiler sources), 282 invocations,
  13 before/after lexer-stream comparisons and executable semantic checks.
- Package CLI: 27 cases/40 commands, cross-compiler package consumption, and
  16 non-destructive input/output alias rejections.
- Native metadata: four stage0/selfhost producer-consumer combinations,
  repeated-load lifetime, final-close invalidation, enum metadata.
- Runtime-free subset: nine target object/IR inspections, host consumer,
  16 negative cases. Foreign target binaries were not executed.
- KNC: 37 scalar-suite cases, including three intentional rejections.
- KNC workflow: listings, superloop toggle, 19 loop-semantic checks,
  eight fused opcodes, writer/listing failure handling.
- Process argument regression: spaces in executable/output paths, empty args,
  quotes, tabs, backslashes and shell metacharacters, without a shell.

## Not a release gate

Full PR acceptance is not claimed. Windows CRT argv is currently ANSI/system
code-page text, while Kinal strings/literals are UTF-8. A direct stage0 probe
with `雪` receives bytes `D1 A9` on CP936, not UTF-8 `E9 9B AA`. The Unicode CLI
argument and diagnostic-path checks remain failing. This needs an end-to-end
UTF-8 CLI/filesystem boundary repair, not a change to test expectations.

Linux/macOS host execution and the full Native/stdlib/aggregate KNC regressions
have not been run for this candidate. Existing uncommitted main-branch work is
not incorporated. A successful bootstrap establishes stage reproducibility,
not complete C-stage0 behavior parity or unrestricted project support.
