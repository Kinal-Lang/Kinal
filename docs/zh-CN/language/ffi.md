# FFI — 外部函数接口

Kinal 提供完整的 C 互操作能力，通过 `Extern`（声明 C 函数）和 `Delegate`（声明函数指针类型）实现与 C 库的零开销集成。

## Extern 函数

`Extern` 声明一个外部 C 函数，调用约定由 `By` 指定：

```kinal
Extern Function int printf(string fmt) By C;
Extern Function void* malloc(usize size) By C;
Extern Function void  free(void* ptr)   By C;
```

### 调用约定

| 关键字 | 说明 |
|--------|------|
| `By C` | C 调用约定（`cdecl`）— 默认 |
| `By System` | 系统调用约定（Windows 上为 `stdcall`） |

```kinal
Extern Function int MessageBoxA(byte* hWnd, string text, string caption, int type) By System;
```

### LinkName 属性

若 C 函数名与 Kinal 中的声明名不同，使用 `[LinkName]` 指定链接时的实际符号名：

```kinal
[LinkName("__kn_time_tick")]
Extern Function int GetTimeTick() By C;
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
Extern Function int __kn_time_tick() By C;

Trusted Function int GetTick()
{
    Return __kn_time_tick();   // 在 Trusted 函数中调用 Extern
}

Safe Function void Demo()
{
    int t = GetTick();  // Safe 函数可以调用 Trusted
}
```

直接在 `Unsafe` 函数中调用：

```kinal
Unsafe Function void DirectCall()
{
    int t = __kn_time_tick();  // 直接调用
}
```

## Delegate 函数（函数指针类型）

`Delegate` 用于声明一个**可在运行时绑定的函数指针变量**：

```kinal
[SymbolName("MyLib.Add")]
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

[SymbolName("MyLib.Add")]
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
Extern Function int strlen(string s) By C;

Trusted Function int GetLen(string s)
{
    Return strlen(s);
}
```

## 结构体与 C 结构体

使用 `Struct By Packed` 确保布局与 C 匹配：

```kinal
Struct CPoint By Packed
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
    draw_point([CPoint*]([byte*](pt)));
}
```

## Extern System 调用约定（Windows）

Windows API（如 `CreateFile`、`VirtualAlloc` 等）使用 `stdcall` 约定：

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
Extern Function double sqrt(double x)   By C;

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
