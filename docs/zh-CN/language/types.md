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

| `int` | 64（平台相关）| 有 | 平台原生整数 |

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

| `float` | 平台默认浮点（通常 64 位） |

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



转义序列：`\0`、`\n`、`\r`、`\t`、`\\`、`\'`、`\"`



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

int a[3] = { 1, 2, 3 };

int b[5] = { 1, 2 };    // 其余元素默认为零

```



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



Kinal 目前支持一维数组；多维结构可通过类封装实现。



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



`struct` 是值语义（栈上分配），字段直接访问：



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



**安全性说明**：数值类型之间的转换在编译期静态检查；`any` 解包失败会在运行时抛出异常。



## 内置类型对象



Kinal 内置了几个特殊运行时类型：



| 类型 | 说明 |

|------|------|

| `IO.Type.Object.Class` | 所有引用类型的根类 |

| `IO.Type.Object.Function` | 函数对象（可存储函数引用） |

| `IO.Type.Object.Block` | Block 对象（代码块封装） |

| `IO.Error` | 标准异常类，含 `Title`、`Message`、`Inner`、`Trace` 字段 |



```kinal

IO.Type.Object.Function fn = Console.PrintLine;

fn("hello");    // 通过函数对象调用



IO.Type.Object.Class obj = fn;

IO.Console.PrintLine(obj.TypeOf());  // 打印类型名

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

