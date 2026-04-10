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
  <a href="#"><img src="https://img.shields.io/badge/version-0.6.0-blue?style=flat-square"></a>
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
- **Dual backends** -- LLVM native compilation and KinalVM bytecode, same code both ways
- **Block objects** -- a unique suspendable code-block mechanism with Record labels and Jump transfers
- **First-class async** -- Async/Await with a built-in event loop
- **C interop** -- zero-overhead Extern/Delegate calls into C libraries
- **Meta annotations** -- custom attribute system queryable at compile time or runtime
- **Complete toolchain** -- compiler, VM, formatter, package manager, LSP server

---

## Quick Start

### Installation

Option 1: use the install script (recommended, it handles the toolchain layout and PATH setup for you)

```bash
curl -sSL https://kinal.org/install.sh | bash
```

Option 2: install manually

Download the pre-built package for your platform from [Releases](https://github.com/user/kinal/releases) and place kinal (and optionally kinalvm) on your PATH.

Then verify the installation:

```bash
kinal --version
```

### Your First Program

Create hello.kn and run:

    kinal run hello.kn

---

## Repository Layout

    Kinal/
    apps/  kinal/ kinal-lsp/ kinalvm/
    libs/  runtime/ std/
    docs/  zh-CN/
    dev/
    tests/
    infra/

---

## Building from Source

Requirements: Python 3.10+ CMake 3.20+ Ninja LLVM/Clang

    python x.py fetch llvm-prebuilt
    python x.py dev
    python x.py test
    python x.py release

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
| kinal vm build/run/pack | KinalVM bytecode build, run, and pack |
| kinal fmt | Code formatter |
| kinal pkg build/info/unpack | Package management |
| kinal-lsp | LSP server for editor completion and diagnostics |

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
