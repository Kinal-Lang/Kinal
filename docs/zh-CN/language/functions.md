# 函数

函数声明结果类型、名称、参数和函数体。以下示例默认相互独立；一个文件只能选择一个程序入口。

## 声明与入口

```kinal
Function int Add(int left, int right)
{
    Return left + right;
}
```

无结果使用 `void`，提前退出 void 函数使用 `Return;`。非 void 函数应在所需路径上返回值；void 函数不能返回一个值。支持普通顶层函数与 `Static Function` 工具函数；类中的 Static 用于区分类方法与实例方法。

Hosted 入口选择一个签名，不要同时放入同一文件：

```kinal
Static Function int Main()
{
    Return 0;
}
```

另一种形式是 `Static Function int Main(string[] args)`。`kinal run file.kn -- first second` 转发程序参数。Freestanding 通常使用 `KMain` 或显式入口符号。参见[编译器 CLI](../cli/compiler.md)。

## 默认值与命名参数

```kinal
Unit Docs.Arguments;
Get IO.Console;

Function int Add(int left = 1, int right = 2)
{
    Return left + right;
}

Static Function int Main()
{
    IO.Console.PrintLine(Add());                       // 3
    IO.Console.PrintLine(Add(10, default));            // 12
    IO.Console.PrintLine(Add(right = 5, left = 10));    // 15
    IO.Console.PrintLine(Add(left = default, right = 5)); // 6
    Return 0;
}
```

带默认值参数位于必填参数之后。省略参数或显式传 `default` 都使用参数声明的默认值，并不是忽略声明后强制传零；没有默认值的参数不能用 `default` 补齐。命名实参按声明名称绑定，未知名称、重复绑定、缺少必填项或类型不兼容会报错。

参数遵守普通值／引用语义。`Swap<T>(T a, T b)` 交换两个标量时只改变局部副本。需要修改调用方状态时，应使用明确的可变引用、返回更新值（例如 Package），或支持的低层指针 API。参见[内存与生命周期](../advanced/memory-management.md)。

## 泛型与递归

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

Function int Fibonacci(int n)
{
    If (n <= 1) { Return n; }
    Return Fibonacci(n - 1) + Fibonacci(n - 2);
}
```

显式指定泛型类型，如 `Identity<int>(42)`。实例化后的函数体必须对所选类型和输出后端有效。递归消耗运行时栈／存储，不代表无限递归或尾调用保证。参见[泛型](generics.md)。

## 匿名函数与闭包

```kinal
Unit Docs.Callables;
Get IO.Console;

Function IO.Type.Object.Function MakeCounter(int start)
{
    int count = start;
    Return Function int()
    {
        count++;
        Return count;
    };
}

Static Function int Main()
{
    Var add = Function int(int left, int right)
    {
        Return left + right;
    };
    IO.Console.PrintLine(add(3, 4)); // 7

    IO.Type.Object.Function counter = MakeCounter(10);
    IO.Console.PrintLine(counter()); // 11
    IO.Console.PrintLine(counter()); // 12
    Return 0;
}
```

函数字面量采用 `Function 结果类型(参数) { ... }`。捕获变量共享对应声明的托管存储，逃逸可调用对象可在创建函数返回后继续保留和更新状态。复制可调用引用仍共享状态。Block 对象使用不同控制流操作，参见[Block](blocks.md)。

导入 `IO.Console` 后，也可将 `IO.Console.PrintLine` 等函数引用存入 `IO.Type.Object.Function`。可调用值是托管 Kinal 对象，不会自动变成任意 C 函数指针；Unsafe 表示转换不能替代 C 委托适配 ABI。

## 显式重载

每个重载参与者都需要两个属性，并具有不同的参数类型签名：

```kinal
[Overload]
[SymbolName("DescribeInt")]
Static Function string Describe(int value) { Return "int"; }

[Overload]
[SymbolName("DescribeText")]
Static Function string Describe(string value) { Return "string"; }
```

调用仍使用 `Describe(...)`，`SymbolName` 标识编译后符号。仅返回类型不同不能区分重载；多个候选均可接收实参时会产生歧义诊断。当前泛型函数不能使用 `[Overload]`。类／接口中受支持的方法重载组也使用同样的显式属性。

同一 Unit 分布在多个文件时，顶层函数仍共享一个命名空间。普通函数在该 Unit 内只能定义一次。每个重载都需要不同的参数类型列表，并使用该 Unit 内尚未占用的编译后符号名。例如，在 `Unit Models` 中，`Box` 和 `Models.Box` 表示同一个参数类型。不同 Unit 可以各自定义同名函数。

## 多个返回值

```kinal
Unit Docs.Results;
Get IO.Console;

Function <int, string> Status()
{
    Return <200, "OK">;
}

Static Function int Main()
{
    Object.Package status<code, message> = Status();
    IO.Console.PrintLine(status.code);
    IO.Console.PrintLine(status.message);
    Return 0;
}
```

Package 别名属于声明，不属于类型。使用常量索引或声明别名访问；Package v1 不支持解构。参见[类型系统](types.md)。

## 安全、异步、Extern 与 Delegate

| 形式 | 用途 |
|------|------|
| `Safe Function` | 安全实现，可调用 Safe 和 Trusted 函数 |
| `Trusted Function` | 允许调用 Unsafe 操作的受审计桥接层 |
| `Unsafe Function` | 显式不安全实现，包括原始指针操作 |
| `Async Function` | 产生任务，声明的结果类型是 Await 后的值类型 |
| `Extern ... By C` | 由原生链接器绑定的外部符号 |
| `Delegate ... By C` | 运行时可绑定的 C ABI 可调用槽 |
| `Delegate ... By Kinal` | 运行时可绑定的 Kinal 可调用槽 |

Extern 和 C 委托签名使用显式 C ABI 编组。Delegate 不能是泛型或异步；Async 的返回包装和调用也不同于普通函数。参见[异步](async.md)、[安全](safety.md)、[FFI](ffi.md)。

Block 内局部声明参见[Block](blocks.md)。跨 Unit 函数需要先导入，再使用可见限定名或显式符号别名调用；普通 `Get` 不会将所有函数注入裸名称作用域。
