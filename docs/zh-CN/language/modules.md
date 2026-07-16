# 模块系统



Kinal 通过 `Unit`（模块声明）和 `Get`（模块导入）管理代码的命名空间与可见性。`Get` 负责可见性，本地项目成员关系则由当前构建输入或 `kinal.knproj` 里的激活 `SourceSet` 决定。



## Unit — 声明模块



每个 `.kn` 文件的第一行（可选）声明所属的单元（Unit）：



```kinal

Unit App.Services;

```



- `Unit` 声明必须在文件顶部，排在所有 `Get` 和其他声明之前

- Unit 名使用点号分隔，形成层级结构（如 `IO.Console`、`App.Utils`）

- 未声明 `Unit` 的文件属于匿名模块，其符号仍可被同目录文件访问



在此 Unit 内声明的函数、类、接口等，其完整限定名为 `Unit名.声明名`，例如：



```kinal

Unit App.Services;

Function string GetVersion() { Return "1.0"; }

// 完整限定名：App.Services.GetVersion

```



## Get — 导入模块



### 1. 打开导入（命名空间方式）



```kinal

Get IO.Console;

```



导入后，`IO.Console` 内的所有公开符号可通过 `IO.Console.符号名` 访问，也可以在无歧义时省略部分前缀。



### 2. 别名导入



```kinal

Get Console By IO.Console;

```



将模块 `IO.Console` 绑定到本地别名 `Console`，之后用 `Console.PrintLine(...)` 代替 `IO.Console.PrintLine(...)`。



```kinal

Get FS By IO.File;

Get Dir By IO.Directory;

FS.ReadAllText("data.txt");

Dir.Exists("logs/");

```



### 3. 符号导入



导入模块中的单个符号：



```kinal

Get PrintLine By IO.Console.PrintLine;

// 现在可以直接调用 PrintLine(...)

PrintLine("Hello!");

```



### 4. 批量符号导入



```kinal

Get IO.Utils { ReadFile By ReadAllText, WriteFile By WriteAllText };

```



用于从模块中按需提取多个符号并重命名。



### Get 的位置规则



`Get` 声明必须出现在 `Unit` 之后、所有其他声明（函数、类等）之前：



```kinal

Unit App.Main;          // ← 首先 Unit

Get IO.Console;         // ← 然后是所有 Get

Get FS By IO.File;

Static Function int Main() { ... }  // ← 然后是其他声明

```

## Alias — 本地符号别名

`Alias` 用来给当前文件里已经可见的符号起一个本地别名：

```kinal
Get IO.Console;

Alias Print By IO.Console.PrintLine;

Static Function int Main()
{
    Print("Hello");
    Return 0;
}
```

规则：
- 语法：`Alias <Name> By <QualifiedSymbol>;`
- `Alias` 本身不会导入模块
- 目标符号会在编译当前文件时被校验；它必须已经通过当前文件的 `Get` 或当前编译作用域变得可见
- `Alias` 只处理普通符号，不会改写关键字、字面量、操作符或标点
- `Alias` 必须和 `Get` 一样放在文件前导区，排在普通声明之前

`Alias` 可以指向当前可见的普通符号，例如类型、函数、枚举成员和静态成员：

```kinal
Unit App.AliasValues;

Get IO.Console;

Alias Print By IO.Console.PrintLine;
Alias Say By App.AliasValues.Box.Say;
Alias Ready By App.AliasValues.Mode.Ready;
```

在使用 Kinal LSP 的编辑器里，本地/导入别名会按目标符号的种类做语义着色。比如指向模块的别名保持模块风格着色，指向函数的别名保持函数风格着色。

## Unsafe Alias

`Unsafe Alias` 是文件本地的危险 token 别名能力，只允许改关键字和字面量：

```kinal
Unsafe Alias fn By Function;
Unsafe Alias yes By true;
Unsafe Alias Take By Get;
Unsafe Alias 真 By true;

Take IO.Console;

Static fn int Main()
{
    If (yes)
        IO.Console.PrintLine("ok");
    Return 0;
}
```

规则：
- 语法：`Unsafe Alias <NameOrLiteral> By <KeywordOrLiteral>;`
- 只影响当前源文件
- 必须出现在文件前导区
- 使用 `Unsafe Alias` 的文件不能声明 `Unit`
- 使用 `Unsafe Alias` 的文件不能通过 `kinal.knproj` / `--project` 编译
- 因为不能声明 `Unit`，这些文件也不会参与普通的同目录模块发现和 `Get`
- 普通 `Unsafe Alias` 不能改写标点或操作符 token
- 左侧别名名可以是普通 UTF-8 标识符，例如 `真`、`返回`

`Unsafe Alias` 只负责改写 token，不会自动把一种新写法变成模块导入或符号导入。如果想把模块名或符号名也本地化，仍然要在 token 别名之后正常写 `Get` / `Alias`：

