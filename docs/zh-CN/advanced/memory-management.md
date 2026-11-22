# 内存与生命周期

本文连接语言值语义、hosted GC、原始内存辅助函数与 C 互操作，面向 C 编译器。Native 内存布局与 VM 值表示不能当作同一套 ABI 使用。

## 值、引用与共享

| 值 | 赋值行为 |
|----|----------|
| 数值、bool、char、enum | 复制值 |
| Struct、Package | 复制值／字段，内部引用仍指向共享存储 |
| 类实例 | 复制对象引用，字段仍共享 |
| 数组 | 复制指向元素存储的描述符／引用；普通赋值不是深拷贝 |
| list、dict、set | 复制集合引用，修改共享 |
| string | 复制不可变文本引用，文本操作可产生新字符串 |

定长数组长度属于静态类型，但不会让描述符赋值自动变成逐元素复制或扩容：

```kinal
Unit Docs.ArrayStorage;
Get IO.Console;

Static Function int Main()
{
    int[3] first = { 1, 2 }; // 声明字面量创建三个元素
    int[5] alias = first;   // 共享已有存储，实际长度仍为三
    alias[0] = 9;
    IO.Console.PrintLine(first[0]);       // 9
    IO.Console.PrintLine(alias.Length()); // 3

    int[] extended = first.Add(4); // 新的外层存储，长度四
    extended[0] = 7;
    IO.Console.PrintLine(first[0]); // 仍为 9
    Return 0;
}
```

复制 Struct、Package、数组时，内部引用仍可共享。`.Add` 复制外层数组存储，不递归克隆内部数组／对象。用较短字面量初始化定长声明会补零；后续赋值、参数传递和返回数组通常不会补齐。

## Hosted 垃圾回收

