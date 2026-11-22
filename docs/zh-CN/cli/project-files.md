# 项目文件：`kinal.knproj`

`kinal.knproj` 定义本地源码图、构建配置、包搜索根、链接输入和编辑器分析范围。它是专用配置语言，不是可执行 Kinal 源码，也不是 JSON。本文对应 C 编译器及共享的项目感知 LSP。

## 选择文件与路径

```bash
kinal build --project .
kinal run --project . --profile native
kinal vm build --project . --profile vm
kinal vm run --project . --profile vm
kinal build --project ./kinal.knproj --emit check -o out/app.kcheck
```

目录参数优先寻找 `kinal.knproj`；`kinal.pkg.json`、`kinal.pkg` 仅保留兼容回退。文件参数可指向其他名称的 `.knproj`。`--profile` 选择配置；未指定则使用 `DefaultProfile`，该字段缺失时使用首个配置。至少需要一个 Profile；未知配置名会报错。

源码路径、输出路径、包搜索根及 `LibDirs`、`LinkFiles`、`LinkRoots` 相对项目文件目录解析。`Libs` 是库名，`LinkArgs` 是原始参数。当前 C 驱动原样传递 `LinkerPath` 和 `Script`；从其他工作目录调用项目时建议为这两项使用绝对路径。

输出、目标、环境、入口和链接器等显式命令行标量选项优先于配置。包根与链接条目会与命令行输入合并，不能按单一标量覆盖理解。Profile 之间没有继承关系。

## 完整双文件工程

```text
hello-project/
├── kinal.knproj
└── src/
    ├── Main.kn
    └── Greeter.kn
```

`kinal.knproj`：

```knproj
Project HelloProject
{
    DefaultProfile = "native";
    Workspace { Ignore = ["out/**", ".git/**"]; }
    SourceSet "app"
    {
        Roots = ["src"];
        Include = ["**/*.kn"];
        RequireUnit = true;
    }
    Profile "native"
    {
        Source { Entry = "src/Main.kn"; Sets = ["app"]; Mode = ReachableUnits; }
        Build { Backend = Native; Environment = Hosted; Output = "out/hello"; }
    }
    Profile "vm"
    {
        Source { Entry = "src/Main.kn"; Sets = ["app"]; Mode = ReachableUnits; }
        Build { Backend = VM; Output = "out/hello.knc"; Superloop = true; }
    }
    Lsp { Profile = "native"; StrictProjectScope = true; }
}
```

`src/Main.kn`：

```kinal
Unit Hello.Main;
Get IO.Console;
Get Hello.Greeter;

Static Function int Main()
{
    IO.Console.PrintLine(Hello.Greeter.Greet("Kinal"));
    Return 0;
}
```

`src/Greeter.kn`：

```kinal
Unit Hello.Greeter;

Function string Greet(string name)
{
    Return "Hello, " + name + "!";
}
```

两种 run 命令均输出 `Hello, Kinal!`。导入身份取决于声明的 Unit，文件名本身不会创建 Unit。普通 `Get Hello.Greeter;` 仍要求像示例一样限定调用名称。

## 配置语法

文件包含一个 `Project <名称> { ... }` 块。命名块采用名称加花括号；字段采用 `Name = value;`。路径和字符串使用引号，列表使用 `["first", "second"]`。布尔值为 `true`、`false`；`Native` 等枚举式值是配置标识符，不需要导入。支持源码词法器的 `//` 与 `/* ... */` 注释。

未知字段和枚举值会报错；当前格式没有任意自定义属性、环境变量插值或 Profile 继承。命令行选项不一定有对应配置字段，请使用下表中的实际字段。

## 根级与源码字段

| 位置 | 字段 | 含义 |
|------|------|------|
| Project | `DefaultProfile` | CLI 默认配置名 |
| `Workspace` | `Ignore` | 本地源码候选排除模式 |
| `SourceSet "名称"` | `Roots` | 递归扫描的源码目录 |
| SourceSet | `Files` | 显式源文件路径 |
| SourceSet | `Include` | 包含模式；省略或空列表表示接受所有候选 |
| SourceSet | `Exclude` | 排除模式，优先于包含模式 |
| SourceSet | `RequireUnit` | 要求声明 Unit，默认 true |
| `Profile "名称".Source` | `Entry` | 入口源文件 |
| Source | `Sets` | 活动 SourceSet 名称；省略或空列表选择全部已声明集合 |
| Source | `Mode` | 收集后的本地文件参与方式 |
| Source | `AutoDiscovery` | 兼容开关，显式 `Mode` 优先 |

模式支持单路径段内的 `*`、一个非分隔符字符的 `?`，以及跨路径段的 `**`。当前匹配不区分大小写，并统一路径分隔符。扫描目录时同时匹配相对根目录和相对项目目录的路径；显式 `Files` 按项目相对路径匹配。`Workspace.Ignore`、`Exclude` 优先于 `Include`。

重叠集合中的文件会去重。显式 `Files` 缺失会报错，缺失的根目录目前被跳过。活动集合必须非空且包含入口。`RequireUnit = false` 只放宽 Unit 检查，不会使 unsafe-alias 文件获得工程编译资格。

### 源码参与模式

| Mode | 编译的本地文件 |
|------|----------------|
| `FileOnly` | 仅入口文件 |
| `EntryUnit` | 入口及活动集合中声明同一 Unit 的其他文件 |
| `ReachableUnits` | 入口 Unit 及通过 `Get` 可达的活动本地 Unit |
| `AllSources` | 活动集合内全部本地文件 |

