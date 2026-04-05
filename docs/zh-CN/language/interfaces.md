# 接口

接口定义了一组方法签名的契约，实现接口的类必须提供所有方法的具体实现。

## 接口声明

```kinal
Interface IShape
{
    Function float Area();
    Function float Perimeter();
}
```

接口只能声明**方法签名**，不能有字段或方法实现。

## 实现接口

使用 `By 接口名` 声明类实现某接口：

```kinal
Class Circle By IShape
{
    Private float radius;

    Public Constructor(float r)
    {
        This.radius = r;
    }

    Public Function float Area()
    {
        Return 3.14159 * This.radius * This.radius;
    }

    Public Function float Perimeter()
    {
        Return 2.0 * 3.14159 * This.radius;
    }
}
```

`By` 后同时跟类和接口时，第一个是父类（其余是接口），但 Kinal 目前以父类和多接口均使用 `By` 语法表示。

实现多个接口：

```kinal
Interface IDrawable
{
    Function void Draw();
}

Interface IResizable
{
    Function void Resize(float factor);
}

Class Rectangle By IDrawable, IResizable
{
    // 必须实现两个接口的所有方法
    Public Function void Draw() { ... }
    Public Function void Resize(float factor) { ... }
}
```

## 通过接口类型使用

接口类型变量可以持有任何实现该接口的类实例：

```kinal
IShape s = New Circle(5.0);
IO.Console.PrintLine([string](s.Area()));

IShape t = New Rectangle(3.0, 4.0);  // 也实现 IShape
IO.Console.PrintLine([string](t.Area()));
```

## 接口的类型判断

结合 `Is` 运算符检查某对象是否实现了某接口：

```kinal
Object obj = ...;
If (obj Is IShape shape)


{
    IO.Console.PrintLine(shape.Area());
}
```

## 方法访问修饰符

接口方法默认是 `Public`。在类中实现接口方法时，仍需显式写访问修饰符：

```kinal
Interface ILogger
{
    Function void Log(string msg);
}

Class FileLogger By ILogger
{
    Public Function void Log(string msg) {  // Public 必须写明
        // 写入文件...
    }
}
```

## 嵌套在类中的接口

接口可以嵌套在类体内：

```kinal
Class Workflow
{
    Interface IStep
    {
        Function bool Execute();
    }

    Class StartStep By IStep
    {
        Public Function bool Execute()
        {
            Return true;
        }
    }
}
```

## 接口与继承

类可以同时继承一个父类并实现多个接口：

```kinal
Class Sprite By GameObject, IDrawable, ICollidable
{
    Public Override Function void Update() { ... }
    Public Function void Draw() { ... }
    Public Function bool CheckCollision(ICollidable other) { Return false; }
}
```

## 完整示例

```kinal
Unit App.Shapes;

Get IO.Console;

Interface IShape
{
    Function float Area();
    Function string Name();
}

Class Circle By IShape
{
    Private float r;
    Public Constructor(float r) { This.r = r; }
    Public Function float Area() { Return 3.14159 * This.r * This.r; }
    Public Function string Name() { Return "Circle"; }
}

Class Square By IShape
{
    Private float side;
    Public Constructor(float s) { This.side = s; }
    Public Function float Area() { Return This.side * This.side; }
    Public Function string Name() { Return "Square"; }
}

Static Function void PrintShapeInfo(IShape shape)
{
    IO.Console.PrintLine(shape.Name() + " area = " + [string](shape.Area()));
}

Static Function int Main()
{
    IShape c = New Circle(3.0);
    IShape s = New Square(4.0);

    PrintShapeInfo(c);
    PrintShapeInfo(s);
    Return 0;
}
```

## 下一步

- [结构体与枚举](structs-enums.md)
- [类与继承](classes.md)
- [泛型](generics.md)
