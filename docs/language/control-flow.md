# Control Flow

## If / Else / ElseIf

Basic conditional statement:

```kinal
If (x > 0)
{
    IO.Console.PrintLine("positive");
}
Else If (x < 0)
{
    IO.Console.PrintLine("negative");
}
Else
{
    IO.Console.PrintLine("zero");
}
```

> `ElseIf` is equivalent to `Else If`; you can use either `ElseIf` (one keyword) or `Else If`.

Single-line mode (no braces, suitable for simple single statements):

```kinal
If (flag) IO.Console.PrintLine("flagged");
```

> It is recommended to always use braces to avoid ambiguity.

### Const If (Compile-Time Conditionals)

When the condition value is known at compile time (e.g., `IO.Target.*` platform constants), `If` becomes a compile-time branch, and branches that do not match **do not participate in semantic checking**:

```kinal
If (IO.Target.OS == IO.Target.OS.Windows)
{
    IO.Console.PrintLine("Windows");
}
Else
{
    UnknownCall();   // This branch is not compiled on non-Windows, so no error is reported
}
```

### `Is` Pattern Matching with `If`

```kinal
Animal a = New Dog();

If (a Is Dog dog)
{
    dog.Bark();   // dog has type Dog within this block
}
```

## While Loop

```kinal
int i = 0;
While (i < 5)
{
    IO.Console.PrintLine(i);
    i++;
}
```

`Break` exits the loop; `Continue` skips the current iteration:

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
    IO.Console.PrintLine(n);  // prints odd numbers
    n++;
}
```

## For Loop

```kinal
For (int i = 0; i < 10; i++)


{
    IO.Console.PrintLine(i);
}
```

Any part can be omitted:

```kinal
int i = 0;
For (; i < 10; )


{
    i++;
}

// Infinite loop
For (;;)


{
    Break;
}
```

The initialization part of a For loop can be a variable declaration or an expression:

```kinal
For (Var i = 0; i < 5; i++)


{
    IO.Console.PrintLine(i);
}
```

## Switch Statement

Switch provides multi-branch selection based on a value:

```kinal
int code = 2;
Switch (code)


{
    Case (1)


    {
        IO.Console.PrintLine("one");
    }
    Case (2)


    {
        IO.Console.PrintLine("two");
    }
    Case (default)


    {
        IO.Console.PrintLine("other");
    }
}
```

> Kinal's Switch has no fall-through (the next Case is not automatically executed), so `Break` is not needed.

### Switch Expression

Switch can also be used as an **expression**, returning a value:

```kinal
string label = Switch (code)


{
    Case (1) { "one" }
    Case (2) { "two" }
    Case (default) { "other" }
};
IO.Console.PrintLine(label);
```

Or in a more compact form (Case contains an expression directly):

```kinal
int doubled = Switch (x)


{
    Case (1) { 2 }
    Case (2) { 4 }
    Case (default) { 0 }
};
```

### Switch on Object Types

Switch can also branch on `any` or reference types by value:

```kinal
any val = 42;
Switch (val)


{
    Case (42) { IO.Console.PrintLine("forty-two"); }
    Case ("hello") { IO.Console.PrintLine("greeting"); }
    Case (default) { IO.Console.PrintLine("unknown"); }
}
```

## If Expression

`If` can also be used as an **expression** (similar to a ternary operator):

```kinal
int max = If (a > b) { a } Else { b };
IO.Console.PrintLine(max);
```

```kinal
string label = If (score >= 60) { "pass" } Else { "fail" };
```

## Break and Continue

`Break`: immediately exits the innermost loop.

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

`Continue`: skips the current iteration and continues with the next one.

```kinal
For (int i = 0; i < 10; i++)
{
    If (i % 2 == 0)
    {
        Continue;  // skip even numbers
    }
    IO.Console.PrintLine(i);  // prints only odd numbers
}
```

## Complete Example with Control Flow

```kinal
Unit App.Demo;
Get IO.Console;

Static Function int Main()
{
    // For loop printing odd numbers from 1 to 10
    For (int i = 1; i <= 10; i++)
    {
        If (i % 2 == 0)
        {
            Continue;
        }
        IO.Console.PrintLine(i);
    }

    // Switch branch example
    string day = "Monday";
    Switch (day)
    {
        Case ("Saturday")
        {
            IO.Console.PrintLine("weekend");
        }
        Case ("Sunday")
        {
            IO.Console.PrintLine("weekend");
        }
        Case (default)
        {
            IO.Console.PrintLine("weekday");
        }
    }

    Return 0;
}
```

> **Note**: The `Case ("Saturday")` and `Case ("Sunday")` above — Kinal currently **does not support** Case stacking (fall-through).  
> For multi-value matching, use `||` inside a Case, or use the `Is` pattern.

## Next Steps

- [Functions](functions.md)
- [Exception Handling](exceptions.md)
- [Block (Code Block Objects)](blocks.md)