Mode 从已收集候选中选择参与者，不会绕过 SourceSet 边界。`AllSources` 也会诊断未被引用文件中的错误。包和标准库解析独立于本地参与模式。

声明了源码集合而未指定 Mode 时，默认 `ReachableUnits`。否则 `AutoDiscovery = true` 映射到 `ReachableUnits`，false 映射到 `FileOnly`。没有源码集合规则的旧式项目保留入口目录发现流程。显式源码图中，`--no-module-discovery` 将本地模式降为 `FileOnly`，仍保留包／标准库解析。新项目建议显式声明 SourceSet 和 Mode。

## Build 字段

| 字段 | 值或含义 |
|------|----------|
| `Backend` | `Native`、`VM` |
| `Environment` | 原生构建的 `Hosted`、`Freestanding` |
| `Runtime` | `None`、`Alloc`、`GC`；原生 freestanding 内存配置 |
| `Panic` | `Trap`、`Loop`；freestanding panic 策略 |
| `Target` | 目标 triple 或支持的别名 |
| `Output` | 输出产物路径 |
| `EntrySymbol` | 入口符号覆盖；通常为 `Main` 或 freestanding 的 `KMain` |
| `Linker` | `LLD`、`Zig`、`MSVC` |
| `LinkerPath` | 链接器可执行文件选择；同时指定 `Linker` |
| `Superloop` | VM superloop 布尔开关 |
| `HideConsole` | 布尔值，默认 `false`。在 Windows 上设为 `true`，原生和 VM 打包的 EXE 启动时只打开应用窗体。 |

`Source.Entry` 指定源码发现的起点，`Build.EntrySymbol` 从本次构建的源码中选择执行入口。例如，两个 Unit 都有 `Main` 时，可设置 `EntrySymbol = "App.Desktop.Main";`。Native 的构建和运行、VM 的构建、运行及打包都会执行所选入口。单独做语义检查时，可以同时检查有各自入口的多个 Unit。

字段省略时沿用命令的正常默认行为。`kinal build`／`run` 要求兼容 Native 的配置；`kinal vm build`／`run`／`pack` 要求兼容 VM 的配置。命令不会因为 Profile 选择了另一后端就静默切换。

`Runtime`、`Panic` 是 freestanding 选项，不是 hosted GC 调优项；`Backend = VM` 也不表示具备裸机 VM。当前 C `.knproj` 语法没有 `Emit`、`LTO`、`CRT`、优化等级或警告字段，应使用相应命令行选项。

## Packages 与 Link 字段

`Packages` 可放在项目级和 Profile 内：

| 字段 | 含义 |
|------|------|
| `Roots` | 本地包搜索根 |
| `OfficialRoots` | 官方标准包搜索根 |
| `Pins` | 固定包版本，如 `["IO.UI@1.0.0"]`。当前配置可覆盖工程中同名包的版本。 |

项目级与配置级搜索根会合并，驱动还会加入项目的 `kpkg/`。包根不是 SourceSet，也不是项目输出目录。清单和 `.klib` 归档参见[包系统](packages.md)。

Profile 的 `Link` 块支持：

| 字段 | 含义 |
|------|------|
| `Script` | freestanding 链接脚本 |
| `NoCRT` | 禁用 CRT 启动代码 |
| `NoDefaultLibs` | 禁用自动注入的平台默认库 |
| `LibDirs` | 库搜索目录列表 |
| `Libs` | 库名列表 |
| `LinkFiles` | 精确目标文件／静态库路径列表 |
| `LinkArgs` | 原始链接参数列表，每项一个参数 |
| `LinkRoots` | 展开为目标相关搜索目录的依赖根 |

包搜索根不能代替缺失的原生链接输入。对象／库必须与目标兼容；排查符号时使用 `--show-link`、`--show-link-search`。参见[链接](../advanced/linking.md)。

## LSP 字段

| 字段 | 含义 |
|------|------|
| `Profile` | 分析配置；否则使用 DefaultProfile，再回退首个配置 |
| `ExtraSets` | 额外参与编辑器分析的集合 |
| `StrictProjectScope` | 当前文件位于活动本地源码图外时禁用宽泛工作区回退 |

LSP 寻找最近的 `kinal.knproj`。额外集合不改变 CLI 构建输入。仅供编辑器使用的测试／辅助源码应独立成集合。后端、环境、入口从所选 Profile 推导，无需额外的编辑器 `analysisEnvironment` 设置。

## 排错

| 现象 | 检查项 |
|------|--------|
| 找不到配置 | `--profile`、`DefaultProfile`、配置声明名 |
| 入口被排除／源码图为空 | Source.Sets、Roots、模式和 Source.Entry |
| 缺失 Unit／unsafe alias | 工程内使用正常声明 Unit 的源码 |
| 未知导入 Unit | 精确 Unit 名、活动模式／集合、包根 |
| `Scoped Symbol Required` | 使用限定名或显式符号别名 |
| CLI 成功、编辑器报错 | 最近项目文件、LSP Profile、ExtraSets、严格范围 |
| 后端不匹配 | Native 配置配原生命令，VM 配置配 vm 命令 |

## 相关文档

- [项目结构](../getting-started/project-structure.md)
- [编译器 CLI](compiler.md)
- [Native 与 VM 支持](../advanced/backend-support.md)
