# IO.File — 文件操作

`IO.File` 提供文件读写、创建、删除、复制等基础操作。所有路径参数均为字符串。

## 导入

```kinal
Get IO.File;
```

---

## 函数参考

### 检查与元数据

| 函数 | 签名 | 说明 |
|------|------|------|
| `Exists(path)` | `string → bool` | 文件是否存在 |
| `Size(path)` | `string → int` | 文件字节大小 |
| `IsEmpty(path)` | `string → bool` | 文件内容是否为空 |

```kinal
If (IO.File.Exists("config.json"))


{
    int size = IO.File.Size("config.json");
    IO.Console.PrintLine(size);
}
```

### 创建

| 函数 | 签名 | 说明 |
|------|------|------|
| `Create(path)` | `string → void` | 创建文件（已存在则清空） |
| `Touch(path)` | `string → void` | 创建文件（已存在则不修改内容） |

```kinal
IO.File.Create("output.txt");        // 清空或新建
IO.File.Touch("log.txt");            // 仅确保文件存在
```

### 读取

| 函数 | 签名 | 说明 |
|------|------|------|
| `ReadAllText(path)` | `string → string` | 读取全部内容 |
| `ReadFirstLine(path)` | `string → string` | 读取第一行 |
| `ReadIfExists(path)` | `string → string` | 文件存在则读取，否则返回 `""` |
| `ReadOrDefault(path, default)` | `string, string → string` | 文件存在则读取，否则返回默认值 |
| `ReadOr(path, default)` | `string, string → string` | 同 `ReadOrDefault`（别名） |
| `ReadLines(path)` | `string → list` | 按行读取，返回 `list` |

```kinal
string content = IO.File.ReadAllText("hello.txt");

string first = IO.File.ReadFirstLine("data.csv");

string cfg = IO.File.ReadOrDefault("config.json", "{}");

list lines = IO.File.ReadLines("names.txt");
int lineCount = IO.Collection.list.Count(lines);
```

### 写入

| 函数 | 签名 | 说明 |
|------|------|------|
| `WriteAllText(path, text)` | `string, string → void` | 覆盖写入全部内容 |
| `AppendAllText(path, text)` | `string, string → void` | 追加文本到文件末尾 |
| `AppendLine(path, line)` | `string, string → void` | 追加一行（自动添加换行符） |
| `WriteLines(path, lines)` | `string, list → void` | 将 list 中每个元素作为一行写入 |

```kinal
IO.File.WriteAllText("out.txt", "Hello, Kinal!\n");

IO.File.AppendLine("log.txt", "New event occurred");

list lines = IO.Collection.list.Create();
IO.Collection.list.Add(lines, "line1");
IO.Collection.list.Add(lines, "line2");
IO.File.WriteLines("data.txt", lines);
```

### 管理操作

| 函数 | 签名 | 说明 |
|------|------|------|
| `Delete(path)` | `string → void` | 删除文件（不存在则抛出异常） |
| `DeleteIfExists(path)` | `string → void` | 文件存在则删除（不存在时静默） |
| `Copy(src, dst)` | `string, string → void` | 复制文件 |
| `Move(src, dst)` | `string, string → void` | 移动/重命名文件 |

```kinal
IO.File.Copy("template.txt", "output.txt");
IO.File.Move("old_name.txt", "new_name.txt");
IO.File.DeleteIfExists("temp.tmp");
```

---

## 完整示例

### 读取并处理 CSV

```kinal
Unit App.FileDemo;

Get IO.Console;
Get IO.File;
Get IO.Text;
Get IO.Collection;

Static Function int Main()
{
    string csvPath = "data.csv";

    If (!IO.File.Exists(csvPath))


    {
        IO.Console.PrintLine("文件不存在");
        Return 1;
    }

    list lines = IO.File.ReadLines(csvPath);
    int total = IO.Collection.list.Count(lines);
    IO.Console.PrintLine([string](total) + " 行数据");

    int i = 0;
    While (i < total)


    {
        string line = [string](IO.Collection.list.Fetch(lines, i));
        list fields = IO.Text.Split(line, ",");
        IO.Console.PrintLine(IO.Collection.list.Fetch(fields, 0));
        i = i + 1;
    }

    Return 0;
}
```

### 写入日志文件

```kinal
Unit App.Logger;

Get IO.File;
Get IO.Time;

Static Function void Log(string message)
{
    string ts = IO.Time.Format(IO.Time.Now(), "yyyy-MM-dd HH:mm:ss");
    IO.File.AppendLine("app.log", "[" + ts + "] " + message);
}
```

---

## 异常

以下情况会抛出 `IO.Error` 异常：

- `ReadAllText`/`ReadLines` 等：文件不存在
- `Delete`：文件不存在
- `WriteAllText`/`AppendLine` 等：路径不可写（权限不足、目录不存在）

### 防御性写法

```kinal
Try
{
    string content = IO.File.ReadAllText("data.txt");
    IO.Console.PrintLine(content);
} Catch IO.Error e
{
    IO.Console.PrintLine("读取失败: " + e.Message);
}
```

---

## 相关

- [IO.Path](path.md) — 路径拼接与操作
- [IO.Text](text.md) — 文本内容处理
- [异常处理](../language/exceptions.md) — Try/Catch
