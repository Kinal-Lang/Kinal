# Native 与 KinalVM 支持范围

本文说明 C 编译器的 Native 和 KNC 后端。表中单独标明需要自举编译器或特定平台 VM 的功能。

## 选择执行路径

| 路径 | 构建／执行 | 所需组件 |
|------|------------|----------|
| Hosted 原生 | `kinal build`、`kinal run` | 编译器、LLVM／链接工具、目标运行时及包内原生资产 |
| 从源码生成 KNC | `kinal vm build`、`kinal vm run source.kn` | 编译器前端／KNC 发射器及匹配 VM |
| 已有字节码 | `kinalvm program.knc`、`kinal vm run program.knc` | 兼容 VM；后者还经过编译器 CLI |
| 打包 VM 可执行文件 | `kinal vm pack` | 兼容 VM 模板、字节码；仍受目标主机依赖约束 |
| Freestanding 原生 | `kinal build --env freestanding` | 目标链接器／启动代码；Alloc／GC 还需自定义运行时 hook |

C 编译器可执行文件本身链接 LLVM，即使输出 KNC 也需要保留相应文件。单独用 `kinalvm` 执行已有 `.knc` 可跳过源码编译和原生链接，不表示编译器能够脱离 LLVM 文件安装。应保留完整发行目录。

KinalVM 当前解释执行字节码，不应描述为 JIT。启动耗时和吞吐依赖实际程序及环境；原生链接发生在构建时，不会在每次运行已构建程序时重做。

Windows x64 自举工具链使用 ThinLTO 构建附带的解释器。已有的 `.knc` 可以直接用更新后的 `kinalvm.exe` 运行。独立 VM 可执行文件需要重新打包才能内置新版解释器。

## 语言与数值行为

| 特性 | Native | KNC／VM |
|------|--------|---------|
| 基本控制流、函数、字符串、数组 | 支持 | 在发射器／运行时边界内支持 |
| IO.UI 2.0 窗体 | Windows DX11 窗口 | 使用自举编译器和更新后的 IO.UI 2.0 包构建。Windows 桌面 VM 通过字节码执行窗体、事件、自定义控件和 3D 投影。 |
| `IO.Math` | 支持 | 两种编译器均支持整个公开 API，包括三角函数、舍入、浮点分类和 `Hypot`。使用配套的 IO.Core 包和 VM。 |
| `IO.Time` | 支持 | 两种编译器都支持计时、休眠、UTC 日期快照和格式化。 |
| `IO.Text` 标量操作 | 支持 | 配套 VM 支持 Length、IsEmpty、Contains、StartsWith、EndsWith、IndexOf、LastIndexOf、Count、Trim、Slice、Replace、ToUpper 和 ToLower。位置和长度均按 UTF-8 字节计算。 |
| `IO.Text.Builder` | 支持 | 两种编译器均支持长度和容量查询、预留空间、追加、清空复用及字符串快照。 |
| 路径与目录 | 完整的 `IO.Path` 和 `IO.Directory` API | 支持全部路径与目录操作，包括递归枚举文件。 |
| Async／Await、Task Run | 原生任务 lowering | 当前不支持通用 Await 发射及 Run 内建调用 |
| 集合与 JSON | list、dict、set、IO.Json | 支持 14 个列表操作、10 个字典操作和 8 个集操作。可解析及格式化嵌套集合与标量根值，也可捕获解析错误。 |
| 泛型函数 | 由共享 Sema 实例化 | 同一前端实例化，生成函数体仍须满足 VM 支持范围 |
| 闭包与可变捕获 | 托管捕获存储 | VM cell，支持常用闭包形式 |
| 类、结构体、Package | 原生布局 | VM 表示；部分方法／转换路径仍有限制 |
| 通过接口引用调用 | 原生分派 | 支持方法和属性访问器，可调用继承的实现，也能按签名区分重载。 |
| 整数常量 | 64 位语言整数 | KNC v3、v4 使用 64 位常量，无符号操作携带位宽 |
| `f32` | LLVM binary32 运算 | binary64 寄存器配合显式 binary32 舍入／转换指令 |
| `float`、`f64` | binary64 | binary64 |
| `f16`、`f128` | LLVM 类型；依赖目标 lowering／辅助函数 | 浮点寄存器仍是 binary64，不具备通用半精度／四倍精度对等性 |
| 动态 `any` 转标量 | 按标签转换 | 两种发射器都支持按运行时标签转换已装箱的整数、浮点数、布尔值、字符和字符串。 |
| 用户 C ABI 调用和委托 | 显式 ABI 编组 | 任意外部调用目标不属于可移植 KNC 能力 |

`--emit check` 在 Sema 后停止，不能证明原生发射／链接、KNC 发射、VM 运行或包内原生库可用。语义检查成功后仍可能出现后端专用的 `Unsupported KNC Feature`。

### 已知表示差异

下表对照两个后端的默认值、数组长度和反射结果。

