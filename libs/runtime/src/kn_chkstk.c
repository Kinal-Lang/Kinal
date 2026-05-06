// Windows stack-probe shims for CRT-free builds.
// The x64/arm64 sequences are adapted from LLVM compiler-rt chkstk routines.

#ifdef _WIN32

#if defined(__x86_64__)

__asm__(
    ".text\n"
    ".globl __chkstk\n"
    ".p2align 4, 0x90\n"
    "__chkstk:\n"
    "  pushq %rcx\n"
    "  pushq %rax\n"
    "  cmpq $0x1000, %rax\n"
    "  leaq 24(%rsp), %rcx\n"
    "  jb 1f\n"
    "2:\n"
    "  subq $0x1000, %rcx\n"
    "  testq %rcx, (%rcx)\n"
    "  subq $0x1000, %rax\n"
    "  cmpq $0x1000, %rax\n"
    "  ja 2b\n"
    "1:\n"
    "  subq %rax, %rcx\n"
    "  testq %rcx, (%rcx)\n"
    "  popq %rax\n"
    "  popq %rcx\n"
    "  retq\n"
    ".globl ___chkstk_ms\n"
    ".p2align 4, 0x90\n"
    "___chkstk_ms:\n"
    "  jmp __chkstk\n"
);

#elif defined(__aarch64__) || defined(__arm64ec__)

__asm__(
    ".text\n"
#if defined(__arm64ec__)
    ".globl __chkstk_arm64ec\n"
    ".p2align 2\n"
    "__chkstk_arm64ec:\n"
#else
    ".globl __chkstk\n"
    ".p2align 2\n"
    "__chkstk:\n"
#endif
    "  lsl x16, x15, #4\n"
    "  mov x17, sp\n"
    "1:\n"
    "  sub x17, x17, #4096\n"
    "  subs x16, x16, #4096\n"
    "  ldr xzr, [x17]\n"
    "  b.gt 1b\n"
    "  ret\n"
);

#else

__attribute__((weak)) void __chkstk(void) {}
__attribute__((weak)) void ___chkstk_ms(void) {}
__attribute__((weak)) void __kn_chkstk_stub(void) {}

#endif

#endif
