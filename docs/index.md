# Kinal Language Documentation

This documentation follows the current C compiler, runtime and standard library. Selfhost-only entries are explicitly marked. Start with installation and Hello World, then use the project, language and API references. [Native/VM support](advanced/backend-support.md) describes backend-specific limits.

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
| [IO.Type](stdlib/types.md) | Conversions, any/string helpers and representation boundaries |
| [IO.Console](stdlib/console.md) | Console input and output |
| [IO.Text](stdlib/text.md) | String operations |
| [IO.File / IO.Directory](stdlib/filesystem.md) | File and directory operations |
| [IO.Path](stdlib/path.md) | Path handling |
| [IO.Collection](stdlib/collections.md) | list, dict, set |
| [IO.Async](stdlib/async.md) | Coroutine tasks and process management |
| [IO.Time](stdlib/time.md) | Time and timing |
| [IO.System](stdlib/system.md) | System calls and dynamic libraries |
| [IO.Meta](stdlib/meta.md) | Runtime metadata queries |
| [IO.Core](stdlib/core.md) | `IO.Char` and `IO.Math` |
| [IO.Json](stdlib/json.md) | JSON objects, lists, formatting and wrappers |
| [IO.Request](stdlib/request.md) | Outbound HTTP client |
| [IO.UI](stdlib/ui.md) | Native desktop UI package |
| [IO.Web](stdlib/web.md) | Embedded HTTP server package |
| [IO.Cat](stdlib/cat.md) | Sample package |

### 🔧 CLI Toolchain
| Document | Description |
|----------|-------------|
| [kinal Compiler](cli/compiler.md) | Complete compiler options reference |
| [Project Files](cli/project-files.md) | Complete kinal.knproj fields, source modes, profiles and LSP rules |
| [Project Metrics](cli/metrics.md) | Selfhost complexity analysis and text/JSON/HTML reports |
| [KinalVM](cli/vm.md) | Bytecode virtual machine and `.knc` format |
| [Code Formatter](cli/fmt.md) | `kinal fmt` usage |
| [Package System](cli/packages.md) | `.knpkg.json`, `.klib`, and package management |

### ⚙️ Advanced Topics
| Document | Description |
|----------|-------------|
| [Compilation Pipeline](advanced/compilation-pipeline.md) | Lexing → Parsing → Semantics → Code Generation |
| [Backend Support](advanced/backend-support.md) | Native/KNC capabilities, limitations and format compatibility |
| [Memory and Lifetimes](advanced/memory-management.md) | Values, shared backing, GC, raw memory and FFI ownership |
| [Diagnostic Code Reference](advanced/error-codes/index.md) | Shared compiler and LSP diagnostic codes, examples, explanations, and fixes |
| [Freestanding / Embedded](advanced/freestanding.md) | `--freestanding`, `--runtime`, hostless environments |
| [Cross-Compilation](advanced/cross-compilation.md) | `--target`, target triples, and platform aliases |
| [Linking and LTO](advanced/linking.md) | Linker backends, LTO, static/dynamic libraries |
| [Runtime Environment Constants](advanced/runtime-environment-constants.md) | Compile-time constants for distinguishing KinalVM vs. native runtime |
| [IO.Version Constants](advanced/version-constants.md) | Compile-time constants for compiler and KinalVM version information |

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

**Compile and run bytecode via KinalVM:**
```bash
kinal vm run hello.kn
```

---

## Language Highlights

- **Statically typed** — All variables have types determined at compile time; `Var` type inference is supported
- **Three-tier safety model** — `Safe` / `Trusted` / `Unsafe` for fine-grained control over dangerous operations
- **Native OOP** — Classes, interfaces, virtual methods, and inheritance with intuitive syntax
- **Block objects** — A unique "suspendable code block" mechanism with label-based jumps
- **Native async** — `Async`/`Await` with documented backend limits
- **Dual backends** — LLVM native compilation + built-in KinalVM bytecode interpreter
- **C interop** — Explicit `Extern` / `Delegate` declarations with C ABI and ownership rules
- **Meta annotations** — Custom annotations and runtime-retained metadata queries

## Example Conventions

Complete examples with Main can be saved as standalone .kn files; multi-file examples require every listed file. Other syntax fragments omit enclosing declarations, imports or earlier definitions; ... denotes a placeholder. Incorrect diagnostic examples are deliberately invalid. Select Native, VM, freestanding and platform requirements as stated on each page; highlighting or frontend success does not prove runtime support. Backslash command continuations use POSIX shell syntax; in PowerShell combine the command into one line and run current-directory Windows executables as .\program.exe.

This reference follows current C source. An older installed release may behave differently. Include tool versions, minimal source, the complete command and actual diagnostics when reporting a problem.
