# 安装指南

本文介绍如何在各平台上安装 Kinal 编译器工具链。

## 系统要求

| 平台 | 架构 |
|------|------|
| Windows 10/11 | x86-64 / ARM64 |
| Linux (glibc 2.17+) | x86-64 / ARM64 |
| macOS 11+ | x86-64 / Apple Silicon |

> Kinal 编译器本身不依赖任何运行时，生成的二进制可在上述系统独立运行。

## 安装编译器

### 使用官方安装脚本

对于 macOS 和 Linux，推荐直接使用官方安装脚本：

```bash
curl -fsSL https://kinal.org/install.sh | bash
```

安装指定版本：

```bash
curl -fsSL https://kinal.org/install.sh | bash -s -- --version v0.6.0
```

脚本默认会把工具链安装到 `~/.local/share/kinal`，并把启动脚本写到 `~/.local/bin`。

如果 `~/.local/bin` 还不在 `PATH` 里，可以手动加入：

```bash
export PATH="$HOME/.local/bin:$PATH"
```

### 从预编译包安装

如果你更想手动安装，也可以从 Release 页面下载对应平台的压缩包并解压：

```bash
# Linux / macOS
tar -xf kinal-*.tar.gz
cd kinal
chmod +x kinal
sudo mv kinal /usr/local/bin/

# Windows
# 解压 zip 后，将整个 kinal 目录加入 PATH
```

### 验证安装

```bash
kinal --help
```

看到帮助信息即表示安装成功。

## 安装 KinalVM（可选）

KinalVM 是 Kinal 的字节码虚拟机，用于运行 `.knc` 文件，或通过 `kinal vm run` 将 `.kn` 源文件先编译为临时 `.knc` 后执行。

`kinalvm` 可执行文件通常随编译器一同分发。将其放入同一目录或 `PATH` 中：

```bash
# 验证
kinalvm --help
```

## 目录结构（典型安装）

```
kinal/
├── kinal          # 主编译器
├── kinalvm        # 字节码虚拟机
└── locales/       # 诊断信息本地化文件
    └── zh-CN.json
```

## 诊断语言设置

Kinal 支持中文诊断信息：

```bash
kinal --lang zh hello.kn
```

或在 `zh-CN.json` 文件存在时，`--lang zh` 会自动加载中文 locale 文件。

## 开发工具支持

### VS Code

安装 **Kinal Language Server** 相关扩展（如有）以获得语法高亮、代码补全和实时诊断。

LSP 服务器位于 `apps/kinal-lsp/`，可本地构建后在编辑器中配置。

## 从源码构建

若需从源码构建 Kinal：

**依赖：**
- CMake 3.20+
- LLVM 14+（需 `llvm-config` 在 PATH 中）
- C 编译器（MSVC / GCC / Clang）
- Python 3（用于 `infra/` 构建脚本）

```bash
# 使用命令行构建
python x.py [dev|dist]

# 或使用 构建 GUI 来快速完善环境与构建
python x.py gui
```

具体 preset 名称可查看 `CMakePresets.json`。

## 下一步

- [Hello World](hello-world.md) — 编写并运行第一个 Kinal 程序
- [项目结构](project-structure.md) — 了解如何组织源文件
