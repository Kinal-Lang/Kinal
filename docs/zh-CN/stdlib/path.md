# IO.Path — 路径操作

`IO.Path` 提供跨平台的文件路径拼接、分解和检查工具。大部分辅助函数处理路径文本，CurrentDirectory／Cwd 会查询主机工作目录。文本规范化不会验证路径是否存在。

## 导入

```kinal
Get IO.Path;
```

---

## 函数参考

### 拼接

| 函数 | 签名 | 说明 |
|------|------|------|
| `Combine(a, b)` | `string, string → string` | 拼接两段路径（可缩写 `Join`） |
| `Join(a, b)` | `string, string → string` | 同 `Combine` |
| `Join3(a, b, c)` | `string, string, string → string` | 拼接三段路径 |
| `Join4(a, b, c, d)` | `string, string, string, string → string` | 拼接四段路径 |

```kinal
string full = IO.Path.Combine("C:/Users/user", "Documents");
// "C:/Users/user/Documents"

string deep = IO.Path.Join3("base", "sub", "file.txt");
// "base/sub/file.txt"
```

### 分解

| 函数 | 签名 | 说明 |
|------|------|------|
| `FileName(path)` | `string → string` | 文件名（含扩展名） |
| `BaseName(path)` | `string → string` | 文件名（不含扩展名，也称 `Stem`） |
| `Extension(path)` | `string → string` | 文件名最后一个点后面的文本，如 `"txt"` |
| `Directory(path)` | `string → string` | 所在目录（也称 `Parent`） |

```kinal
string path = "/home/user/docs/report.pdf";

IO.Path.FileName(path);    // "report.pdf"
IO.Path.BaseName(path);    // "report"
IO.Path.Extension(path);   // "pdf"
IO.Path.Directory(path);   // "/home/user/docs"
```

本页的路径操作在 Native 和 KinalVM 中均可使用。可移植字节码遵循 VM 运行平台的路径规则。

### 变换

| 函数 | 签名 | 说明 |
|------|------|------|
| `Normalize(path)` | `string → string` | 规范化路径（去除 `.`/`..`，统一分隔符） |
| `ChangeExtension(path, ext)` | `string, string → string` | 替换扩展名 |
| `EnsureTrailingSeparator(path)` | `string → string` | 确保路径以分隔符结尾 |

```kinal
IO.Path.Normalize("a/b/../c/./d");         // "a/c/d"
IO.Path.ChangeExtension("file.txt", ".md"); // "file.md"
IO.Path.EnsureTrailingSeparator("/home");   // "/home/"
```

### 检查

| 函数 | 签名 | 说明 |
|------|------|------|
| `HasExtension(path)` | `string → bool` | 是否有扩展名 |
| `IsAbsolute(path)` | `string → bool` | 是否为绝对路径 |
| `IsRelative(path)` | `string → bool` | 是否为相对路径 |

```kinal
IO.Path.HasExtension("file.txt");  // true
IO.Path.HasExtension("Makefile");  // false
IO.Path.IsAbsolute("/usr/bin");    // true
IO.Path.IsRelative("../docs");     // true
```

### 环境

| 函数 | 签名 | 说明 |
|------|------|------|
| `Cwd()` | `→ string` | 获取当前工作目录 |

```kinal
string cwd = IO.Path.Cwd();
string output = IO.Path.Combine(cwd, "output");
```

---

## 平台说明

`IO.Path` 会自动适配当前平台的路径分隔符：

- **Windows**：`\`（内部统一转换，也接受 `/`）
- **Linux / macOS**：`/`

跨平台代码中推荐始终使用 `IO.Path.Combine` 拼接路径，而非手动拼接字符串。

---

## 完整示例

```kinal
Unit App.PathDemo;

Get IO.Console;
Get IO.Path;
Get IO.File;

Static Function int Main()
{
    // 构建输出路径
    string base  = IO.Path.Cwd();
    string outDir = IO.Path.Combine(base, "output");
    string file   = IO.Path.Combine(outDir, "result.txt");

    IO.Console.PrintLine(file);
    // e.g., /home/user/project/output/result.txt

    // 分解路径信息
    IO.Console.PrintLine(IO.Path.FileName(file));   // result.txt
    IO.Console.PrintLine(IO.Path.Extension(file));  // txt
    IO.Console.PrintLine(IO.Path.Directory(file));  // /home/user/project/output

    // 修改扩展名
    string htmlFile = IO.Path.ChangeExtension(file, ".html");
    IO.Console.PrintLine(htmlFile); // /home/user/project/output/result.html

    Return 0;
}
```

---

## 相关

- [IO.File](filesystem.md) — 文件读写（使用路径字符串）
- [IO.System](system.md) — `CommandLine`、`FileExists` 等系统接口

公开别名还包括 Directory 对应的 Parent(path)、BaseName 对应的 Stem(path)、Cwd 对应的 CurrentDirectory()，参数／结果类型与原接口相同。分隔符、Windows 盘符／UNC 和绝对路径规则依据编译目标 IO.Target.OS，不是编译器主机；POSIX 示例展示 POSIX 输出，Windows 规范化可使用反斜杠。CurrentDirectory／Cwd 需要主机服务；大部分源码辅助函数为 Trusted，可由 Safe 调用。Normalize 是词法规范化，不是文件系统或符号链接的真实路径解析。
