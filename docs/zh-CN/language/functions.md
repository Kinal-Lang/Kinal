# 函数

## 函数声明

基本语法：

```kinal
Function 返回类型 函数名(参数列表)
{
    // 函数体
    Return 值;
}
```

示例：

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}

Function string Greet(string name)
{
    Return "Hello, " + name + "!";
}

Function void PrintValue(int n)
{
    Console.PrintLine(n);
    // 无需 Return（void 函数）
}
```

## 静态函数

`Static` 修饰符表示函数不属于任何类实例：

```kinal
Static Function int Square(int n)


{
    Return n * n;
}
```

通常模块级（全局）的工具函数都声明为 `Static`。程序入口 `Main` 必须是 `Static`：

```kinal
Static Function int Main()


{
    Return 0;
}

// 接受命令行参数版本
Static Function int Main(string[] args)


{
    Return 0;
}
```

## 参数默认值

参数可以有默认值，有默认值的参数必须排在无默认值参数的后面：

```kinal
Function int Power(int base, int exp = 2)


{
    int result = 1;
    For (int i = 0; i < exp; i++)


    {
        result = result * base;
    }
    Return result;
}

Power(3);       // 9（exp = 2）
Power(3, 3);    // 27
```

## 命名参数

调用函数时可以通过名称指定参数，无需顾虑参数顺序：

```kinal
Function string Format(string text, int indent = 0, bool uppercase = false)
{
    // ...
}

Format(text = "hello", uppercase = true);   // indent 使用默认值
Format(uppercase = true, text = "world");   // 顺序可以任意
```

`default` 关键字可以在命名参数调用中显式使用类型默认值：

```kinal
Add(p1 = default, p2 = 5);    // p1 使用默认值 1
```

## 泛型函数

使用 `<T>` 声明类型参数：

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

Function T First<T>(T a, T b)
{
    Return a;
}

// 调用时指定类型
int i = Identity<int>(42);
string s = Identity<string>("hello");
int f = First<int>(3, 7);   // 3
```

泛型支持多个类型参数：

```kinal
Function void Swap<T>(T a, T b)
{
    T temp = a;
    a = b;
    b = temp;
}
```

## 匿名函数（Lambda）

使用 `Function 类型(参数列表) { 体 }` 语法创建匿名函数：

```kinal
// 赋值给变量
IO.Type.Object.Function fn = Function int(int a, int b)
{
    Return a + b;
};

// 或使用 Var 推断
Var add = Function int(int a, int b)
{
    Return a + b;
};

// 调用
int result = add(3, 4);  // 7
```

匿名函数可以**捕获**外部变量（闭包）：

```kinal
string msg = "before";
IO.Type.Object.Block cap = Block </
    Console.PrintLine(msg);  // 捕获 msg
/>;
msg = "after";
cap.Run();   // 打印 "after"（捕获的是引用）
```

详见 [Block](blocks.md) 和 [异步编程](async.md)。

## 函数对象

函数可以赋值给变量或作为参数传递：

```kinal
IO.Type.Object.Function p = IO.Console.PrintLine;
p("hello");  // 通过函数对象调用

Var q = p;   // Var 推断
q("world");
```

函数对象的类型为 `IO.Type.Object.Function`，可赋给 `IO.Type.Object.Class`：

```kinal
IO.Type.Object.Class c = p;
IO.Console.PrintLine(c.TypeOf());  // 打印类型名
```

## 函数指针（低层互操作）

在 `Unsafe` 上下文中可以将函数对象转换为裸指针：

```kinal
Unsafe Function void Demo()
{
    IO.Type.Object.Function fn = IO.Console.PrintLine;
    byte* ptr = [byte*](fn);   // 转为裸函数指针
}
```

详见 [FFI](ffi.md)。

## `Delegate` 函数（C 回调桥接）

`Delegate` 声明用于将 C 函数指针映射为 Kinal 可调用符号：

```kinal
[SymbolName("MyLib.Callback")]
Delegate Function int my_callback(int code) By C;
```

详见 [FFI](ffi.md)。

## 安全级别修饰符

函数可以声明安全级别（详见 [安全等级](safety.md)）：

```kinal
Safe Function void SafeOp() { ... }       // 只能调用 Safe 函数
Trusted Function void Bridge() { ... }    // 可以调用 Unsafe
Unsafe Function void RawOp() { ... }      // 可以使用裸指针
```

## 异步函数

```kinal
Async Function int Compute()


{
    int a = Await StepOne();
    int b = Await StepTwo(a);
    Return b;
}
```

详见 [异步编程](async.md)。

## `Extern` 函数（C 互操作）

```kinal
Extern Function int printf(string fmt) By C;
Extern Function void* malloc(usize size) By C;
```

详见 [FFI](ffi.md)。

## 递归

Kinal 完整支持递归，包括间接递归：

```kinal
Function int Fibonacci(int n)


{
    If (n <= 1)


    {
        Return n;
    }
    Return Fibonacci(n - 1) + Fibonacci(n - 2);
}
```

## 函数重载

Kinal **不支持**同名函数重载（即同一 Unit 内不能有两个同名函数）。可以通过不同命名或泛型区分功能。

## 返回值

- `Return 表达式;` — 返回值
- `Return;` — 在 `void` 函数中提前返回
- 若函数执行到末尾未 `Return`，编译器会报 "Missing Return" 错误（对非 `void` 函数）

## 局部函数（Block 内定义）

在 Block 字面量内可以定义局部函数、类和其他声明（详见 [Block](blocks.md)）：

```kinal
Block ScopeDecl
</
    Function int LocalAdd(int a, int b)


    {
        Return a + b;
    }
    IO.Console.PrintLine(LocalAdd(3, 4));
/>
ScopeDecl.Run();
```

## 下一步

- [类与继承](classes.md)
- [泛型](generics.md)
- [异步编程](async.md)
- [安全等级](safety.md)
