# 变量与常量

## 变量声明

Kinal 的变量声明有两种形式：**显式类型** 和 **类型推断**。

### 显式类型声明

```kinal
int count = 0;
string name = "Kinal";
bool flag = true;
float pi = 3.14159;
```

格式：`类型 变量名 = 初始值;`

### 类型推断（Var）

使用 `Var` 关键字，让编译器从初始值推断类型：

```kinal
Var count = 0;       // 推断为 int
Var name = "Kinal";  // 推断为 string
Var flag = true;     // 推断为 bool
Var obj = New MyClass();  // 推断为 MyClass
```

`Var` 仅在初始值类型明确时可用，不能声明未初始化的 `Var` 变量。

### 声明时不初始化

显式类型声明可以不提供初始值：

```kinal
int x;          // 数值存储从零开始；显式初始化更清楚
string text;
```

当前编译器接受这类声明，并将数值和定长数组存储清零，不存在旧文档所述的通用“使用前未赋值”编译错误。引用值建议显式初始化：默认字符串在 Native 与 VM 间的表示目前不同，参见[后端支持](../advanced/backend-support.md)。

### 数组声明

```kinal
int[3] nums;             // 定长数组，元素清零
int[] vals = { 1, 2, 3 }; // 动态数组
```

在托管环境下，`int[2][4] grid;` 创建四行，每行有两个整数，各行独立存储。最右侧的长度对应最外层。写成 `int[2][4] grid = {{7}};` 时，首行为 `{7, 0}`，其余三行均为 `{0, 0}`。写成 `int[][4] grid;` 时，四行初始长度均为零。如果使用已有数组变量初始化某一行，该行与原变量共享元素。补齐发生在声明时。之后赋值、传参或返回数组字面量时，各行保留实际写出的长度。

在托管环境下，每个默认初始化的结构体都有自己的定长数组字段。例如，声明 `Struct Row { int[3] Cells; }` 后，`Row[2] rows;` 会创建两组独立的三个元素。写入 `rows[0].Cells[1]` 不会影响 `rows[1].Cells[1]`。复制已有的行时，数组描述符也随之复制，因此副本与原行共享元素。

这项规则适用于局部变量、全局变量、静态字段和新对象的字段。构造函数可以直接使用这些数组。全局初始化表达式也可以写入后面声明的默认数组。如果后面的声明带有显式初始化表达式，该表达式仍按声明顺序执行。

在托管环境下，有类型的 Package 按每一项的声明类型初始化。例如，声明 `<int[2], int> pair;` 后，`pair[0].Length()` 为 2，`pair[1]` 为 0。给定长数组、结构体或有类型的 Package 赋 `default` 时，其中的定长数组会获得新的存储。此前已保存的数组引用仍指向原数组。

递归结构可以使用动态数组，例如 `Struct Node { Node[] Children; }`。如果将该字段改成长度大于零的定长数组，默认初始化就会无限展开，编译器在生成默认值时会报错。

## 常量

使用 `Const` 声明不能重新赋值的绑定：

```kinal
Const int MaxSize = 100;
Const string Version = "1.0.0";
Const bool Debug = true;
```

`Const` 必须有初始化表达式，并禁止重新赋值。初始化可以是函数调用等运行时表达式，`Const` 本身不保证编译期求值。数组长度等要求整数常量表达式的上下文，仍要求表达式可在编译期求值。

## 变量赋值

```kinal
count = 10;
name = "World";
```

复合赋值运算符：

```kinal
count += 5;    // count = count + 5
count -= 2;    // count = count - 2
count *= 3;    // count = count * 3
count /= 2;    // count = count / 2
count %= 7;    // count = count % 7
```

自增/自减：

```kinal
count++;   // 后置自增
count--;   // 后置自减
```

前置（`++i`、`--i`）和后置（`i++`、`i--`）形式均支持。前置表达式返回更新后的值，后置表达式返回更新前的值。

## 作用域

变量的作用域遵循块作用域规则：

```kinal
Get IO.Console;

Static Function int Main()
{
    int x = 1;          // 函数作用域

    If (x > 0)
    {
        int y = 2;      // If 块作用域
        x = y;          // 合法：x 在外层可见
    }
    // y 在此处不可访问

    For (int i = 0; i < 3; i++)
    {
        IO.Console.PrintLine(i);  // i 仅在 for 内部可见
    }

    Return x;
}
```

## default 值

`default` 用于表示某类型的零值，常用于带默认参数的函数调用：

```kinal
Function int Add(int a = 1, int b = 2)
{
    Return a + b;
}

Add(default, default);  // 使用两个默认值：1 + 2 = 3
Add(10, default);       // 10 + 2 = 12
Add(default, 5);        // 1 + 5 = 6
```

`default` 也可赋给变量，表示该类型的零值：
```kinal
int n = default;     // 0
string s = "";       // 显式指定空字符串
bool b = default;    // false
```

## null 值

引用类型（class 实例、string、any、list、dict 等）可赋值为 `null`：

```kinal
string s = null;
MyClass obj = null;
any a = null;

If (s == null)
{
    IO.Console.PrintLine("空字符串引用");
}
```

原始值类型（`int`、`bool`、`char` 等）**不能**为 `null`。

## 全局变量

在函数外部声明的变量为全局变量：

```kinal
Unit App;

int GlobalCounter = 0;   // 全局变量

Static Function void Increment()
{
    GlobalCounter++;
}
```

全局引用类型默认可设为 `null` 并在使用前初始化：

```kinal
list GlobalList = null;

Static Function void Init()
{
    GlobalList = list.Create();
}
```

## 变量遮蔽

内层作用域中可以声明与外层同名的变量（遮蔽），但实际上不推荐这样做：

```kinal
int x = 1;
If (true)
{
    int x = 2;  // 遮蔽外层 x，在此块内 x = 2
    IO.Console.PrintLine(x);  // 2
}
IO.Console.PrintLine(x);  // 1（外层 x 未被修改）
```

## 下一步

- [运算符](operators.md)
- [控制流](control-flow.md)
- [函数](functions.md)

