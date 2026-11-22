# kinal vm — 字节码虚拟机

`kinal vm` 是用于构建、检查、执行和打包 KinalVM 字节码的子命令组。

## 字节码兼容性

编译器生成 KNC2 格式版本 4。四字节 magic、两个小端 u16（格式版本、标志）之后是编译器版本字符串（u32 字节长度加 UTF-8 内容）。后续内容沿用版本 3 的布局。新版 KinalVM 也能读取版本 2、3。版本 2 的整数常量为有符号 32 位，版本 3、4 为有符号 64 位。

生成 KNC 的编译器版本与当前运行器预期的编译器版本不同时，程序仍可运行。成功时保持静默。读出生成方版本后，加载或执行期间的错误会附带双方的版本号。客体正常返回非零整数也属于成功执行。版本 2、3 没有生成方版本记录。只支持旧格式的运行器需要升级才能读取版本 4。

## 在 Native 宿主中编译并执行源码

执行器对象由 Native 程序创建。当前标准运行器中的 KNC 程序尚不支持嵌套创建执行器。

引用可选包 `IO.Kinal.VM.Executor` 后，可以在进程内编译一段 Kinal 源码，直接从对象中取出字节码。

```kinal
Unit Host;
Get VM By IO.Kinal.VM.Executor;

Static Function int Main()
{
    VM.Executor compiler = New VM.Executor();
    compiler.Compile("Static Function int Main() { Return 42; }");
    byte[] image = compiler.Knc;
    VM.Executor runner = New VM.Executor();
    runner.LoadBytes(image);
    Return runner.Run();
}
```

用 `LoadFile(path)` 从文件加载，也可以自己取得字节数组后调用 `LoadBytes(data)`。`Run(arguments)` 每次启动新的客体并返回其入口函数的结果。宿主可以捕获未处理的客体异常 `IO.Error`。编译或加载失败时，对象保留上一个有效的 KNC。

如果动态源码引用标准库，将所需标准库包放在宿主旁的 `stdpkg`，或设置 `compiler.StandardLibraryRoot`。当安装目录只读时，可将 `compiler.CacheDirectory` 设为可写的包解压目录。源码与 KNC 都可以全程留在内存。普通 Native 应用仅在引用执行器时才引入该模块。

## 数组存储

KinalVM 为整数、字符、浮点和布尔数组分配连续的清零缓冲区，布尔值每项占一个字节。字符串和对象数组在首次写入时按页分配，每页 64 个元素。未写入的字符串元素读作 `""`，对象元素读作 `null`。

把数组赋给另一个变量后，两者共享元素存储。`array.Add(value)` 返回独立数组，之后可以分别替换元素。两个数组中指向同一对象的引用仍然指向该对象。已有 `.knc` 换用新版运行器即可生效，独立 VM 可执行文件需要重新打包。

在原生宿主中嵌入 `IO.Kinal.VM` 时，可用 `ExecutionContext.ArrayLengthOf`、`ArrayGetValue` 和 `ArraySetValue` 访问客体数组。直接读写过 `HeapArray` 旧存储字段的宿主，升级包并重新构建时需改用这些方法。

## 对象句柄

VM 按页保存对象、数组、指针和闭包捕获单元的句柄，每页 256 项，按需分配。对象存活期间，经过客体 GC 后仍可使用原句柄。对象回收后，VM 可以将句柄分给其他对象。

原生宿主可以用 `ExecutionContext.ObjectAt`、`ArrayAt`、`PointerAt`、`CellAt` 查询单个槽位，空槽和越界均返回 `null`。要让客体对象在回调期间保持存活，用 `ctx.Heap.Pin(value)` 固定它，结束后调用 `ctx.Heap.Unpin(token)`。

`Objects`、`Arrays`、`Pointers`、`Cells` 属性保留了可变数组接口。首次读取某个属性时，VM 会把对应的整张表转换为长度精确的连续数组。按索引修改会反映到 VM 中，直到追加元素时生成新的数组。逐项查询推荐使用上述单槽方法，以保留分页存储。

---

## 子命令

