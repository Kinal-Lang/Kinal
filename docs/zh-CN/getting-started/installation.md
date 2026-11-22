# 安装指南

本文安装基于 C 实现的 Kinal 工具链。源码文档对应当前工作区，旧发行版可能缺少新 CLI 选项或字节码指令，应核对实际执行工具的版本。

## 平台与依赖

官方主机构建流程包含 Windows、Linux、macOS 的 x64 和 ARM64。选择与操作系统及 CPU 架构同时匹配的压缩包。操作系统／libc／SDK 兼容性取决于具体发行构建环境，不能仅根据平台名称推导最低系统版本。

保留完整解压目录。编译器需要 LLVM 库，原生构建还需要所选链接器、目标运行时和系统库，包也可能增加原生依赖。生成的原生程序不需要编译器本身，但仍可能依赖 CRT、系统库或包内库。参见[后端支持](../advanced/backend-support.md)。

## Linux 与 macOS 安装脚本

安装最新已发布版本：

```bash
curl -fsSL https://kinal.org/install.sh | bash
```

工具链默认安装在 `~/.local/share/kinal`，启动脚本默认写入 `~/.local/bin`。必要时将后者加入 shell 的 PATH：

```bash
export PATH="$HOME/.local/bin:$PATH"
```

要持久生效，将 export 写入所用 shell 的启动配置。安装器选项作用于安装器进程，不会自动修改已经运行的父 shell。

选择具体发行版前，可查看可用版本和本机安装状态：

```bash
curl -fsSL https://kinal.org/install.sh | bash -s -- --list-versions
curl -fsSL https://kinal.org/install.sh | bash -s -- --list-installed
curl -fsSL https://kinal.org/install.sh | bash -s -- --current
curl -fsSL https://kinal.org/install.sh | bash -s -- --status
```

| 选项 | 用途 |
|------|------|
| `--version <tag>` | 安装指定的已发布标签 |
| `--update` | 更新到最新或显式选择的标签 |
| `--set-default <tag>` | 切换到一个已安装版本 |
| `--remove-version <tag>` | 移除一个已安装版本 |
| `--link <dir>` | 注册本地完整工具链目录，默认本地名称 dev |
| `--root-dir <dir>` | 覆盖工具链安装根目录 |
| `--install-dir <dir>` | 覆盖启动脚本目录 |
| `--dry-run` | 显示计划，不修改安装文件 |
| `--help` | 查看安装器完整选项 |

使用 `--list-versions` 中实际存在的标签；源码 `VERSION` 值不能证明同名发行版已经发布。安装器默认启用 SHA-256 验证，除非显式关闭。

### 可选 Zig 工具链

Linux／macOS hosted 链接在可用时优先选用 Zig。安装器提供可选 Zig 扩展，交互环境可能询问是否安装。无人值守时可显式选择：

```bash
curl -fsSL https://kinal.org/install.sh | bash -s -- --with-zig
# 另一种选择：跳过可选工具链
curl -fsSL https://kinal.org/install.sh | bash -s -- --without-zig
```

`--with-extension zig`、`--without-extension zig` 是等价形式。对应版本的启动脚本会导出已安装 Zig 路径。未安装 Zig 时驱动可回退 LLD，但成功与否仍依赖平台库和链接配置。参见[链接](../advanced/linking.md)。

### 代理与自定义目录

脚本支持标准 `HTTPS_PROXY`、`HTTP_PROXY`、`ALL_PROXY`、`NO_PROXY`。`--proxy` 控制安装器下载；若获取脚本本身也需要代理，应同时为 curl 设置环境：

```bash
HTTPS_PROXY=http://127.0.0.1:7890 \
curl -fsSL https://kinal.org/install.sh | \
bash -s -- --proxy http://127.0.0.1:7890
```

需要私有 GitHub API 访问时，优先使用安装器支持的 `GITHUB_TOKEN` 环境变量，避免把 token 写入 shell 历史、项目文件或文档示例。

使用默认目录卸载：

```bash
curl -fsSL https://kinal.org/install.sh | bash -s -- --uninstall
```

自定义安装目录时，管理／卸载应传回相同的 `--root-dir`、`--install-dir`。仅移除一个版本使用独立的 `--remove-version` 操作。

