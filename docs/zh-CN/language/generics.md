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

显式类型实参使用调用处的导入解析，声明中的参数和返回类型使用定义处的导入解析。
例如，库可以用自己的 `Model` 别名声明
`Function Model.Box Select<T>(T[] tags, Model.Box[] boxes)`。
调用方如果把同一个模块命名为 `Data`，就可以传入 `Data.Box[]`，并以 `Data.Box` 接收结果。
库中的函数可以分布在同一 Unit 的多个源码文件里。

类方法和属性访问器里可以直接调用泛型函数。下面的 getter 会以 `Counter` 实例化 `ReadCount`。编译器会检查 `Counter.Count` 是否存在，以及它的类型是否符合返回类型。

```kinal
Function int ReadCount<T>(T item) { Return item.Count; }

Class Counter
{
    Public int Count;
    Public Property int Total { Get { Return ReadCount<Counter>(This); } }
}
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

## 值语义与调用组合

`Swap<T>(T a, T b)` 只交换形参副本，不修改调用方的标量变量。需要修改调用方状态时，返回 Package 或使用明确的可变引用。实例化后的函数体接受正常检查，例如数组不支持直接聚合相等，因此 `AreEqual<int[]>(...)` 无效。

当前 Parser 会拒绝某些嵌套显式泛型调用，例如 `Print<string>(Describe<bool>(true))`。应像完整示例一样，先把内部结果存入带类型的局部变量，再传给外部调用。这是实现边界，不代表泛型会自动推断所有嵌套类型实参。

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
    string description = Describe<bool>(true);
    Print<string>(description);

    Return 0;
}
```

输出：
```
7
2.7000000000000002
值为: true
```

## 下一步

- [函数](functions.md)
- [类与继承](classes.md)

泛型函数体还受后端支持范围约束，参见[后端支持](../advanced/backend-support.md)。

泛型函数对象可通过匿名函数适配：在一个有具体参数／返回类型的匿名函数内调用 Identity<int>(value)。当前解析器不接受单独的 Identity<int> 引用表达式。
