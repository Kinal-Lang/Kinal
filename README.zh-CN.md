<p align="right">
  <a href="README.md">English</a> | <strong>简体中文</strong>
</p>

<p align="center">
  <img src="infra/assets/Kinal.png" alt="Kinal" width="100">
</p>

<h1 align="center">Kinal</h1>

<p align="center">
  <strong>兼具底层控制与现代体验的编译型语言</strong>
</p>

<p align="center">
  <a href="https://github.com/Kinal-Lang/Kinal/releases/latest"><img src="https://img.shields.io/github/v/release/Kinal-Lang/Kinal?display_name=tag&sort=semver&style=flat-square&label=version"></a>
  <a href="#"><img src="https://img.shields.io/badge/backend-LLVM%20%2B%20KinalVM-orange?style=flat-square"></a>
  <a href="#"><img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey?style=flat-square"></a>
  <a href="#"><img src="https://img.shields.io/badge/license-MIT-green?style=flat-square"></a>
  <a href="#"><img src="https://img.shields.io/badge/PRs-welcome-brightgreen?style=flat-square"></a>
</p>

<p align="center">
  <a href="docs/zh-CN/index.md">文档</a> ·
  <a href="docs/zh-CN/getting-started/installation.md">安装</a> ·
  <a href="docs/zh-CN/getting-started/hello-world.md">快速开始</a> ·
  <a href="docs/zh-CN/language/overview.md">语言概览</a> ·
  <a href="docs/zh-CN/cli/compiler.md">CLI 参考</a>
</p>

---

Kinal 的诞生，来自对现有语言取舍方式的不满足。

我们不接受"要性能就牺牲体验"，
也不接受"要易用就放弃底层控制"。

Kinal 的目标很简单——
**用清晰直观的语法，提供真正的原生控制与跨平台能力。**

```kinal
Unit App;
Get IO.Console;

Static Function int Main()
{
    IO.Console.PrintLine("Hello, Kinal!");
    Return 0;
}
```

```bash
kinal run hello.kn              # 编译并直接运行（类似 go run）
kinal build hello.kn -o hello   # 编译为原生可执行文件
kinal vm run hello.kn           # 通过 KinalVM 字节码运行
```

---

