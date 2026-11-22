<p align="right">
  <strong>English</strong> | <a href="README.zh-CN.md">简体中文</a>
</p>

<p align="center">
  <img src="infra/assets/Kinal.png" alt="Kinal" width="100">
</p>

<h1 align="center">Kinal</h1>

<p align="center">
  <strong>A compiled language with native control and modern ergonomics</strong>
</p>

<p align="center">
  <a href="https://github.com/Kinal-Lang/Kinal/releases/latest"><img src="https://img.shields.io/github/v/release/Kinal-Lang/Kinal?display_name=tag&sort=semver&style=flat-square&label=version"></a>
  <a href="#"><img src="https://img.shields.io/badge/backend-LLVM%20%2B%20KinalVM-orange?style=flat-square"></a>
  <a href="#"><img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey?style=flat-square"></a>
  <a href="#"><img src="https://img.shields.io/badge/license-MIT-green?style=flat-square"></a>
  <a href="#"><img src="https://img.shields.io/badge/PRs-welcome-brightgreen?style=flat-square"></a>
</p>

<p align="center">
  <a href="docs/index.md">Docs</a> |
  <a href="docs/getting-started/installation.md">Installation</a> |
  <a href="docs/getting-started/hello-world.md">Quick Start</a> |
  <a href="docs/language/overview.md">Language Overview</a> |
  <a href="docs/cli/compiler.md">CLI Reference</a>
</p>

---

Kinal was born from dissatisfaction with the trade-offs made by existing languages.

We don't accept "sacrifice ergonomics for performance",
and we don't accept "give up low-level control for ease of use".

Kinal's goal is simple --
**provide genuine native control and cross-platform capability, with clear and intuitive syntax.**

---

## Core Features

- **Statically typed** -- compile-time type checking with Var inference for concise, safe code
- **Three-tier safety** -- Safe / Trusted / Unsafe precisely bound dangerous operations
- **Native OOP** -- classes, interfaces, virtual methods, generics, with clean syntax
- **Dual backends** -- LLVM native compilation and KinalVM bytecode, with explicit backend support limits
- **Block objects** -- code-block objects with Record labels and explicit range/jump execution
- **First-class async** -- Native Async/Await with Task runtime support
- **C interop** -- Extern/Delegate calls with explicit C ABI and ownership rules
- **Meta annotations** -- custom attributes with runtime-retained IO.Meta records
- **Complete toolchain** -- compiler, VM, formatter, package manager, LSP server

---

Use `kinal` to build native executables or KNC bytecode. Download the SDK for Windows, Linux or macOS from [Releases](https://github.com/Kinal-Lang/Kinal/releases/latest).

## Quick Start

### Installation

Option 1: use the install script (recommended, it handles the toolchain layout and PATH setup for you)

```bash
curl -fsSL https://kinal.org/install.sh | bash
```

Option 2: install manually

Download the matching bundle from [Releases](https://github.com/Kinal-Lang/Kinal/releases), extract it completely, and add its tool directory to PATH. Preserve LLVM libraries, runtime files, linker tools and standard packages beside the executables.

Then verify the installation:

```bash
kinal --version
```

### Your First Program

Create `hello.kn`:

```kinal
Unit Hello;
Get IO.Console;

Static Function int Main()
{
    IO.Console.PrintLine("Hello, Kinal!");
    Return 0;
}
```

```bash
kinal run hello.kn
kinal vm run hello.kn
```

The two backends share language analysis but have different runtime capabilities; see [Backend Support](docs/advanced/backend-support.md). For larger programs start with [Project Files](docs/cli/project-files.md), and for ownership/GC see [Memory and Lifetimes](docs/advanced/memory-management.md).

---

## Repository Layout

    Kinal/
    apps/  kinal/ kinal-lsp/ kinalvm/
    libs/  runtime/ std/
    docs/  zh-CN/
    dev/
    tests/
    benchmarks/
    infra/

Compare stage0 and stage3 build time, startup time, and runtime on Windows with the [Kinal benchmark suite](benchmarks/README.md).

---

## Building from Source

Requirements: Python 3.10+ CMake 3.20+ Ninja LLVM/Clang

    python x.py fetch llvm-prebuilt
    python x.py dev
    python x.py test
    python x.py dist

See [Build Guide](dev/build.md) for details.

---

## Documentation

| | English | Chinese |
|---|---------|---------|
| User docs | [docs/](docs/index.md) | [docs/zh-CN/](docs/zh-CN/index.md) |
| Developer docs | [dev/](dev/build.md) | [dev/zh-CN/](dev/zh-CN/build.md) |

---

## Toolchain

| Tool | Description |
|------|-------------|
| kinal build | Compile to native executable, object file, assembly, or LLVM IR |
| kinal run | Compile and run immediately |
| kinal vm build/run/disasm/pack | KinalVM bytecode build, inspection, run, and pack |
| kinal fmt | Code formatter |
| kinal pkg build/info/unpack | Package management |
| kinal-lsp-server | LSP server for editor completion and diagnostics |

---

## Editor Support

Kinal has an official [VS Code extension](https://marketplace.visualstudio.com/items?itemName=kinal.kinal).

Extension source: [Kinal-Lang/Kinal-VSCode](https://github.com/Kinal-Lang/Kinal-VSCode)

---

## Diagnostic Localization

    kinal build --lang en main.kn
    kinal build --lang zh main.kn

---

## License

This project is open source under the [MIT License](LICENSE.txt).
