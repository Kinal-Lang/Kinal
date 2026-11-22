# 类与继承

## 类声明

```kinal
Class 类名
{
    // 字段
    // 方法
    // 构造函数
}
```

示例：

```kinal
Class Person
{
    Public string Name;
    Public int Age;

    Public Constructor(string name, int age)
    {
        This.Name = name;
        This.Age = age;
    }

    Public Function string ToString()
    {
        Return This.Name + "(" + [string](This.Age) + ")";
    }
}
```

## 字段

```kinal
Class Box
{
    Public int Width;            // 公开字段
    Private float _weight;       // 私有字段
    Public Static int Count = 0; // 静态字段
}
```

字段可以有初始值：

```kinal
Class Config
{
    Public int MaxRetries = 3;
    Public string DefaultName = "unknown";
}
```

## 属性

属性可以只声明 `Get`、只声明 `Set`，也可以同时声明两者。在 `Set` 中通过 `value` 取得赋入的值。

```kinal
Class Meter
{
    Private int count;
    Public Property int Reading { Get { Return count; } }
    Public Property int ResetTo { Set { count = value; } }
    Public Property int Level { Get { Return count; } Set { count = value; } }
}
```

执行 `meter.ResetTo = 42` 会调用一次 `Set`，随后读取 `meter.Reading` 得到 42。`meter.Level += step` 这样的复合赋值需要两个访问器。编译器对接收对象求值一次，然后依次调用 `Get`、计算 `step`、调用 `Set`。

对于可读写的数值属性，`meter.Level++` 返回递增前的值，`++meter.Level` 返回递增后的值。递减遵循相同规则。在 `GetMeter().Level++` 中，Kinal 只调用一次 `GetMeter()`，随后在同一个对象上依次调用一次 `Get` 和一次 `Set`。

## 访问控制

| 修饰符 | 说明 |
|--------|------|
| `Public` | 任何地方均可访问 |
| `Private` | 仅类内部可访问 |
| `Protected` | 类内部及派生类可访问 |
| `Internal` | 同模块内可访问 |

```kinal
Class Account
{
    Public string Owner;     // 公开
    Private float balance;   // 私有

    Public Function float GetBalance()
    {
        Return This.balance;  // 内部可访问私有字段
    }

    Private Function void Validate() { ... }
}
```

## 构造函数

### 旧式命名构造语法

旧式 Function ClassName(...) 写法仍可用，但会产生 W-SYN-00002。新代码优先使用下一节的 Constructor 关键字：

```kinal
Class Counter
{
    Private int value;

    Public Function Counter(int start)
    {
        This.value = start;
    }
}

Counter c = New Counter(10);
```

### `Constructor` 关键字

也可以用 `Constructor` 关键字声明构造函数：

```kinal
Class Timer
{
    Private int ms;

    Public Constructor(int milliseconds)
    {
        This.ms = milliseconds;
    }
}
```

### 默认构造函数

若不声明任何构造函数，编译器会自动生成一个无参的默认构造函数：

```kinal
Class Empty {}   // 编译器自动生成默认构造函数

Empty e = New Empty();
```

### 参数默认值

构造函数同样支持参数默认值：

```kinal
Class Pair
{
    Public int Left;
    Public int Right;

    Public Constructor(int left = 0, int right = 0)
    {
        This.Left = left;
        This.Right = right;
    }
}

Pair a = New Pair();         // 0, 0
Pair b = New Pair(1);        // 1, 0
Pair c = New Pair(1, 2);     // 1, 2
Pair d = New Pair(default, 5); // 0, 5
```

## `This` 关键字

在方法内部，`This` 指向当前实例：

```kinal
Class Vector
{
    Public float X;
    Public float Y;

    Public Function Vector Add(Vector other)
    {
        Vector result = New Vector();
        result.X = This.X + other.X;
        result.Y = This.Y + other.Y;
        Return result;
    }
}
```

## 隐式 This

当字段/方法名不与局部变量冲突时，可以省略 `This.`：

