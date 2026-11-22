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

位置以 UTF-8 字节计算。例如 `IndexOf("雪山雪", "山")` 返回 3。
`LastIndexOf("aaaa", "aaa")` 返回 1，对应从第二个字节开始的匹配。
`Count("aaaa", "aaa")` 也返回 1，每次计数后会跳过整个匹配。
查找空串时，`IndexOf` 和 `Count` 返回 0，`LastIndexOf` 返回原文的字节长度。
`Replace` 遇到空模式会保留原文内容。空分隔符会让 `Split` 返回一个完整字段，
相邻或末尾的分隔符则会保留相应的空字段。

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
IO.Text.Slice("a雪z", 1, 3);          // “雪”占三个 UTF-8 字节
```

`Slice` 将负起点按零处理，遇到文本末尾就停止。长度小于等于零时返回空串。从长记录中提取定长字段时，直接传入字段的字节数，原生实现就只扫描到所需范围。`StartsWith` 也会在前缀匹配完成或遇到不同字节时立即返回。

### 分割与合并

| 函数 | 签名 | 说明 |
|------|------|------|
| `Split(s, sep)` | `string, string → list` | 按分隔符分割，返回 `list` |
| `Join(parts, sep)` | `list, string → string` | 用分隔符合并 list 中的字符串 |
| `Lines(s)` | `string → list` | 按行分割 |

这三个操作都支持 Native 和 KinalVM。`Split` 会保留空字段，包括末尾的空字段。

```kinal
list parts = IO.Text.Split("a,b,c", ",");
// parts = ["a", "b", "c"]

string joined = IO.Text.Join(parts, " | ");
// "a | b | c"

list lines = IO.Text.Lines("line1\nline2\nline3");
// ["line1", "line2", "line3"]
```

## 分段构造文本

原生后端和 KinalVM 都支持 `IO.Text.Builder`，适合片段数量在运行时才确定的文本拼接。缓冲区按需成倍扩容。`Build()` 返回独立字符串，`Clear()` 将长度重置为零并保留缓冲区。

```kinal
IO.Text.Builder line = New IO.Text.Builder();
line.EnsureCapacity(64);
line.Append("name=");
line.Append("雪山", 3);           // 追加前三个 UTF-8 字节。
line.AppendChar('!');
string label = line.Build();     // "name=雪!"
line.Clear();                    // label 仍保留原来的内容。
```

只读属性 `Capacity` 返回已预留的字节数。`EnsureCapacity(minimumBytes)` 预留至少指定的容量，保留当前文本和长度。已有容量足够时可直接复用，负数会抛出 `IO.Error`。

`Length` 按字节计数。`Append(value, maxBytes = -1)` 遇到首个 NUL 或达到字节上限时停止。负数上限表示追加整个字符串，传入 null 追加零字节。`AppendChar('\0')` 会抛出 `IO.Error`。需要输出有效的 UTF-8 时，请在字符边界处截取。

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

这 16 个函数和 `Builder` 都属于 IO.Core。Safe 代码可以调用这些 Trusted 函数。
大小写转换只处理 ASCII 字母。字符串以 NUL 结尾，含有 NUL 的二进制字段请使用字节缓冲区。

[子串基准](../../../benchmarks/text_search.kn)覆盖开头命中、长前缀失配、重复替换和末尾空字段。
