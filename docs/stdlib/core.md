# IO.Core

`IO.Core` is the official package that provides `IO.Char` and `IO.Math`.

## Import

```kinal
Get IO;
```

`IO.Char` and `IO.Math` live under the `IO` unit once the package is available to the project.

## IO.Char

`IO.Char` provides small character classification and ASCII case-conversion helpers.

### Function Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `IsDigit(value)` | `char → bool` | Whether the character is `0`-`9` |
| `IsLower(value)` | `char → bool` | Whether the character is lowercase ASCII |
| `IsUpper(value)` | `char → bool` | Whether the character is uppercase ASCII |
| `IsLetter(value)` | `char → bool` | Whether the character is an ASCII letter |
| `IsWhitespace(value)` | `char → bool` | Whether the character is space, tab, CR, or LF |
| `ToLower(value)` | `char → char` | Convert uppercase ASCII to lowercase |
| `ToUpper(value)` | `char → char` | Convert lowercase ASCII to uppercase |

```kinal
IO.Char.IsDigit('7');       // true
IO.Char.IsLetter('Q');      // true
IO.Char.ToLower('R');       // 'r'
IO.Char.ToUpper('m');       // 'M'
```

## IO.Math

`IO.Math` mixes pure Kinal integer helpers with floating-point wrappers backed by the runtime math bridge.

### Integer Helpers

| Function | Signature | Description |
|----------|-----------|-------------|
| `Abs(value)` | `int → int` | Absolute value |
| `Min(left, right)` | `int, int → int` | Smaller of two integers |
| `Max(left, right)` | `int, int → int` | Larger of two integers |
| `Clamp(value, minValue, maxValue)` | `int, int, int → int` | Clamp to range |
| `Sign(value)` | `int → int` | `-1`, `0`, or `1` |

### Floating-Point Helpers

Common floating-point helpers include:

- `Sin`, `Cos`, `Tan`
- `Asin`, `Acos`, `Atan`, `Atan2`
- `Sinh`, `Cosh`, `Tanh`
- `Sqrt`, `Cbrt`, `Pow`
- `Exp`, `Log`, `Log2`, `Log10`
- `Floor`, `Ceil`, `Round`, `Trunc`
- `FAbs`, `FMod`, `FMin`, `FMax`
- `CopySign`, `IsInf`, `IsNaN`
- `Lerp`, `ClampF64`, `ClampF32`
- `Radians`, `Degrees`

There are also `f32` convenience forms such as `SinF`, `SqrtF`, `PowF`, `FloorF`, `RoundF`, and `Atan2F`.

### Constants

The package also exposes:

- `IO.PI`
- `IO.E`
- `IO.TAU`
- `IO.DEG2RAD`
- `IO.RAD2DEG`

## Example

```kinal
Unit App.CoreDemo;

Get Console By IO.Console;
Get IO;

Static Function int Main()
{
    Console.PrintLine(IO.Char.IsDigit('5'));
    Console.PrintLine(IO.Char.ToUpper('q'));

    Console.PrintLine(IO.Math.Abs(-5));
    Console.PrintLine(IO.Math.Clamp(15, 0, 10));
    Console.PrintLine(IO.Math.Sqrt(16.0));
    Console.PrintLine(IO.Math.Radians(180.0));
    Return 0;
}
```

## Notes

- `IO.Char` is ASCII-oriented.
- `IO.Math` floating-point functions are trusted wrappers over runtime-provided C math functions.

## See Also

- [Standard Library Overview](overview.md)
- [IO.Text](text.md)
- [IO.Time](time.md)
