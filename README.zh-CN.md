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
  <a href="#"><img src="https://img.shields.io/badge/version-0.5.0-blue?style=flat-square"></a>
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

## 核心特色

- **静态类型** — 编译期类型检查，`Var` 类型推断让代码简洁而安全
- **三级安全模型** — `Safe` / `Trusted` / `Unsafe` 精确划定危险操作边界
- **原生面向对象** — 类、接口、虚方法、泛型，语法直观自然
- **双后端** — LLVM 原生编译与 KinalVM 字节码解释，同一份代码两种运行方式
- **Block 对象** — 独创的可暂停代码块机制，支持 Record 标签和 Jump 跳转
- **完整异步** — `Async`/`Await` 一等公民，内建事件循环
- **C 互操作** — `Extern`/`Delegate` 零开销调用 C 函数库
- **Meta 元数据** — 编译期与运行时均可查询的自定义注解系统
- **完整工具链** — 编译器、虚拟机、格式化器、包管理、LSP 语言服务器

---

## 快速开始

### 安装

从 [Releases](https://github.com/user/kinal/releases) 下载对应平台的预编译包，将 `kinal`（和可选的 `kinalvm`）放入 `PATH`。

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
└── infra/              # 构建基础设施
```

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
python x.py release
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
| `kinal vm build/run/pack` | KinalVM 字节码构建、执行、打包 |
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
