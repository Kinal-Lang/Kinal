# 链接配置

本页描述 C 编译器的 Native 链接。`--emit check`、`tokens`、`ast`、`ir`、`asm`、`obj` 均在最终链接前停止；KNC 使用独立的 [VM 路径](../cli/vm.md)。

## 后端与工具选择

| 选项 | 含义 |
|------|------|
| `--linker lld` | LLD 集成；Windows 和 freestanding 默认值 |
| `--linker zig` | Zig 驱动；hosted Linux/macOS 默认优先使用，找不到 Zig 时警告并回退 LLD |
| `--linker msvc` | MSVC 链接器，仅用于 Windows 目标 |
| `--linker-path <file>` | 覆盖所选工具的可执行文件 |
| `--show-link` | 输出实际执行的链接命令 |
| `--show-link-search` | 输出解析后的库搜索目录 |

选择后端不会安装工具。完整发行包包含工具与运行时资产；源码构建还需准备目标 SDK／库。跨工作目录调用时优先使用绝对 `--linker-path`。

```bash
kinal build main.kn -o app --show-link --show-link-search
kinal build main.kn -o app --linker zig
kinal build main.kn -o app --linker lld --linker-path /usr/local/bin/ld.lld
```

Windows LLD 使用 COFF 链接器接口，Linux 使用 ELF 约定。附加参数必须符合实际工具的接口，不能假定所有后端参数相同。

## 库、对象与搜索根

| 选项 | 含义 |
|------|------|
| `--link-file <file>` | 传入原生对象／归档／库文件 |
| `-L <dir>`／`--lib-dir <dir>` | 增加库搜索目录 |
| `-l <name>`／`--lib <name>` | 请求指定名称的库 |
| `--link-root <dir>` | 将存在的 `lib/<triple>`、`lib/<平台别名>`、`lib` 和根目录加入搜索路径 |
| `--link-arg <arg>` | 添加一个链接器参数；多个参数需重复选项 |

`--link-root` 不会递归链接找到的所有对象。仍需通过 `-l` 或 `--link-file` 选择输入。

```bash
kinal build main.kn --link-file build/helper.o -L vendor/lib -l helper -o app
kinal build main.kn --link-root vendor -l helper -o app
```

示例假定相应文件已存在。架构、对象格式、调用约定和 CRT 必须兼容。POSIX 上使用 C 数学函数可能还需 `-l m`。

`.klib` 保存 Kinal 源码、原生资产与包清单。应配置 `--pkg-root packages` 或项目包根，不要将 `.klib` 传给 `--link-file`。包的 FFI 元数据可以选择原生资产及系统库。参见[包管理](../cli/packages.md)。

## 输出种类

| 种类 | 常见输出 | 说明 |
|------|----------|------|
| `--kind exe` | `.exe` 或无扩展名程序 | 默认；hosted 入口为 `Static Function int Main(...)` |
| `--kind static` | `.lib`／`.a` | 原生对象归档；使用方仍需兼容依赖 |
| `--kind shared` | `.dll`／`.so`／`.dylib` | 动态库；仍需确定导出规则和 ABI |

```bash
kinal build library.kn --kind static -o library.lib
kinal build library.kn --kind shared -o library.dll
```

其他平台应使用相应文件名。扩展名本身不会选择输出种类。导出 C 函数时使用 [FFI](../language/ffi.md) 中支持的 `[Cdecl]`／`[SymbolName(...)]` 声明；Kinal 字符串、托管对象和数组描述符不会自动成为可移植 C 数据布局。

## LTO

`--lto` 等价于 full；显式值为 `--lto=off`（默认）、`--lto=full`、`--lto=thin`。

```bash
kinal build main.kn --lto=thin -o app
```

LTO 依赖兼容的编译器／链接器 bitcode 支持。已有机器码库仍是机器码输入；LTO 不会还原其源码，也不保证性能提升。应比较实际体积和工作负载性能。出现 bitcode 版本错误时核对发行包中的 LLVM 工具版本。

## Hosted CRT

`--crt auto|dynamic|static` 选择支持的 hosted CRT 策略，尤其是 Windows CRT 库族。它不接受文件名；自定义对象使用 `--link-file`。

`--no-default-libs` 禁止自动补入默认库。若程序使用控制台、分配、GC 等服务，就必须自行补全依赖。`--crt static` 不代表所有外部库或整个程序都是静态链接；仅选择 Zig 也不保证生成静态 musl 程序。

## Freestanding 链接

C 驱动仅在 freestanding 下接受 `--no-crt` 和 `--link-script`，hosted 构建会拒绝它们。需要准备启动环境、链接脚本和运行时钩子：

```bash
kinal build kernel.kn --target bare-arm64 \
    --freestanding --runtime none --panic trap \
    --no-crt --link-script board.ld -o kernel.elf --show-link
```

命令假定 `kernel.kn` 和 `board.ld` 已存在。默认语言入口为 `KMain`，编译器生成 `__kn_entry` 包装入口。调用前需按引导协议初始化栈和各节；完整源码及运行时要求见 [Freestanding](freestanding.md)。

## 故障定位

| 症状 | 核对内容 |
|------|----------|
| 找不到工具 | 完整发行目录、可执行文件路径和所选后端 |
| 未定义符号 | 实际链接命令、`-l`／文件、符号名称、导出、ABI 和传递依赖 |
| 对象不兼容 | 架构、ELF／COFF／Mach-O 格式、bitcode 与工具版本 |
| 缺少 CRT／SDK | 目标运行时资产及平台 SDK；选择目标不会安装 SDK |
| 启动时找不到动态库 | 运行时搜索路径及动态依赖部署，与链接搜索路径分别处理 |
| `.klib` 被拒绝 | 改为通过包发现加载 |

`--keep-temps` 可保留中间文件。先验证 `--emit obj`，再查看 `--show-link`，区分代码生成和链接问题。目标与路径规则见[跨平台编译](cross-compilation.md)和[项目文件](../cli/project-files.md)。