使用 `kinal` 可以构建原生可执行文件或 KNC 字节码。Windows、Linux 和 macOS 的 SDK 可在[发行页面](https://github.com/Kinal-Lang/Kinal/releases/latest)下载。

两条后端共享语言分析，但运行能力存在差异，参见[后端支持](docs/zh-CN/advanced/backend-support.md)。大型程序参见[项目文件](docs/zh-CN/cli/project-files.md)，GC 与所有权参见[内存和生命周期](docs/zh-CN/advanced/memory-management.md)。

## 核心特色

- **静态类型** — 编译期类型检查，`Var` 类型推断让代码简洁而安全
- **三级安全模型** — `Safe` / `Trusted` / `Unsafe` 精确划定危险操作边界
- **原生面向对象** — 类、接口、虚方法、泛型，语法直观自然
- **双后端** — LLVM 原生编译与 KinalVM 字节码解释，功能边界见后端支持表
- **Block 对象** — 代码块对象，支持 Record 标签、区间执行和 Jump 跳转
- **Native 异步** — `Async`/`Await` 与 Task 运行时支持
- **C 互操作** — `Extern`/`Delegate` 遵循明确 C ABI 与所有权规则
- **Meta 元数据** — 自定义属性与 IO.Meta 运行时保留记录
- **完整工具链** — 编译器、虚拟机、格式化器、包管理、LSP 语言服务器

---

## 快速开始

### 安装


选项1：使用安装脚本（推荐，自动处理依赖和环境变量）

```bash
curl -fsSL https://kinal.org/install.sh | bash
```

选项2：手动安装

从 [Releases](https://github.com/Kinal-Lang/Kinal/releases) 下载对应平台的预编译包，完整解压后将工具目录加入 `PATH`。LLVM 库、运行时、链接工具和标准包应随可执行文件保留，不能仅移动单个程序。


最后验证安装：

```bash
kinal --version   # 验证安装
```

### 第一个程序

创建 `hello.kn`：

```kinal
Unit App;
Get IO.Console;

Static Function int Main()
{
    IO.Console.PrintLine("Hello, Kinal!");
    Return 0;
}
```

```bash
# 方式一：编译为原生可执行文件
kinal build hello.kn -o hello
./hello

# 方式二：快速运行（编译+执行+自动清理）
kinal run hello.kn

# 方式三：通过 KinalVM 运行
kinal vm run hello.kn
```

---

## 语言速览

```kinal
Unit App.Demo;
Get IO.Console;
Get IO.Async;

// 接口
Interface IShape
{
    Function float Area();
}

// 类 + 继承接口
Class Circle By IShape
{
    Private float radius;

    Public Constructor(float r)
    {
        This.radius = r;
    }

    Public Function float Area()
    {
        Return 3.14159 * This.radius * This.radius;
    }
}

// 泛型函数
Function void PrintValue<T>(T value)
{
    IO.Console.PrintLine([string](value));
}

// 异步函数
Async Function string FetchGreeting()
{
    Await IO.Async.Sleep(100);
    Return "Hello from async!";
}

// 入口
Static Async Function int Main()
{
    // 面向对象
    IShape shape = New Circle(5.0);
    IO.Console.PrintLine([string](shape.Area()));

    // 泛型
    PrintValue<int>(42);

    // 异步
    string msg = Await FetchGreeting();
    IO.Console.PrintLine(msg);

    Return 0;
}
```

---

## 项目结构

```
Kinal/
├── apps/
│   ├── kinal/          # Kinal 编译器（C + LLVM）
│   ├── kinal-lsp/      # LSP 语言服务器（C++）
│   └── kinalvm/        # KinalVM 字节码虚拟机（用 Kinal 编写）
├── libs/
│   ├── runtime/        # 运行时库（C）
│   └── std/            # 标准库包（stdpkg）
├── docs/               # 英文文档
│   └── zh-CN/          # 中文文档
├── dev/                # 开发者文档
├── tests/              # 测试用例
├── benchmarks/         # Kinal 性能基准
└── infra/              # 构建基础设施
```

在 Windows 上对比 stage0 和 stage3 的构建、启动和运行耗时，见 [Kinal 性能基准](benchmarks/README.md)。

---

## 从源码构建

**依赖：** Python 3.10+ · CMake 3.20+ · Ninja · LLVM/Clang

```bash
# 获取预构建 LLVM
python x.py fetch llvm-prebuilt

# 开发构建
python x.py dev

# 运行测试
python x.py test

# 发布构建
python x.py dist
```

详见 [构建指南](dev/zh-CN/build.md)。

---

## 文档

|  | 中文 | English |
|---|------|---------|
| 用户文档 | [docs/zh-CN/](docs/zh-CN/index.md) | [docs/](docs/index.md) |
| 开发者文档 | [dev/zh-CN/](dev/zh-CN/build.md) | [dev/](dev/build.md) |

---

## 工具链

| 工具 | 说明 |
|------|------|
| `kinal build` | 编译为原生可执行文件、目标文件、汇编或 LLVM IR |
| `kinal run` | 编译并立即运行，类似 `go run` |
| `kinal vm build/run/disasm/pack` | KinalVM 字节码构建、检查、执行、打包 |
| `kinal fmt` | 代码格式化 |
| `kinal pkg build/info/unpack` | 包管理 |
| `kinal-lsp` | LSP 语言服务器（VS Code 等编辑器补全、诊断） |

---

## 编辑器支持

Kinal 提供官方 [VS Code 插件](https://marketplace.visualstudio.com/items?itemName=kinal.kinal)，支持：

- 语法高亮
- 自动补全
- 错误诊断
- 跳转定义
- 快速反射
- 元数据描述

👉 在 VS Code 扩展市场搜索 **Kinal** 或直接安装：[Kinal for VS Code](https://marketplace.visualstudio.com/items?itemName=kinal.kinal)

插件源码：[Kinal-Lang/Kinal-VSCode](https://github.com/Kinal-Lang/Kinal-VSCode)

---

## 诊断本地化

Kinal 支持中英文错误提示：

```bash
kinal build --lang zh main.kn    # 中文诊断
kinal build --lang en main.kn    # 英文诊断（默认）
```

---

## 许可证

本项目基于 [MIT License](LICENSE.txt) 开源。
