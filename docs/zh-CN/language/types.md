# 类型系统

Kinal 是静态类型语言，所有值的类型在编译期确定。本文详细介绍类型分类与使用方式。

## 原始类型

### 布尔类型

```kinal
bool flag = true;
bool done = false;
```

字面值：`true` / `false`

### 整数类型

| 类型 | 位宽 | 有符号 | 范围 |
|------|------|--------|------|
| `byte` | 8 | 无 | 0 ~ 255 |
| `int` | 64 | 有 | 语言内部的有符号 64 位整数 |
| `i8` | 8 | 有 | -128 ~ 127 |
| `i16` | 16 | 有 | -32768 ~ 32767 |
| `i32` | 32 | 有 | -2147483648 ~ 2147483647 |
| `i64` | 64 | 有 | ±9.2×10¹⁸ |
| `u8` | 8 | 无 | 0 ~ 255 |
| `u16` | 16 | 无 | 0 ~ 65535 |
| `u32` | 32 | 无 | 0 ~ 4294967295 |
| `u64` | 64 | 无 | 0 ~ 1.8×10¹⁹ |
| `isize` | 平台指针宽度 | 有 | 平台相关 |
| `usize` | 平台指针宽度 | 无 | 平台相关 |

```kinal
int n = 42;
i32 small = -100;
u64 big = 18446744073709551615;
byte b = 0xFF;
```

> **注意**：`byte` 值溢出（> 255）会在编译期产生警告或错误。

### 浮点类型

| 类型 | 标准 |
|------|------|
| `float` | IEEE 754 binary64，与 `f64` 相同 |
| `f16` | IEEE 754 半精度 |
| `f32` | IEEE 754 单精度 |
| `f64` | IEEE 754 双精度 |
| `f128` | 四倍精度 |

```kinal
float pi = 3.14159;
f32 x = 1.5f;
```

### 字符类型

`char` 表示单个字节字符（ASCII 字符）：

```kinal
char c = 'A';
char newline = '\n';
char quote = '\'';
int code = [int](c);   // 65
```

转义序列：`\0`、`\n`、`\r`、`\t`、`\b`、`\\`、`\'`、`\"`

### 字符串类型

`string` 是不可变的字节序列（UTF-8 友好）：

```kinal
string name = "Kinal";
string path = "C:\\Users";
```

字符串可直接用 `+` 拼接：

```kinal
string greeting = "Hello, " + name + "!";
```

在循环中，KinalNative 可以为局部字符串预留容量，供后续的 `text += part;` 或 `text = text + part;` 复用。已经保存的值会保留原有内容。例如，执行 `string saved = text;` 后再向 `text` 追加文本，`saved` 仍然是保存时的字符串。若已知最终字节数，可以用 `IO.Text.Builder` 的 `EnsureCapacity` 提前预留空间。

字符串前缀（详见 [字符串特性](strings.md)）：

```kinal
string raw = [r]"无转义\n原始字符串";
string fmt = [f]"值是 {name}";     // 格式化字符串
char[] chars = [c]"abc";           // 转为字符数组
```

### any 类型

`any` 可持有任意类型的值（装箱机制）：

```kinal
any value = 42;
any obj = "hello";
any none = null;
value.IsNull();      // false
value.TypeName();    // "int"
value.ToString();    // "42"
value.IsInt();       // true
```

类型检查与转换：

```kinal
any x = 100;
If (x.IsInt())
{
    int n = [int](x);
}
```

## 数组

### 定长数组

```kinal
int[3] a = { 1, 2, 3 };
int[5] b = { 1, 2 };    // 其余元素默认为零
```

局部数值数组每次执行声明时都会先清零，包括在循环中再次执行同一条声明。对于只在本地按下标访问的大型定长数组，KinalNative 可以复用存储空间。递归调用和并发调用各自拥有独立的活动数组。返回数组、由闭包捕获数组或返回元素指针时，原有存储仍然有效。

### 动态数组

```kinal
int[] nums = { 1, 2, 3 };
int[] empty = {};
```

### 数组操作

```kinal
int len = nums.Length();
int first = nums[0];
nums[1] = 99;
// 数组支持 Add 方法（返回新数组）
nums = nums.Add(4);
```

### 多维数组

Kinal 现在支持嵌套数组语法。`T[][]` 表示 `T[]` 的数组，`T[][][]` 表示 `T[][]` 的数组。

```kinal
int[][] grid = {
    { 1, 2 },
    { 3 }
};
string[][][] words = {
    {
        { "a", "b" },
        { "c" }
    }
};
any[][][] values = {
    {
        { 1, "two" },
        { true }
    }
};
```

多维数组按 jagged array 处理，内层数组长度可以不同。使用 `any` 时需要显式写出数组维度，例如 `any[][][]`。

## Package

`Object.Package` 是编译期结构化值，用于在不声明新 `struct` 的情况下返回或传递一组固定数量的值。

Package 类型使用尖括号：

```kinal
Function <int, string> Status()
{
    Return <200, "OK">;
}
```

重复的元素类型可以使用重复后缀：

```kinal
Function <int(2), string(2)> Repeated()
{
    Return <1, 2, "left", "right">;
}
```

当希望变量类型由初始化表达式推断时，可以使用 `Object.Package`：

```kinal
Object.Package status = Status();
IO.Console.PrintLine(status[0]);  // 200
IO.Console.PrintLine(status[1]);  // OK
```

Package 索引必须是编译期常量。

可以在变量声明上附加字段别名。别名只属于这个变量，不属于 Package 类型：

```kinal
Object.Package status<code, message> = Status();
IO.Console.PrintLine(status.code);
IO.Console.PrintLine(status.message);
```

