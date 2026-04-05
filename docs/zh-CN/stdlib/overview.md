# 标准库概览

Kinal 的标准库统一位于 `IO.*` 命名空间下，按功能模块分组。所有标准库都可以通过 `Get` 导入使用。

## 模块速览

| 模块 | 说明 | 文档 |
|------|------|------|
| `IO.Console` | 控制台输入/输出 | [→](console.md) |
| `IO.Text` | 字符串操作工具 | [→](text.md) |
| `IO.File` | 文件读写操作 | [→](filesystem.md) |
| `IO.Directory` | 目录操作 | [→](filesystem.md) |
| `IO.Path` | 路径拼接与解析 | [→](path.md) |
| `IO.Collection` | list / dict / set | [→](collections.md) |
| `IO.Time` | 时间与计时 | [→](time.md) |
| `IO.Async` | 协程任务与进程管理 | [→](async.md) |
| `IO.System` | 系统调用与动态库加载 | [→](system.md) |
| `IO.Meta` | 运行时元数据查询 | [→](meta.md) |
| `IO.Type` | 类型转换函数 | 内置，无需独立导入 |
| `IO.Target` | 编译期平台常量 | 内置 |
| `IO.Math` | 数学函数 | 通过 IO.Core 包 |
| `IO.Char` | 字符工具函数 | 通过 IO.Core 包 |
| `IO.UI` | 图形界面（UI 组件） | 通过 IO.UI 包 |
| `IO.Web` | Web 服务（Civet） | 通过 IO.Web 包 |

## 导入方式

```kinal
// 打开整个模块（使用时需要全限定名）
Get IO.Console;
IO.Console.PrintLine("hello");

// 别名导入
Get IO.Console;
IO.Console.PrintLine("hello");

// 符号导入
Get PrintLine By IO.Console.PrintLine;
PrintLine("hello");
```

## 内置类型方法

以下方法通过 `IO.Type.any` 接口在**所有值**上可用，无需导入：

```kinal
42.ToString();       // "42"
42.TypeName();       // "int"
42.Equals(42);       // true
42.IsNull();         // false
true.ToString();     // "true"
```

`any` 值的类型检测：

```kinal
any v = 3.14;
v.IsNull();     // false
v.IsFloat();    // true
v.IsInt();      // false
v.IsString();   // false
v.IsNumber();   // true
v.IsObject();   // false
v.IsPointer();  // false
v.IsBool();     // false
v.IsChar();     // false
v.Tag();        // 内部类型标签（int）
```

## 标准输出入口

最常用的两个标准库函数：

```kinal
Get IO.Console;

IO.Console.PrintLine("Hello!");           // 打印并换行
IO.Console.Print("不换行输出");
string input = IO.Console.ReadLine();     // 读取一行输入
```

支持多参数可变参数（`varargs`）：

```kinal
IO.Console.PrintLine("a=", 1, ", b=", true);  // 打印所有参数
```

## 类型系统补充：IO.Type

`IO.Type` 提供类型转换函数（等价于 `[type](expr)` 语法）：

```kinal
IO.Type.int(any_value);      // 转为 int
IO.Type.string(any_value);   // 转为 string
IO.Type.bool(any_value);     // 转为 bool
IO.Type.float(any_value);    // 转为 float
// ...
```

## IO.Target — 编译期常量

```kinal
// 当前编译目标（const if 使用）
IO.Target           // 目标平台值
IO.Target.OS        // 操作系统枚举
IO.Target.OS.Windows
IO.Target.OS.Linux
IO.Target.OS.MacOS
```

用于编译期条件分支（参见 [控制流 → Const If](../language/control-flow.md#const-if编译期条件)）。

## IO.Core 包（官方核心包）

`IO.Core` 包含 `IO.Char` 和 `IO.Math` 等基础工具：

```kinal
// 使用前需要在项目中配置 IO.Core 包
Get IO;   // 导入 IO 命名空间

IO.Char.IsDigit('5');   // true
IO.Char.IsLetter('A');  // true
IO.Char.ToLower('Z');   // 'z'
IO.Char.ToUpper('a');   // 'A'

IO.Math.Abs(-5);        // 5.0
IO.Math.Sqrt(16.0);     // 4.0
IO.Math.Max(3, 7);      // 7
```

## IO.UI 包（图形界面）

```kinal
Get IO.UI;

IO.UI.Window window = New IO.UI.Window("我的应用", 800, 600);
IO.UI.Button btn = New IO.UI.Button("确定", 10, 10, 80, 30);
window.Add(btn);
Return window.Run();
```

## IO.Web 包（Web 服务）

通过 Civet 框架提供 HTTP 服务能力（平台相关特性）。

## 下一步

- [IO.Console](console.md)
- [IO.Text](text.md)
- [IO.Collections](collections.md)
- [IO.File / IO.Directory](filesystem.md)
