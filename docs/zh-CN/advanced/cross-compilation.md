# 交叉编译



Kinal 基于 LLVM 后端，天然支持交叉编译——在一台机器上为另一平台编译可执行文件。



---



## 核心选项



```bash

kinal --target <三元组> [其他选项] <文件.kn> -o <输出>

```



---



## 内置平台别名



Kinal 提供一套平台快捷别名，无需记忆完整的 LLVM 三元组：



| 别名 | 等效三元组 | 说明 |

|------|-----------|------|

| `win86` | `i686-windows-msvc` | Windows 32 位 |

| `win64` | `x86_64-windows-msvc` | Windows 64 位 |

| `linux64` | `x86_64-linux-gnu` | Linux x86_64 |

| `linux-arm64` | `aarch64-linux-gnu` | Linux ARM64 |

| `bare64` | `x86_64-unknown-none` | 裸机 x86_64 |

| `bare-arm64` | `aarch64-unknown-none` | 裸机 ARM64 |



---



## 常见交叉编译场景



### 在 Linux 上编译 Windows 程序



```bash

kinal --target win64 main.kn -o main.exe --linker zig

```



> 推荐使用 `--linker zig` 进行 Windows 交叉编译，zig 工具链包含 Windows 目标的 C 运行时头文件。



### 在 Windows 上编译 Linux 程序



```bash

kinal --target linux64 main.kn -o main --linker zig

```



### 编译 ARM64 Linux 程序



```bash

kinal --target linux-arm64 main.kn -o main

```



---



## 链接器选择



| 链接器 | 选项 | 适用场景 |

|--------|------|---------|

| `lld` | `--linker lld`（默认） | 本机编译，速度最快 |

| `zig` | `--linker zig` | 交叉编译，自带 cross 工具链 |

| `msvc` | `--linker msvc` | Windows 原生，需安装 MSVC |



zig 工具链（`zig cc`）是最推荐的交叉编译选项，因为它内置了各平台的 sysroot，无需单独安装交叉编译工具链：



```bash

# 安装 zig（用作 clang 前端 + 交叉链接器）

# 然后直接使用：

kinal --target linux-arm64 main.kn -o myapp --linker zig

```



---



## 系统根目录（Sysroot）



编译需要系统头文件和库时，可能需要指定 sysroot：



```bash

kinal --target linux-arm64 \

      -L /path/to/arm64-sysroot/lib \

      main.kn -o myapp

```



---



## 静态链接（推荐用于发布）



交叉编译时推荐静态链接，避免目标平台上的动态库版本问题：



```bash

kinal --target linux64 \

      --linker zig \

      -o myapp-linux \

      main.kn

```



---



## 构建矩阵示例



同时为多个平台构建的脚本：



```bash

#!/bin/bash

TARGETS=(

    "win64:main-windows.exe"

    "linux64:main-linux"

    "linux-arm64:main-linux-arm64"

)



for entry in "${TARGETS[@]}"; do

    target="${entry%%:*}"

    output="${entry##*:}"

    echo "编译 $target -> $output ..."

    kinal --target "$target" --linker zig main.kn -o "dist/$output"

done

```



---



## 注意事项



### 平台相关代码



使用 `IO.Target.OS` 条件来处理平台差异：

```kinal
Static Function string GetConfigPath()
{
    If (IO.Target.OS == IO.Target.OS.Windows)
    {
        Return IO.Path.Combine(IO.System.GetEnv("APPDATA"), "myapp");
    }
    Return IO.Path.Combine(IO.System.GetEnv("HOME"), ".config/myapp");
}

```



### C 标准库函数的可用性



交叉编译时，`Extern` FFI 调用的 C 函数必须在目标平台上存在。使用 `--linker zig` 时，zig 负责提供目标平台的 C 运行时。



---



## 相关



- [编译器 CLI](../cli/compiler.md) — `--target`、`--linker` 选项

- [链接](linking.md) — 链接器后端详解

- [裸机开发](freestanding.md) — 无 OS 目标

- [FFI](../language/ffi.md) — 平台特定 C 接口

