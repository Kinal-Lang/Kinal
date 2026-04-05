# IO.Console — Console I/O

`IO.Console` provides standard console input/output capabilities.

## Import

```kinal
Get IO.Console;
// or
Get IO.Console;
```

## Function Reference

### PrintLine

Prints a line of content followed by a newline (`\n`).

**Signature:** `PrintLine(any... args) → void` (variadic, Safety: Safe)

```kinal
IO.Console.PrintLine("Hello, World!");
IO.Console.PrintLine(42);
IO.Console.PrintLine(true);
IO.Console.PrintLine();                         // Print empty line

// Multiple arguments: print each in sequence, separated by spaces
IO.Console.PrintLine("a=", 1, ", b=", true);
```

Output:
```
Hello, World!
42
true

a= 1 , b= true
```

### Print

Prints content **without a newline**.

**Signature:** `Print(any... args) → void` (variadic, Safety: Safe)

```kinal
IO.Console.Print("Hello");
IO.Console.Print(", ");
IO.Console.Print("World");
IO.Console.PrintLine();  // Manual newline
```

### ReadLine

Reads a line of user input from the console (blocking, waits for Enter).

**Signature:** `ReadLine() → string` (Safety: Safe)

```kinal
IO.Console.Print("Enter your name: ");
string name = IO.Console.ReadLine();
IO.Console.PrintLine("Hello, " + name);
```

## Complete Example

```kinal
Unit App;

Get IO.Console;

Static Function int Main()
{
    // Basic output
    IO.Console.PrintLine("Welcome to Kinal!");
    IO.Console.PrintLine();

    // Numeric output
    int n = 42;
    float pi = 3.14159;
    IO.Console.PrintLine("Integer: ", n);
    IO.Console.PrintLine("Float: ", pi);

    // Formatted string
    string msg = [f]"n={n}, pi={pi}";
    IO.Console.PrintLine(msg);

    // Read input
    IO.Console.Print("Enter a number: ");
    string input = IO.Console.ReadLine();
    int value = [int](input);
    IO.Console.PrintLine("You entered: ", value);
    IO.Console.PrintLine("Its square is: ", value * value);

    Return 0;
}
```

## Notes

- `PrintLine` and `Print` automatically convert arguments to strings (via `any.ToString()`)
- Printing `null` outputs `"null"`
- With multiple arguments, each argument is joined with a space or no separator (depending on implementation)

## See Also

- [IO.Text](text.md) — String processing
- [String Features](../language/strings.md)