Package 值可以显式物化为数组：

```kinal
any[] items = [any[]](status);
int[] nums = [int[]](<1, 2, 3>);
```

空 Package 不受支持。Package v1 不支持解构、命名 Package 字面量或命名 Package 类型。

## 指针

指针是 `Unsafe` 特性，需要在 `Unsafe` 上下文中使用：

```kinal
Unsafe Function void Demo()
{
    byte* ptr = null;
    int* ip = null;
}
```

深度指针：`byte**`（二级指针）。

指针主要用于 FFI 互操作和低层内存操作，日常开发中应尽量避免。

详见 [安全等级](safety.md) 和 [FFI](ffi.md)。

## 引用类型

### class

`class` 实例是引用类型（堆分配），通过 `New` 创建：

```kinal
Class Point
{
    Public int X;
    Public int Y;
    Public Constructor(int x, int y)
    {
        This.X = x;
        This.Y = y;
    }
}
Point p = New Point(3, 4);
p.X = 10;
```

类变量可赋值为 `null`：

```kinal
Point q = null;
If (q != null)
{
    ...
}
```

### 类型判断：`Is`

```kinal
Animal a = New Dog();
IO.Console.PrintLine(a Is Dog);     // true
IO.Console.PrintLine(a Is Animal);  // true
IO.Console.PrintLine(a Is Cat);     // false
```

模式匹配（配合 `If`）：

```kinal
If (a Is Dog dog)
{
    // dog 是已类型化的 Dog 实例
    dog.Bark();
}
```

### list（动态列表）

```kinal
list l = list.Create();
l.Add("apple");
l.Add("banana");
int count = l.Count();            // 2
any val = l.Fetch(0);             // "apple"
string s = [string](l.Peek());    // 最后一项
```

### dict（字典）

```kinal
dict d = dict.Create();
d.Set("name", "Kinal");
d.Set("version", 1);
string name = [string](d.Fetch("name"));
bool has = d.Contains("version"); // true
```

### set（集合）

```kinal
set s = set.Create();
s.Add("a");
s.Add("b");
bool has = s.Contains("a"); // true
```

## 值类型

### struct

`Struct` 使用值语义，赋值复制字段。存储可位于局部、全局、其他值或被托管捕获保留的位置；值语义不保证只在栈上分配。

```kinal
Struct Point
{
    int X;
    int Y;
}
Point p;
p.X = 3;
p.Y = 4;
```

支持对齐与 Packed 属性（详见 [结构体与枚举](structs-enums.md)）。

### enum

```kinal
Enum Color
{
    Red,
    Green,
    Blue
}
Color c = Color.Red;
Switch (c)
{
    Case (Color.Red) { IO.Console.PrintLine("红色"); }
    Case (default) { IO.Console.PrintLine("其他"); }
}
```

指定底层类型：

```kinal
Enum Flag By u8
{
    None = 0,
    Read = 1,
    Write = 2
}
```

## 类型转换

Kinal 使用 `[类型](值)` 语法进行显式类型转换：

```kinal
int i = 65;
char c = [char](i);         // 'A'
string s = [string](i);     // "65"
float f = [float](i);       // 65.0
// any 解包
any a = 42;
int n = [int](a);
// 字符串转数值
int parsed = [int]("42");
bool b = [bool]("1");       // true
```

使用 `[类型](值)` 进行转换。两种 KNC 编译器都可以解包标量，例如 `[int](items.Fetch(0))`。若调用者只接受特定类型的值，可先用 `Is` 检查。

## 内置类型对象

Kinal 内置了几个特殊运行时类型：

| 类型 | 说明 |
|------|------|
| `IO.Type.Object.Class` | 类对象层次的内建根类；不要推断字符串／数组具有相同对象布局 |
| `IO.Type.Object.Function` | 函数对象（可存储函数引用） |
| `IO.Type.Object.Block` | Block 对象（代码块封装） |
| `IO.Type.Object.Package` | Package 占位类型，用于从初始化表达式推断具体元素类型 |
| `IO.Error` | 标准异常类，含 `Title`、`Message`、`Inner`、`Trace` 字段 |

```kinal
Get IO.Console;

IO.Type.Object.Function fn = Function int(int value) { Return value + 1; };
int result = fn(41);
IO.Type.Object.Class obj = fn;
IO.Console.PrintLine(result);        // 42
IO.Console.PrintLine(obj.TypeOf());  // 静态类型名称
```

## 类型方法

所有值类型都提供以下内置方法（通过 `any` 接口）：

```kinal
int n = 42;
n.ToString();   // "42"
n.TypeName();   // "int"
n.Equals(42);   // true
n.IsNull();     // false
```

字符串专有方法见 [IO.Text](../stdlib/text.md)。

## 下一步

- [变量与常量](variables.md)

- [运算符](operators.md)

- [结构体与枚举](structs-enums.md)


## 表示、别名与后端边界

int 为 64 位语言整数，但 By C 边界映射到 32 位 C int。f16／f128 的 Native 目标支持与 VM 精度不是同一保证。数组赋值会复制描述符并共享底层元素；Add 创建新的外层存储。固定长度初始化器可以补零，但后续赋值、参数及返回值的长度检查不等于“总会补零”。这些边界的完整示例见[内存与生命周期](../advanced/memory-management.md)。

any 采用动态表示，其类型检查不是完整源码反射；TypeOf() 和 TypeName() 的意义不同。全部转换和值辅助接口见 [IO.Type](../stdlib/types.md)。list／dict／set 的创建、键和值约束见[集合](../stdlib/collections.md)。两种 KNC 编译器都支持这些集合。
