# IO.Type：转换与值检查

本页列出 C 编译器内建的转换和成员辅助接口，无需安装额外包。声明语法与数值宽度见[类型系统](../language/types.md)；Native／VM 差异见[后端支持](../advanced/backend-support.md)。

## 显式转换

通常使用 `[类型](表达式)`；不要使用旧的 `int(value)` 写法。`IO.Type.<名称>(any value)` 是内建转换函数形式。以下名称各接受一个 `any` 参数，返回对应类型，均为 Safe：

| 名称 | 返回类型 |
|------|------|
| `bool`, `byte`, `char`, `int`, `float` | 与各名称相同 |
| `i8`, `i16`, `i32`, `i64` | 与各名称相同 |
| `u8`, `u16`, `u32`, `u64` | 与各名称相同 |
| `isize`, `usize` | 与各名称相同 |
| `f16`, `f32`, `f64`, `f128` | 与各名称相同 |
| `string`, `any`, `dict`, `list` | 与各名称相同 |

`any` 是动态载体，不表示任意转换都成功。转换不会代替输入验证；处理外部文本时先检查格式和范围。窄整数、带符号／无符号转换以及浮点转换受目标表示规则约束；VM 并不提供 f16／f128 精度对等保证。`dict`／`list` 转换不会将任意对象解析成集合，JSON 请使用 `IO.Json` 的检查与解析接口。

## any 成员辅助方法

参数表省略接收者 `any value`。例如 `value.IsInt()` 对应 `IO.Type.any.IsInt(value)`。

| 成员签名 | 含义 |
|------|------|
| `bool IsNull()` | 是否为空 |
| `string ToString()` | 值的显示文本 |
| `bool Equals(any other)` | 值相等检查 |
| `int Tag()` | 内部表示标签；勿持久化为公共类型编号 |
| `string TypeName()` | 运行时表示的类型名 |
| `bool IsNumber()` | 数值表示检查 |
| `bool IsPointer()` | 原始指针表示检查 |
| `bool IsInt()` | 整数表示检查 |
| `bool IsFloat()` | 浮点表示检查 |
| `bool IsString()` | 字符串检查 |
| `bool IsBool()` | 布尔值检查 |
| `bool IsChar()` | 字符表示检查 |
| `bool IsObject()` | 对象表示检查 |

这些检查描述运行时表示，并非完整源码类型反射。`value.TypeOf()` 提供编译期类型文字；`value.TypeName()` 描述后端运行时表示。例如同一个固定数组在 Native 中可能显示 `int[]`，在 VM 中显示 `array`。不要依赖 Tag 数字或显示名称来序列化对象、选择 FFI 布局或确定数组固定界限。

## string 成员辅助方法

| 成员签名 | 含义 |
|------|------|
| `int Length()` | 字节长度 |
| `string Concat(string other)` | 拼接文本 |
| `bool Equals(string other)` | 文本相等 |
| `char[] ToChars()` | 转换为字符数组 |

均为 Safe。字符串使用 NUL 结尾的字节文本；长度、索引和字符数组不等同于 Unicode 字素处理。更多搜索、切片、替换接口见 [IO.Text](text.md)。数组还支持自身的 `Length()`、`Add()`、`TypeOf` 等语言操作，不应将数组操作与 list 方法混用。

## 完整示例

```kinal
Get IO.Console;

Static Function int Main()
{
    any value = 42;
    IO.Console.PrintLine(value.IsInt(), value.IsNumber());
    IO.Console.PrintLine([string](value));
    string text = "Kinal";
    IO.Console.PrintLine(text.Length(), text.Equals("Kinal"));
    char[] chars = text.ToChars();
    IO.Console.PrintLine(chars.Length());
    Return 0;
}
```

```text
true true
42
5 true
5
```

`IO.Error` 参见[异常](../language/exceptions.md)，`IO.GC`／`IO.Memory` 参见[内存与生命周期](../advanced/memory-management.md)，`IO.Volatile`／`IO.Panic` 参见 [Freestanding](../advanced/freestanding.md)。
