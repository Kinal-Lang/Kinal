# Classes and Inheritance

## Class Declaration

```kinal
Class ClassName
{
    // fields
    // methods
    // constructors
}
```

Example:

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

## Fields

```kinal
Class Box
{
    Public int Width;            // public field
    Private float _weight;       // private field
    Public Static int Count = 0; // static field
}
```

Fields can have initial values:

```kinal
Class Config
{
    Public int MaxRetries = 3;
    Public string DefaultName = "unknown";
}
```

## Access Control

| Modifier | Description |
|----------|-------------|
| `Public` | Accessible from anywhere |
| `Private` | Accessible only within the class |
| `Protected` | Accessible within the class and derived classes |
| `Internal` | Accessible within the same module |

```kinal
Class Account
{
    Public string Owner;     // public
    Private float balance;   // private

    Public Function float GetBalance()
    {
        Return This.balance;  // private fields accessible internally
    }

    Private Function void Validate() { ... }
}
```

## Constructors

### Named Constructors

A method whose name matches the class name automatically becomes a constructor:

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

### The `Constructor` Keyword

Constructors can also be declared with the `Constructor` keyword:

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

### Default Constructor

If no constructor is declared, the compiler automatically generates a no-argument default constructor:

```kinal
Class Empty {}   // compiler auto-generates a default constructor

Empty e = New Empty();
```

### Default Parameter Values

Constructors also support default parameter values:

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

## The `This` Keyword

Inside a method, `This` refers to the current instance:

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

## Implicit This

When a field or method name does not conflict with a local variable, `This.` can be omitted:

```kinal
Class Node
{
    Public int Value;

    Public Function int Double()
    {
        Return Value * 2;   // implicit This.Value
    }
}
```

## Static Members

`Static` methods and fields belong to the class itself, not to instances:

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

## Static Classes

In a `Static Class`, all members are static and it cannot be instantiated:

```kinal
Static Class Utils
{
    Public Function string Reverse(string s)
    {
        // ...
    }
}

// Call directly, no New needed
string r = Utils.Reverse("hello");
```

Using `New` with a static class produces a compile error.

## Inheritance

Use the `By` keyword to specify the base class:

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
        This.Name = name;  // directly access base class field
    }

    Public Override Function string Sound()
    {
        Return "Woof!";
    }
}
```

### Calling Base Class Members: `Base`

```kinal
Class Derived By Base
{
    Public Override Function string Describe()
    {
        string parentDesc = Base.Describe();
        Return parentDesc + " (derived)";
    }
}
```

### Virtual Method Modifiers

| Modifier | Description |
|----------|-------------|
| `Virtual` | Method can be overridden by subclasses |
| `Override` | Overrides a virtual method from the base class |
| `Abstract` | Abstract method; must be implemented by subclasses |
| `Sealed` | Method cannot be further overridden by subclasses |

Abstract class example:

```kinal
Abstract Class Shape
{
    Public Abstract Function float Area();

    Public Function void Print()
    {
        IO.Console.PrintLine("Area: " + [string](This.Area()));
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

### Sealed Classes

A `Sealed Class` cannot be inherited:

```kinal
Sealed Class FinalNode
{
    Public int Value;
}
```

## Nested Types

Classes can declare nested classes, interfaces, structs, and enums:

```kinal
Class Container
{
    Class Iterator
    {
        Public int Index;
    }

    Enum State { Empty, Loading, Ready }

    // Using nested types
    Public Iterator GetIterator()
    {
        Return New Iterator();
    }
}
```

## null and Reference Safety

Class instance variables can be `null`; access should be checked first:

```kinal
MyClass obj = null;

If (obj != null)


{
    obj.DoSomething();
}
```

## Complete Example

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

## Next Steps

- [Interfaces](interfaces.md)
- [Structs and Enums](structs-enums.md)
- [Safety Levels](safety.md)
