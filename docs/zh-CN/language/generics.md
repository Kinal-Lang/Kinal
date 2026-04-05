# 泛型

Kinal 支持函数级别的泛型，使用类型参数实现可复用的算法。

## 泛型函数

使用 `<T>` 声明类型参数：

```kinal
Function T Identity<T>(T value)
{
    Return value;
}
```

调用时指定具体类型：

```kinal
int i = Identity<int>(42);
string s = Identity<string>("hello");
```

## 多类型参数

```kinal
Function T First<T>(T a, T b)
{
    Return a;
}

Function void Swap<T>(T a, T b)
{
    T tmp = a;
    a = b;
    b = tmp;
}
```

多个类型参数用逗号分隔：

```kinal
Function void Pair<A, B>(A first, B second)
{
    IO.Console.PrintLine([string](first));
    IO.Console.PrintLine([string](second));
}

Pair<int, string>(42, "hello");
```

## 泛型与类型安全

类型参数在编译期被具体化，每种类型组合会生成独立的实现，保持类型安全：

```kinal
Function bool AreEqual<T>(T a, T b)
{
    Return a == b;
}

AreEqual<int>(1, 1);          // true
AreEqual<string>("a", "b");   // false
AreEqual<bool>(true, false);  // false
```

## 当前限制

Kinal 当前泛型支持**仅限于函数级别**。暂不支持：
- 泛型类（`Class Container<T>`）
- 泛型接口
- 类型约束（如 `where T : IShape`）

这些特性在语言演进中可能加入。

## 完整示例

```kinal
Unit App.Generics;

Get IO.Console;

// 泛型求最大值
Function T Max<T>(T a, T b)
{
    If (a > b)
    {
        Return a;
    }
    Return b;
}

// 泛型打印
Function void Print<T>(T value)
{
    IO.Console.PrintLine([string](value));
}

// 泛型容器包装
Function string Describe<T>(T value)
{
    Return "值为: " + [string](value);
}

Static Function int Main()
{
    int maxInt = Max<int>(3, 7);
    float maxFloat = Max<float>(1.5, 2.7);

    Print<int>(maxInt);
    Print<float>(maxFloat);
    Print<string>(Describe<bool>(true));

    Return 0;
}
```

输出：
```
7
2.7
值为: true
```

## 下一步

- [函数](functions.md)
- [类与继承](classes.md)
