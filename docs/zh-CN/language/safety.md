# 安全等级

安全标注控制编译器允许哪些操作和调用，是语言边界，不是对外部代码、原始地址、资源所有权或全部运行行为的正确性证明。

## 调用规则与默认值

| 调用方 | Safe 被调函数 | Trusted 被调函数 | Unsafe 被调函数 | 受检查的原始指针操作 |
|--------|---------------|------------------|-----------------|----------------------|
| `Safe` | 允许 | 允许 | 拒绝 | 拒绝 |
| `Trusted` | 允许 | 允许 | 允许 | 允许 |
| `Unsafe` | 允许 | 允许 | 允许 | 允许 |

普通未标注函数默认是 **Safe**，不是无限制执行。Extern 未通过支持的显式声明规则提供其他边界时默认 Unsafe。通过委托、可调用包装层调用时也需要合适的安全上下文。

`Trusted` 有意将允许不安全工作的实现暴露为 Safe 可调用接口，作者负责验证输入并维护接口契约。当前 C 编译器不会仅因为函数体标为 Trusted 就禁止其操作指针。

## Safe 代码

```kinal
Unit Docs.SafeArithmetic;
Get IO.Console;

Safe Function int Add(int left, int right)
{
    Return left + right;
}

Static Safe Function int Main()
{
    IO.Console.PrintLine(Add(3, 4));
    Return 0;
}
```

Safe 函数可使用普通语言值、托管分配、控制流及允许的库调用，不能直接调用 Unsafe 函数，也不能执行受保护的原始指针解引用／算术、指针与整数转换等操作。`ReadAllText` 等名称本身不决定安全性，应查看真实 API 声明。

## Trusted FFI 桥接

以下是 Native 示例；任意 C 调用不是可移植 KNC 能力：

```kinal
Unit Docs.TrustedBridge;
Get IO.Console;

Extern Unsafe Function int abs(int value) By C;

Trusted Function int Absolute(int value)
{
    Return abs(value);
}

Static Safe Function int Main()
{
    IO.Console.PrintLine(Absolute(-7)); // 7
    Return 0;
}
```

Safe 调用方信任 Absolute 的契约。传入无效外部指针、使用错误 ABI 位宽或过早卸载动态库，都不会因为包装器标为 Trusted 而自动修复。参见[FFI](ffi.md)。

## 原始指针实现

Trusted 和 Unsafe 实现均可执行原始内存操作。选择用于表达：调用方应信任受审计的安全接口，还是必须接受 Unsafe 调用边界。

```kinal
Unsafe Function byte ReadByte(byte* address)
{
    Return address[0];
}
```

调用方必须提供有效、可访问地址；指针语法不携带分配长度、生命周期或所有权证明。`IO.Memory` 提供 Trusted 字节复制／填充／比较函数，同样需要明确的缓冲区约束。参见[内存与生命周期](../advanced/memory-management.md)。

## Block 与入口

Block 字面量可带安全标注：

```kinal
Unsafe Block RawRegion </
    byte* address = null;
/>;
```

显式 Unsafe Block 是有意设置的边界，不代表整个外层文件已经受审计。`Unsafe Alias` 是另一项文件局部 token 预处理功能，不是普通不安全内存块的别名。参见[Block](blocks.md)、[模块](modules.md)。

Hosted Main 可标为 Safe、Trusted、Unsafe，但 Unsafe Main 会产生警告。该警告本身不是链接失败；使用 `--Werror` 时警告可以导致构建失败。只有少量操作需要时，优先缩小桥接函数或不安全区域范围。

## 标注不保证的事项

- Safe 代码仍可能抛异常、分配失败、耗尽资源，或调用实现错误的 Trusted 包装器。
- Trusted 不会自动清理路径、验证长度、确保传输安全或释放句柄。
- Unsafe 允许出现操作，不会使无效操作自动有效。
- Native 与 VM 共享前端安全检查，但后端／运行时支持是另一项要求。

## 相关文档

- [函数](functions.md)
- [FFI](ffi.md)
- [内存与生命周期](../advanced/memory-management.md)
- [后端支持](../advanced/backend-support.md)
