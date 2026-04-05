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
kinal --freestanding main.kn -o firmware.elf
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
| `alloc` | Enable heap allocation (requires implementing `malloc`/`free` or using a custom allocator; default) |
| `none` | Completely disable heap allocation; reference types and collections cannot be used |
| `gc` | Enable garbage collection runtime (suitable for desktop scenarios) |

Embedded scenarios typically use `--runtime none`:

```bash
kinal --freestanding --runtime none main.kn -o firmware.elf
```

### `--panic <strategy>`

Controls how runtime panics (such as out-of-bounds access, null pointer dereference) are handled:

| Value | Description |
|-------|-------------|
| `trap` | Execute a trap instruction (allows debugger capture; recommended for embedded) |
| `loop` | Infinite loop (non-recoverable, deterministic behavior) |

```bash
kinal --freestanding --panic trap main.kn -o firmware.elf
```

---

## Bare-Metal Target Platforms

Use `--target` to select an embedded/bare-metal target:

| Alias | Equivalent Triple | Description |
|-------|------------------|-------------|
| `bare64` | `x86_64-unknown-none` | x86_64 bare-metal |
| `bare-arm64` | `aarch64-unknown-none` | ARM64 bare-metal |

Or use a full LLVM triple:

```bash
# ARM Cortex-M4
kinal --target thumbv7em-unknown-none-eabihf \
      --freestanding --runtime none \
      main.kn -o firmware.elf

# RISC-V 32-bit
kinal --target riscv32i-unknown-none-elf \
      --freestanding --runtime none \
      main.kn -o firmware.elf
```

---

## Linker Scripts

Embedded targets typically require a custom linker script to define the memory layout:

```bash
kinal --target bare-arm64 \
      --freestanding --runtime none \
      --link-script linker.ld \
      --no-crt \
      main.kn -o kernel.elf
```

Example linker script `linker.ld`:

```ld
ENTRY(_start)

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

In a freestanding environment, the program entry point is typically not `Main()`, but a custom assembly or Extern function.

In Kinal, FFI can be used to interface with a custom entry point:

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

## Complete Example: Minimal Bare-Metal Program

```kinal
Unit Bare.Main;

// Assume external GPIO register address is defined
Const usize GPIO_ODR = 0x40020014;

Extern Function void delay(int ms) By C;

Unsafe Function int bare_main()
{
    While (true)
    {
        // Turn on LED
        int* gpio = [int*](GPIO_ODR);
        *gpio = *gpio | 0x20;  // Set bit5
        delay(500);

        // Turn off LED
        *gpio = *gpio & ~0x20; // Clear bit5
        delay(500);
    }
    Return 0;
}
```

Build command:

```bash
kinal build bare_main.kn \
      --target thumbv7em-unknown-none-eabihf \
      --freestanding \
      --runtime none \
      --panic trap \
      --no-crt \
      --link-script stm32.ld \
      -o firmware.elf
```

---

## Limitations with `--runtime none`

When using `--runtime none`, the following language features are unavailable:

| Feature | Reason |
|---------|--------|
| `New ClassName()` | Heap allocation |
| `list`, `dict`, `set` | Dynamic collections require heap allocation |
| `string` concatenation | Dynamic strings require heap allocation |
| `async`/`await` | Depends on scheduler and heap |
| Exceptions (`Throw`/`Catch`) | Depends on runtime |

Features that remain available:

- `Struct` and `Enum` (value types, stack-allocated)
- Primitive types (`int`, `float`, `bool`, etc.)
- Fixed-size arrays (`int[16]`, etc.)
- Functions, math operations, bitwise operations
- `Extern` FFI (access to C libraries/hardware drivers)
- Pointers (`Unsafe` context)

---

## See Also

- [Compiler CLI](../cli/compiler.md) — `--freestanding`, `--runtime`, `--panic` options
- [Cross-Compilation](cross-compilation.md) — Target platform triples
- [Linking](linking.md) — Linker scripts and custom CRT
- [FFI](../language/ffi.md) — Hardware driver interfaces
- [Safety Levels](../language/safety.md) — Unsafe context
- [IO.Runtime Constants](runtime-environment-constants.md) — Distinguishing native and VM execution backends
