# 项目结构

Kinal 支持单文件程序、多文件模块和完整的 Package 系统。本文介绍三种组织方式。

## 单文件程序

最简单的结构：一个 `.kn` 文件即为一个完整程序。

```
myapp/
└── main.kn
```

编译：
```bash
kinal build main.kn -o myapp
```

## 多文件模块（同一 Unit）

多个 `.kn` 文件可以共享同一个 `Unit` 名，编译器会将它们合并为同一模块。

```
myapp/
├── main.kn        # Unit App.Main;
├── utils.kn       # Unit App.Utils;
└── models.kn      # Unit App.Models;
```

**main.kn**
```kinal
Unit App.Main;

Get App.Utils;
Get App.Models;
Get IO.Console;

Static Function int Main()
{
    Var msg = App.Utils.Greet("world");
    IO.Console.PrintLine(msg);
    Return 0;
}
```

**utils.kn**
```kinal
Unit App.Utils;

Function string Greet(string name)
{
    Return "Hello, " + name + "!";
}
```

编译时只需指定入口文件，编译器会自动发现同目录下的其他 `.kn` 文件：
```bash
kinal build main.kn -o myapp
```

如需禁用自动发现：
```bash
kinal build main.kn utils.kn models.kn -o myapp --no-module-discovery
```

## 同一文件多个 Unit

一个文件也可以贡献多个声明给不同单元（通过嵌套 Block）。但通常推荐每文件一个 Unit。

## Package 结构

对于可复用的库，使用 Package 格式：

```
MyLib/
├── 1.0.0/
│   ├── package.knpkg.json
│   ├── src/
│   │   └── MyLib/
│   │       ├── Core.kn
│   │       └── Utils.kn
│   └── lib/
│       └── MyLib.klib    # 预编译的包（可选）
```

**package.knpkg.json**
```json
{
  "kind": "package",
  "name": "MyLib",
  "version": "1.0.0",
  "summary": "我的库简介",
  "source_root": "src",
  "entry": "src/MyLib/Core.kn"
}
```

使用 Package：
```bash
# 指定包根目录
kinal build main.kn --pkg-root ./packages -o myapp
```

在代码中：
```kinal
Get MyLib;       // 打开整个 MyLib 模块
Get ML By MyLib; // 以别名 ML 访问
```

## 项目清单文件

对于大型项目，可以使用 JSON 清单文件：

**manifest.json**（示例）
```json
{
  "entry": "main.kn",
  "output": "myapp",
  "pkg-roots": ["./packages"]
}
```

编译：
```bash
kinal build --project manifest.json
```

## 打包与分发

将包源码打包为 `.klib`（一个包含源代码和元信息的压缩归档）：

```bash
kinal pkg build --manifest ./MyLib/1.0.0/ -o output/MyLib.klib
```

从 `.klib` 恢复源码：
```bash
kinal pkg unpack MyLib.klib -o ./recovered/
```

## 文件扩展名

| 扩展名 | 说明 |
|--------|------|
| `.kn` / `.kinal` | Kinal 源文件 |
| `.knc` | 编译后的字节码文件（KNC 格式） |
| `.klib` | 打包的 Package 归档 |
| `.knpkg.json` | Package 清单文件 |

## 下一步

- [语言概览](../language/overview.md)
- [kinal 编译器 CLI](../cli/compiler.md)
- [Package 系统](../cli/packages.md)
