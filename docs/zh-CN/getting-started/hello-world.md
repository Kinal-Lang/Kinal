# Hello World

本文带你完成第一个 Kinal 程序——从编写到运行。

## 第一个程序

新建文件 `hello.kn`，输入：

```kinal
Unit App;
Get IO.Console;

Static Function int Main()
{
    IO.Console.PrintLine("Hello, Kinal!");
    Return 0;
}
```

## 逐行解析

```kinal
Unit App;
```
声明当前源文件所属的**单元（Unit）**，相当于其他语言的模块或命名空间。

```kinal
Get IO.Console;
```
导入标准库模块 `IO.Console`，之后可以使用 `IO.Console.PrintLine(...)` 向控制台输出。

```kinal
Static Function int Main()
{
    ...
}
```
程序入口函数。`Static` 表示这是一个静态函数（无需类实例），返回类型为 `int`，返回 `0` 表示成功。

```kinal
IO.Console.PrintLine("Hello, Kinal!");
```
向控制台输出一行文本。

```kinal
Return 0;
```
返回退出码 0。

## 编译与运行

### 方式一：编译为原生可执行文件

```bash
kinal build hello.kn -o hello
./hello         # Linux/macOS
hello.exe       # Windows
```

输出：
```
Hello, Kinal!
```

### 方式二：通过 KinalVM 直接运行（无需 LLVM）

```bash
kinal vm run hello.kn
```

编译器会将 `.kn` 编译成 `.knc` 字节码后立即由 KinalVM 执行。

### 方式三：仅生成字节码文件

```bash
kinal vm build hello.kn -o hello.knc
kinalvm hello.knc
```

## 带命令行参数的入口

`Main` 函数也可接受命令行参数：

```kinal
Unit App;
Get IO.Console;

Static Function int Main(string[] args)
{
    If (args.Length() > 0)
    {
        IO.Console.PrintLine("第一个参数: " + args[0]);
    }
    Return 0;
}
```

## 异步入口

若程序需要顶层 `Await`，可将 `Main` 声明为异步：

```kinal
Unit App;
Get IO.Console;
Get IO.Async;

Static Async Function int Main()


{
    Await IO.Async.Sleep(100);
    IO.Console.PrintLine("异步 Hello!");
    Return 0;
}
```

## 常见错误排查

| 错误信息 | 原因 | 解决 |
|---------|------|------|
| `Missing ';' after Unit` | Unit 行末忘记分号 | 加上 `;` |
| `Expected Semicolon` | 表达式语句末尾缺分号 | 加上 `;` |
| `kinalvm: command not found` | 未安装 KinalVM | 参见[安装指南](installation.md) |

## 下一步

- [项目结构](project-structure.md) — 多文件与 Package 组织
- [语言概览](../language/overview.md) — 深入了解 Kinal 的核心概念