```kinal
Class Node
{
    Public int Value;

    Public Function int Double()
    {
        Return Value * 2;   // 隐式 This.Value
    }
}
```

## 静态成员

`Static` 方法和字段属于类本身，不属于实例：

```kinal
Class MathHelper
{
    Public Static float PI = 3.14159;

    Public Static Function float CircleArea(float r)
    {
        Return PI * r * r;
    }
}

float area = MathHelper.CircleArea(5.0);
float pi = MathHelper.PI;
```

## 静态类

`Static Class` 中所有成员都是静态的，不能被实例化：

```kinal
Static Class Utils
{
    Public Function string Reverse(string s)
    {
        // ...
    }
}

// 直接调用，无需 New
string r = Utils.Reverse("hello");
```

使用 `New` 创建静态类实例会产生编译错误。

## 继承

使用 `By` 关键字指定父类：

```kinal
Class Animal
{
    Public string Name;

    Public Constructor(string name)
    {
        This.Name = name;
    }

    Public Virtual Function string Sound()
    {
        Return "...";
    }
}

Class Dog By Animal
{
    Public Constructor(string name)
    {
        This.Name = name;  // 直接访问父类字段
    }

    Public Override Function string Sound()
    {
        Return "Woof!";
    }
}
```

### 调用父类成员：`Base`

```kinal
Class ParentClass
{
    Public Virtual Function string Describe() { Return "parent"; }
}

Class Derived By ParentClass
{
    Public Override Function string Describe()
    {
        string parentDesc = Base.Describe();
        Return parentDesc + " (derived)";
    }
}
```

### 虚方法修饰符

| 修饰符 | 说明 |
|--------|------|
| `Virtual` | 方法可被子类重写 |
| `Override` | 重写父类的虚方法 |
| `Abstract` | 抽象方法，必须由子类实现 |
| `Sealed` | 方法不能再被子类重写 |

抽象类示例：

```kinal
Abstract Class Shape
{
    Public Abstract Function float Area();

    Public Function void Print()
    {
        IO.Console.PrintLine("面积: " + [string](This.Area()));
    }
}

Class Circle By Shape
{
    Private float radius;

    Public Constructor(float r) { This.radius = r; }

    Public Override Function float Area()
    {
        Return 3.14159 * This.radius * This.radius;
    }
}
```

### 密封类

`Sealed Class` 不能被继承：

```kinal
Sealed Class FinalNode
{
    Public int Value;
}
```

## 嵌套类型

类内部可以声明嵌套类、接口、结构体和枚举：

```kinal
Class Container
{
    Class Iterator
    {
        Public int Index;
    }

    Enum State { Empty, Loading, Ready }

    // 使用嵌套类型
    Public Function Iterator GetIterator()
    {
        Return New Iterator();
    }
}
```

## null 与引用安全

类实例变量可以为 `null`，访问前应检查：

```kinal
MyClass obj = null;

If (obj != null)
{
    obj.DoSomething();
}
```

## 完整示例

```kinal
Unit App;

Get IO.Console;

Interface IGreeter
{
    Function string Greet(string name);
}

Class Greeter By IGreeter
{
    Private string prefix;

    Public Safe Constructor(string prefix)
    {
        This.prefix = prefix;
    }

    Public Safe Function string Greet(string name)
    {
        Return This.prefix + name;
    }

    Public Static Safe Function Greeter Hello()
    {
        Return New Greeter("Hello, ");
    }
}

Static Safe Function int Main(string[] args)
{
    IGreeter g = Greeter.Hello();
    IO.Console.PrintLine(g.Greet("Kinal"));  // "Hello, Kinal"
    Return 0;
}
```

## 下一步

- [接口](interfaces.md)
- [结构体与枚举](structs-enums.md)
- [安全等级](safety.md)


类使用共享引用；静态字段属于类本身。受检查的向下转换与自定义 Cast 方法参见[运算符](operators.md)。本页接口分派示例面向 Native，VM 限制参见[后端支持](../advanced/backend-support.md)。类声明不保证 C ABI 布局。
