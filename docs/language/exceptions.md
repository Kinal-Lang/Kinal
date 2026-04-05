# Exception Handling

Kinal implements exception handling through `Try`/`Catch`/`Throw`; exception objects can be of any type.

## Basic Syntax

```kinal
Try
{
    // code that may throw an exception
}
Catch (Type variableName)
{
    // handle the exception
}
```

## Throw

`Throw` throws an exception; the value can be of any type:

```kinal
Throw "something went wrong";             // throw a string
Throw New IO.Error("Title", "Details");   // throw an IO.Error object
Throw 42;                                  // throw an integer (not recommended but valid)
```

## Try/Catch

Catching a specific type of exception:

```kinal
Try
{
    Throw "boom";
}
Catch (string e)
{
    IO.Console.PrintLine("Caught string exception: " + e);
}
```

Catching an `IO.Error` object:

```kinal
Try
{
    Throw New IO.Error("File Error", "File not found");
}
Catch (IO.Error err)
{
    IO.Console.PrintLine(err.Title);
    IO.Console.PrintLine(err.Message);
}
```

## IO.Error — Standard Exception Class

`IO.Error` is Kinal's built-in standard exception class with the following fields:

| Field | Type | Description |
|-------|------|-------------|
| `Title` | `string` | Exception title (brief description) |
| `Message` | `string` | Detailed information |
| `Inner` | `IO.Error` | Nested root-cause exception (can be `null`) |
| `Trace` | `string` | Call stack trace (filled in automatically) |

```kinal
IO.Error err = New IO.Error("Network Error", "Connection timed out");
IO.Console.PrintLine(err.Title);    // Network Error
IO.Console.PrintLine(err.Message);  // Connection timed out
```

### Exception Chaining

Use the `Inner` field to chain root causes:

```kinal
Try
{
    // low-level operation
    IO.Error inner = New IO.Error("Socket Error", "Connection refused");
    Throw New IO.Error("HTTP Request Failed", "Unable to establish connection", inner);
}
Catch (IO.Error err)
{
    IO.Console.PrintLine(err.Title);
    IO.Console.PrintLine(err.Message);
    If (err.Inner != null)
    {
        IO.Console.PrintLine("Root cause: " + err.Inner.Title);
    }
}
```

Complete constructor forms:

```kinal
// IO.Error(title, message) — two-parameter version
IO.Error e1 = New IO.Error("Title", "Message");

// Setting Inner via a field after construction
IO.Error e2 = New IO.Error("Outer", "Outer message");
// Note: the Inner field needs to be set separately after construction (see chained exception test below)
```

## Catch Without Parameters

If you don't need the exception information, you can omit the `Catch` parameters (catches all types):

```kinal
Try
{
    RiskyOperation();
} Catch
{
    IO.Console.PrintLine("An exception occurred");
}
```

## Returning via Throw

When returning from a function via an exception:

```kinal
Function int SafeDivide(int a, int b)
{
    If (b == 0)
    {
        Throw New IO.Error("Division Error", "Divisor cannot be zero");
    }
    Return a / b;
}

Try
{
    int result = SafeDivide(10, 0);
}
Catch (IO.Error err)
{
    IO.Console.PrintLine(err.Title);  // Division Error
}
```

## Complete Example

```kinal
Unit App.ExceptionDemo;

Get IO.Console;

Function int Divide(int a, int b)
{
    If (b == 0)
    {
        Throw New IO.Error("ArithmeticError", "Division by zero");
    }
    Return a / b;
}

Function void Risky()
{
    // Nested Try
    Try
    {
        int r = Divide(10, 0);
        IO.Console.PrintLine(r);
    }
    Catch (IO.Error inner)
    {
        Throw New IO.Error("Operation Failed", "Calculation failed", inner);
    }
}

Static Function int Main()
{
    Try
    {
        Risky();
    }
    Catch (IO.Error err)
    {
        IO.Console.PrintLine("Error: " + err.Title);
        IO.Console.PrintLine("Message: " + err.Message);
        If (err.Inner != null)
        {
            IO.Console.PrintLine("Root cause: " + err.Inner.Title + " - " + err.Inner.Message);
        }
    }
    Catch (string msg)
    {
        IO.Console.PrintLine("String exception: " + msg);
    }

    Return 0;
}
```

## Important Notes

- `Catch` only catches exceptions whose type **exactly matches**; Kinal currently does not support polymorphic catching (i.e., `Catch (IO.Error e)` does not catch subclasses of `IO.Error`)
- Uncaught exceptions will terminate the program
- Exception handling is also available inside `Safe` functions

## Next Steps

- [Classes and Inheritance](classes.md)
- [Safety Levels](safety.md)
