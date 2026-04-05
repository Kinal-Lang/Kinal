# 控制流

## If / Else / ElseIf

基本条件语句：

```kinal
If (x > 0)
{
    IO.Console.PrintLine("正数");
}
Else If (x < 0)
{
    IO.Console.PrintLine("负数");
}
Else
{
    IO.Console.PrintLine("零");
}
```

> `ElseIf` 是 `Else If` 的等价写法，也可以用 `ElseIf`（一个关键字）。

单行模式（无花括号，适用于简单单语句）：

```kinal
If (flag) IO.Console.PrintLine("flagged");
```

> 推荐始终使用花括号，避免歧义。

### Const If（编译期条件）

当条件的值在编译期已知（如 `IO.Target.*` 平台常量），`If` 变为编译期分支，不通过的分支**不参与语义检查**：

```kinal
If (IO.Target.OS == IO.Target.OS.Windows)
{
    IO.Console.PrintLine("Windows");
}
Else
{
    UnknownCall();   // 非 Windows 时此分支不被编译，不会报错
}
```

### Is 模式匹配与 If

```kinal
Animal a = New Dog();

If (a Is Dog dog)
{
    dog.Bark();   // dog 的类型在此块内为 Dog
}
```

## While 循环

```kinal
int i = 0;
While (i < 5)
{
    IO.Console.PrintLine(i);
    i++;
}
```

`Break` 退出循环，`Continue` 跳过当前迭代：

```kinal
int n = 0;
While (true)


{
    If (n >= 10)


    {
        Break;
    }
    If (n % 2 == 0)


    {
        n++;
        Continue;
    }
    IO.Console.PrintLine(n);  // 打印奇数
    n++;
}
```

## For 循环

```kinal
For (int i = 0; i < 10; i++)


{
    IO.Console.PrintLine(i);
}
```

各部分均可省略：

```kinal
int i = 0;
For (; i < 10; )


{
    i++;
}

// 无限循环
For (;;)


{
    Break;
}
```

For 循环的初始化部分可以是变量声明或表达式：

```kinal
For (Var i = 0; i < 5; i++)


{
    IO.Console.PrintLine(i);
}
```

## Switch 语句

Switch 提供基于值的多分支选择：

```kinal
int code = 2;
Switch (code)


{
    Case (1)


    {
        IO.Console.PrintLine("一");
    }
    Case (2)


    {
        IO.Console.PrintLine("二");
    }
    Case (default)


    {
        IO.Console.PrintLine("其他");
    }
}
```

> Kinal 的 Switch 没有 fall-through（不会自动执行下一个 Case），不需要 `Break`。

### Switch 表达式

Switch 也可以作为**表达式**使用，返回一个值：

```kinal
string label = Switch (code)


{
    Case (1) { "一" }
    Case (2) { "二" }
    Case (default) { "其他" }
};
IO.Console.PrintLine(label);
```

或更紧凑的形式（Case 中直接是表达式）：

```kinal
int doubled = Switch (x)


{
    Case (1) { 2 }
    Case (2) { 4 }
    Case (default) { 0 }
};
```

### 对象类型的 Switch

Switch 也可对 `any` 或引用类型按值分支：

```kinal
any val = 42;
Switch (val)


{
    Case (42) { IO.Console.PrintLine("四十二"); }
    Case ("hello") { IO.Console.PrintLine("问好"); }
    Case (default) { IO.Console.PrintLine("未知"); }
}
```

## If 表达式

`If` 也可以作为表达式（类似三元运算符）：

```kinal
int max = If (a > b) { a } Else { b };
IO.Console.PrintLine(max);
```

```kinal
string label = If (score >= 60) { "及格" } Else { "不及格" };
```

## Break 与 Continue

`Break`：立即退出最内层循环。

```kinal
While (true)


{
    int input = GetInput();
    If (input < 0)


    {
        Break;
    }
    Process(input);
}
```

`Continue`：跳过当前迭代，继续下一次循环。

```kinal
For (int i = 0; i < 10; i++)
{
    If (i % 2 == 0)
    {
        Continue;  // 跳过偶数
    }
    IO.Console.PrintLine(i);  // 只打印奇数
}
```

## 结合控制流的完整示例

```kinal
Unit App.Demo;
Get IO.Console;

Static Function int Main()
{
    // For 循环打印 1~10 中的奇数
    For (int i = 1; i <= 10; i++)
    {
        If (i % 2 == 0)
        {
            Continue;
        }
        IO.Console.PrintLine(i);
    }

    // Switch 分支示例
    string day = "Monday";
    Switch (day)
    {
        Case ("Saturday")
        {
            IO.Console.PrintLine("周末");
        }
        Case ("Sunday")
        {
            IO.Console.PrintLine("周末");
        }
        Case (default)
        {
            IO.Console.PrintLine("工作日");
        }
    }

    Return 0;
}
```

> **注意**：上面的 `Case ("Saturday") Case ("Sunday")` — Kinal 目前**不支持** Case 叠加（fall-through）。  
> 若需要多值匹配，请在 Case 内用 `||` 判断，或使用 `Is` 模式。

## 下一步

- [函数](functions.md)
- [异常处理](exceptions.md)
- [Block（代码块对象）](blocks.md)
