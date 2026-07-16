# 项目结构

Kinal 支持单文件程序、多文件模块、Package，以及完整的项目文件。本文介绍常见目录结构，并说明 `kinal.knproj` 如何定义本地项目图。

## 单文件程序

最简单的结构就是一个 `.kn` 文件：

```text
myapp/
└── main.kn
```

编译：

```bash
kinal build main.kn -o myapp
```

## 多文件模块

多个文件可以共同组成一个程序：

```text
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

如果不走 project 模式、直接按入口文件编译，编译器仍然可以自动发现附近的文件：

```bash
kinal build main.kn -o myapp
```

如果想禁用这种旧式的目录自动发现：

```bash
kinal build main.kn utils.kn models.kn -o myapp --no-module-discovery
```

## 同一文件多个 Unit

一个文件也可以通过嵌套 Block 给多个 Unit 贡献声明，但通常还是建议一文件一个 Unit。

## Package 结构

可复用库使用 Package 清单：

```text
MyLib/
├── 1.0.0/
│   ├── package.knpkg.json
│   ├── src/
│   │   └── MyLib/
│   │       ├── Core.kn
│   │       └── Utils.kn
│   └── lib/
│       └── MyLib.klib
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
kinal build main.kn --pkg-root ./packages -o myapp
```

在代码中：

```kinal
Get MyLib;
Get ML By MyLib;
```

## 项目文件（`kinal.knproj`）

对于稍大的项目，建议把构建配置和项目边界都放进项目根目录的 `kinal.knproj`。

```text
myapp/
├── kinal.knproj
├── src/
│   ├── Main.kn
│   └── App/
│       └── Greeter.kn
├── tests/
│   └── Main.kn
└── kpkg/
```

**kinal.knproj**

```kinal
Project MyApp
{
    DefaultProfile = "native";

    Workspace
    {
        Ignore = ["out/**", ".git/**"];
    }

    Packages
    {
        Roots = ["./kpkg"];
    }

    SourceSet "app"
    {
        Roots = ["src"];
        Include = ["**/*.kn"];
        Exclude = ["generated/**"];
        RequireUnit = true;
    }

    SourceSet "tests"
    {
        Roots = ["tests"];
        Include = ["**/*.kn"];
        RequireUnit = true;
    }

    Profile "native"
    {
        Source
        {
            Entry = "src/Main.kn";
            Sets = ["app"];
            Mode = ReachableUnits;
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
            Sets = ["app"];
            Mode = ReachableUnits;
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
        ExtraSets = ["tests"];
        StrictProjectScope = true;
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
    Workspace { ... }
    SourceSet "app" { ... }
    Packages { ... }
    Profile "native" { ... }
    Lsp { ... }
}
```

- `Project <Name>`：声明项目名
- `DefaultProfile`：当命令行未传 `--profile` 时使用的 Profile
- `Workspace`：定义工具应该忽略的路径或模式
- `SourceSet "<name>"`：定义哪些本地源文件属于项目
- `Packages`：对所有 Profile 生效的共享包根目录
- `Profile "<name>"`：一组构建配置
- `Lsp`：选择编辑器分析使用的 Profile，以及额外可见的 `SourceSet`

### 顶层字段

#### `DefaultProfile`

```kinal
DefaultProfile = "native";
```

它会影响这些命令默认选择哪个 Profile：

```bash
kinal build --project .
kinal run --project .
```

如果省略 `DefaultProfile`，则默认使用第一个声明的 Profile。

#### `Workspace`

```kinal
Workspace
{
    Ignore = ["out/**", ".git/**"];
}
```

- `Ignore`：CLI / LSP 不应当作正常项目内容去扫描的路径模式

#### `SourceSet`

```kinal
SourceSet "app"
{
    Roots = ["src"];
    Files = ["tools/Generate.kn"];
    Include = ["**/*.kn"];
    Exclude = ["scratch/**"];
    RequireUnit = true;
}
```

`SourceSet` 用来控制本地项目图。

- `Roots`：递归扫描的目录
- `Files`：额外显式加入的源文件
- `Include`：允许进入该集合的模式
- `Exclude`：从该集合排除的模式
- `RequireUnit`：该集合中的文件是否要求声明 `Unit`

在 project 模式下，本地源码成员关系由激活的 `SourceSet` 决定，而不是靠宽松的整目录猜测。

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
    ExtraSets = ["tests"];
    StrictProjectScope = true;
}
```

- `Profile`：编辑器分析优先使用的 Profile
- `ExtraSets`：在编辑器里额外可见的 `SourceSet`
- `StrictProjectScope`：让 LSP 严格停留在声明过的项目图内，而不是回退到宽松的整个工作区扫描

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

- `Source`：入口文件和本地源码图模式
- `Build`：后端、运行环境、输出路径、目标平台等构建设置
- `Link`：链接器与链接输入设置
- `Packages`：仅对该 Profile 生效的额外包根目录

### `Source` 部分

```kinal
Source
{
    Entry = "src/Main.kn";
    Sets = ["app"];
    Mode = ReachableUnits;
}
```

- `Entry`：该 Profile 的入口源文件
- `Sets`：该 Profile 激活哪些 `SourceSet`
- `Mode`：本地文件如何被拉入构建

`Mode` 支持这些值：

- `FileOnly`：只编译入口文件
- `EntryUnit`：编译入口文件，以及声明同一 `Unit` 的其他文件
- `ReachableUnits`：从入口单元开始，再根据 `Get` 拉入本地 Unit
- `AllSources`：把激活的 `SourceSet` 中所有文件全部编译进去

`AutoDiscovery` 仍然保留兼容，等价关系如下：

- `AutoDiscovery = true`：等价于 `Mode = ReachableUnits`
- `AutoDiscovery = false`：等价于 `Mode = FileOnly`

### `Build` 部分

```kinal
Build
{
    Backend = Native;
    Environment = Hosted;
    Output = "out/myapp";
}
```

支持的字段：

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

- `Native`：生成本地可执行文件或本地目标产物
- `VM`：生成 KinalVM 字节码或面向 VM 的输出

常见命令对应关系：

- `Native` Profile：`kinal build --project .`、`kinal run --project .`
- `VM` Profile：`kinal vm build --project .`、`kinal vm run --project .`、`kinal vm pack --project .`

#### `Environment`

```kinal
Environment = Hosted;
```

支持值：

- `Hosted`：普通操作系统进程环境，默认入口通常是 `Main`
- `Freestanding`：裸机、内核、无宿主环境，默认入口通常是 `KMain`

#### `Runtime`

```kinal
Runtime = None;
```

支持值：

- `None`
- `Alloc`
- `GC`

这个字段主要对 freestanding/native 这类配置有意义。

#### `Panic`

```kinal
Panic = Trap;
```

支持值：

- `Trap`
- `Loop`

#### `Target`

```kinal
Target = "x86_64-linux-gnu";
```

用于覆盖该 Profile 的目标三元组。

#### `Output`

```kinal
Output = "out/myapp";
```

当命令行没有传 `-o` 时，使用这里定义的输出路径。

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

当链接器不在 `PATH` 中时，可以在这里指定其路径。

#### `Superloop`

```kinal
Superloop = true;
```

这个字段主要用于 VM Profile，用来控制生成的字节码或 bundle 是否启用 superloop 模式。

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
- `LinkRoots`：依赖根目录，会被展开成目标相关的搜索路径

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

    SourceSet "app"
    {
        Roots = ["src"];
        Include = ["**/*.kn"];
    }

    Profile "native"
    {
        Source
        {
            Entry = "src/Main.kn";
            Sets = ["app"];
            Mode = ReachableUnits;
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
            Sets = ["app"];
            Mode = ReachableUnits;
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

    SourceSet "kernel"
    {
        Roots = ["src"];
        Include = ["**/*.kn"];
    }

    Profile "kernel"
    {
        Source
        {
            Entry = "src/kernel.kn";
            Sets = ["kernel"];
            Mode = ReachableUnits;
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

### 命令与 Profile 的匹配关系

- `kinal build --project .` 需要 `Native` Profile
- `kinal run --project .` 需要 `Native` 且通常应为 `Hosted` Profile
- `kinal vm build --project .` 需要 `VM` Profile
- `kinal vm run --project .` 需要 `VM` Profile
- `kinal vm pack --project .` 需要 `VM` Profile

如果项目同时有 native 和 VM Profile，记得用 `--profile` 明确选择。

## 打包与分发

将源码打包成 `.klib`：

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
| `.klib` | 打包归档 |
| `.knpkg.json` | Package 清单文件 |

## 下一步

- [语言概览](../language/overview.md)
- [kinal 编译器 CLI](../cli/compiler.md)
- [Package 系统](../cli/packages.md)
