# IO.System — 系统接口

`IO.System` 提供进程控制、命令行参数、动态库加载等底层系统操作。

> **安全要求**：`Exec`、`LoadLibrary`、`GetSymbol`、`CloseLibrary` 需要 `Trusted` 或 `Unsafe` 安全级别。
> `Exit`、`CommandLine`、`FileExists`、`LastError` 可在 `Safe` 上下文中使用。

## 导入

```kinal
Get IO.System;
```

---

## 函数参考

### 进程

| 函数 | 签名 | 说明 |
|------|------|------|
| `Exit(code)` | `int → void` | 立即终止当前进程，返回指定退出码 |
| `CommandLine()` | `→ list` | 返回命令行参数列表（`list` of `string`） |
| `Exec(cmd)` | `string → int` | (**Trusted**) 执行 shell 命令，返回退出码 |

```kinal
// 获取命令行参数
list args = IO.System.CommandLine();
int argc = IO.Collection.list.Count(args);
If (argc < 2)


{
    IO.Console.PrintLine("需要一个参数");
    IO.System.Exit(1);
}

string firstArg = [string](IO.Collection.list.Fetch(args, 1));
IO.Console.PrintLine("参数: " + firstArg);
```

```kinal
// 执行外部命令（Trusted）
Trusted
{
    int code = IO.System.Exec("git status");
    If (code != 0)


    {
        IO.Console.PrintLine("命令执行失败");
    }
}
```

### 文件检查

| 函数 | 签名 | 说明 |
|------|------|------|
| `FileExists(path)` | `string → bool` | 检查文件是否存在（同 `IO.File.Exists`，但无需导入 IO.File） |

```kinal
If (!IO.System.FileExists("config.json"))


{
    IO.Console.PrintLine("配置文件不存在");
    IO.System.Exit(1);
}
```

### 动态库（Trusted）

| 函数 | 签名 | 说明 |
|------|------|------|
| `LoadLibrary(path)` | `string → any` | 加载动态库，返回库句柄 |
| `GetSymbol(lib, name)` | `any, string → any` | 从库中获取符号（函数指针） |
| `CloseLibrary(lib)` | `any → void` | 卸载动态库 |

```kinal
Trusted
{
    any lib = IO.System.LoadLibrary("libmath.so");
    any sqrt = IO.System.GetSymbol(lib, "sqrt");

    // 结合 Delegate 使用
    // Delegate Function float MathSqrt(float x) By C;
    // MathSqrt fn = [MathSqrt](sqrt);
    // float result = fn(16.0);

    IO.System.CloseLibrary(lib);
}
```

> 动态库加载通常与 FFI/Delegate 配合使用，参见 [FFI](../language/ffi.md)。

### 错误信息

| 函数 | 签名 | 说明 |
|------|------|------|
| `LastError()` | `→ string` | 获取最后一次系统错误的描述字符串 |

```kinal
Trusted


{
    any lib = IO.System.LoadLibrary("missing.so");
    If (lib == null)


    {
        IO.Console.PrintLine("加载失败: " + IO.System.LastError());
    }
}
```

---

## 完整示例

### CLI 工具框架

```kinal
Unit App.CLI;
Get IO.Console;
Get IO.System;
Get IO.Collection;
Get IO.Text;

Static Function void PrintUsage()


{
    IO.Console.PrintLine("用法: myapp <命令> [选项]");
    IO.Console.PrintLine("命令:");
    IO.Console.PrintLine("  build   - 构建项目");
    IO.Console.PrintLine("  clean   - 清理构建产物");
    IO.Console.PrintLine("  help    - 显示帮助");
}

Static Function int Main()


{
    list args = IO.System.CommandLine();
    int argc  = IO.Collection.list.Count(args);

    If (argc < 2)


    {
        PrintUsage();
        Return 1;
    }

    string cmd = [string](IO.Collection.list.Fetch(args, 1));

    If (IO.Text.Contains(cmd, "build"))


    {
        IO.Console.PrintLine("构建中...");
        Trusted { IO.System.Exec("make all"); }
    }
    Else If (IO.Text.Contains(cmd, "clean"))


    {
        IO.Console.PrintLine("清理中...");
        Trusted { IO.System.Exec("make clean"); }
    }
    Else If (IO.Text.Contains(cmd, "help"))


    {
        PrintUsage();
    }
    Else


    {
        IO.Console.PrintLine("未知命令: " + cmd);
        Return 1;
    }

    Return 0;
}
```

---

## 相关

- [安全级别](../language/safety.md) — Trusted 上下文的使用
- [FFI](../language/ffi.md) — 与动态库符号配合使用
- [IO.Path](path.md) — 路径操作
