# 编译流程详解

本文介绍 Kinal 源文件从代码到可执行文件的完整编译管线。

---

## 总览

```
.kn 源文件
    │
    ▼
┌─────────────┐
│   词法分析   │  kn_lexer.c
│  (Lexer)    │  Token 流
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   语法分析   │  kn_parser.c + parser/*.inc
│  (Parser)   │  抽象语法树 (AST)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   语义分析   │  kn_sema.c
│  (Sema)     │  类型检查、名称解析、安全检查
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   代码生成   │  基于 LLVM
│  (Codegen)  │  LLVM IR
└──────┬──────┘
       │
       ├──────────────────────────────────┐
       │                                  │
       ▼                                  ▼
┌─────────────┐                   ┌──────────────┐
│  LLVM 后端  │                   │  KNC 字节码   │
│  优化+目标  │                   │  生成器       │
│  代码生成   │                   └──────┬───────┘
└──────┬──────┘                          │
       │                                 ▼
       ▼                          ┌──────────────┐
 目标文件 (.o)                    │  .knc 字节码  │
       │                          └──────┬───────┘
       ▼                                 │
┌─────────────┐                          ▼
│   链接器    │                   ┌──────────────┐
│ lld/zig/   │                   │  KinalVM     │
│ msvc       │                   │  执行        │
└──────┬──────┘                  └──────────────┘
       │
       ▼
 可执行文件/库
```

---

## 阶段详解

### 1. 词法分析（Lexer）

**源文件**：`apps/kinal/src/kn_lexer.c`

词法分析器将源文件字节流转换为 Token 序列。

主要工作：
- 识别关键字（`If`, `While`, `Function`, `Class` 等，均首字母大写）
- 识别标识符、数字字面量、字符串字面量
- 处理字符串前缀：`[r]"`、`[f]"`、`[c]"`
- 跳过注释（`//` 和 `/* */`）
- 追踪源码位置（行号、列号）用于错误报告

生成的 Token 类型定义于 `apps/kinal/include/kn/lexer.h`。

### 2. 语法分析（Parser）

**源文件**：`apps/kinal/src/kn_parser.c` + `apps/kinal/src/parser/*.inc`

解析器将 Token 序列转换为抽象语法树（AST）。

解析器按功能划分为多个片段（`.inc` 文件）：

| 文件 | 负责内容 |
|------|---------|
| `parse_decl.inc` | 顶级声明（Unit、Function、Class、Struct、Enum、Meta） |
| `parse_expr.inc` | 表达式（运算、函数调用、成员访问、类型转换） |
| `parse_stmt.inc` | 语句（If、While、For、Return、Try、Throw、Block） |
| `parse_type.inc` | 类型表达式（基本类型、数组、指针、泛型） |

关键语法特点：
- 类型转换语法 `[type](expr)` 由解析器特殊处理
- 字符串前缀 `[raw]"..."` / `[f]"..."` / `[c]"..."` 在词法阶段与解析阶段协同处理
- 泛型函数 `Function T Name<T>` 的类型参数在声明阶段解析

AST 节点类型定义于 `apps/kinal/include/kn/ast.h`。

### 3. 语义分析（Sema）

**源文件**：`apps/kinal/src/kn_sema.c`

语义分析器对 AST 进行类型检查和语义验证。

主要工作：
- **名称解析**：将每个标识符绑定到其声明
- **类型推导**：推断 `Var` 声明的类型
- **类型检查**：验证表达式、赋值、函数调用的类型兼容性
- **安全检查**：确保 `Unsafe` API 只在 `Trusted`/`Unsafe` 上下文中调用
- **模块解析**：处理 `Get` 导入，定位标准库和用户模块
- **内置类型注入**：`kn_builtins.c` 将 `IO.Type.Object.Class`、`IO.Error` 等注入符号表
- **标准库绑定**：`kn_std.c` 将所有标准库函数绑定为可调用的内置函数

### 4. 代码生成（Codegen）

语义验证通过后，代码生成器遍历带类型注解的 AST，生成后端代码。

Kinal 支持两种后端路径：

#### LLVM 路径

1. **LLVM IR 生成**：将 AST 转换为 LLVM 中间表示
2. **LLVM 优化**：应用 LLVM 优化 pass（`-O0`/`-O1`/`-O2`/`-O3`）
3. **目标代码生成**：LLVM 后端生成目标平台汇编/目标文件
4. **链接**：lld/zig/msvc 链接器生成最终可执行文件或库

#### KNC 路径

1. **字节码生成**：将 AST 转换为 `.knc` 字节码
2. **可选打包**：`kinal vm pack` 将字节码与 KinalVM 打包

---

## 内置类型注入

在语义分析开始前，`kn_builtins.c` 向符号表注入以下内置类型：

| 类型 | 说明 |
|------|------|
| `IO.Type.Object.Class` | 所有 `Class` 类型的根基类，提供 `ToString()` 等方法 |
| `IO.Type.Object.Function` | 函数对象类型（含 `Invoke` 和 `Env` 字段） |
| `IO.Type.Object.Block` | Block 对象类型（继承自 Function） |
| `IO.Error` | 标准异常类（`Title`, `Message`, `Inner`, `Trace` 字段） |
| `IO.Time.DateTime` | 时间结构体（由 `IO.Time.Now()` 返回） |
| `IO.Collection.list` | 内置 `list` 类型 |
| `IO.Collection.dict` | 内置 `dict` 类型 |
| `IO.Collection.set` | 内置 `set` 类型 |

---

## 标准库函数绑定

`kn_std.c` 包含所有标准库函数的注册表（`g_*_funcs[]` 数组）。每条记录包含：

- 函数全限定名（如 `IO.Console.PrintLine`）
- 参数类型签名
- 返回类型
- 安全级别要求（`SAFE`/`TRUSTED`/`UNSAFE`）
- 内置函数实现引用

编译器在语义分析阶段通过此注册表解析标准库调用。

---

## 错误报告

Kinal 编译器的诊断系统支持：

- 精确的行/列定位
- 彩色终端输出（`--color auto/always/never`）
- 中英文双语诊断（`--lang zh/en`）
- 自定义本地化文件（`--locale-file`）

---

## 编译缓存与中间文件

使用 `--keep-temps` 可保留编译过程中生成的中间文件：

```bash
kinal build main.kn --emit ir --keep-temps
```

保留的文件：
- `main.ll` — LLVM IR
- `main.s` — 汇编代码
- `main.o` — 目标文件

---

## 相关

- [编译器 CLI](../cli/compiler.md) — 完整编译选项
- [KinalVM](../cli/vm.md) — 字节码执行路径
- [交叉编译](cross-compilation.md) — 多平台编译
- [链接](linking.md) — 链接器配置
- [IO.Runtime 常量](runtime-environment-constants.md) — 在源码中区分原生与 VM 后端
