# 语言概览

本文从宏观角度介绍 Kinal 的设计理念与核心特性，帮助你快速建立对这门语言的整体认知。

## 设计哲学

Kinal 的核心设计目标：

1. **清晰可读** — 关键字首字母大写，语义明确，减少歧义
2. **安全分层** — 内存操作、指针访问有明确的安全边界
3. **零成本抽象** — 面向对象特性编译为高效原生代码
4. **双后端灵活性** — 同一代码既可编译为原生二进制，也可在 VM 中解释运行
5. **模块化标准库** — IO.* 命名空间提供标准功能，可按需导入

## 语言风格速览

```kinal
Unit App.Demo;

Get IO.Console;

// 接口定义
Interface IShape
{
    Function float Area();
}

// 类定义，实现接口
Class Circle By IShape
{
    Private float radius;

    Public Constructor(float radius)
    {
        This.radius = radius;
    }

    Public Function float Area()
    {
        Return 3.14159 * This.radius * This.radius;
    }
}

// 泛型函数
Function T Max<T>(T a, T b)
{
    Return a > b ? a : b;  // 注：Kinal 没有三元运算符，此仅为示意
}

// 异步函数
Async Function string FetchName()
{
    Await IO.Async.Sleep(10);
    Return "Kinal";
}

// 程序入口
Static Function int Main()
{
    IShape s = New Circle(5.0);
    IO.Console.PrintLine([string](s.Area()));

    int bigger = Max<int>(3, 7);
    IO.Console.PrintLine(bigger);

    Return 0;
}
```

## 核心特性一览

### 类型系统

- **原始类型**：`bool`、`byte`、`char`、`int`、`float`、`string`、`any`
- **精确数值**：`i8`/`u8` 到 `i64`/`u64`、`f16`/`f32`/`f64`/`f128`、`isize`/`usize`
- **引用类型**：`class` 实例、内置集合（`list`、`dict`、`set`）
- **值类型**：`struct`（值语义结构体）、`enum`（枚举）
- **指针**：`byte*`、`T*`（仅在 `Unsafe` 上下文中使用）
- **数组**：`int[3]`（定长）、`int[]`（动态）
- **any 类型**：可持有任意值，类似装箱机制

### 模块系统

```kinal
Unit MyApp;                      // 声明当前文件的单元名
Get IO.Console;                  // 打开整个模块
Get IO.Console;       // 以别名导入模块
Get PrintLine By IO.Console.PrintLine; // 导入单个符号
```

### 安全级别

```kinal
Safe Function void SafeOp() { ... }       // 只能调用 Safe 函数
Trusted Function void BridgeOp() { ... }  // 可调用 Unsafe 函数
Unsafe Function void DangerOp() { ... }   // 可进行指针操作
```

详见 [安全等级](safety.md)。

### 面向对象

```kinal
Class Animal
{
    Public Virtual Function string Sound() { Return "..."; }
}

Class Dog By Animal
{
    Public Override Function string Sound() { Return "Woof!"; }
}
```

- 单继承（`By`）
- 接口多实现
- `Virtual` / `Override` / `Abstract` / `Sealed`
- 访问控制：`Public` / `Private` / `Protected` / `Internal`

### 异步/并发

```kinal
Static Async Function int Main()
{
    int result = Await Compute();
    Return result;
}
```

- `Async Function` 返回 `IO.Async.Task`
- `Await` 等待任务完成
- `IO.Async.Run(task)` 同步等待异步任务结果

### Block（代码块对象）

Kinal 独特的 Block 机制——将一段代码封装为对象，支持延迟执行和标签跳转：

```kinal
IO.Type.Object.Block flow = Block FlowControl </
    IO.Console.PrintLine("Step A");
    Record StepB
    IO.Console.PrintLine("Step B");
    Record StepC
    IO.Console.PrintLine("Step C");
/>;

flow.Run();                        // 执行全部
flow.RunUntil(flow.StepB);        // 执行到 StepB 停止
flow.JumpAndRunUntil(flow.StepB, flow.StepC); // 从 StepB 跑到 StepC
```

详见 [Block](blocks.md)。

### Meta（元数据注解）

```kinal
Meta Version(string text)
{
    On @function;
    Keep @compile;
}

[Version("1.0")]
Function void MyFunc() { ... }
```

- 可定义自定义属性
- 支持 `@source`、`@compile`、`@runtime` 三种生命周期
- 运行时可通过 `IO.Meta` 查询

### FFI（C 互操作）

```kinal
Extern Function int printf(string fmt) By C;

Trusted Function void Demo()
{
    printf("Hello from C!\n");
}
```

详见 [FFI](ffi.md)。

## 编译流程概览

```
.kn 源文件
    ↓ 词法分析（Lexer）
Token 流
    ↓ 语法分析（Parser）
AST（抽象语法树）
    ↓ 语义分析（Sema）
类型化 AST
    ↓ 代码生成（Codegen via LLVM）
LLVM IR
    ↓ LLVM 优化 + 机器码生成
目标文件 .obj / .o
    ↓ 链接（LLD / MSVC / zig）
可执行文件 / 库
```

或走字节码路径：
```
.kn 源文件 → 类型化 AST → KNC 字节码（.knc）→ KinalVM 执行
```

详见 [编译流程详解](../advanced/compilation-pipeline.md)。

## 关键字一览

| 类别 | 关键字 |
|------|--------|
| 声明 | `Unit`, `Get`, `By` |
| 类型声明 | `Class`, `Interface`, `Struct`, `Enum`, `Meta` |
| 函数 | `Function`, `Extern`, `Delegate`, `Return` |
| 访问控制 | `Public`, `Private`, `Protected`, `Internal` |
| 修饰符 | `Static`, `Virtual`, `Override`, `Abstract`, `Sealed` |
| 构造 | `Constructor`, `New`, `This`, `Base` |
| 安全 | `Safe`, `Trusted`, `Unsafe` |
| 异步 | `Async`, `Await` |
| 控制流 | `If`, `Else`, `ElseIf`, `While`, `For`, `Switch`, `Case`, `Break`, `Continue` |
| 异常 | `Try`, `Catch`, `Throw` |
| 变量 | `Var`, `Const` |
| Block | `Block`, `Record`, `Jump` |
| 字面值 | `true`, `false`, `null`, `default` |
| 类型判断 | `Is` |
| 导入 | `Get`, `By` |

## 下一步

- [类型系统](types.md)
- [变量与常量](variables.md)
- [函数](functions.md)
