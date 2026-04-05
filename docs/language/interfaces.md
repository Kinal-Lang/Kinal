# Interfaces

An interface defines a contract of method signatures; any class implementing the interface must provide concrete implementations for all methods.

## Interface Declaration

```kinal
Interface IShape
{
    Function float Area();
    Function float Perimeter();
}
```

Interfaces can only declare **method signatures**; they cannot have fields or method implementations.

## Implementing an Interface

Use `By InterfaceName` to declare that a class implements an interface:

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

When `By` is followed by both a class and interfaces, the first is the base class (the rest are interfaces), though in Kinal both base classes and multiple interfaces are specified with the `By` syntax.

Implementing multiple interfaces:

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
    // Must implement all methods from both interfaces
    Public Function void Draw() { ... }
    Public Function void Resize(float factor) { ... }
}
```

## Using Interface Types

An interface-typed variable can hold an instance of any class that implements the interface:

```kinal
IShape s = New Circle(5.0);
IO.Console.PrintLine([string](s.Area()));

IShape t = New Rectangle(3.0, 4.0);  // also implements IShape
IO.Console.PrintLine([string](t.Area()));
```

## Type Checking on Interfaces

Use the `Is` operator to check whether an object implements an interface:

```kinal
Object obj = ...;
If (obj Is IShape shape)


{
    IO.Console.PrintLine(shape.Area());
}
```

## Method Access Modifiers

Interface methods are `Public` by default. When implementing interface methods in a class, the access modifier must still be written explicitly:

```kinal
Interface ILogger
{
    Function void Log(string msg);
}

Class FileLogger By ILogger
{
    Public Function void Log(string msg) {  // Public must be explicit
        // write to file...
    }
}
```

## Interfaces Nested Inside Classes

Interfaces can be nested inside a class body:

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

## Interfaces and Inheritance

A class can simultaneously inherit from one base class and implement multiple interfaces:

```kinal
Class Sprite By GameObject, IDrawable, ICollidable
{
    Public Override Function void Update() { ... }
    Public Function void Draw() { ... }
    Public Function bool CheckCollision(ICollidable other) { Return false; }
}
```

## Complete Example

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

## Next Steps

- [Structs and Enums](structs-enums.md)
- [Classes and Inheritance](classes.md)
- [Generics](generics.md)
