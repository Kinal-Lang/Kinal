# IO.Core

`IO.Core` 是官方基础包，提供 `IO.Char` 和 `IO.Math`。

## 导入

```kinal
Get IO;
```

只要项目中已经接入这个包，`IO.Char` 和 `IO.Math` 就会挂在 `IO` 单元下。

## IO.Char

`IO.Char` 提供简单的字符分类与 ASCII 大小写转换函数。

### 函数参考

| 函数 | 签名 | 说明 |
|------|------|------|
| `IsDigit(value)` | `char → bool` | 是否为 `0`-`9` |
| `IsLower(value)` | `char → bool` | 是否为 ASCII 小写字母 |
| `IsUpper(value)` | `char → bool` | 是否为 ASCII 大写字母 |
| `IsLetter(value)` | `char → bool` | 是否为 ASCII 字母 |
| `IsWhitespace(value)` | `char → bool` | 是否为空格、制表、回车或换行 |
| `ToLower(value)` | `char → char` | 将 ASCII 大写转小写 |
| `ToUpper(value)` | `char → char` | 将 ASCII 小写转大写 |

```kinal
IO.Char.IsDigit('7');       // true
IO.Char.IsLetter('Q');      // true
IO.Char.ToLower('R');       // 'r'
IO.Char.ToUpper('m');       // 'M'
```

## IO.Math

`IO.Math` 同时提供纯 Kinal 的整数工具和通过运行时数学桥接实现的浮点函数。

### 整数函数

| 函数 | 签名 | 说明 |
|------|------|------|
| `Abs(value)` | `int → int` | 绝对值 |
| `Min(left, right)` | `int, int → int` | 两个整数中的较小值 |
| `Max(left, right)` | `int, int → int` | 两个整数中的较大值 |
| `Clamp(value, minValue, maxValue)` | `int, int, int → int` | 限制到指定范围 |
| `Sign(value)` | `int → int` | 返回 `-1`、`0` 或 `1` |

### 浮点函数

常用浮点函数包括：

- `Sin`、`Cos`、`Tan`
- `Asin`、`Acos`、`Atan`、`Atan2`
- `Sinh`、`Cosh`、`Tanh`
- `Sqrt`、`Cbrt`、`Pow`、`Hypot`
- `Exp`、`Log`、`Log2`、`Log10`
- `Floor`、`Ceil`、`Round`、`Trunc`
- `FAbs`、`FMod`、`FMin`、`FMax`
- `CopySign`、`IsInf`、`IsNaN`
- `Lerp`、`ClampF64`、`ClampF32`
- `Radians`、`Degrees`

另外还有 `SinF`、`SqrtF`、`PowF`、`FloorF`、`RoundF`、`Atan2F` 这类 `f32` 便捷形式。

原生后端与配套的 KinalVM 都支持本节的数学函数。计算二维距离可用 `IO.Math.Hypot(dx, dy)`。例如 `Hypot(3.0, 4.0)` 返回 `5.0`。即使直接计算平方会溢出或下溢，`Hypot` 也能给出量程内的结果。`HypotF` 接受并返回 `f32`。

### 常量

这个包还暴露了：

- `IO.PI`
- `IO.E`
- `IO.TAU`
- `IO.DEG2RAD`
- `IO.RAD2DEG`

## 示例

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

## 说明

- `IO.Char` 面向 ASCII 字符。
- `IO.Math` 的浮点函数是对运行时 C 数学函数的 trusted 封装。

## 相关

- [标准库概览](overview.md)
- [IO.Text](text.md)
- [IO.Time](time.md)

## 完整 IO.Math 签名

下表覆盖当前 IO.Core 源码的全部公开类方法。Get IO 暴露 IO.Math 和 IO.Char。Trusted 可由 Safe 调用，但目标仍需提供原生数学依赖。

| 函数 | 签名 | 安全等级 |
|------|------|----------|
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

IO.PI、IO.E、IO.TAU、IO.DEG2RAD、IO.RAD2DEG 是 Unit IO 的 f64 常量。F32 便捷包装可先按 f64 计算再收窄，不是独立通用 libm 实现。定义域错误、NaN／Inf 和舍入遵守底层操作，必要时检查 IsNaN／IsInf。包可导入不代表所有数学包装均兼容 KNC。

## 其他 IO.Core Unit

- [IO.Console](console.md)
- [IO.File / IO.Directory](filesystem.md)
- [IO.Path](path.md)
- [IO.System](system.md)
- [IO.Text](text.md)
- [IO.Time](time.md)
- [IO.Memory / IO.GC](../advanced/memory-management.md)
