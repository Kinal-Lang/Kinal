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
- `Sqrt`、`Cbrt`、`Pow`
- `Exp`、`Log`、`Log2`、`Log10`
- `Floor`、`Ceil`、`Round`、`Trunc`
- `FAbs`、`FMod`、`FMin`、`FMax`
- `CopySign`、`IsInf`、`IsNaN`
- `Lerp`、`ClampF64`、`ClampF32`
- `Radians`、`Degrees`

另外还有 `SinF`、`SqrtF`、`PowF`、`FloorF`、`RoundF`、`Atan2F` 这类 `f32` 便捷形式。

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
