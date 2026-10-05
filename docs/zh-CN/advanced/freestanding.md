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
kinal build --freestanding main.kn -o firmware.elf
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
| `alloc` | 需要目标提供分配、字符串以及栈帧/根登记钩子 |
| `none` | 裸机模式的默认值：不进行堆分配，不支持托管对象和动态集合 |
| `gc` | Hosted 模式的默认值；裸机模式需要目标提供 GC/运行时钩子 |

裸机模式默认使用 `none`，也可以显式指定：

```bash
kinal build --freestanding --runtime none main.kn -o firmware.elf
```

Alloc 和 GC 配置不会附带裸机分配器或收集器。`__kn_*` ABI 见
`libs/runtime/include/kn/freestanding.h`；字符串、集合、异常及异步操作
需要相应的钩子。Alloc 同样会调用栈帧/根登记钩子；不执行垃圾回收的
自定义运行时可以将这些钩子实现为空操作。

### `--panic <策略>`

控制运行时 panic（如越界访问、空指针解引用）的处理方式：

| 值 | 说明 |
|----|------|
| `trap` | 执行陷阱指令（使调试器捕获，推荐用于嵌入式） |
| `loop` | 无限循环（不可恢复，确定性行为） |

```bash
kinal build --freestanding --panic trap main.kn -o firmware.elf
```

---

## 裸机目标平台

使用 `--target` 选择嵌入式/裸机目标：

| 别名 | 等效三元组 | 说明 |
|------|-----------|------|
| `bare64` | `x86_64-unknown-none-elf` | x86_64 裸机 |
| `bare-arm64` | `aarch64-unknown-none-elf` | ARM64 裸机 |

当前编译器支持 X86 和 AArch64 目标后端，不支持 Cortex-M/Thumb 或
RISC-V。也可以使用受支持的完整 LLVM 三元组，例如：

```bash
kinal build --target aarch64-unknown-none-elf \
      --freestanding --runtime none \
      main.kn --emit obj -o firmware.o
```

成功生成或链接目标文件不等于已经能在某块开发板上启动。应用仍需提供
匹配的内存布局、启动协议、已初始化的栈及平台启动代码。

---

## 链接脚本

嵌入式目标通常需要自定义链接脚本来定义内存布局：

```bash
kinal build --target bare-arm64 \
      --freestanding --runtime none \
      --link-script linker.ld \
      --no-crt \
      main.kn -o kernel.elf
```

示例链接脚本 `linker.ld`：

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

## 裸机入口点

Kinal 默认入口为 `KMain`；`--entry KernelMain` 可选择其他无参数或单指针
参数的函数。编译器导出 `__kn_entry` 包装函数，在初始化全局变量后调用
所选入口。引导程序或平台启动代码必须先满足目标 ABI 的要求。
下面的示例需要使用 `--entry KernelMain`：

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

## 完整示例：极简裸机核心

此示例仅使用静态存储和 volatile 操作，不假定特定开发板的 GPIO 地址
或启动协议。

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

使用与平台匹配的 `linker.ld` 构建 ARM64 裸机 ELF：

```bash
kinal build bare_main.kn \
      --target bare-arm64 \
      --freestanding --runtime none --panic trap \
      --no-crt --link-script linker.ld \
      -o firmware.elf
```

入口符号为 `__kn_entry`。执行前，启动环境必须加载 ELF 段并初始化栈和
需要清零的存储区。

---

## 与 `--runtime none` 的限制

使用 `--runtime none` 时，以下语言特性不可用：

| 特性 | 原因 |
|------|------|
| `New ClassName()` | 堆分配 |
| `list`、`dict`、`set` | 动态集合，需堆分配 |
| `async`/`await` | 依赖调度器和堆 |
| 异常（`Throw`/`Catch`） | 依赖运行时 |

仍可使用的特性：

- `Struct` 和 `Enum`（值类型，栈分配）
- 原始类型（`int`、`float`、`bool` 等）
- 固定大小数组（`int[16]` 等）
- 函数、基础算术与位操作
- 静态字符串、长度/相等比较及有界临时字符串拼接/格式化
- 基本 `any` 值、标签/类型判断及相等比较
- `Extern` FFI（访问 C 库/硬件驱动）
- 指针（`Unsafe` 上下文）

---

核心字符串辅助函数共享八个 512 字节的临时槽。拼接结果最多为 511 字节
加终止符；浮点格式化会截断为整数。这些结果只在临时槽被再次使用前
有效。字符串解析、聚合值装箱和动态分配的字符串存储仍需运行时支持。

## 相关

- [编译器 CLI](../cli/compiler.md) — `--freestanding`、`--runtime`、`--panic` 选项
- [交叉编译](cross-compilation.md) — 目标平台三元组
- [链接](linking.md) — 链接脚本和自定义 CRT
- [FFI](../language/ffi.md) — 硬件驱动接口
- [安全级别](../language/safety.md) — Unsafe 上下文
- [IO.Runtime 常量](runtime-environment-constants.md) — 区分原生与 VM 执行后端
