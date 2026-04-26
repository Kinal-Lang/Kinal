# kinal CLI 总览

`kinal` 是 Kinal 语言的统一命令行工具。所有操作通过**子命令**区分；每个子命令拥有独立的选项集。

## 子命令一览

| 子命令 | 用途 |
|--------|------|
| [`build`](#kinal-build) | 编译源码为原生可执行文件、目标文件、汇编或 IR |
| [`run`](#kinal-run) | 编译源文件为临时原生可执行文件并立即运行 |
| [`fmt`](fmt.md) | 格式化 `.kn` 源文件 |
| [`vm`](vm.md) | 字节码相关操作（构建 `.knc`、执行、打包 vm-exe） |
| [`pkg`](packages.md) | 包管理（构建 `.klib`、查看信息、解包） |

```
kinal [options] <subcommand> [options] [args...]
kinal build [options] <input file> [file2 ...]
kinal run [options] <file.kn> [-- program-args...]
```

---

## 全局选项

### 查看版本

```bash
kinal --version            # 显示编译器版本
kinal -V                   # 同上（简写）
kinal --version --verbose  # 同时显示 KinalVM 版本
```

### 诊断语言与本地化

`--lang` 和 `--locale-file` 可放在子命令**之前**（全局）或**之后**（局部），效果相同：

```bash
kinal --lang zh build main.kn    # 全局指定
kinal build --lang zh main.kn    # 等效：局部指定
```

| 选项 | 默认 | 说明 |
|------|------|------|
| `--lang <en\|zh>` | `en` | 诊断输出语言 |
| `--locale-file <文件>` | — | 自定义本地化 JSON（覆盖内置语言包） |

> **提示：** 指定 `--lang zh` 时，工具会自动查找编译器同目录下的 `locales/zh-CN.json`，通常无需手动传 `--locale-file`。

---

## 快速参考

```bash
# 编译为可执行文件
kinal build main.kn -o main

# 编译为目标文件
kinal build main.kn --emit obj -o main.o

# 输出 LLVM IR
kinal build main.kn --emit ir

# 快速运行（原生编译，执行后自动清理）
kinal run main.kn

# 构建字节码
kinal vm build main.kn -o main.knc

# 从项目文件构建
kinal build --project .

# 通过 VM 执行
kinal vm run main.kn

# 格式化代码
kinal fmt main.kn

# 查看 .klib 信息
kinal pkg info mylib.klib
```

---

## kinal build

编译 `.kn` 源文件为原生产物。

```
kinal build [选项] <文件.kn> [文件2.kn ...]
```

### 输出控制

| 选项 | 说明 |
|------|------|
| `-o, --output <文件>` | 指定输出文件路径 |
| `--emit <模式>` | 输出格式（见下表），默认 `bin` |
| `--kind <类型>` | 产物类型：`exe`（默认）、`static`、`shared` |

#### `--emit` 模式

| 模式 | 说明 |
|------|------|
| `bin` | 链接成可执行文件（默认） |
| `obj` | 生成目标文件（`.o` / `.obj`） |
| `asm` | 生成汇编代码 |
| `ir` | 生成 LLVM IR（`.ll`） |

### 目标平台

| 选项 | 说明 |
|------|------|
| `--target <三元组>` | 目标平台三元组（如 `x86_64-linux-gnu`） |
| `--freestanding` | 裸机/嵌入式模式 |
| `--env <环境>` | 运行环境：`hosted`（默认）或 `freestanding` |
| `--runtime <类型>` | 运行时：`none`、`alloc`、`gc` |
| `--panic <策略>` | panic 处理：`trap`、`loop` |
| `--entry <符号>` | 覆盖入口符号（默认 `Main`/`KMain`） |

#### 内置平台别名

| 别名 | 等效三元组 |
|------|-----------|
| `win86` | `i686-windows-msvc` |
| `win64` | `x86_64-windows-msvc` |
| `linux64` | `x86_64-linux-gnu` |
| `linux-arm64` | `aarch64-linux-gnu` |
| `bare64` | `x86_64-unknown-none` |
| `bare-arm64` | `aarch64-unknown-none` |

### 链接选项

| 选项 | 说明 |
|------|------|
| `-L, --lib-dir <目录>` | 添加库搜索目录 |
| `-l, --lib <库名>` | 链接指定库 |
| `--link-file <文件>` | 链接指定目标文件 |
| `--link-root <目录>` | 添加链接根目录 |
| `--link-arg <参数>` | 传递额外参数给链接器 |
| `--link-script <脚本>` | 指定链接脚本（`.ld`） |
| `--linker <后端>` | 链接器后端：`lld`（默认）、`zig`、`msvc` |
| `--linker-path <路径>` | 指定链接器可执行文件路径 |
| `--lto[=off\|full\|thin]` | 链接时优化（默认 `off`） |
| `--crt <auto\|dynamic\|static>` | CRT 选择（默认 `auto`） |
| `--no-crt` | 不链接 C 运行时 |
| `--no-default-libs` | 不链接默认库 |
| `--show-link` | 显示链接命令 |
| `--show-link-search` | 显示库搜索路径 |

### 项目与包

| 选项 | 说明 |
|------|------|
| `--project <文件\|目录>` | 从 `kinal.knproj` 或旧项目清单构建 |
| `--profile <名称>` | 选择 `kinal.knproj` 中的 Profile |
| `--pkg-root <目录>` | 添加包搜索根目录 |
| `--stdpkg-root <目录>` | 覆盖标准包根目录 |

当 `--project` 传入目录时，`kinal` 会优先查找 `kinal.knproj`。如果没有显式传 `--profile`，则使用项目文件中的 `DefaultProfile`。

### 前端选项

| 选项 | 说明 |
|------|------|
| `--no-module-discovery` | 禁用自动模块发现 |
| `--keep-temps` | 保留中间文件 |

### 诊断

| 选项 | 说明 |
|------|------|
| `-h, --help` | 显示帮助信息 |
| `--color <auto\|always\|never>` | 颜色输出 |
| `--lang <en\|zh>` | 诊断语言 |
| `--locale-file <文件>` | 自定义本地化文件 |
| `--Werror` | 将警告视为错误 |
| `--warn-level <级别>` | 警告级别（`0` 禁用警告） |

### 开发者选项

| 选项 | 说明 |
|------|------|
| `--trace` | 打印编译阶段追踪 |
| `--time` | 打印编译阶段耗时 |
| `--dump-locale-en <文件>` | 导出英文本地化模板 JSON（供翻译使用） |

---

## 输入文件类型

`kinal build` 支持多种输入格式，可从不同阶段接入编译管线：

| 扩展名 | 说明 |
|--------|------|
| `.kn` / `.kinal` / `.fx` | 完整的前端编译 |
| `.ll` / `.s` / `.asm` | 从中间表示继续编译 |
| `.o` / `.obj` | 从目标文件继续链接 |
| `.knc` | 字节码，使用 `kinal vm run` 或 `kinal vm pack` |

---

## kinal run

编译源文件为原生可执行文件并立即执行，执行完成后自动删除临时文件，适合快速迭代，体验类似 `go run`。

```
kinal run [选项] <文件.kn> [-- 程序参数...]
```

| 选项 | 说明 |
|------|------|
| `--no-module-discovery` | 禁用源文件/模块自动发现 |
| `--project <文件\|目录>` | 从 `kinal.knproj` 或旧项目清单运行 |
| `--profile <名称>` | 选择 `kinal.knproj` 中的 Profile |
| `--pkg-root <目录>` | 添加本地包根目录 |
| `--stdpkg-root <目录>` | 覆盖官方 stdpkg 根目录 |
| `--keep-temps` | 执行完成后保留临时可执行文件 |
| `-L, --lib-dir <目录>` | 添加库搜索目录 |
| `-l, --lib <库名>` | 链接指定库 |
| `--link-file <文件>` | 链接指定目标文件或归档文件 |
| `--link-root <目录>` | 添加链接根目录 |
| `--link-arg <参数>` | 传递额外参数给链接器 |
| `--lang <en\|zh>` | 诊断语言 |
| `--locale-file <文件>` | 自定义本地化文件 |
| `--color <auto\|always\|never>` | 诊断颜色模式 |
| `--Werror` | 将警告视为错误 |
| `--warn-level <n>` | 警告级别（0 禁用警告） |

`kinal run` 在后台将源文件通过 LLVM 后端编译为临时原生可执行文件，执行后自动清理。`--` 之后的所有参数原样传递给被执行的程序。

> 如需执行 `.knc` 字节码，请改用 `kinal vm run`。

```bash
# 快速运行
kinal run main.kn

# 使用项目的默认 Profile 运行
kinal run --project .

# 传递程序参数
kinal run main.kn -- arg1 arg2

# 保留临时可执行文件以便调试
kinal run --keep-temps main.kn
```

---

## 常用工作流

### 开发调试

```bash
# 快速编译并运行（原生，类 go run）
kinal run main.kn

# 查看 LLVM IR
kinal build main.kn --emit ir --keep-temps
```

### 生产构建

```bash
# 优化构建，启用 LTO
kinal build main.kn -o app --lto

# 静态库
kinal build mylib.kn --kind static -o libmylib.a

# 动态库
kinal build mylib.kn --kind shared -o libmylib.so
```

### 交叉编译

```bash
# 从 Linux 编译 Windows 可执行文件
kinal build --target win64 main.kn -o main.exe --linker zig

# 嵌入式裸机目标
kinal build --target bare-arm64 \
      --freestanding \
      --runtime none \
      --panic trap \
      main.kn -o firmware.elf
```

### 中文诊断

```bash
kinal build --lang zh main.kn
```

---

## VM / 包管理快速参考

```bash
# 字节码操作
kinal vm build main.kn -o main.knc
kinal vm build --project . --profile vm
kinal vm run main.kn                       # 或 main.knc
kinal vm pack main.kn -o myapp             # 独立可执行文件

# 包管理
kinal pkg build --manifest mylib/ -o mylib.klib
kinal pkg info mylib.klib
kinal pkg unpack mylib.klib -o output_dir/
```

> 详细选项参见 [kinal vm](vm.md) 和 [kinal pkg](packages.md)。

---

## 相关

- [kinal fmt](fmt.md) — 代码格式化
- [kinal vm](vm.md) — 字节码虚拟机操作
- [kinal pkg](packages.md) — 包管理
- [交叉编译](../advanced/cross-compilation.md) — 跨平台构建详解
- [链接](../advanced/linking.md) — 链接器配置
- [裸机开发](../advanced/freestanding.md) — 嵌入式/无操作系统环境
- [CLI 规范](cli-spec.md) — CLI 设计规范（开发者参考）
