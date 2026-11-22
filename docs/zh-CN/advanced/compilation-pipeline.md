# 编译流水线

本文描述 C 编译器。Native 与 KNC 共享前端分析；KNC 从有类型程序直接生成，不由 LLVM IR 翻译而来。

## 流水线与停止点

```text
CLI／项目／包源码选择
           |
      Lexer + Parser
           |
     Sema / typed AST
           |
   Call + Binary HIR 切片
         /       \
  Native LLVM    KNC writer
       |             |
  IR／汇编／对象     .knc
       |             |
   原生链接器     KinalVM 解释器
       |             |
 可执行文件／库   可选打包 VM 程序
```

| 命令／emit 模式 | 最后相关阶段 |
|----------------|--------------|
| `--emit tokens` | 词法 token 摘要 |
| `--emit ast` | Parser AST 摘要 |
| `--emit check` | 成功的 Sema／已解析计划摘要 |
| `--emit ir` | Native LLVM IR |
| `--emit asm`／`obj` | 原生目标发射 |
| build 默认 bin | 原生链接 |
| vm build | KNC 字节码写出 |
| run／vm run | 必要编译后执行 |

前端摘要用于确定性测试／检查，不是可执行代码或稳定包接口。检查成功不能验证后端支持、链接输入、运行依赖或程序行为。

## 源码选择与导入

驱动加载所选工程／Profile、收集候选源码，并在准备编译时解析 Unit／包导入。显式 SourceSet 约束本地成员；导入决定可见性，不赋予无限制文件系统参与权限。包通过清单／归档提供源码及原生资产。

入口文件发现会索引入口目录，再加入参与的同 Unit／已导入 Unit 文件。Unsafe-alias 文件仅限本地，不能作为工程 Unit 导入。参见[项目文件](../cli/project-files.md)、[包](../cli/packages.md)。

## Lexer 与 Parser

词法器识别源码字节、标识符、关键字、字面量、标点和注释，保留位置。支持 Unicode 标识符；源码字节列在 LSP UTF-16 协议边界转换。

Parser 构建声明／语句／表达式，并应用文件局部 unsafe-alias token 预处理。C Parser 的 include 实现片段为：

| apps/kinal/src/parser 下文件 | 责任 |
|-----------------------------|------|
| `kn_parser_decl.inc` | 声明、修饰符、属性与 Meta |
| `kn_parser_type_expr.inc` | 类型、表达式、调用、转换和字符串前缀 |
| `kn_parser_stmt.inc` | 语句及字面量转义辅助 |
| `kn_parser_float.inc` | 浮点字面量解析辅助 |

词法有效仍可能语法错误；编译期非活动分支也必须语法有效。声明级 AST 摘要不能证明名称／类型已解析。

## Sema 与已解析计划

Sema 检查类型、作用域、导入、默认值、泛型、重载、安全、入口等规则。参与程序由驱动提供，Sema 不会从注册表下载缺失包。

成功分析后，每个绑定的直接／成员调用具有 KnResolvedCall，每个二元表达式具有 KnResolvedBinary，保存操作分类、转换、指针侧、符号和目标整数宽度。Native 与 KNC 消费这些决定，不独立重新选择调用含义或数值转换。

这是**增量 HIR 切片**，不是覆盖全部表达式／语句的后端独立 IR。其他族仍依赖 typed AST。编译期条件只绑定活动语义分支；非活动平台语法可有意不绑定，不能计入未解析计划。

emit check 写出 format=kinal-sema-v1、hir_format=kinal-call-hir-v1、binary_hir_format=kinal-binary-hir-v1 和结构计数。更详细边界见[开发架构](../../../dev/zh-CN/architecture.md)。

## Native LLVM 与链接

原生 lowering 生成目标 LLVM 类型、函数、全局、运行时调用和 GC 根，再按请求输出 IR、汇编或对象。链接器组合对象、启动／CRT、运行时辅助和兼容包库。

当前 CLI 提供 LTO 控制；内部优化流程不代表公开支持 -O0／-O1／-O2／-O3。应使用已记录选项并查看实际链接命令，不假定优化或库策略。

外来目标对象／IR 不是主机可执行程序。Extern ABI 和运行时 hook 必须匹配目标，参见[链接](linking.md)、[跨编译](cross-compilation.md)、[FFI](../language/ffi.md)。

## KNC 与运行边界

KNC 写入器直接消费语义绑定程序。共享 Sema 成功后，仍可能因不支持的特性发射失败。Superloop 是已识别循环融合，不是任务调度器。

当前写入格式为 3，魔数仍 KNC2，加载器也读取 v2。VM 使用独立值／寄存器和解释器。打包将兼容 VM 模板与字节码组合，不会变成 LLVM 原生代码。参见[VM CLI](../cli/vm.md)、[后端支持](backend-support.md)。

## 内建与标准包

kn_builtins.c 注入编译器所属对象／错误／集合／任务支持类型；kn_std.c 管理内建身份、lowering 归属、配置可用性和稀疏 KNC ID，不等于实现所有公开标准库函数。

版本化 IO.Core／IO.Json／IO.Request／IO.UI／IO.Web 等包提供普通 Kinal 声明与窄原生桥接。C 编译器可以使用 Kinal 编写的标准包，这与编译器自举无关；应保持包 ABI／生命周期边界。

## 诊断与检查

诊断包含阶段／代码／位置并支持本地化。先解决最早相关阶段：未知 Unit 不能靠添加任意链接库解决，缺少原生符号通常也不是 Parser 错误。

```bash
kinal build main.kn --emit tokens -o out/main.tokens
kinal build main.kn --emit ast -o out/main.ast
kinal build main.kn --emit check -o out/main.kcheck
kinal build main.kn --emit ir -o out/main.ll --trace --time
kinal build main.kn -o out/main --show-link --show-link-search
```

keep-temps 保留适用中间文件。临时源码／包解压目录不是通用增量构建缓存，也不是公开序列化编译器状态。
