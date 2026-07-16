# 项目结构

```
Kinal/
├── VERSION                  # Kinal 版本号（单一来源）
├── CMakeLists.txt           # 根 CMake 配置
├── x.py                     # 构建入口脚本
├── apps/
│   ├── kinal/               # Kinal 编译器（C）
│   │   ├── include/kn/      # 公共头文件（含 version.h）
│   │   └── src/             # 编译器源码
│   ├── kinal-lsp/           # LSP 语言服务器（C++）
│   │   └── server/src/
│   └── kinalvm/             # KinalVM 虚拟机（用 Kinal 语言编写）
│       ├── VERSION           # VM 版本号
│       └── src/
├── libs/
│   ├── runtime/             # 运行时库（C）
│   └── std/                 # 标准库包（stdpkg）
├── infra/
│   ├── assets/              # 资源文件（locales 等）
│   ├── cmake/               # CMake 模块
│   ├── scripts/             # Python 构建脚本
│   └── toolchains/          # CMake 工具链文件
├── tests/                   # 编译器测试用例
├── docs/                    # 用户文档
└── dev/                     # 开发者规范文档（本目录）
```

## 核心组件

- **Kinal 编译器** (`apps/kinal/`)：C 语言实现，使用 LLVM 后端生成本机代码或 VM 字节码
- **KinalVM** (`apps/kinalvm/`)：用 Kinal 语言自身编写的字节码虚拟机
- **LSP 服务器** (`apps/kinal-lsp/`)：C++ 实现，提供 IDE 语言支持
- **运行时** (`libs/runtime/`)：目标平台运行时库（C）
- **标准库** (`libs/std/`)：以 stdpkg 形式分发的标准库包

## 编译器流水线

```text
Lexer -> Parser AST -> Sema -> Typed HIR 切片（Call v1 + Binary v1）-> Native LLVM / KNC
```

HIR 边界正在增量落地。Sema 将每个直接调用和成员调用规范化为
`KnResolvedCall`，其中保存已经选定的 builtin、函数、delegate，或
静态/直接/虚方法/接口方法目标。Native 与 KNC 必须从该记录分派，不能再根据
面向 Parser 的名称和标志重新推断调用类型。

Typed Binary HIR v1 对每个 `EXPR_BINARY` 应用同一规则。Sema 会保存一个
后端无关的 `KnResolvedBinary` plan，其中包含：

- `kind`：数值算术、字符串拼接、指针算术、位运算、字符串/引用/标量相等、
  数值比较或逻辑短路 lowering
- `lhs_coercion` 与 `rhs_coercion`：已经选定的左右操作数类型
- `pointer_side`：指针算术中哪一侧是指针
- `is_unsigned` 与 `integer_bits`：有无符号语义及最终的 8/16/32/64 位宽；
  `isize`、`usize` 跟随目标指针宽度

Native LLVM 与 KNC 都直接消费该 plan。后端可以为已记录的 plan 选择机械指令，
但不能重复 Sema 的类型分类，也不能另选转换类型。Binary HIR v1 不定义 aggregate
或 `any` 值的相等比较；Sema 会在进入任一后端前拒绝这类表达式。

这还不是完整的 Typed HIR；其他表达式家族还没有后端无关的 resolved plan，仍依赖
typed AST 语义，后续会逐步迁入同一边界。`kinal build --emit check` 会输出确定性的
`kinal-call-hir-v1` 与 `kinal-binary-hir-v1` 结构计数，用于编译阶段差分测试。

## KNC 无符号整数 ABI

与有符号语义不同的无符号整数运算使用追加在现有 opcode 空间之后的稳定 KNC
opcode：

| ID | Opcode |
|---:|---|
| 130 | `UDivInt` |
| 131 | `URemInt` |
| 132 | `ULtInt` |
| 133 | `ULeInt` |
| 134 | `UGtInt` |
| 135 | `UGeInt` |
| 136 | `LShrInt` |

每条指令固定为五字节：`[opcode, dst, lhs, rhs, width]`。`width` 来自 Binary HIR
的 `integer_bits`，只能是 8、16、32 或 64。这些 ID 和位宽字节属于字节码 ABI，
不能重排或省略。

现有 KNC 条件、回边和 superloop 比较快捷路径只适用于有符号数。无符号比较会
跳过这些快捷路径，生成对应的无符号 opcode，并回退到通用的“表达式 + 分支”路径，
从而避免优化改变无符号排序语义。

## Builtin 注册边界

`KnBuiltinKind` 是编译器全局稳定的 builtin 身份空间，并以
`KN_BUILTIN_COUNT` 结尾。后端职责分组和稀疏的 KNC/VM ABI ID 映射集中在
`kn_std.c`；KNC 直接消费该映射，不再维护第二份 switch。VM ID 属于稳定 ABI，
尚未实现的保留 ID 必须保留空洞，不能重排。

Native lowering 先按 platform、collections、system、text、filesystem、dynamic
六类职责分派；collections 再按 string/dict/list/set/math/conversion 拆分。新增
builtin 时必须同时更新集中注册表和对应 Native helper。
`tests/check_builtin_registry.py` 会静态验证：

~~~text
全部 builtin kind == 恰好一个 Native lowering
KNC 可能生成的 builtin ID ⊆ VM handler ⊆ Bytecode.BuiltinId 声明
~~~

VM 尚不支持的 builtin 必须在 KNC 编译期失败，不能生成只会在运行期触发
`Unregistered builtin` 的字节码。

## 大函数边界

Sema 表达式分析通过单一职责 helper 承载叶子表达式、直接调用和成员 builtin
家族，使递归分派器保持较小。成员 builtin probe 必须返回显式的 boolean“已处理”
结果；unknown type 可以是合法的错误结果，不能拿来充当“未命中”哨兵。

Hosted runtime IR 由 `build_runtime_hosted` 协调，按固定顺序调用声明、console、
字符串、转换、time、entry 等 helper。`HostedPlatformApi` 只携带 console 与
entry 共同需要的少量 Win32 LLVM handle。拆分 helper 时必须保持 LLVM 声明和函数体
创建顺序，因为生成 IR 属于编译器差分测试的行为表面。

O0+ASan 构建使用 `-Wframe-larger-than=16384` 作为结构守卫；Sema 分派器、
builtin 分派器和 hosted runtime 协调器都必须保持在该阈值以下。