| 表达式／声明 | Native | VM |
|-------------|--------|----|
| `string text;` 后调用 `text.IsNull()` | `true` | `false`，表示为空字符串 |
| 定长数组 `values.TypeOf()` | 含静态长度，如 `int[3]` | 同样保留静态长度 |
| 定长数组 `values.TypeName()` | 去长度的名字，如 `int[]` | 运行时标签名 `array` |
| 默认结构体中的 `int[3] Cells`，包括局部变量、全局变量、静态字段、对象字段和结构体数组元素 | 托管环境下为 3 | 3 |
| 托管环境下声明 `int[2][4] grid;` 时的长度 | 4 行，每行 2 个元素 | 4 行，每行 2 个元素 |
| 执行 `int[2][4] grid = default;` 或 `grid = default;` 后的形状 | 托管环境下新建四行，每行两个元素 | 新建四行，每行两个元素 |
| 声明 `<int[2], int> pair;` 后第一项的长度 | 托管环境下为 2 | 2 |

建议显式写 `string text = "";` 或其他所需值，不依赖隐式引用默认值。需要静态类型拼写时使用 `TypeOf()`；运行时显示名不适合作为跨后端序列化或类型身份格式。参见[类型系统](../language/types.md)。

## 标准库与原生依赖

KinalVM 会复用函数帧，并跟踪仍可访问的对象、数组、指针和被闭包捕获的变量。分配量达到阈值会触发自动回收，也可调用 `IO.GC.Collect()` 立即回收。存活引用的句柄保持有效，失去引用的数组会释放其基本类型缓冲区。

字符串数组和装箱值数组在写入元素时按需分配存储。较短的尾页占 4 或 16 个槽，完整页占 64 个槽。`Array.Add` 生成的新数组可以和原数组共享已写入的页。随后修改任一数组时，VM 只复制受影响的共享页，两边的元素可独立修改。

当前注册表为每个 Native 内建函数规定实现归属。KNC 暴露更小的稳定 builtin-ID 集合；保留 ID 不代表存在 VM handler。不支持的内建函数应在字节码编译时报错，而非产生运行时 `Unregistered builtin`。

一个包可以同时包含普通 Kinal 源码、运行时叶子函数及目标原生对象，因此：

- 导入成功不能证明包内所有函数都兼容 VM。
- 原生 UI、HTTP、动态库等主机 API 需要各自文档中的目标资产，不能把任意 C 库直接复制进字节码。
- 纯源码辅助函数在传递调用均受支持时可用于两个后端。
- 原生程序仍可能依赖系统库、CRT、OpenSSL、包内动态库或运行时文件，分发前应检查目标与包要求。
- 字节码可移植性要求 VM 版本、操作及运行时／平台假设兼容；编译期 `IO.Target` 值已经固化在产物中。

结合[标准库概览](../stdlib/overview.md)与各包的平台／依赖说明使用本文。

## KNC 格式兼容

一个函数可以使用 0 至 254 号寄存器。编译清单中的 `registers=` 显示函数帧的寄存器数量。连续多个 `If` 判断可以复用条件表达式及 `Return`、`Throw` 表达式使用的临时寄存器。

当前写入器生成格式 **3**，魔数仍为 `KNC2`。v3 整数常量使用小端有符号 64 位条目，当前加载器仍接受 v2 的有符号 32 位条目。只支持 v2 的旧 VM 会拒绝 v3，部署时应配套更新编译器与 VM。

`FloatToF32`（137）、`IntToF32`（138）负责 binary32 转换。自举编译器用 `CallRange`（139）传递超过四个物理参数的调用。C 编译器对具有四个或更多显式实参的构造函数、普通实例方法、虚方法和接口方法也使用此指令，首个参数寄存器保存接收者。两种编译器用 `UIntToFloat`（140）将 64 位无符号整数直接转换为 binary64。分发时需要支持相应指令的 VM。组件版本（`kinal`、`kinalvm`）与字节码格式版本是独立契约。

`--listing` 在编译时输出含局部名称的清单；对已有 `.knc` 反汇编无法恢复未存入字节码的源码局部名称。参见[VM CLI](../cli/vm.md)。

## 条件代码与验证

使用 `IO.Runtime.IsNative`／`IO.Runtime.IsVM` 选择输出后端路径；`IO.Target.*` 描述目标，`IO.Host.*` 描述运行编译器的主机。它们与 `--runtime none/alloc/gc` 内存配置是不同维度。

```bash
kinal build main.kn --emit check -o out/main.kcheck
kinal run main.kn
kinal vm run main.kn
kinal vm build main.kn --no-superloop -o out/main-no-superloop.knc
kinalvm out/main-no-superloop.knc
```

验证应用声称支持的路径，比较输出和退出行为，而非只检查编译成功。跨平台声明应在对应主机执行；生成外来目标 IR／对象是另一种更窄的验证。

## 相关文档

- [编译器 CLI](../cli/compiler.md)
- [项目文件](../cli/project-files.md)
- [内存与生命周期](memory-management.md)
- [运行时常量](runtime-environment-constants.md)
- [Freestanding](freestanding.md)
