# Kinal Language Documentation

> **Kinal** is a statically typed, object-oriented general-purpose compiled language. It builds native code via LLVM and includes a built-in KNC bytecode virtual machine (KinalVM) for dual-mode execution. Design goals: clear and intuitive syntax, clearly defined safety levels, and a complete toolchain.

---

## Table of Contents

### 🚀 Getting Started
| Document | Description |
|----------|-------------|
| [Installation Guide](getting-started/installation.md) | How to install the Kinal compiler and toolchain |
| [Hello World](getting-started/hello-world.md) | Write and run your first Kinal program |
| [Project Structure](getting-started/project-structure.md) | Organizing single-file, multi-file, and package projects |

### 📖 Language Reference
| Document | Description |
|----------|-------------|
| [Language Overview](language/overview.md) | Key design principles and language highlights |
| [Type System](language/types.md) | Primitive types, arrays, pointers, classes, enums, and more |
| [Variables and Constants](language/variables.md) | `Var`, `Const`, and type inference |
| [Operators](language/operators.md) | Arithmetic, logical, bitwise, and type conversion operators |
| [Control Flow](language/control-flow.md) | `If`/`Else`, `While`, `For`, `Switch`, `Break`/`Continue` |
| [Functions](language/functions.md) | Function declarations, default parameters, named arguments, type parameters, anonymous functions |
| [Classes and Inheritance](language/classes.md) | `Class`, constructors, access control, inheritance, virtual methods |
| [Interfaces](language/interfaces.md) | `Interface` declarations and implementations |
| [Structs and Enums](language/structs-enums.md) | `Struct`, `Enum`, and underlying types |
| [Module System](language/modules.md) | `Unit`, `Get`, aliases, and symbol imports |
| [Generics](language/generics.md) | Generic functions and type parameters |
| [Safety Levels](language/safety.md) | Three-tier safety model: `Safe`, `Trusted`, `Unsafe` |
| [Async Programming](language/async.md) | `Async`/`Await`, Tasks, and `IO.Async` |
| [Blocks](language/blocks.md) | Block literals, Record labels, and Jump transfers |
| [Meta (Metadata Annotations)](language/meta.md) | Custom attributes, lifecycles, and runtime reflection |
| [FFI (Foreign Function Interface)](language/ffi.md) | `Extern`, `Delegate`, calling conventions, C interop |
| [String Features](language/strings.md) | String concatenation, formatting, raw strings, and character arrays |
| [Exception Handling](language/exceptions.md) | `Try`/`Catch`/`Throw` and `IO.Error` |

### 📦 Standard Library
| Document | Description |
|----------|-------------|
| [Standard Library Overview](stdlib/overview.md) | Overview of the IO.* namespace |
| [IO.Console](stdlib/console.md) | Console input and output |
| [IO.Text](stdlib/text.md) | String operations |
| [IO.File / IO.Directory](stdlib/filesystem.md) | File and directory operations |
| [IO.Path](stdlib/path.md) | Path handling |
| [IO.Collections](stdlib/collections.md) | list, dict, set |
| [IO.Async](stdlib/async.md) | Coroutine tasks and process management |
| [IO.Time](stdlib/time.md) | Time and timing |
| [IO.System](stdlib/system.md) | System calls and dynamic libraries |
| [IO.Meta](stdlib/meta.md) | Runtime metadata queries |

### 🔧 CLI Toolchain
| Document | Description |
|----------|-------------|
| [kinal Compiler](cli/compiler.md) | Complete compiler options reference |
| [KinalVM](cli/vm.md) | Bytecode virtual machine and `.knc` format |
| [Code Formatter](cli/fmt.md) | `kinal fmt` usage |
| [Package System](cli/packages.md) | `.knpkg.json`, `.klib`, and package management |

### ⚙️ Advanced Topics
| Document | Description |
|----------|-------------|
| [Compilation Pipeline](advanced/compilation-pipeline.md) | Lexing → Parsing → Semantics → Code Generation |
| [Freestanding / Embedded](advanced/freestanding.md) | `--freestanding`, `--runtime`, hostless environments |
| [Cross-Compilation](advanced/cross-compilation.md) | `--target`, target triples, and platform aliases |
| [Linking and LTO](advanced/linking.md) | Linker backends, LTO, static/dynamic libraries |
| [Runtime Environment Constants Proposal](advanced/runtime-environment-constants.md) | Compile-time constants for distinguishing KinalVM vs. native runtime |

---

## Kinal at a Glance

```kinal
Unit Hello;

Get IO.Console;

Static Function int Main()
{
    IO.Console.PrintLine("Hello, Kinal!");
    Return 0;
}
```

**Compile and run:**
```bash
kinal build hello.kn -o hello
./hello
```

**Run via KinalVM (no LLVM required):**
```bash
kinal vm run hello.kn
```

---

## Language Highlights

- **Statically typed** — All variables have types determined at compile time; `Var` type inference is supported
- **Three-tier safety model** — `Safe` / `Trusted` / `Unsafe` for fine-grained control over dangerous operations
- **Native OOP** — Classes, interfaces, virtual methods, and inheritance with intuitive syntax
- **Block objects** — A unique "suspendable code block" mechanism with label-based jumps
- **Full async** — First-class `Async`/`Await` that integrates seamlessly with synchronous code
- **Dual backends** — LLVM native compilation + built-in KinalVM bytecode interpreter
- **C interop** — Zero-overhead `Extern` / `Delegate` calls to C function libraries
- **Meta annotations** — A custom annotation system queryable at compile time or runtime
