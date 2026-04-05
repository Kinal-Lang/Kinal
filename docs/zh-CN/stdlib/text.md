# IO.Text — 字符串操作

`IO.Text` 提供字符串查询、转换、分割等工具函数。

## 导入

```kinal
Get IO.Text;
```

## 函数参考

### 长度与检查

| 函数 | 签名 | 说明 |
|------|------|------|
| `Length(s)` | `string → int` | 字符串字节长度 |
| `IsEmpty(s)` | `string → bool` | 是否为空字符串 |
| `Contains(s, sub)` | `string, string → bool` | 是否包含子串 |
| `StartsWith(s, prefix)` | `string, string → bool` | 是否以指定前缀开头 |
| `EndsWith(s, suffix)` | `string, string → bool` | 是否以指定后缀结尾 |

```kinal
string s = "Hello, Kinal!";

IO.Text.Length(s);              // 13
IO.Text.IsEmpty("");            // true
IO.Text.Contains(s, "Kinal");  // true
IO.Text.StartsWith(s, "Hello"); // true
IO.Text.EndsWith(s, "!");      // true
```

### 搜索

| 函数 | 签名 | 说明 |
|------|------|------|
| `IndexOf(s, sub)` | `string, string → int` | 第一次出现位置，-1 表示未找到 |
| `LastIndexOf(s, sub)` | `string, string → int` | 最后一次出现位置 |
| `Count(s, sub)` | `string, string → int` | 子串出现次数 |

```kinal
IO.Text.IndexOf("abcabc", "bc");      // 1
IO.Text.LastIndexOf("abcabc", "bc"); // 4
IO.Text.Count("banana", "an");        // 2
```

### 变换

| 函数 | 签名 | 说明 |
|------|------|------|
| `ToUpper(s)` | `string → string` | 转大写 |
| `ToLower(s)` | `string → string` | 转小写 |
| `Trim(s)` | `string → string` | 去除首尾空白 |
| `Replace(s, old, new)` | `string, string, string → string` | 替换所有匹配子串 |
| `Slice(s, start, length)` | `string, int, int → string` | 截取子串 |

```kinal
IO.Text.ToUpper("hello");            // "HELLO"
IO.Text.ToLower("WORLD");            // "world"
IO.Text.Trim("  hello  ");           // "hello"
IO.Text.Replace("aXbXc", "X", "-"); // "a-b-c"
IO.Text.Slice("Hello, World!", 7, 5); // "World"
```

### 分割与合并

| 函数 | 签名 | 说明 |
|------|------|------|
| `Split(s, sep)` | `string, string → list` | 按分隔符分割，返回 `list` |
| `Join(parts, sep)` | `list, string → string` | 用分隔符合并 list 中的字符串 |
| `Lines(s)` | `string → list` | 按行分割 |

```kinal
list parts = IO.Text.Split("a,b,c", ",");
// parts = ["a", "b", "c"]

string joined = IO.Text.Join(parts, " | ");
// "a | b | c"

list lines = IO.Text.Lines("line1\nline2\nline3");
// ["line1", "line2", "line3"]
```

## 完整示例

```kinal
Unit App.TextDemo;
Get IO.Console;
Get IO.Text;

Static Function int Main()


{
    string text = "  Hello, Kinal World!  ";

    string trimmed  = IO.Text.Trim(text);
    string upper    = IO.Text.ToUpper(trimmed);
    string replaced = IO.Text.Replace(upper, "WORLD", "DEVELOPER");

    IO.Console.PrintLine(trimmed);   // Hello, Kinal World!
    IO.Console.PrintLine(upper);     // HELLO, KINAL WORLD!
    IO.Console.PrintLine(replaced);  // HELLO, KINAL DEVELOPER!

    list words = IO.Text.Split(trimmed, " ");
    IO.Console.PrintLine(IO.Collection.list.Count(words));  // 3

    string rejoined = IO.Text.Join(words, "-");
    IO.Console.PrintLine(rejoined);  // Hello,-Kinal-World!

    Return 0;
}
```

## 相关

- [字符串特性](../language/strings.md) — 字符串字面量前缀
- [IO.Console](console.md) — 控制台输出
- [IO.File](filesystem.md) — 文件文本读写