```kinal
Unsafe Alias 获取 By Get;
Unsafe Alias 来自 By By;
Unsafe Alias 别名 By Alias;
Unsafe Alias 静态 By Static;
Unsafe Alias 函数 By Function;
Unsafe Alias 整数 By int;
Unsafe Alias 返回 By Return;

获取 控制台 来自 IO.Console;
获取 打印原 来自 IO.Console.PrintLine;
别名 打印行 来自 打印原;

静态 函数 整数 Main()
{
    控制台.PrintLine("module");
    打印行("symbol");
    返回 0;
}
```

启用 Kinal LSP 时，`Unsafe Alias` 改写出来的拼写也会按目标 token 的种类做语义着色，所以本地化后的 `Get` / `Return` / `int` 等会和原关键字、类型保持同样的高亮风格。

## Unsafe Unsafe Unsafe Alias

`Unsafe Unsafe Unsafe Alias` 是完整的 token 级重写形式：

```kinal
Unsafe Unsafe Unsafe Alias != By ==;
Unsafe Unsafe Unsafe Alias !! By 1;
Unsafe Unsafe Unsafe Alias ========= By ==;
Unsafe Unsafe Unsafe Alias 1011 By ;;
```

它包含 `Unsafe Alias` 的全部行为，并且还能改写操作符和标点 token。限制保持一致：
- 只影响当前文件
- 只能放在前导区
- 不能声明 `Unit`
- 不能走 project / `knproj` 编译
- 三重 `Unsafe` 会在词法之后按完整 token 序列匹配，所以 `!!`、`=========` 这类连续操作符串也可以作为别名源

如果目标本身就是 `;`，要多写一个分号作为指令结束符：

```kinal
Unsafe Unsafe Unsafe Alias 1011 By ;;
```

这表示把 `1011` 改写成单个 `;`。



## 模块跨文件自动发现

在不走 project 模式时，Kinal 可以自动发现同目录树里的 `.kn` 文件，并把它们纳入同一次编译。

在 `kinal.knproj` 构建里，本地成员关系改由激活的 `SourceSet` 决定。此时 `Get` 只会在这些来源里解析：

- 激活 `SourceSet` 中的本地 Unit
- 包根目录
- 标准库根目录

如需禁用这种旧式的目录自动发现：

```bash

kinal build main.kn --no-module-discovery

```



## 限定名调用

无论是否导入，都可以直接写完整限定名：



```kinal

// 即使没有 `Get IO.Console;`，也可以这样写：

IO.Console.PrintLine("Hello");

```

不过在 project 模式下，仅仅写出完整限定名并不会自动扩张本地项目边界。目标 Unit 仍然必须属于激活的 `SourceSet`，或者来自 package / stdlib 根目录。



## 内置 IO.* 标准库模块



标准库的所有模块均在 `IO.*` 命名空间下：



```kinal

Get IO.Console;       // 控制台

Get IO.File;          // 文件操作

Get IO.Directory;     // 目录操作

Get IO.Path;          // 路径工具

Get IO.Text;          // 字符串工具

Get IO.Time;          // 时间

Get IO.Async;         // 异步

Get IO.System;        // 系统调用

Get IO.Meta;          // 元数据反射

Get IO.Target;        // 编译目标信息（平台常量）

```



详见 [标准库概览](../stdlib/overview.md)。



## IO.Target — 编译期平台常量



`IO.Target` 提供编译期平台判断，配合 `If` 实现条件编译：



```kinal

If (IO.Target.OS == IO.Target.OS.Windows)
{
    Console.PrintLine("运行于 Windows");

}
Else If (IO.Target.OS == IO.Target.OS.Linux)
{
    Console.PrintLine("运行于 Linux");

}

```



常见常量：

- `IO.Target.OS.Windows` / `IO.Target.OS.Linux` / `IO.Target.OS.MacOS`

- `IO.Target` — 当前目标（参与 `Const If` 判断）



## 完整示例



**math.kn**

```kinal

Unit App.Math;

Function int GCD(int a, int b)
{

    While (b != 0)
    {
        int t = b;

        b = a % b;

        a = t;

    }

    Return a;

}

Function int LCM(int a, int b)
{

    Return a / GCD(a, b) * b;

}

```



**main.kn**

```kinal

Unit App.Main;

Get Console By IO.Console;

Get App.Math;              // 打开 App.Math 模块

Static Function int Main()
{

    int g = App.Math.GCD(48, 18);    // 6

    int l = App.Math.LCM(4, 6);     // 12

    Console.PrintLine([string](g));

    Console.PrintLine([string](l));

    Return 0;

}

```



## 下一步



- [泛型](generics.md)

- [标准库概览](../stdlib/overview.md)

- [Package 系统](../cli/packages.md)


