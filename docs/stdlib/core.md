# IO.Core

`IO.Core` is the official package containing IO.Char/IO.Math and the public Console, File, Directory, Memory, Path, System, Text and Time Units. The Unit name used by Get is distinct from the package identity.

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
- `Sqrt`, `Cbrt`, `Pow`, `Hypot`
- `Exp`, `Log`, `Log2`, `Log10`
- `Floor`, `Ceil`, `Round`, `Trunc`
- `FAbs`, `FMod`, `FMin`, `FMax`
- `CopySign`, `IsInf`, `IsNaN`
- `Lerp`, `ClampF64`, `ClampF32`
- `Radians`, `Degrees`

There are also `f32` convenience forms such as `SinF`, `SqrtF`, `PowF`, `FloorF`, `RoundF`, and `Atan2F`.

These functions are available on Native and the matching KinalVM. Use `IO.Math.Hypot(dx, dy)` for a 2D distance. For example, `Hypot(3.0, 4.0)` returns `5.0`. It avoids intermediate overflow and underflow from squaring the inputs directly. `HypotF` takes and returns `f32`.

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

## Complete IO.Math Signatures

All public class methods in the current IO.Core source are listed below. `Get IO;` exposes IO.Math and IO.Char. Trusted methods may be called by Safe functions, while their native math dependencies must still be available for the target.

| Function | Signature | Safety |
|----------|-----------|--------|
| `IO.Math.Sin` | `f64 Sin(f64 x)` | Trusted |
| `IO.Math.Cos` | `f64 Cos(f64 x)` | Trusted |
| `IO.Math.Tan` | `f64 Tan(f64 x)` | Trusted |
| `IO.Math.Asin` | `f64 Asin(f64 x)` | Trusted |
| `IO.Math.Acos` | `f64 Acos(f64 x)` | Trusted |
| `IO.Math.Atan` | `f64 Atan(f64 x)` | Trusted |
| `IO.Math.Atan2` | `f64 Atan2(f64 y, f64 x)` | Trusted |
| `IO.Math.Sinh` | `f64 Sinh(f64 x)` | Trusted |
| `IO.Math.Cosh` | `f64 Cosh(f64 x)` | Trusted |
| `IO.Math.Tanh` | `f64 Tanh(f64 x)` | Trusted |
| `IO.Math.Sqrt` | `f64 Sqrt(f64 x)` | Trusted |
| `IO.Math.Hypot` | `f64 Hypot(f64 x, f64 y)` | Trusted |
| `IO.Math.Cbrt` | `f64 Cbrt(f64 x)` | Trusted |
| `IO.Math.Pow` | `f64 Pow(f64 base, f64 exponent)` | Trusted |
| `IO.Math.Exp` | `f64 Exp(f64 x)` | Trusted |
| `IO.Math.Log` | `f64 Log(f64 x)` | Trusted |
| `IO.Math.Log2` | `f64 Log2(f64 x)` | Trusted |
| `IO.Math.Log10` | `f64 Log10(f64 x)` | Trusted |
| `IO.Math.Floor` | `f64 Floor(f64 x)` | Trusted |
| `IO.Math.Ceil` | `f64 Ceil(f64 x)` | Trusted |
| `IO.Math.Round` | `f64 Round(f64 x)` | Trusted |
| `IO.Math.Trunc` | `f64 Trunc(f64 x)` | Trusted |
| `IO.Math.FAbs` | `f64 FAbs(f64 x)` | Trusted |
| `IO.Math.FMod` | `f64 FMod(f64 x, f64 y)` | Trusted |
| `IO.Math.FMin` | `f64 FMin(f64 x, f64 y)` | Trusted |
| `IO.Math.FMax` | `f64 FMax(f64 x, f64 y)` | Trusted |
| `IO.Math.CopySign` | `f64 CopySign(f64 x, f64 y)` | Trusted |
| `IO.Math.IsInf` | `bool IsInf(f64 x)` | Trusted |
| `IO.Math.IsNaN` | `bool IsNaN(f64 x)` | Trusted |
| `IO.Math.SinF` | `f32 SinF(f32 x)` | Trusted |
| `IO.Math.CosF` | `f32 CosF(f32 x)` | Trusted |
| `IO.Math.TanF` | `f32 TanF(f32 x)` | Trusted |
| `IO.Math.SqrtF` | `f32 SqrtF(f32 x)` | Trusted |
| `IO.Math.HypotF` | `f32 HypotF(f32 x, f32 y)` | Trusted |
| `IO.Math.PowF` | `f32 PowF(f32 base, f32 exponent)` | Trusted |
| `IO.Math.FloorF` | `f32 FloorF(f32 x)` | Trusted |
| `IO.Math.CeilF` | `f32 CeilF(f32 x)` | Trusted |
| `IO.Math.RoundF` | `f32 RoundF(f32 x)` | Trusted |
| `IO.Math.FAbsF` | `f32 FAbsF(f32 x)` | Trusted |
| `IO.Math.Atan2F` | `f32 Atan2F(f32 y, f32 x)` | Trusted |
| `IO.Math.Abs` | `int Abs(int value)` | Safe |
| `IO.Math.Min` | `int Min(int left, int right)` | Safe |
| `IO.Math.Max` | `int Max(int left, int right)` | Safe |
| `IO.Math.Clamp` | `int Clamp(int value, int minValue, int maxValue)` | Safe |
| `IO.Math.Sign` | `int Sign(int value)` | Safe |
| `IO.Math.Lerp` | `f64 Lerp(f64 a, f64 b, f64 t)` | Trusted |
| `IO.Math.LerpF` | `f32 LerpF(f32 a, f32 b, f32 t)` | Trusted |
| `IO.Math.ClampF64` | `f64 ClampF64(f64 value, f64 minVal, f64 maxVal)` | Trusted |
| `IO.Math.ClampF32` | `f32 ClampF32(f32 value, f32 minVal, f32 maxVal)` | Trusted |
| `IO.Math.Radians` | `f64 Radians(f64 degrees)` | Trusted |
| `IO.Math.Degrees` | `f64 Degrees(f64 radians)` | Trusted |
| `IO.Math.RadiansF` | `f32 RadiansF(f32 degrees)` | Trusted |
| `IO.Math.DegreesF` | `f32 DegreesF(f32 radians)` | Trusted |

The five constants IO.PI, IO.E, IO.TAU, IO.DEG2RAD and IO.RAD2DEG are f64 values in Unit IO. F32 convenience wrappers can compute through f64 before narrowing; they are not a general independent libm implementation. Domain errors, NaN/Inf and rounding follow the underlying operation; check IsNaN/IsInf when relevant. An imported package does not make every math wrapper KNC-compatible.

## Other IO.Core Units

- [IO.Console](console.md)
- [IO.File / IO.Directory](filesystem.md)
- [IO.Path](path.md)
- [IO.System](system.md)
- [IO.Text](text.md)
- [IO.Time](time.md)
- [IO.Memory / IO.GC](../advanced/memory-management.md)
