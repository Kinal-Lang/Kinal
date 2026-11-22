# IO.File 与 IO.Directory

这两个 IO.Core Unit 提供文本文件操作和目录发现，需要分别导入实际使用的 Unit。源码实现为 Trusted，Safe 调用方可使用公开接口。Native 和 KinalVM 的文件追加、可选读取行为一致，例如文件缺失时 `ReadIfExists` 返回 null。

## 完整 IO.File API

路径参数均为 string；文本操作使用 NUL 结尾文本，不是任意二进制缓冲区接口。

| 函数 | 结果 | 行为 |
|------|------|------|
| `Exists(string path)` | `bool` | 查询文件是否存在 |
| `Create(string path)` | `bool` | 创建／截断 |
| `Touch(string path)` | `bool` | 必要时创建，不截断已有内容 |
| `ReadAllText(string path)` | `string` | 读取文本，当前失败后备为空文本 |
| `ReadFirstLine(string path)` | `string` | 读至首个 LF、NUL 或文件末尾，去掉终止符和行末的一个 CR。读取失败时返回空串。 |
| `ReadIfExists(string path)` | `string` | 缺失时返回 null，否则读取 |
| `ReadOrDefault(string path, string fallback)` | `string` | 缺失时使用后备值 |
| `ReadOr(string path, string fallback)` | `string` | ReadOrDefault 别名 |
| `WriteAllText(string path, string text)` | `bool` | 覆盖写入 |
| `AppendAllText(string path, string text)` | `bool` | 在文件末尾追加，文件缺失时创建。传入 null 或空串时追加零字节。 |
| `AppendLine(string path, string text)` | `bool` | 在文本后追加一个 LF。传入 null 或空串时只写一个 LF。 |
| `Delete(string path)` | `bool` | 尝试删除文件 |
| `DeleteIfExists(string path)` | `bool` | 缺失或删除成功时为 true |
| `Size(string path)` | `int` | 字节数，失败为 -1 |
| `IsEmpty(string path)` | `bool` | Size 是否恰为零 |
| `Copy(string source, string destination)` | `bool` | 复制文件 |
| `Move(string source, string destination)` | `bool` | 移动／重命名 |
| `ReplaceText(string path, string from, string to)` | `bool` | 替换文本并重写，源文件缺失为 false |
| `ReadLines(string path)` | `list` | 读取文本后调用 IO.Text.Lines |
| `WriteLines(string path, list lines)` | `bool` | 用 LF 连接后写入，不隐式追加末尾 LF |

应检查 bool 结果；返回 false 不等于捕获了异常。ReadAllText 返回空文本无法区分合法空文件与所有失败。ReadIfExists／ReadOr 只预检存在性，不能消除权限问题或检查后的竞争。Size 为 -1 不是空文件。

`ReadFirstLine` 每次最多读取 4 KiB。即使首行后还有很长的内容，内存开销也只与首行长度有关。行内的 CR 和开头的 UTF-8 BOM 都会保留。对管道读取时，遇到 LF 或 NUL 即可返回，写入端可以继续保持打开。同一块中终止符后面的字节也已从管道中读走。

Native 和 KinalVM 都使用系统的追加模式。追加只需文件的写权限，会保留原有的每个字节，包括嵌入的 NUL，内存开销与原文件大小无关。在支持并发追加的本地文件系统上，系统会把每次写入放到当时的文件末尾。较大的文本可能分多次写入，其他进程的内容可能出现在分块之间。写入失败时，文件可能已经追加了部分内容。父目录缺失时，可先调用 `IO.Directory.Ensure`。

## 完整 IO.Directory API

Native 和 KinalVM 都支持这六个目录操作。`Files(path, recursive)` 返回排序后的文件路径列表。传入 `true` 可包含子目录中的文件。Windows 目录查询支持 UTF-8 路径和盘符根目录。

```kinal
Get IO.Directory;
```

| 函数 | 结果 | 行为 |
|------|------|------|
| `Exists(string path)` | `bool` | 查询目录是否存在 |
| `Create(string path)` | `bool` | 创建一级目录 |
| `Ensure(string path)` | `bool` | 创建目录及缺失父目录 |
| `Delete(string path)` | `bool` | 删除空目录 |
| `DeleteIfExists(string path)` | `bool` | 缺失或删除空目录成功时为 true |
| `Files(string path, bool recursive)` | `list` | 按稳定 ordinal 顺序返回普通文件路径 |

目录无法打开时 Files 返回空列表。recursive=false 只收集直接普通文件，true 递归普通子目录，不跟随目录符号链接／reparse point。空结果不是精确权限诊断；Delete 不递归删除非空树。

## 完整文本读写

示例使用工作目录下独立的临时示例目录，请在适合创建这些示例文件的位置运行：

```kinal
Unit Docs.Files;
Get IO.Console;
Get IO.File;
Get IO.Directory;
Get IO.Path;

Static Function int Main()
{
    string directory = "docs_files_example";
    If (!IO.Directory.Ensure(directory)) { Return 1; }
    string path = IO.Path.Combine(directory, "message.txt");
    If (!IO.File.WriteAllText(path, "Hello, Kinal!\n")) { Return 2; }

    IO.Console.PrintLine(IO.File.ReadFirstLine(path));
    IO.Console.PrintLine(IO.File.Size(path) > 0);
    IO.Console.PrintLine(IO.Directory.Files(directory, false).Count());
    IO.Console.PrintLine(IO.File.DeleteIfExists(path));
    IO.Directory.Delete(directory);
    Return 0;
}
```

全新示例目录中的预期输出：

```text
Hello, Kinal!
true
1
true
```

ReadLines／Split 可用于简单分隔文本；只按逗号切分不是完整 CSV 解析器，带引号的逗号、转义引号及嵌入换行需要专门解析。

## 路径、编码与后端边界

相对路径基于程序工作目录，不自动相对源码文件或 knproj。目标路径规则使用 IO.Path。Native Windows 多条主机边界仍使用 ANSI API，UTF-8 源码字面量不代表通用 Unicode 路径支持。

在 KinalVM 中，可以用 `IO.File.ReadLines(path)` 取得列表，交给 `IO.Text.Join(lines, separator)` 拼接，也可用 `IO.File.WriteLines(path, lines)` 写入行列表。

## 相关文档

- [IO.Path](path.md)
- [IO.Text](text.md)
- [IO.System](system.md)
- [内存与生命周期](../advanced/memory-management.md)
