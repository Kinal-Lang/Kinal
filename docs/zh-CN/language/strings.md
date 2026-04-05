# 字符串特性

Kinal 的字符串支持多种字面量前缀、格式化字符串和字符数组转换。

## 基本字符串

```kinal
string s = "Hello, World!";
string path = "C:\\Users\\test";  // 使用转义序列
```

标准转义序列：

| 转义 | 含义 |
|------|------|
| `\n` | 换行 |
| `\r` | 回车 |
| `\t` | 制表符 |
| `\\` | 反斜杠 |
| `\"` | 双引号 |
| `\'` | 单引号 |
| `\0` | 空字符 |

## 字符串前缀

### `[r]` — 原始字符串

原始字符串中，反斜杠不作为转义字符处理：

```kinal
string raw = [r]"C:\Users\test\n";
// raw 的值为字面量 "C:\Users\test\n"（6个字符，不含换行）
Console.PrintLine(raw.Length());  // 18（实际字符数）
```

等价别名：`[raw]`

### `[f]`/`[format]` — 格式化字符串（字符串插值）

在双花括号内插入表达式：

```kinal
int value = 42;
string name = "Kinal";
string msg = [f]"value={value}, name={name}";
Console.PrintLine(msg);  // value=42, name=Kinal
```

表达式插值：

```kinal
int x = 5;
Console.PrintLine([f]"x squared = {x * x}");  // x squared = 25
Console.PrintLine([format]"sum={1 + 2 + 3}");  // sum=6
```

花括号转义（双花括号输出单花括号）：

```kinal
Console.PrintLine([f]"{{{value}}}");  // {42}
```

等价别名：`[format]`

### `[c]`/`[chars]` — 字符数组

将字符串字面量转换为 `char[]` 数组：

```kinal
char[] chars = [c]"abc";
Console.PrintLine(chars.Length());  // 3
Console.PrintLine(chars[0]);        // a
Console.PrintLine(chars[2]);        // c
```

等价别名：`[chars]`

### 前缀组合

前缀可以组合使用，从左到右依次应用：

```kinal
int val = 7;
// 先格式化，再转为字符数组
Var chars = [f][c]"z={val}";
// 等价于先 [f]"z={val}" → "z=7"，再 [c]"z=7" → ['z','=','7']
Console.PrintLine(chars.Length());  // 3

// 先应用 raw，再转为字符数组
Var rawChars = [chars][raw]"\n";
// [raw]"\n" → "\n"（两个字符：\和n），再 [chars] → ['\','n']
Console.PrintLine(rawChars.Length()); // 2
```

## 字符串拼接

使用 `+` 拼接字符串：

```kinal
string greeting = "Hello" + ", " + "World" + "!";
```

数值转字符串再拼接：

```kinal
int n = 42;
string msg = "数字是: " + [string](n);
```

或使用格式化字符串：

```kinal
string msg = [f]"数字是: {n}";
```

## 字符串方法

字符串方法通过 `IO.Text` 模块提供（详见 [IO.Text](../stdlib/text.md)）：

```kinal
Get IO.Text;

string s = "Hello, World!";

IO.Text.Length(s);          // 13
IO.Text.Contains(s, "World");  // true
IO.Text.StartsWith(s, "Hello"); // true
IO.Text.ToUpper(s);         // "HELLO, WORLD!"
IO.Text.Slice(s, 7, 5);     // "World"
IO.Text.Replace(s, "World", "Kinal"); // "Hello, Kinal!"
IO.Text.Split(s, ", ");     // list: ["Hello", "World!"]
```

字符串内置的方法（直接调用，无需 `IO.Text`）：

```kinal
s.Length();    // 字符长度
s.Equals(t);  // 内容比较
s.ToString(); // 返回自身（any 接口兼容）
```

## 字符串比较

```kinal
string a = "hello";
string b = "hello";
bool same = a == b;   // true（内容比较）
bool diff = a != b;   // false
```

## 字符与字符串互转

```kinal
// char 转 string
char c = 'A';
string s = [string](c);    // "A"（单字符字符串）

// int → char → string
int code = 65;
char ch = [char](code);    // 'A'

// 字符串转字符数组
char[] arr = IO.Type.string.ToChars("abc");  // ['a', 'b', 'c']
// 或
char[] arr2 = [c]"abc";
```

## char* 与 string 互操作

在 Unsafe 上下文中，`string` 可转换为 `char*` 或 `byte*`（C 字符串）：

```kinal
Unsafe Function void Demo(string s)
{
    byte* ptr = [byte*](s);   // 获取底层 C 字符串指针
}
```

`char*` 与 `byte*` 的拼接有限制：不能将 `char*` 与 `string` 直接拼接（编译器会报错），必须先转换回 `string`。

## 字符串到数值

```kinal
int n = [int]("42");       // 42
float f = [float]("3.14"); // 3.14
bool b = [bool]("1");      // true
bool b2 = [bool]("true");  // true
```

## 下一步

- [IO.Text 标准库](../stdlib/text.md)
- [类型系统](types.md)