| 子命令 | 用途 |
|--------|------|
| `kinal vm build` | 将 `.kn` 源文件编译为 `.knc` 字节码 |
| `kinal vm run` | 执行 `.kn` 源文件或 `.knc` 字节码文件 |
| `kinal vm disasm` | 输出已有 `.knc` 文件的可读反汇编 |
| `kinal vm pack` | 将源文件或 `.knc` 打包为独立可执行文件（vm-exe） |

---

## kinal vm build

编译源文件为 `.knc` KinalVM 字节码。

自举编译器在每次构建时为已绑定的函数和全局变量建立索引，并按需扩容字节码缓冲区。含有大量事件处理函数、普通函数或全局初始化表达式的项目可以更快完成编译。

```bash
kinal vm build <文件.kn> [-o <输出.knc>]
```

| 选项 | 说明 |
|------|------|
| `-o, --output <文件>` | 输出文件路径（默认同名 `.knc`） |
| `--project <文件\|目录>` | 从 `kinal.knproj` 或旧项目清单构建 |
| `--profile <名称>` | 选择 `kinal.knproj` 中的 Profile |
| `--no-superloop` | 禁用 KNC 超级循环融合 |
| `--superloop` | 启用 KNC 超级循环融合（默认） |
| `--listing <文件>` | 写出可读的编译期指令清单（建议扩展名 `.knasm`） |
| `--no-module-discovery` | 禁用自动模块发现 |
| `--pkg-root <目录>` | 添加本地包根目录 |
| `--stdpkg-root <目录>` | 添加官方 stdpkg 根目录覆盖 |

```bash
# 编译为字节码
kinal vm build main.kn -o main.knc

# 从项目文件构建
kinal vm build --project . --profile vm

# 禁用超级循环
kinal vm build --no-superloop main.kn

# 生成保留源码局部变量名的可读清单
kinal vm build main.kn -o main.knc --listing main.knasm
```

---

## kinal vm run

执行 `.kn` 源文件或 `.knc` 字节码文件。

```bash
kinal vm run <文件.kn|文件.knc> [-- 程序参数...]
```

| 选项 | 说明 |
|------|------|
| `--project <文件\|目录>` | 从 `kinal.knproj` 或旧项目清单运行 |
| `--profile <名称>` | 选择 `kinal.knproj` 中的 Profile |
| `--superloop` | 启用超级循环（默认） |
| `--no-superloop` | 禁用超级循环 |
| `--vm-path <路径>` | 指定 KinalVM 可执行文件路径 |
| `--no-module-discovery` | 编译源码输入时禁用自动模块发现 |
| `--keep-temps` | 保留自动生成的临时 `.knc` 文件 |

```bash
# 直接执行源码
kinal vm run main.kn

# 运行项目的默认 Profile
kinal vm run --project .

# 传递参数
kinal vm run main.knc -- one "two words"
kinalvm main.knc one "two words"
```

以上两条命令都会向 `Main(string[] args)` 传入 `["one", "two words"]`。省略参数时得到空数组。打包后的可执行文件直接接收参数，包括 `--version` 和 `--disasm`。如需向旧版自举编译器生成的字节码传参，请用新版编译器重新构建，并使用配套的 VM 运行。

---

## kinal vm disasm

从已有 `.knc` 文件输出可读指令清单。

```bash
kinal vm disasm [--vm-path <文件>] <文件.knc>
```

该形式只依赖二进制，包含函数名、常量值、内建函数名、寄存器和跳转目标。KNC 二进制当前不保存局部变量名；如需源码局部变量注释，应在编译时使用 `kinal vm build --listing <文件>`。

独立 VM 也提供相同的二进制反汇编入口：

```bash
kinalvm --disasm main.knc
```

---

## kinal vm pack

将字节码与 KinalVM 打包为独立可执行文件（vm-exe），部署时仍需满足所选 VM 模板的主机依赖。

已有 `.knc` 用新版 KinalVM 启动即可获得解释器的性能改进。独立 EXE 自带解释器，需要用新版 VM 重新打包原有字节码。

```bash
kinal vm pack <文件.kn|文件.knc> [-o <输出>]
```

