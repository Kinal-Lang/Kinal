# FFI — 外部函数接口

Kinal 通过 `Extern` 声明 C 函数，通过 `Delegate` 声明函数指针签名。当前 Native ABI 支持有明确范围；必须匹配数值宽度、调用约定、结构布局和资源所有权，不能将托管类型直接当作 C 布局。

## Extern 函数

`Extern` 声明一个外部 C 函数，调用约定由 `By` 指定：

```kinal
Extern Function int puts(string text) By C;
Extern Function void* malloc(usize size) By C;
Extern Function void  free(void* ptr)   By C;
```

### 调用约定

| 关键字 | 说明 |
|--------|------|
| `By C` | C 调用约定（`cdecl`）— 默认 |
| `By System` | 系统调用约定（Windows 上为 `stdcall`） |

### C ABI 标量映射

`By C`、`By System` 和 `Delegate ... By C` 使用独立的边界 ABI。Kinal 内部
表示保持不变，参数和返回值在调用边界完成封送。

| Kinal 类型 | C ABI 表示 |
|------------|------------|
| `bool` | 32 位 C 真假值 / Win32 `BOOL`；零为假，非零为真 |
| `int` | 32 位有符号 C `int` |
| `i8`…`i64`、`u8`…`u64` | 对应的固定宽度整数 |
| `isize`、`usize` | 目标指针宽度的有符号/无符号整数 |
| `f32`、`f64`、`float` | C `float`、C `double`、C `double` |
| `string` | 以零结尾的 `const char*` |
| `T*` | C 指针 |

Kinal `int` 在语言内部仍是 64 位值。传给 C 时截断为 32 位 C `int`，C
`int` 返回值再符号扩展为 Kinal `int`。外部声明并非真正的 C `int` 时，
应优先使用固定宽度类型。FFI v1 不支持数组、类、Package 和结构体按值
传递，应改用指针。C `_Bool`、C `long`、可变参数以及平台相关聚合 ABI
若与上表不同，应通过显式 C 包装层接入。

```kinal
Extern Function int MessageBoxA(byte* hWnd, string text, string caption, int type) By System;
```

### LinkName 属性

若 C 函数名与 Kinal 中的声明名不同，使用 `[LinkName]` 指定链接时的实际符号名：

```kinal
[LinkName("__kn_time_tick")]
Extern Function u64 GetTimeTick() By C;
```

### LinkLib 属性

自动链接指定库：

```kinal
[LinkLib("sqlite3")]
Extern Function int sqlite3_open(string filename, byte** db) By C;
```

等价于编译时传入 `-l sqlite3`，但更直观且随代码走。

### 调用 Extern 函数

`Extern` 函数调用需要在 `Trusted` 或 `Unsafe` 上下文中：

```kinal
Extern Function u64 __kn_time_tick() By C;

Trusted Function u64 GetTick()
{
    Return __kn_time_tick();   // 在 Trusted 函数中调用 Extern
}

Safe Function void Demo()
{
    u64 t = GetTick();  // Safe 函数可以调用 Trusted
}
```

直接在 `Unsafe` 函数中调用：

```kinal
Unsafe Function void DirectCall()
{
    u64 t = __kn_time_tick();  // 直接调用
}
```

## Delegate 函数（函数指针类型）

`Delegate` 用于声明一个**可在运行时绑定的函数指针变量**：

```kinal
[SymbolName("Add")]
Delegate Function int dyn_add(int a, int b) By C;
```

声明后，`dyn_add` 是一个可被赋值的全局变量。通过 `IO.System` 动态加载库后，将函数指针赋给它：

```kinal
byte* lib = IO.System.LoadLibrary("mylib.so");
byte* ptr = IO.System.GetSymbol(lib, dyn_add);  // 使用 Delegate 名查找符号
dyn_add = ptr;  // 绑定

// 之后像普通函数一样调用
int result = dyn_add(3, 4);
```

完整动态库加载示例：

```kinal
Unit App.DynLoader;

Get IO.Console;
Get IO.System;

[SymbolName("Add")]
Delegate Function int dyn_add(int a, int b) By C;

Unsafe Static Function int Main(string[] args)
{
    If (args.Length() == 0)
    {
        IO.Console.PrintLine("缺少库路径参数");
        Return 1;
    }

    byte* lib = IO.System.LoadLibrary(args[0]);
    If (lib == null)
    {
        IO.Console.PrintLine(IO.System.LastError());
        Return 2;
    }

    byte* addPtr = IO.System.GetSymbol(lib, dyn_add);
    If (addPtr == null)
    {
        IO.Console.PrintLine(IO.System.LastError());
        IO.System.CloseLibrary(lib);
        Return 3;
    }

    dyn_add = addPtr;
    IO.Console.PrintLine(dyn_add(20, 22));  // 42

    IO.System.CloseLibrary(lib);
    Return 0;
}
```

## 指针操作（Unsafe 特性）

在 `Unsafe` 上下文中可以使用裸指针：

```kinal
Unsafe Function void MemCopy(byte* dst, byte* src, usize len)
{
    For (usize i = 0; i < len; i++)
    {
        dst[i] = src[i];
    }
}
```

