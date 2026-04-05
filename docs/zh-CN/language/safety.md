# 安全等级

Kinal 内置三级安全模型，对内存操作和危险函数调用进行显式分层。这是 Kinal 区别于很多语言的重要设计——你始终知道一段代码是否涉及潜在危险操作。

## 三个级别

| 级别 | 关键字 | 说明 |
|------|--------|------|
| 安全 | `Safe` | 只能调用带 `Safe` 修饰的函数；严格禁止指针操作 |
| 受信 | `Trusted` | 可以调用 `Safe` 和 `Unsafe` 函数；不直接操作裸指针 |
| 不安全 | `Unsafe` | 可以进行所有操作，包括裸指针和直接内存操作 |

未指定安全级别时，默认为不加限制（等同于非受控环境）。

## Safe

`Safe` 函数保证不会产生未定义行为：

```kinal
Public Safe Function int Add(int a, int b)
{
    Return a + b;
}

Safe Function string BuildGreeting(string name)
{
    Return "Hello, " + name + "!";
}
```

Safe 函数内部：
- 不能调用 `Trusted` 或 `Unsafe` 函数
- 不能使用裸指针（`byte*`、`int*` 等）
- 不能使用 `Extern` FFI 函数（除非该函数也标注为 `Safe`）

## Trusted

`Trusted` 是 Safe 与 Unsafe 之间的桥梁：

```kinal
Trusted Function int ReadTick()
{
    Return __kn_time_tick();  // 调用外部 C 函数
}
```

Trusted 函数：
- 可以调用 `Unsafe` 函数和 `Extern` 函数
- 本身不直接操作裸指针
- 可以被 `Safe` 函数调用（对调用方隐藏了危险细节）

典型用途：封装底层 C API，暴露给安全层使用。

## Unsafe

`Unsafe` 函数可以执行所有操作：

```kinal
Unsafe Function void Copy(byte* dst, byte* src, usize len)
{
    For (usize i = 0; i < len; i++)


    {
        dst[i] = src[i];
    }
}

Unsafe Function byte* Allocate(usize size)
{
    Return [byte*](IO.System.Malloc(size));
}
```

Unsafe 可以：
- 操作裸指针（读写、算术）
- 调用 `Extern` C 函数
- 进行类型指针强制转换

## Block 中的安全级别

`Block` 字面量也支持安全标注：

```kinal
Unsafe Block UnsafeRegion </
    byte* ptr = null;  // 允许指针操作
/>
```

## 安全级别传播规则

函数的安全级别决定了其可以调用什么：

```
Safe  → 只能调用 Safe
Trusted → 可以调用 Safe + Unsafe + Extern
Unsafe → 可以调用任何东西
```

这个规则在**编译期**强制执行，违反会产生编译错误。

## 为何需要安全分层？

### 不使用分层的问题

如果所有代码都默认 Unsafe，开发者很难分辨哪些代码涉及内存危险。

### Kinal 的方案

- **日常代码**写在 `Safe` 函数中，编译器保证安全性
- **底层适配层**写在 `Trusted` 函数中，封装 C 调用
- **极端底层代码**写在 `Unsafe` 函数中，明确标注风险

```kinal
// 安全层：业务逻辑
Safe Function void ProcessFile(string path)
{
    string content = IO.File.ReadAllText(path);  // IO.File 函数是 Safe 的
    // 处理内容...
}

// 受信层：FFI 封装
Extern Function void* fopen(string path, string mode) By C;

Trusted Function byte* OpenFileRaw(string path)
{
    Return [byte*](fopen(path, "rb"));  // 包装 C 函数
}

// 不安全层：裸操作
Unsafe Function int ReadByte(byte* fp)
{
    // 直接调用底层读取函数
}
```

## 实际应用场景

### 互操作封装（常见模式）

```kinal
// 1. 声明 C 函数（Extern 不设安全级别）
Extern Function int __kn_sys_file_read(byte* fp, byte* buf, int len) By C;

// 2. Trusted 桥接层
Trusted Function int FileRead(byte* fp, byte* buf, int len)
{
    Return __kn_sys_file_read(fp, buf, len);
}

// 3. Safe 业务层（调用者无需关心底层细节）
Safe Function string ReadChunk(byte* fp, int len)
{
    // 在 Safe 函数中通过 Trusted 间接调用 C
    Return BuildStringFromResult(FileRead(fp, null, len));
}
```

### FFI 危险函数调用约束

```kinal
Extern Function void* LoadLibrary(string path) By C;

// 直接调用 LoadLibrary 要求函数为 Trusted 或 Unsafe
Trusted Function byte* LoadLib(string path)
{
    Return [byte*](LoadLibrary(path));
}
```

## 程序入口的安全性

`Main` 函数可以是任意级别，但若 `Main` 标注为 `Unsafe` 会产生编译警告：

```kinal
// 推荐：
Static Safe Function int Main() { ... }
Static Function int Main() { ... }

// 会有警告：
Static Unsafe Function int Main() { ... }
```

## 下一步

- [FFI 外部函数接口](ffi.md)
- [异步编程](async.md)
- [裸机与嵌入式](../advanced/freestanding.md)
