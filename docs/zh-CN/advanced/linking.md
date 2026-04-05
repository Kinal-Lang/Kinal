# 链接配置

本文介绍 Kinal 的链接器集成，包括链接器后端选择、库依赖、LTO、静态/动态库构建等。

---

## 链接器后端

Kinal 支持三种链接器后端：

| 后端 | 选项 | 说明 |
|------|------|------|
| **lld** | `--linker lld`（默认） | LLVM 内置链接器，速度最快，跨平台 |
| **zig** | `--linker zig` | 使用 zig 工具链，最佳交叉编译支持 |
| **msvc** | `--linker msvc` | Windows MSVC 链接器，需安装 Visual Studio |

```bash
# 默认（lld）
kinal build main.kn -o app

# zig 链接器（推荐用于交叉编译）
kinal build main.kn -o app --linker zig

# 指定链接器路径
kinal build main.kn -o app --linker-path /usr/local/bin/ld.lld
```

---

## 库依赖

### 链接系统库

```bash
# 链接 pthread 和 math 库
kinal build main.kn -o app -l pthread -l m

# 添加库搜索目录
kinal build main.kn -o app -L /usr/local/lib -l mylib
```

### 链接目标文件

```bash
# 链接已编译的目标文件
kinal build main.kn -o app --link-file mylib.o --link-file utils.o

# 指定链接根目录（搜索 .o 和 .a 文件）
kinal build main.kn -o app --link-root ./build/lib
```

### 链接 `.klib`

```bash
kinal build main.kn -o app --link-file mylib.klib
```

---

## 输出类型

| 类型 | 选项 | 说明 |
|------|------|------|
| 可执行文件 | `--kind exe`（默认） | 系统可执行文件 |
| 静态库 | `--kind static` | `.a` / `.lib` 静态库 |
| 动态库 | `--kind shared` | `.so` / `.dll` 动态库 |

```bash
# 静态库
kinal build mylib.kn --kind static -o libmylib.a

# 动态库
kinal build mylib.kn --kind shared -o libmylib.so
# Windows:
kinal build mylib.kn --kind shared -o mylib.dll
```

---

## 链接时优化（LTO）

LTO 允许链接器跨编译单元进行优化，通常可显著减小二进制体积并提升运行速度：

```bash
kinal build main.kn -o app --lto
```

LTO 会：
- 对所有目标文件进行全程序分析
- 删除未使用的函数和数据（死代码消除）
- 内联跨模块函数调用
- 更积极地优化跨模块引用

> LTO 会增加编译时间，建议仅在发布构建中启用。

---

## C 运行时（CRT）

### 默认行为

默认情况下，Kinal 会链接平台标准 CRT：
- **Linux**：`crt0.o`、`crti.o`、`crtn.o`（来自 glibc/musl）
- **Windows**：MSVC CRT 或 MinGW CRT

### 禁用 CRT

```bash
# 不链接 CRT（裸机/嵌入式）
kinal build main.kn -o app --no-crt

# 不链接默认库
kinal build main.kn -o app --no-default-libs

# 指定自定义 CRT
kinal build main.kn -o app --crt mycrt.o
```

---

## 链接脚本

嵌入式目标通常需要自定义内存布局：

```bash
kinal build main.kn -o firmware.elf \
      --target bare-arm64 \
      --link-script memory_layout.ld \
      --no-crt
```

---

## 额外链接参数

通过 `--link-arg` 可向链接器传递任意参数：

```bash
# 设置入口点
kinal build main.kn -o app --link-arg -e --link-arg _start

# 添加 rpath
kinal build main.kn -o app --link-arg -rpath --link-arg /usr/local/lib
```

---

## 调试链接

查看实际执行的链接命令：

```bash
kinal build main.kn -o app --show-link
```

输出示例：

```
link: ld.lld -o app main.o -lc -lm --dynamic-linker /lib64/ld-linux-x86-64.so.2
```

---

## 典型构建配置

### 开发构建

```bash
kinal build main.kn -o app
```

### 发布构建（带 LTO）

```bash
kinal build main.kn -o app --lto
```

### 静态链接发布（无外部依赖）

```bash
kinal build main.kn -o app \
      --linker zig \
      --lto
```

> zig 链接器会自动进行静态 musl libc 链接（Linux），生成完全静态的可执行文件。

### 嵌入式固件

```bash
kinal build main.kn \
      --target bare-arm64 \
      --freestanding \
      --runtime none \
      --panic trap \
      --no-crt \
      --link-script board.ld \
      -o firmware.elf
```

---

## 相关

- [编译器 CLI](../cli/compiler.md) — 完整链接选项
- [交叉编译](cross-compilation.md) — 跨平台链接
- [裸机开发](freestanding.md) — 嵌入式链接配置
- [FFI](../language/ffi.md) — 链接外部 C 库
