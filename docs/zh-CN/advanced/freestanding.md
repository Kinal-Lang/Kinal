# 裸机与嵌入式开发

Kinal 支持无操作系统（freestanding）环境下的开发，适用于嵌入式系统、内核、固件等场景。

---

## 核心概念

在"裸机"模式下：

- **没有操作系统**：没有 `main` 函数约定（或需要手动管理）
- **没有标准 C 运行时**：需要手动或不使用 CRT
- **没有堆分配**（默认）：除非显式配置运行时
- **没有标准库**：大部分 `IO.*` 标准库不可用

---

## 关键编译选项

### `--freestanding`

启用裸机模式，等同于 `--env freestanding`：

```bash
kinal --freestanding main.kn -o firmware.elf
```

效果：
- 不链接操作系统 syscall 接口
- 不自动注入 CRT 初始化代码
- 禁用依赖 OS 的标准库函数

### `--env <环境>`

| 值 | 说明 |
|----|------|
| `hosted` | 有操作系统的标准环境（默认） |
| `freestanding` | 无操作系统环境 |

### `--runtime <类型>`

控制运行时支持级别：

| 值 | 说明 |
|----|------|
| `alloc` | 启用堆分配（需要实现 `malloc`/`free` 或使用自定义分配器，默认） |
| `none` | 完全禁用堆分配，不可使用引用类型和集合 |
| `gc` | 启用垃圾回收运行时（适合桌面场景） |

嵌入式场景通常使用 `--runtime none`：

```bash
kinal --freestanding --runtime none main.kn -o firmware.elf
```

### `--panic <策略>`

控制运行时 panic（如越界访问、空指针解引用）的处理方式：

| 值 | 说明 |
|----|------|
| `trap` | 执行陷阱指令（使调试器捕获，推荐用于嵌入式） |
| `loop` | 无限循环（不可恢复，确定性行为） |

```bash
kinal --freestanding --panic trap main.kn -o firmware.elf
```

---

## 裸机目标平台

使用 `--target` 选择嵌入式/裸机目标：

| 别名 | 等效三元组 | 说明 |
|------|-----------|------|
| `bare64` | `x86_64-unknown-none` | x86_64 裸机 |
| `bare-arm64` | `aarch64-unknown-none` | ARM64 裸机 |

或使用完整 LLVM 三元组：

```bash
# ARM Cortex-M4
kinal --target thumbv7em-unknown-none-eabihf \
      --freestanding --runtime none \
      main.kn -o firmware.elf

# RISC-V 32位
kinal --target riscv32i-unknown-none-elf \
      --freestanding --runtime none \
      main.kn -o firmware.elf
```

---

## 链接脚本

嵌入式目标通常需要自定义链接脚本来定义内存布局：

```bash
kinal --target bare-arm64 \
      --freestanding --runtime none \
      --link-script linker.ld \
      --no-crt \
      main.kn -o kernel.elf
```

示例链接脚本 `linker.ld`：

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

## 裸机入口点

在裸机环境中，程序入口通常不是 `Main()`，而是自定义的汇编或 Extern 函数。

在 Kinal 中可以使用 FFI 与自定义入口对接：

```kinal
Unit Kernel.Boot;

// 声明自定义入口，由链接脚本或汇编跳转过来
Trusted Function void KernelMain()
{
    // 初始化代码
    InitHardware();
    InitMemory();

    // 主循环
    While (true)
    {
        ProcessInterrupts();
    }
}

Trusted Function void InitHardware()
{
    // 硬件初始化（通过 Extern FFI 调用驱动）
}

Trusted Function void InitMemory()
{
    // 内存初始化
}

Trusted Function void ProcessInterrupts()
{
    // 处理中断
}
```

---

## 安全约束

裸机模式下，所有硬件操作必须在 `Trusted` 或 `Unsafe` 上下文中进行：

```kinal
// 读写内存映射寄存器（需要 Unsafe）
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

## 完整示例：极简裸机程序

```kinal
Unit Bare.Main;

// 假设已定义外部 GPIO 寄存器地址
Const usize GPIO_ODR = 0x40020014;

Extern Function void delay(int ms) By C;

Unsafe Function int bare_main()
{
    While (true)
    {
        // 点亮 LED
        int* gpio = [int*](GPIO_ODR);
        *gpio = *gpio | 0x20;  // 设置 bit5
        delay(500);

        // 熄灭 LED
        *gpio = *gpio & ~0x20; // 清除 bit5
        delay(500);
    }
    Return 0;
}
```

编译命令：

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

## 与 `--runtime none` 的限制

使用 `--runtime none` 时，以下语言特性不可用：

| 特性 | 原因 |
|------|------|
| `New ClassName()` | 堆分配 |
| `list`、`dict`、`set` | 动态集合，需堆分配 |
| `string` 拼接 | 动态字符串，需堆分配 |
| `async`/`await` | 依赖调度器和堆 |
| 异常（`Throw`/`Catch`） | 依赖运行时 |

仍可使用的特性：

- `Struct` 和 `Enum`（值类型，栈分配）
- 原始类型（`int`、`float`、`bool` 等）
- 固定大小数组（`int[16]` 等）
- 函数、Math 运算、位操作
- `Extern` FFI（访问 C 库/硬件驱动）
- 指针（`Unsafe` 上下文）

---

## 相关

- [编译器 CLI](../cli/compiler.md) — `--freestanding`、`--runtime`、`--panic` 选项
- [交叉编译](cross-compilation.md) — 目标平台三元组
- [链接](linking.md) — 链接脚本和自定义 CRT
- [FFI](../language/ffi.md) — 硬件驱动接口
- [安全级别](../language/safety.md) — Unsafe 上下文
- [IO.Runtime 常量](runtime-environment-constants.md) — 区分原生与 VM 执行后端
