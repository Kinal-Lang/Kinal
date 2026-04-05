# kinal vm — Bytecode Virtual Machine

`kinal vm` is the subcommand group for managing KinalVM bytecode, providing build, run, and pack operations.

---

## Subcommands

| Subcommand | Purpose |
|------------|---------|
| `kinal vm build` | Compile `.kn` source files to `.knc` bytecode |
| `kinal vm run` | Execute `.kn` source files or `.knc` bytecode files |
| `kinal vm pack` | Bundle source files or `.knc` into a standalone executable (vm-exe) |

---

## kinal vm build

Compile source files to `.knc` KinalVM bytecode.

```bash
kinal vm build <file.kn> [-o <output.knc>]
```

| Option | Description |
|--------|-------------|
| `-o, --output <file>` | Output file path (default: same name with `.knc` extension) |
| `--no-superloop` | Disable KNC superloop fusion |
| `--superloop` | Enable KNC superloop fusion (default) |
| `--no-module-discovery` | Disable automatic module discovery |

```bash
# Compile to bytecode
kinal vm build main.kn -o main.knc

# Disable superloop
kinal vm build --no-superloop main.kn
```

---

## kinal vm run

Execute a `.kn` source file or `.knc` bytecode file.

```bash
kinal vm run <file.kn|file.knc> [-- program-args...]
```

| Option | Description |
|--------|-------------|
| `--superloop` | Enable superloop (default) |
| `--no-superloop` | Disable superloop |
| `--vm-path <path>` | Specify the KinalVM executable path |
| `--no-module-discovery` | Disable auto-discovery when compiling source input |
| `--keep-temps` | Retain automatically generated temporary `.knc` files |
| `--pkg-root <dir>` | Add a local package root |
| `--stdpkg-root <dir>` | Add an official stdpkg root override |

```bash
# Run source directly
kinal vm run main.kn

# Pass arguments
kinal vm run main.knc -- arg1 arg2
```

---

## kinal vm pack

Bundle bytecode together with KinalVM into a standalone executable (vm-exe) for zero-dependency deployment.

```bash
kinal vm pack <file.kn|file.knc> [-o <output>]
```

| Option | Description |
|--------|-------------|
| `-o, --output <file>` | Output executable path |
| `--vm-path <path>` | Specify the KinalVM path used for packing |
| `--target <triple>` | Target platform (affects the VM binary bundled) |

```bash
# Pack from source
kinal vm pack main.kn -o myapp

# Pack from existing bytecode
kinal vm pack main.knc -o myapp

# Run the packed program
./myapp arg1 arg2
```

---

## kinal run vs kinal vm

| | `kinal run` | `kinal vm build` + `kinal vm run` |
|---|---|---|
| Compilation backend | LLVM native code | KinalVM bytecode |
| Steps | One step | Multi-step |
| Intermediate files | Auto-cleaned (temporary exe) | `.knc` is retained |
| Use case | Quick run of `.kn` source | Production bytecode, distribution, cross-platform |

`kinal run main.kn` performs: compile → native exe → run → delete temporary exe.  
`kinal vm run` takes the VM route: if the input is source code, it first compiles to a temporary `.knc`, then passes it to KinalVM for execution.

```bash
# Equivalent operations (VM route):
kinal vm build main.kn -o main.knc
kinal vm run main.knc
```

---

## `.knc` Bytecode Format

`.knc` is Kinal's native bytecode format:

- **Platform-independent** — The same `.knc` file runs on any platform with KinalVM installed
- **Compact** — Smaller than LLVM IR
- **Fast startup** — Skips the linking phase

---

## Superloop Scheduling Model

KinalVM's async runtime is event-loop based:

- **Enabled** (default): Supports `IO.Async.Spawn`, `Await`, and coroutine scheduling
- **Disabled** (`--no-superloop`): Single-threaded sequential execution, suitable for synchronous programs or embedded scenarios

---

## Standalone kinalvm Tool

`kinalvm` is a standalone KinalVM executable that can run `.knc` directly:

```bash
kinalvm [--superloop|--no-superloop] <file.knc> [-- program-args...]
```

---

## KinalVM vs. Native Compilation

| Feature | KinalVM (`.knc`) | Native Compilation |
|---------|-----------------|-------------------|
| Execution speed | Slower (interpreted/JIT) | Fast (machine code) |
| Startup time | Fast (no linking) | Slower (linking required) |
| Platform independence | Yes | No |
| Deployment requirements | Requires KinalVM | No extra dependencies |
| Use case | Development, scripting, embedding | Production deployment |

---

## See Also

- [CLI Overview](compiler.md) — Overview of all subcommands
- [Async Programming](../language/async.md) — Superloop and coroutines
- [Freestanding](../advanced/freestanding.md) — Using the VM in constrained environments
- [IO.Runtime Constants](../advanced/runtime-environment-constants.md) — Detecting the current execution backend in source code
- [CLI Specification](cli-spec.md) — CLI design specification (developer reference)
