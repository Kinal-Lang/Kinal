# Kinal 语言文档

> **Kinal** 是一门静态类型、面向对象的通用编译型语言，基于 LLVM 构建原生代码，并内置 KNC 字节码虚拟机（KinalVM）以双链路使用。设计目标是：语法清晰直观、安全等级分明、工具链完整。

---

## 文档目录

### 🚀 快速入门
| 文档 | 说明 |
|------|------|
| [安装指南](getting-started/installation.md) | 如何安装 Kinal 编译器和工具链 |
| [Hello World](getting-started/hello-world.md) | 编写并运行你的第一个 Kinal 程序 |
| [项目结构](getting-started/project-structure.md) | 单文件、多文件与 Package 的组织方式 |

### 📖 语言参考
| 文档 | 说明 |
|------|------|
| [语言概览](language/overview.md) | 关键设计理念与语言特色概述 |
| [类型系统](language/types.md) | 原始类型、数组、指针、类、枚举等 |
| [变量与常量](language/variables.md) | `Var`、`Const`、类型推断 |
| [运算符](language/operators.md) | 算术、逻辑、位运算、类型转换 |
| [控制流](language/control-flow.md) | `If`/`Else`、`While`、`For`、`Switch`、`Break`/`Continue` |
| [函数](language/functions.md) | 函数声明、参数默认值、命名参数、类型参数、匿名函数 |
| [类与继承](language/classes.md) | `Class`、构造函数、访问控制、继承、虚方法 |
| [接口](language/interfaces.md) | `Interface` 声明与实现 |
| [结构体与枚举](language/structs-enums.md) | `Struct`、`Enum` 及底层类型 |
| [模块系统](language/modules.md) | `Unit`、`Get`、别名与符号导入 |
| [泛型](language/generics.md) | 泛型函数与类型参数 |
| [安全等级](language/safety.md) | `Safe`、`Trusted`、`Unsafe` 三级安全模型 |
| [异步编程](language/async.md) | `Async`/`Await`、Task、`IO.Async` |
| [Block（代码块对象）](language/blocks.md) | Block 字面量、Record 标签、Jump 跳转 |
| [Meta（元数据注解）](language/meta.md) | 自定义属性、生命周期、运行时反射 |
| [FFI 外部函数接口](language/ffi.md) | `Extern`、`Delegate`、调用约定，与 C 互操作 |
| [字符串特性](language/strings.md) | 字符串拼接、格式化、原始字符串、字符数组 |
| [异常处理](language/exceptions.md) | `Try`/`Catch`/`Throw`、`IO.Error` |

### 📦 标准库
| 文档 | 说明 |
|------|------|
| [标准库概览](stdlib/overview.md) | IO.* 命名空间总览 |
| [IO.Console](stdlib/console.md) | 控制台输入输出 |
| [IO.Text](stdlib/text.md) | 字符串操作 |
| [IO.File / IO.Directory](stdlib/filesystem.md) | 文件与目录操作 |
| [IO.Path](stdlib/path.md) | 路径处理 |
| [IO.Collections](stdlib/collections.md) | list、dict、set |
| [IO.Async](stdlib/async.md) | 协程任务与进程管理 |
| [IO.Time](stdlib/time.md) | 时间与计时 |
| [IO.System](stdlib/system.md) | 系统调用、动态库 |
| [IO.Meta](stdlib/meta.md) | 运行时元数据查询 |

### 🔧 CLI 工具链
| 文档 | 说明 |
|------|------|
| [kinal 编译器](cli/compiler.md) | 完整编译器选项参考 |
| [KinalVM](cli/vm.md) | 字节码虚拟机与 `.knc` 格式 |
| [代码格式化](cli/fmt.md) | `kinal fmt` 使用说明 |
| [Package 系统](cli/packages.md) | `.knpkg.json`、`.klib` 与包管理 |

### ⚙️ 进阶主题
| 文档 | 说明 |
|------|------|
| [编译流程详解](advanced/compilation-pipeline.md) | 词法 → 解析 → 语义 → 代码生成 |
| [裸机与嵌入式](advanced/freestanding.md) | `--freestanding`、`--runtime`、无宿主环境 |
| [跨平台交叉编译](advanced/cross-compilation.md) | `--target`、目标三元组与平台别名 |
| [链接与 LTO](advanced/linking.md) | 链接器后端、LTO、静态/动态库 |
| [运行时环境常量提案](advanced/runtime-environment-constants.md) | 区分 KinalVM 与原生运行时的编译期常量设计 |

---

## Kinal 速览

```kinal
Unit Hello;

Get IO.Console;

Static Function int Main()
{
    IO.Console.PrintLine("Hello, Kinal!");
    Return 0;
}
```

**编译并运行：**
```bash
kinal build hello.kn -o hello
./hello
```

**使用虚拟机运行（无需编译到原生代码）：**
```bash
kinal vm run hello.kn
```

---

## 语言亮点

- **静态类型** — 所有变量在编译期确定类型，支持 `Var` 类型推断
- **三级安全模型** — `Safe` / `Trusted` / `Unsafe` 细粒度控制危险操作
- **原生 OOP** — 类、接口、虚方法、继承，语法直观
- **Block 对象** — 独特的"可暂停代码块"机制，支持标签跳转
- **完整异步** — `Async`/`Await` 一等公民，与同步代码无缝协作
- **双后端** — LLVM 原生编译 + 内置 KinalVM 字节码解释器
- **C 互操作** — `Extern` / `Delegate` 零开销调用 C 函数库
- **Meta 元数据** — 编译期或运行时可查询的自定义注解系统