| 选项 | 说明 |
|------|------|
| `-o, --output <文件>` | 输出可执行文件路径 |
| `--project <文件\|目录>` | 从 `kinal.knproj` 或旧项目清单打包 |
| `--profile <名称>` | 选择 `kinal.knproj` 中的 Profile |
| `--vm-path <路径>` | 指定用于打包的 KinalVM 路径 |
| `--target <三元组>` | 目标平台（影响打包的 VM 二进制） |
| `--superloop` | 启用超级循环融合 |
| `--no-superloop` | 禁用超级循环融合 |
| `--no-module-discovery` | 对源码输入禁用自动发现 |
| `--pkg-root <目录>` | 添加本地包根目录 |
| `--stdpkg-root <目录>` | 添加官方 stdpkg 根目录覆盖 |

```bash
# 从源码打包
kinal vm pack main.kn -o myapp

# 从项目文件打包
kinal vm pack --project . --profile vm -o myapp

# 从已有字节码打包
kinal vm pack main.knc -o myapp

# 运行打包后的程序
./myapp arg1 arg2
```

---

## kinal run vs kinal vm

| | `kinal run` | `kinal vm build` + `kinal vm run` |
|---|---|---|
| 编译后端 | LLVM 原生代码 | KinalVM 字节码 |
| 步骤 | 一步到位 | 分步操作 |
| 中间文件 | 自动清理（临时 exe） | 保留 `.knc` |
| 用途 | 快速运行 `.kn` 源文件 | 生产字节码、分发、跨平台 |

`kinal run main.kn` 在后台执行：编译 → 原生 exe → 运行 → 删除临时 exe。  
`kinal vm run` 则走 VM 路线：如果输入是源码，会先编译成临时 `.knc`，再交给 KinalVM 执行。

```bash
# 等效操作（使用 VM 路径）：
kinal vm build main.kn -o main.knc
kinal vm run main.knc
```

---

## `.knc` 字节码格式

`.knc` 是 Kinal 的原生字节码格式：

- **可移植字节码** — 要求 VM 和运行时／平台操作兼容；编译期目标常量保持固化
- **结构化二进制** — 包含常量、函数、类型和指令表，体积取决于程序
- **直接执行** — 运行已有字节码时跳过源码编译和原生链接

---

## Superloop 循环融合

Superloop 是针对已识别整数循环模式的字节码优化，包括适用的空计数循环、求和和数组填充／求和。它用专用循环指令替代重复解释分派，保留所支持模式的循环语义。

融合默认开启。编译源码时用 no-superloop 对照普通 lowering，并通过 listing／反汇编查看所选指令。该选项不提供 Async／Await、不创建任务调度器、不切换线程模型，也不会使 VM 变成 freestanding。当前 C KNC 拒绝通用 Await 和 IO.Async.Run。

---

## 独立 kinalvm 工具

`kinalvm` 是独立的 KinalVM 可执行文件，可直接运行或检查 `.knc`：

```bash
kinalvm <文件.knc>
kinalvm --disasm <文件.knc>
```

---

## KinalVM 与原生编译对比

| 特性 | KinalVM (`.knc`) | 原生编译 |
|------|-----------------|---------|
| 执行方式 | 字节码解释器，没有已声明的 JIT | 编译后的机器码 |
| 启动 | 加载字节码进入 VM | 加载已经链接好的可执行文件 |
| 可移植性 | 要求兼容 VM／运行时 | 按目标 OS／ABI 重新构建 |
| 部署要求 | 匹配 VM 或打包模板及主机依赖 | 目标运行时／系统库／包依赖 |
| 适用场景 | 开发、脚本、嵌入 | 生产部署 |

---

## 相关

- [CLI 总览](compiler.md) — 所有子命令概览
- [异步编程](../language/async.md) — 超级循环与协程
- [裸机开发](../advanced/freestanding.md) — VM 在受限环境中的使用
- [IO.Runtime 常量](../advanced/runtime-environment-constants.md) — 在源码中判断当前执行后端
- [IO.Version 版本常量](../advanced/version-constants.md) — 在源码中读取编译器与 VM 版本信息
- [CLI 规范](cli-spec.md) — CLI 设计规范（开发者参考）

语言限制、数值行为和部署要求参见[后端支持](../advanced/backend-support.md)。独立 VM 支持 `--version`、`--disasm`；命令帮助使用 `kinal vm --help`。
