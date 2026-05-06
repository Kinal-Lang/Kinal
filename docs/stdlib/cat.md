# IO.Cat

`IO.Cat` is a small sample package that exposes the `IO.Cat.Rommy` class.

## Import

```kinal
Get IO.Cat;
```

## Rommy

`IO.Cat.Rommy` is a light example class with a guarded constructor and a few text-returning interaction methods.

### Constructor

```kinal
IO.Cat.Rommy rommy = New IO.Cat.Rommy(true, true);
```

If either constructor argument is `false`, the package throws `IO.Error`.

### Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `Speak(something)` | `string → string` | Returns a response string |
| `Touch()` | `→ string` | Returns a short interaction string |
| `Feed()` | `→ string` | Returns a short interaction string |
| `Play()` | `→ string` | Returns a short interaction string |

## Example

```kinal
Unit App.CatDemo;

Get Console By IO.Console;
Get IO.Cat;

Static Function int Main()
{
    IO.Cat.Rommy rommy = New IO.Cat.Rommy(true, true);
    Console.PrintLine(rommy.Speak("hello"));
    Console.PrintLine(rommy.Touch());
    Return 0;
}
```

## Notes

- `IO.Cat` is an example package rather than a core runtime dependency.
- The constructor intentionally validates the two opt-in arguments before creating the instance.

## See Also

- [Standard Library Overview](overview.md)
- [IO.Console](console.md)
