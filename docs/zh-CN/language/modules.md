# 模块系统



Kinal 通过 `Unit`（模块声明）和 `Get`（模块导入）管理代码的命名空间与可见性。



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



## 模块跨文件自动发现



编译时，Kinal 默认会自动发现同目录和子目录中的 `.kn` 文件，将它们纳入同一编译单元。



只要两个文件属于同一编译（无论 Unit 名是否相同），它们之间的符号就可以相互引用。



如需禁用：

```bash

kinal build main.kn --no-module-discovery

```



## 限定名调用



无论是否导入，都可以直接使用完整限定名调用：



```kinal

// 即使没有 `Get IO.Console;`，也可以这样写：

IO.Console.PrintLine("Hello");

```



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