## 手工安装，包括 Windows

1. 从 [Kinal Releases](https://github.com/Kinal-Lang/Kinal/releases) 下载匹配资产。
2. 将整个压缩包解压到固定目录。
3. 将该目录或提供的启动脚本目录加入 PATH。
4. 打开新终端，验证实际选择的工具。

不要只把 `kinal`／`kinal.exe` 移到其他目录。LLVM 库、链接工具、标准包和运行时是安装的一部分。移动工具链时应保留相对结构；Unix 下还应保留可执行权限。

典型发行目录包含：

```text
kinal/
├── kinal[.exe]
├── kinalvm[.exe]
├── kinal-lsp-server[.exe]
├── VERSION
├── locales/
├── llvm/
├── linker/
├── runtime/
├── stdpkg/
└── docs/
```

LLVM 共享库等主机相关文件也可能直接位于可执行文件旁。具体名称取决于主机发行包。

## 验证安装

```bash
kinal --version --verbose
kinal --help
kinalvm --version
```

独立 `kinalvm` 支持执行、版本查询和 `--disasm`，当前未实现 `--help`。编译器侧 VM 命令帮助使用 `kinal vm --help`。

创建 `hello.kn`：

```kinal
Unit Hello;
Get IO.Console;

Static Function int Main()
{
    IO.Console.PrintLine("Hello, Kinal!");
    Return 0;
}
```

```bash
kinal run hello.kn
kinal vm build hello.kn -o hello.knc
kinalvm hello.knc
```

两条路径均应输出 `Hello, Kinal!`。版本／帮助只验证工具启动；这个程序额外验证源码编译、原生链接及字节码执行。

## 诊断语言

```bash
kinal build --lang en hello.kn
kinal build --lang zh hello.kn
```

`--lang` 可放在子命令之前或之后。中文通常从编译器旁的 `locales/zh-CN.json` 加载，`--locale-file` 可选择自定义文件。移动可执行文件时保留语言包目录。

## VS Code

安装官方 [Kinal 扩展](https://marketplace.visualstudio.com/items?itemName=kinal.kinal)。编译／格式化使用 `kinal`，语言服务使用 `kinal-lsp-server`。PATH 无法发现它们时，将 `kinal.path`、`kinal.serverPath` 设置为完整可执行文件路径。

扩展支持 `.kn`、`.kinal`、`.knproj` 和 **Kinal: New Project File**。分析范围由最近的 `kinal.knproj` 驱动，参见[项目文件](../cli/project-files.md)及[扩展 README](https://github.com/Kinal-Lang/Kinal-VSCode)。

## 从源码构建 C 编译器

项目使用 Python 3.10+、CMake 3.20+、Ninja、主机 C／C++ 编译器和 LLVM／Clang。当前主机 workflow 选择 LLVM 21；应使用仓库的 bootstrap／配置，不应假定任意较旧 LLVM 都兼容。

```bash
python x.py fetch llvm-prebuilt
python x.py doctor
python x.py dev --release
python x.py test --release
```

暂存工具链位于 `out/stage/host-release/`；`python x.py dist` 生成发行包。系统 LLVM 选择及平台依赖参见[构建指南](../../../dev/zh-CN/build.md)。

## 排错

| 现象 | 处理 |
|------|------|
| 找不到命令 | 检查 PATH、打开新终端或使用完整路径 |
| 运行了意外版本 | Windows 用 `where.exe kinal`，Unix 用 `command -v kinal`；核对安装器当前版本 |
| 缺少 LLVM DLL／共享库 | 恢复完整且匹配的发行目录 |
| 编译器启动但原生链接失败 | 核对链接器、运行时、目标库及 `--show-link` |
| 新 `.knc` 被拒绝 | 随编译器更新 VM；当前写入格式为 3 |
| 可选 HTTP／UI 包失败 | 查看对应包的平台与原生依赖说明 |
| 编辑器与 CLI 不一致 | 检查工具路径、LSP 配置和项目源码范围 |

## 下一步

- [Hello World](hello-world.md)
- [项目结构](project-structure.md)
- [编译器 CLI](../cli/compiler.md)
