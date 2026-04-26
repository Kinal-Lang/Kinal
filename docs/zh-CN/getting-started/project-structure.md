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

## 项目文件（`kinal.knproj`）

对于稍大的项目，可以把构建配置放在项目根目录的 `kinal.knproj` 里。

```
myapp/
├── kinal.knproj
├── src/
│   └── Main.kn
└── kpkg/
```

**kinal.knproj**
```kinal
Project MyApp
{
    DefaultProfile = "native";

    Packages
    {
        Roots = ["./kpkg"];
    }

    Profile "native"
    {
        Source
        {
            Entry = "src/Main.kn";
            AutoDiscovery = true;
        }

        Build
        {
            Backend = Native;
            Environment = Hosted;
            Output = "out/myapp";
        }
    }

    Profile "vm"
    {
        Source
        {
            Entry = "src/Main.kn";
            AutoDiscovery = true;
        }

        Build
        {
            Backend = VM;
            Output = "out/myapp.knc";
            Superloop = true;
        }
    }

    Lsp
    {
        Profile = "native";
    }
}
```

使用默认 Profile 编译：
```bash
kinal build --project .
```

显式选择其他 Profile：
```bash
kinal vm build --project . --profile vm
```

当 `--project` 传入目录时，`kinal` 会优先查找 `kinal.knproj`。旧的项目清单格式例如 `kinal.pkg.json` 仍然保留兼容。

### `kinal.knproj` 的整体结构

根结构如下：

```kinal
Project MyApp
{
    DefaultProfile = "native";
    Packages { ... }
    Profile "native" { ... }
    Lsp { ... }
}
```

- `Project <Name>`：声明项目名
- `DefaultProfile`：当命令行未传 `--profile` 时使用的 Profile
- `Packages`：对所有 Profile 生效的包根目录配置
- `Profile "<name>"`：一组构建配置
- `Lsp`：告诉语言服务器在编辑器里应该按哪个 Profile 做分析

### 顶层字段

#### `DefaultProfile`

```kinal
DefaultProfile = "native";
```

这个字段决定了以下命令默认使用哪个 Profile：

```bash
kinal build --project .
kinal run --project .
```

如果省略 `DefaultProfile`，则默认使用第一个声明的 Profile。

#### `Packages`

```kinal
Packages
{
    Roots = ["./kpkg", "../shared-packages"];
    OfficialRoots = ["../stdpkg"];
}
```

- `Roots`：本地包根目录，等价于命令行里的 `--pkg-root`
- `OfficialRoots`：官方包根目录，等价于命令行里的 `--stdpkg-root`

这些设置会对所有 Profile 生效；如果某个 Profile 自己再声明 `Packages`，则是在这里的基础上追加。

#### `Lsp`

```kinal
Lsp
{
    Profile = "native";
}
```

这个块只影响编辑器中的语义分析、跳转、诊断和补全，不会强制 CLI 使用该 Profile 编译。

LSP 的 Profile 选择顺序是：

1. `Lsp.Profile`
2. `DefaultProfile`
3. 第一个声明的 Profile

### `Profile` 块

一个项目可以定义多个 Profile，例如本地可执行、VM、调试、发布、裸机内核等。

```kinal
Profile "native"
{
    Source { ... }
    Build { ... }
    Link { ... }
    Packages { ... }
}
```

每个 Profile 可以包含这些部分：

- `Source`：源码入口和自动发现行为
- `Build`：后端、运行环境、输出路径、目标平台等构建设置
- `Link`：链接器与链接输入设置
- `Packages`：仅对该 Profile 生效的额外包根目录

### `Source` 部分

```kinal
Source
{
    Entry = "src/Main.kn";
    AutoDiscovery = true;
}
```

- `Entry`：该 Profile 的入口源文件
- `AutoDiscovery`：是否自动发现入口附近的其他 `.kn` 文件

常见用法：

- `true`：适合大多数普通项目
- `false`：适合你希望完全显式控制输入文件的情况

### `Build` 部分

```kinal
Build
{
    Backend = Native;
    Environment = Hosted;
    Output = "out/myapp";
}
```

支持的字段有：

- `Backend`
- `Environment`
- `Runtime`
- `Panic`
- `Target`
- `Output`
- `EntrySymbol`
- `Linker`
- `LinkerPath`
- `Superloop`

#### `Backend`

```kinal
Backend = Native;
```

支持值：

- `Native`：生成本地程序或本地目标文件
- `VM`：生成 KinalVM 字节码或面向 VM 的产物

通常和命令这样对应：

- `Native` Profile：`kinal build --project .`、`kinal run --project .`
- `VM` Profile：`kinal vm build --project .`、`kinal vm run --project .`、`kinal vm pack --project .`

#### `Environment`

```kinal
Environment = Hosted;
```

支持值：

- `Hosted`：普通操作系统进程环境，默认入口通常是 `Main`
- `Freestanding`：裸机、内核、无宿主环境，默认入口通常是 `KMain`

如果你在做内核、引导程序、裸机运行时，通常就要用 `Freestanding`。

