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
int x;          // x 未初始化，稍后赋值
string text;
```

> **警告**：未初始化的变量在使用前必须先赋值，否则会产生编译错误。

### 类数组声明

```kinal
int nums[3];           // 定长数组，未初始化
int vals[] = { 1, 2, 3 }; // 动态数组
```

## 常量

使用 `Const` 声明编译期常量：

```kinal
Const int MaxSize = 100;
Const string Version = "1.0.0";
Const bool Debug = true;
```

常量必须在声明时赋值，且值必须是编译期可求值的字面量。常量不能被重新赋值。

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

> Kinal 目前只支持**后置**自增/自减（`i++`），不支持前置（`++i`）。

## 作用域

变量的作用域遵循块作用域规则：

```kinal
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
string s = default;  // ""（空字符串）
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