自举版的托管运行时为每次 GC 分配保留 16 字节块头。进程内存还包含存活对象、系统分配器的对齐开销、标记和排序用的临时空间、已加载代码和栈。GC 回收对象后，系统分配器可以保留相应的页以供复用。Windows 基准分别测量编译器、生成的原生程序和 KinalVM 的峰值工作集与峰值提交量。[内存基准和结果](../../../benchmarks/README.md#compiler-and-program-memory-on-windows)

自举运行时回收大堆时，会借用标记栈给已分配块的地址排序。已注册的根可以保存块内地址，例如数组中某个元素的指针。回收器会找到包含该地址的整块分配，并将其保留。[回收耗时和内存实测](../../../benchmarks/README.md#hosted-collector-address-index)

Hosted Native 产物使用随工具链提供的托管运行时。C 实现跟踪托管分配，保守扫描已注册根／内部指针及活动原生栈。全局／静态根、逃逸闭包存储通过运行时根机制保活。

```kinal
Unit Docs.Collection;
Get IO.Console;
Get IO.GC;

Static Function int Main()
{
    string[] values = { "kept", "alive" };
    IO.GC.Collect();
    IO.Console.PrintLine(values[0]); // kept
    Return 0;
}
```

`IO.GC.Collect()` 请求回收，不是确定性析构。保守可达性可能使值在最后一次源码使用后继续保留。离开作用域不保证立即释放内存，分配次数也不能保证准确回收时机。

GC 管理 Kinal 托管存储，并不管理所有操作系统资源。动态库、文件／句柄、套接字、服务器和外部分配应通过各自所属 API 关闭或释放，不能假定对象被回收就会自动释放任意外部资源。

## KinalVM 垃圾回收

KinalVM 在累积分配一定量的字节码程序内存后自动回收。显式调用 `IO.GC.Collect()` 时，回收会在调用返回前完成。KinalVM 从活动栈帧、全局变量、异常和宿主持有的值开始追踪，沿对象字段、数组、闭包环境和内部指针找到存活数据。失去外部引用的环也可以回收。编写 VM 宿主扩展时，若要跨回调保存字节码值，可用 `int token = ctx.Heap.Pin(value)` 保留，并在持有方关闭时调用 `ctx.Heap.Unpin(token)`。Windows 桌面运行器以此保留已打开窗口对应的句柄。

## 闭包与捕获变量

逃逸可调用对象可保留其捕获存储。同一声明的捕获共享一个 cell，兄弟可调用对象能够看到彼此的修改。循环体内声明的变量每次迭代拥有独立声明实例；循环外声明的变量仍共享。复制可调用引用不会克隆捕获状态。

捕获引用会保留所引用对象；捕获 Struct 不代表递归深拷贝其中的引用。语法及控制流参见[函数](../language/functions.md)与[Block](../language/blocks.md)。

## `IO.Memory` 原始操作

`IO.Memory` 属于 `IO.Core`，由自包含字节循环实现，导入它不要求 hosted GC。使用 `Get IO.Memory;`。辅助函数声明为 Trusted，可被 Safe 代码调用；原始指针操作本身仍受安全模型约束。

| 函数 | 签名 | 行为 |
|------|------|------|
| `Copy` | `void Copy(byte* destination, byte* source, usize count)` | 正向逐字节复制；源／目标为空时不操作 |
| `Set` | `void Set(byte* destination, byte value, usize count)` | 填充字节；目标为空时不操作 |
| `Zero` | `void Zero(byte* destination, usize count)` | 填零；目标为空时不操作 |
| `Compare` | `int Compare(byte* left, byte* right, usize count)` | 字节字典序比较，返回 -1／0／1 |

`Compare` 在指针相同或 count 为零时返回零；其余情况下 null 排在非 null 前。这些函数不分配内存，也不验证真实缓冲区大小。`Copy` 是正向复制，不是保证重叠安全的 `memmove`，应使用不重叠区域。count 单位是字节，不是带类型的元素数。

## FFI 所有权与生命周期

- 精确匹配 C 声明。Kinal `int` 内部为 i64，跨 C ABI 按 C `int`／i32 编组；固定宽度和指针宽度类型使用各自 ABI。
- 传递字符串／数组指针通常是调用期间借用。外部 API 长期保留指针时必须设计所有权与保活；借用指针本身不是通用 GC 保活契约。
- 外部堆内存通过匹配的外部分配器释放，不能交给无关的 Kinal 分配路径。
- 数组参数退化为数据指针。外部指针按 `T[]` 返回时 Kinal 长度未知、为零；可索引不代表访问一定有效。
- C 仍可能调用回调时，应保留回调及捕获对象。库最终卸载后不能继续调用其中的符号／委托。
- Struct／Package 按值 C ABI、任意可变参数不在支持边界内，应编写显式 C 包装。

声明、委托、链接元数据和数组编组参见[FFI](../language/ffi.md)。安全标注控制哪些操作允许出现，不会验证外部地址、缓冲区长度、所有权或回调生命周期。

## Freestanding 配置

| 配置 | 目标侧责任 |
|------|------------|
| `none` | 无通用托管堆，使用支持的核心操作及目标自有存储 |
| `alloc` | 提供分配器／字符串 hook 和 frame／root hook；不回收的实现可将根 hook 设为空操作 |
| `gc` | 提供分配器、回收器和根／生命周期 hook |

`--runtime alloc`、`gc` 不会自动附带嵌入式分配器或 GC。`libs/runtime/include/kn/freestanding.h` 声明核心 hook ABI，包括 `__kn_gc_alloc`、frame／root、字符串拼接和回收。使用集合、异常和异步时还需要对应附加 hook。

Freestanding 目标文件编译成功，不能证明最终可执行镜像具有完整运行时符号、启动初始化、内存映射和平台服务。实际目标镜像的链接与运行需要单独验证。

## 相关文档

- [类型系统](../language/types.md)
- [变量](../language/variables.md)
- [安全等级](../language/safety.md)
- [集合](../stdlib/collections.md)
- [后端支持](backend-support.md)
- [Freestanding](freestanding.md)