#### `Runtime`

```kinal
Runtime = None;
```

支持值：

- `None`
- `Alloc`
- `GC`

这个字段主要对 freestanding/native 这类配置有意义，用来控制运行时支持级别。

#### `Panic`

```kinal
Panic = Trap;
```

支持值：

- `Trap`
- `Loop`

这个字段用于控制 freestanding 风格构建中的 panic 行为。

#### `Target`

```kinal
Target = "x86_64-linux-gnu";
```

用于显式指定目标三元组。

#### `Output`

```kinal
Output = "out/myapp";
```

当命令行没有传 `-o` 或 `--output` 时，使用这里定义的输出路径。

#### `EntrySymbol`

```kinal
EntrySymbol = "KernelMain";
```

用于覆盖默认入口符号推断。

默认规则：

- hosted native：默认入口是 `Main`
- freestanding native：默认入口是 `KMain`
- VM：默认入口是 `Main`

#### `Linker`

```kinal
Linker = LLD;
```

支持值：

- `LLD`
- `Zig`
- `MSVC`

#### `LinkerPath`

```kinal
LinkerPath = "C:/toolchains/lld-link.exe";
```

当链接器不在 `PATH` 中，或你希望固定使用某个工具链里的链接器时使用。

#### `Superloop`

```kinal
Superloop = true;
```

这个字段主要用于 VM Profile，用来控制生成的字节码 / VM bundle 是否启用 superloop 模式。

### `Link` 部分

```kinal
Link
{
    Script = "link/kernel.ld";
    NoCRT = true;
    NoDefaultLibs = true;
    LibDirs = ["./libs"];
    Libs = ["mylib"];
    LinkFiles = ["./prebuilt/startup.obj"];
    LinkArgs = ["--gc-sections"];
    LinkRoots = ["./deps"];
}
```

支持的字段：

- `Script`：链接脚本路径
- `NoCRT`：不链接 CRT 启动对象
- `NoDefaultLibs`：不自动注入默认系统库
- `LibDirs`：库搜索目录
- `Libs`：按名字链接库
- `LinkFiles`：精确链接输入，例如 `.obj`、`.lib`、`.a`
- `LinkArgs`：原样传给链接器的参数
- `LinkRoots`：依赖根目录，会被展开为目标相关的搜索路径

这一节本质上就是命令行里 `-L`、`-l`、`--link-file`、`--link-root`、`--link-arg` 的项目文件版本。

### Profile 内单独的 `Packages`

某个 Profile 也可以在全局 `Packages` 之外再追加自己的包根目录：

```kinal
Profile "native"
{
    Packages
    {
        Roots = ["./native-packages"];
    }
}
```

适合某个 Profile 需要额外包，而其他 Profile 不需要的情况。

### 示例：Hosted Native + VM

```kinal
Project MyApp
{
    DefaultProfile = "native";

    Packages
    {
        Roots = ["./kpkg"];
    }

    Profile "native"
    {
        Source
        {
            Entry = "src/Main.kn";
            AutoDiscovery = true;
        }

        Build
        {
            Backend = Native;
            Environment = Hosted;
            Output = "out/myapp";
        }
    }

    Profile "vm"
    {
        Source
        {
            Entry = "src/Main.kn";
            AutoDiscovery = true;
        }

        Build
        {
            Backend = VM;
            Output = "out/myapp.knc";
            Superloop = true;
        }
    }

    Lsp
    {
        Profile = "native";
    }
}
```

### 示例：Freestanding 内核 Profile

```kinal
Project KinalOS
{
    DefaultProfile = "kernel";

    Profile "kernel"
    {
        Source
        {
            Entry = "src/kernel.kn";
            AutoDiscovery = true;
        }

        Build
        {
            Backend = Native;
            Environment = Freestanding;
            Runtime = None;
            Panic = Loop;
            Target = "x86_64-unknown-none";
            Output = "out/kernel.elf";
            EntrySymbol = "KMain";
            Linker = LLD;
        }

        Link
        {
            Script = "link/kernel.ld";
            NoCRT = true;
            NoDefaultLibs = true;
        }
    }

    Lsp
    {
        Profile = "kernel";
    }
}
```

这种 Profile 就很适合内核、引导阶段代码或其他裸机目标。

### 命令与 Profile 的匹配关系

- `kinal build --project .` 需要 `Native` Profile
- `kinal run --project .` 需要 `Native` 且通常应为 `Hosted` Profile
- `kinal vm build --project .` 需要 `VM` Profile
- `kinal vm run --project .` 需要 `VM` Profile
- `kinal vm pack --project .` 需要 `VM` Profile

如果项目同时有 native 和 VM Profile，记得用 `--profile` 明确选择。

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
| `.knproj` | 项目文件 |
| `.knc` | 编译后的字节码文件（KNC 格式） |
| `.klib` | 打包的 Package 归档 |
| `.knpkg.json` | Package 清单文件 |

## 下一步

- [语言概览](../language/overview.md)
- [kinal 编译器 CLI](../cli/compiler.md)
- [Package 系统](../cli/packages.md)