指针算术：

```kinal
Unsafe Function void PrintBytes(byte* ptr, int count)
{
    For (int i = 0; i < count; i++)
    {
        IO.Console.PrintLine([int](ptr[i]));
    }
}
```

多级指针：

```kinal
Unsafe Function byte** GetAllocTable()
{
    // ...
}
```

## 指针类型转换

在 Unsafe 上下文中，用 `[类型*](表达式)` 进行指针类型转换：

```kinal
Unsafe Function void Demo(byte* raw)
{
    int* ip = [int*](raw);   // byte* 转 int*
    *ip = 42;
}
```

函数对象转指针：

```kinal
Unsafe Function void GetFuncPtr()
{
    IO.Type.Object.Function fn = MyFunc;
    byte* ptr = [byte*](fn);
}
```

## 字符串与 C 字符串互操作

Kinal 的 `string` 内部是以 null 结尾的字节序列，可以直接传给期望 `const char*` 的 C 函数：

```kinal
Extern Function usize strlen(string s) By C;

Trusted Function usize GetLen(string s)
{
    Return strlen(s);
}
```

## 结构体与 C 结构体

必须逐项匹配 C 字段宽度、偏移、对齐与打包方式。仅当 C 声明也采用 packed 布局时才使用 By Packed；它不是通用 C 布局开关：

```kinal
Struct CPoint
{
    i32 x;
    i32 y;
}

Extern Function void draw_point(CPoint* p) By C;

Unsafe Function void Demo()
{
    CPoint pt;
    pt.x = 10;
    pt.y = 20;
    draw_point(&pt);
}
```

## Extern System 调用约定（Windows）

By System 选择平台系统调用约定；32 位 Windows 包含 stdcall，64 位 Windows 使用统一平台 ABI。声明应匹配实际目标 API：

```kinal
[LinkName("VirtualAlloc")]
Extern Function byte* VirtualAlloc(
    byte* lpAddress,
    usize dwSize,
    u32 flAllocationType,
    u32 flProtect
) By System;
```

## 链接时的考虑

手动链接外部库：
```bash
kinal build main.kn -l mylib -L ./lib/ -o app
```

或在代码中用 `[LinkLib]` 属性声明（推荐）：
```kinal
[LinkLib("mylib")]
Extern Function void mylib_init() By C;
```

## 完整 C 互操作示例

```kinal
Unit App.FFIDemo;

Get IO.Console;

// 声明 C 标准库函数
Extern Function int    puts(string s)    By C;
Extern Function int    abs(int n)        By C;
Extern Function f64 sqrt(f64 x)   By C;

Trusted Function void Demo()
{
    puts("Hello from puts!");
    IO.Console.PrintLine([int](abs(-99)));
    IO.Console.PrintLine(sqrt(2.0));
}

Static Function int Main()
{
    Demo();
    Return 0;
}
```

## 下一步

- [安全等级](safety.md)
- [Meta 元数据](meta.md)
- [裸机与嵌入式](../advanced/freestanding.md)


## 数组参数、所有权与部署

C ABI 数组参数退化为数据指针。按 Kinal `T[]` 返回的 C 指针长度未知、为零，索引需要调用方自行掌握边界。Kinal 定长数组字段使用描述符／后备存储，不是内联 C 数组字段，不能只写 `T[N]` 就假定匹配含数组的 C 结构体。

借用字符串／数组和回调必须在每次外部使用时仍有效。外部分配由匹配分配器释放；所有调用／回调停止后才能关闭动态库。原始函数对象表示转换不产生 C ABI 回调，应使用 `Delegate ... By C` 的适配路径。

完整 sqrt 示例使用 f64，不存在语言类型 double。Linux 需要链接系统数学库，例如 `kinal build ffi.kn -l m -o ffi`；目标 CRT／链接配置应提供 puts、abs、sqrt。动态加载示例还需要目标兼容且导出 Add 的库，使用 kinal run 时在 `--` 后传入库路径。

源码字符串是面向字节的 UTF-8 文本，但 Windows argv／文件系统／ANSI API 边界并不保证通用 UTF-8，应显式匹配外部 API 编码。Packed 仅适用于 C 声明本身也采用相应打包规则的布局，并非通用 C 布局开关。By System 在 32 位 Windows 对应 stdcall，64 位 Windows 使用统一 ABI。参见[内存与生命周期](../advanced/memory-management.md)。

## 为动态加载示例创建 C 库

将下面文件保存为 add.c。Windows 使用 clang -shared add.c -o add.dll；Linux 使用 clang -shared -fPIC add.c -o libadd.so；macOS 使用 clang -dynamiclib add.c -o libadd.dylib。随后运行 kinal run loader.kn -- 库的绝对路径。消费者示例应输出 42，并在最后一次调用后关闭库。

```c
#if defined(_WIN32)
__declspec(dllexport)
#endif
int Add(int a, int b)
{
    return a + b;
}
```

此处 C int 对应 Delegate 中的 Kinal int ABI 转换。By System 在 32 位 Windows 的栈清理约定与 By C 不同；x64／ARM64 遵循其目标平台 ABI，不能套用 32 位 stdcall 的栈规则。
