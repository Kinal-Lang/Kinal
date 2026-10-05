# Freestanding and Embedded Development

Kinal supports development in environments without an operating system (freestanding), suitable for embedded systems, kernels, firmware, and similar use cases.

---

## Core Concepts

In "bare-metal" mode:

- **No operating system**: No `main` function convention (or must be managed manually)
- **No standard C runtime**: Must be handled manually or bypassed entirely
- **No heap allocation** (by default): Unless a runtime is explicitly configured
- **No standard library**: Most `IO.*` standard library functions are unavailable

---

## Key Compiler Options

### `--freestanding`

Enables bare-metal mode, equivalent to `--env freestanding`:

```bash
kinal build --freestanding main.kn -o firmware.elf
```

Effects:
- Does not link OS syscall interfaces
- Does not automatically inject CRT initialization code
- Disables standard library functions that depend on the OS

### `--env <environment>`

| Value | Description |
|-------|-------------|
| `hosted` | Standard environment with an operating system (default) |
| `freestanding` | Environment without an operating system |

### `--runtime <type>`

Controls the runtime support level:

| Value | Description |
|-------|-------------|
| `alloc` | Custom allocation/string hooks plus frame/root hooks; the target supplies their implementation |
| `none` | Freestanding default: no heap allocation; managed objects and dynamic collections are unavailable |
| `gc` | Hosted default; freestanding use requires custom GC/runtime hooks |

Freestanding builds default to `none`; the explicit form is:

```bash
kinal build --freestanding --runtime none main.kn -o firmware.elf
```

The Alloc and GC profiles do not bundle a bare-metal allocator or collector. The
`__kn_*` ABI is documented in `libs/runtime/include/kn/freestanding.h`; additional
string, collection, exception or async operations require their corresponding
hooks. Alloc also emits frame/root calls, which a non-collecting runtime can
implement as no-ops.

### `--panic <strategy>`

Controls how runtime panics (such as out-of-bounds access, null pointer dereference) are handled:

| Value | Description |
|-------|-------------|
| `trap` | Execute a trap instruction (allows debugger capture; recommended for embedded) |
| `loop` | Infinite loop (non-recoverable, deterministic behavior) |

```bash
kinal build --freestanding --panic trap main.kn -o firmware.elf
```

---

## Bare-Metal Target Platforms

Use `--target` to select an embedded/bare-metal target:

| Alias | Equivalent Triple | Description |
|-------|------------------|-------------|
| `bare64` | `x86_64-unknown-none-elf` | x86_64 bare-metal |
| `bare-arm64` | `aarch64-unknown-none-elf` | ARM64 bare-metal |

The current compiler target backends support X86 and AArch64. Cortex-M/Thumb
and RISC-V are not supported targets. A full supported LLVM triple can also be
used, for example:

```bash
kinal build --target aarch64-unknown-none-elf \
      --freestanding --runtime none \
      main.kn --emit obj -o firmware.o
```

Target emission and linking do not establish that a board can boot the result.
A matching memory map, boot protocol, initialized stack and platform startup
code are still the application's responsibility.

---

## Linker Scripts

Embedded targets typically require a custom linker script to define the memory layout:

```bash
kinal build --target bare-arm64 \
      --freestanding --runtime none \
      --link-script linker.ld \
      --no-crt \
      main.kn -o kernel.elf
```

Example linker script `linker.ld`:

```ld
ENTRY(__kn_entry)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS
{
    .text : { *(.text*) } > FLASH
    .data : { *(.data*) } > RAM AT > FLASH
    .bss  : { *(.bss*)  } > RAM
}
```

---

## Bare-Metal Entry Point

The default Kinal entry is `KMain`; `--entry KernelMain` selects another
zero-argument or one-pointer function. The compiler exports an `__kn_entry`
wrapper that initializes globals and invokes that function. A bootloader or
platform startup stub must satisfy the target ABI before entering the wrapper.
For the example below, build with `--entry KernelMain`:

```kinal
Unit Kernel.Boot;

// Declare custom entry point, jumped to from the linker script or assembly
Trusted Function void KernelMain()
{
    // Initialization code
    InitHardware();
    InitMemory();

    // Main loop
    While (true)
    {
        ProcessInterrupts();
    }
}

Trusted Function void InitHardware()
{
    // Hardware initialization (via Extern FFI to call drivers)
}

Trusted Function void InitMemory()
{
    // Memory initialization
}

Trusted Function void ProcessInterrupts()
{
    // Handle interrupts
}
```

---

## Safety Constraints

In bare-metal mode, all hardware operations must be performed within a `Trusted` or `Unsafe` context:

```kinal
// Read/write memory-mapped registers (requires Unsafe)
Unsafe Function void WriteReg(usize addr, int value)
{
    int* ptr = [int*](addr);
    *ptr = value;
}

Unsafe Function int ReadReg(usize addr)
{
    int* ptr = [int*](addr);
    Return *ptr;
}
```

---

## Complete Example: Minimal Bare-Metal Core

This example uses only static memory and volatile operations. It makes no claim
about a particular board's GPIO addresses or boot protocol.

```kinal
Unit Bare.Main;
Get IO.Volatile;

u64 Counter = 0;

Trusted Static Function void KMain()
{
    While (true)
    {
        IO.Volatile.Write64(&Counter, IO.Volatile.Read64(&Counter) + 1);
    }
}
```

Build a bare ARM64 ELF using a platform-appropriate `linker.ld`:

```bash
kinal build bare_main.kn \
      --target bare-arm64 \
      --freestanding --runtime none --panic trap \
      --no-crt --link-script linker.ld \
      -o firmware.elf
```

The entry is `__kn_entry`. The selected boot environment must load the ELF
sections and initialize the stack and zero-filled storage before executing it.

---

## Limitations with `--runtime none`

When using `--runtime none`, the following language features are unavailable:

| Feature | Reason |
|---------|--------|
| `New ClassName()` | Heap allocation |
| `list`, `dict`, `set` | Dynamic collections require heap allocation |
| `async`/`await` | Depends on scheduler and heap |
| Exceptions (`Throw`/`Catch`) | Depends on runtime |

Features that remain available:

- `Struct` and `Enum` (value types, stack-allocated)
- Primitive types (`int`, `float`, `bool`, etc.)
- Fixed-size arrays (`int[16]`, etc.)
- Functions, primitive arithmetic and bitwise operations
- Static strings, length/equality, and bounded scratch-string concatenation/formatting
- Primitive `any` values, tag/type predicates and equality
- `Extern` FFI (access to C libraries/hardware drivers)
- Pointers (`Unsafe` context)

---

Core string helpers share eight 512-byte scratch slots. Concatenation is bounded
to 511 bytes plus a terminator; float formatting truncates to an integer. These
results are temporary and are overwritten as the scratch slots are reused.
String parsing, aggregate boxing and dynamically allocated string storage still
require a runtime.

## See Also

- [Compiler CLI](../cli/compiler.md) — `--freestanding`, `--runtime`, `--panic` options
- [Cross-Compilation](cross-compilation.md) — Target platform triples
- [Linking](linking.md) — Linker scripts and custom CRT
- [FFI](../language/ffi.md) — Hardware driver interfaces
- [Safety Levels](../language/safety.md) — Unsafe context
- [IO.Runtime Constants](runtime-environment-constants.md) — Distinguishing native and VM execution backends
