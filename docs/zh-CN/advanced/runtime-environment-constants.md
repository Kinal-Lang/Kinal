# IO.Runtime（运行后端常量）

`IO.Runtime.*` 是一组编译期常量，用于在源码中判断当前产物的执行后端是 LLVM 原生编译还是 KinalVM 字节码。

与 `IO.Target.*`、`IO.Host.*` 一样，这些符号在语义阶段直接折叠为字面值，不产生任何运行时开销，死分支也会被后端正常消除。

> **注意：** `IO.Runtime` 描述的是执行后端（native / vm），不是运行库模式。`--runtime none/alloc/gc` 控制的是内存分配策略，两者是独立的维度。

---

## 常量一览

### 后端标识

以下符号都返回当前后端对应的 `int` 值：

| 符号 | 说明 |
|------|------|
| `IO.Runtime` | 当前后端，与 `IO.Runtime.Current` 等价 |
| `IO.Runtime.Current` | 同上 |
| `IO.Runtime.Kind` | 同上 |
| `IO.Runtime.Kind.Current` | 同上 |

枚举值：

| 符号 | 值 | 说明 |
|------|----|----|
| `IO.Runtime.Unknown` | `0` | 未知 |
| `IO.Runtime.Native` | `1` | LLVM 原生编译 |
| `IO.Runtime.VM` | `2` | KinalVM 字节码 |

`IO.Runtime.Kind.Native` / `IO.Runtime.Kind.VM` / `IO.Runtime.Kind.Unknown` 是对应的带前缀别名，值相同。

### 字符串常量

| 符号 | 类型 | 说明 |
|------|------|------|
| `IO.Runtime.Name` | `string` | 后端名称，`"native"` 或 `"vm"` |

### 布尔别名

| 符号 | 类型 | 等价写法 |
|------|------|---------|
| `IO.Runtime.IsNative` | `bool` | `IO.Runtime == IO.Runtime.Native` |
| `IO.Runtime.IsVM` | `bool` | `IO.Runtime == IO.Runtime.VM` |

---

## 用法示例

```kinal
Unit MyLib;

Get IO.Console;

Static Function void PrintBackend()
{
    If (IO.Runtime.IsVM)
    {
        IO.Console.PrintLine("running on KinalVM");
    }
    Else
    {
        IO.Console.PrintLine("native build");
    }
}
```

整数比较同样有效：

```kinal
If (IO.Runtime == IO.Runtime.VM)
{
    // ...
}
```

---

## 各命令对应的值

| 命令 | `IO.Runtime` |
|------|-------------|
| `kinal build` | `Native` |
| `kinal run` | `Native` |
| `kinal vm build` | `VM` |
| `kinal vm pack` | `VM` |
| `kinal vm run <file.kn>` | `VM` |
| `--emit obj` / `--emit asm` / `--emit llvm-ir` | `Native` |
| `--emit knc` | `VM` |

`IO.Runtime.Name` 对应的字符串值：

| 命令 | `IO.Runtime.Name` |
|------|------------------|
| `kinal build` / `kinal run` | `"native"` |
| `kinal vm build` / `kinal vm run` / `--emit knc` | `"vm"` |

常量在编译期写入产物，之后不可更改。用 `kinal vm build` 生成的 `.knc` 中，不管后续以何种方式执行，`IO.Runtime` 始终是 `VM`。

---

## 与 IO.Target / IO.Host 的对比

三组常量都在语义阶段折叠，来源和用途不同：

| 常量族 | 描述的维度 | 典型用途 |
|--------|------------|----------|
| `IO.Target.*` | 目标平台（OS、架构、指针宽度…） | 平台差异编译、条件链接 |
| `IO.Host.*` | 宿主机（运行编译器的机器） | 交叉编译时区分宿主与目标 |
| `IO.Runtime.*` | 执行后端（native / vm） | 库代码针对 VM 提供回退实现 |

`IO.Runtime` 与 `--runtime none/alloc/gc` 无关，后者描述运行库的内存分配策略，不影响 `IO.Runtime` 的值。

---

## 相关

- [编译流程详解](compilation-pipeline.md) — 语义阶段常量折叠的时机
- [KinalVM](../cli/vm.md) — `kinal vm build`、`kinal vm run` 命令
- [裸机与嵌入式](freestanding.md) — `--runtime` 内存模式
- [交叉编译](cross-compilation.md) — `IO.Target.*` 与平台信息常量
