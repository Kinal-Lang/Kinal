# kinal vm — Bytecode Virtual Machine

`kinal vm` is the subcommand group for building, inspecting, running, and packaging KinalVM bytecode.

## Bytecode compatibility

The compiler emits KNC2 wire revision 4. Immediately after the magic, little-endian u16 revision and u16 flags comes the producer compiler version (u32 byte length followed by UTF-8). The remaining layout matches revision 3. KinalVM reads revisions 2 and 3 as well. Revision 2 uses signed 32-bit integer constants. Revisions 3 and 4 use signed 64-bit constants.

A different producer compiler version is allowed. Successful execution is silent. If decoding or execution fails after the producer is known, the diagnostic includes the current compiler version expected by the runner and the KNC producer version. A guest returning a nonzero integer is a normal return. Legacy images contain no producer metadata. Runners that understand only revisions 2 and 3 require an update to read revision 4.

## Compile and run in a Native host

The executor itself runs in a Native host. A KNC guest cannot instantiate it inside the standard runner.

Import the optional `IO.Kinal.VM.Executor` package to compile a source string in process and retrieve its bytecode.

```kinal
Unit Host;
Get VM By IO.Kinal.VM.Executor;

Static Function int Main()
{
    VM.Executor compiler = New VM.Executor();
    compiler.Compile("Static Function int Main() { Return 42; }");
    byte[] image = compiler.Knc;
    VM.Executor runner = New VM.Executor();
    runner.LoadBytes(image);
    Return runner.Run();
}
```

Use `LoadFile(path)` to read a complete KNC file or `LoadBytes(data)` for a caller-supplied image. `Run(arguments)` starts a fresh guest and returns its entry point result. Uncaught guest exceptions reach the host as `IO.Error`. A failed compile or load preserves the previous image.

For imports in runtime-compiled code, deploy the guest's standard-library packages in `stdpkg` next to the host or set `compiler.StandardLibraryRoot`. Set `compiler.CacheDirectory` to a writable package extraction directory when required. Source and KNC data stay in memory throughout compilation. The optional package includes the KNC frontend and backend. Ordinary Native apps can be distributed independently of it.

## Array storage

KinalVM allocates a contiguous zeroed buffer for an integer, character, floating-point
or boolean array. Booleans use one byte per element. String and object arrays create
storage in 64-element pages on the first write. An untouched string element reads as
`""`, and an untouched object element reads as `null`.

Assigning an array to another variable shares its indexed storage. `array.Add(value)`
returns an independent array. Either array can subsequently replace an element.
Object references stored in both arrays still refer to the same object. Existing
`.knc` files use this implementation with an updated runner. Repack standalone VM
executables to include it.

Native hosts embedding `IO.Kinal.VM` can access guest arrays through
`ExecutionContext.ArrayLengthOf`, `ArrayGetValue` and `ArraySetValue`. Hosts
that accessed the old typed fields of `HeapArray` directly must migrate to these
methods when rebuilding against the updated package.

## Guest object handles

The VM stores object, array, pointer and capture-cell handles in 256-slot pages.
Growing the heap adds pages as needed. A live object's handle stays valid across
guest collections. The collector can reuse a released handle for another object.

Native hosts can look up a single slot with `ExecutionContext.ObjectAt`,
`ArrayAt`, `PointerAt` or `CellAt`. Each returns `null` for a vacant or
out-of-range slot. To retain a guest object across a callback, use
`ctx.Heap.Pin(value)` and later `ctx.Heap.Unpin(token)`.

The `Objects`, `Arrays`, `Pointers` and `Cells` properties still expose mutable,
exact-length arrays. Reading one materializes that entire table and switches it
to flat storage. Indexed writes are shared until an append creates a new array.
Use the single-slot methods in loops to keep paged storage.

---

## Subcommands

| Subcommand | Purpose |
|------------|---------|
| `kinal vm build` | Compile `.kn` source files to `.knc` bytecode |
| `kinal vm run` | Execute `.kn` source files or `.knc` bytecode files |
| `kinal vm disasm` | Print a readable disassembly of an existing `.knc` file |
| `kinal vm pack` | Bundle source files or `.knc` into a standalone executable (vm-exe) |

---

## kinal vm build

Compile source files to `.knc` KinalVM bytecode.

The selfhost compiler indexes bound functions and globals once per build and grows its bytecode buffers as needed. This reduces compilation time for projects with many event handlers, functions, or global initializers.

```bash
kinal vm build <file.kn> [-o <output.knc>]
```

| Option | Description |
|--------|-------------|
| `-o, --output <file>` | Output file path (default: same name with `.knc` extension) |
| `--project <file\|dir>` | Build from `kinal.knproj` or a legacy project manifest |
| `--profile <name>` | Select a profile from `kinal.knproj` |
| `--no-superloop` | Disable KNC superloop fusion |
| `--superloop` | Enable KNC superloop fusion (default) |
| `--listing <file>` | Write a readable compiler listing (conventionally `.knasm`) |
| `--no-module-discovery` | Disable automatic module discovery |
| `--pkg-root <dir>` | Add a local package root |
| `--stdpkg-root <dir>` | Add an official stdpkg root override |

```bash
# Compile to bytecode
kinal vm build main.kn -o main.knc

# Build from a project file
kinal vm build --project . --profile vm

# Disable superloop
kinal vm build --no-superloop main.kn

# Keep source-level local names in a readable listing
kinal vm build main.kn -o main.knc --listing main.knasm
```

---

## kinal vm run

Execute a `.kn` source file or `.knc` bytecode file.

```bash
kinal vm run <file.kn|file.knc> [-- program-args...]
```

| Option | Description |
|--------|-------------|
| `--project <file\|dir>` | Run from `kinal.knproj` or a legacy project manifest |
| `--profile <name>` | Select a profile from `kinal.knproj` |
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

# Run the default project profile
kinal vm run --project .

# Pass arguments
kinal vm run main.knc -- one "two words"
kinalvm main.knc one "two words"
```

Both commands pass `["one", "two words"]` to `Main(string[] args)`. With no
arguments the array is empty. A packed executable takes its arguments directly,
including `--version` and `--disasm`. To receive arguments in existing bytecode
built by an older selfhost compiler, rebuild it with the updated compiler and run
it with the matching VM.

---

## kinal vm disasm

Print a readable instruction listing from an existing `.knc` file.

```bash
kinal vm disasm [--vm-path <file>] <file.knc>
```

This form works from the binary alone and includes function names, constant values, builtin names, registers, and jump targets. Local variable names are not part of the KNC binary format; use `kinal vm build --listing <file>` when source-level local-name comments are required.

The standalone VM exposes the same binary disassembler:

```bash
kinalvm --disasm main.knc
```

---

## kinal vm pack

Bundle bytecode together with KinalVM into a standalone executable (vm-exe) for deployment with a matching VM template and its host dependencies.

An existing `.knc` picks up interpreter performance improvements when launched with the updated runner. A packed executable carries its own interpreter. Repack the same bytecode with the new VM to update a standalone application.

```bash
kinal vm pack <file.kn|file.knc> [-o <output>]
```

| Option | Description |
|--------|-------------|
| `-o, --output <file>` | Output executable path |
| `--project <file\|dir>` | Pack from `kinal.knproj` or a legacy project manifest |
| `--profile <name>` | Select a profile from `kinal.knproj` |
| `--vm-path <path>` | Specify the KinalVM path used for packing |
| `--target <triple>` | Target platform (affects the VM binary bundled) |
| `--superloop` | Enable superloop fusion |
| `--no-superloop` | Disable superloop fusion |
| `--no-module-discovery` | Disable auto-discovery for source input |
| `--pkg-root <dir>` | Add a local package root |
| `--stdpkg-root <dir>` | Add an official stdpkg root override |

```bash
# Pack from source
kinal vm pack main.kn -o myapp

# Pack from a project file
kinal vm pack --project . --profile vm -o myapp

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

- **Portable bytecode** — Requires a compatible VM and supported runtime/platform operations; compile-time target constants remain fixed
- **Structured binary** — Contains constant, function, type and instruction tables; size depends on the program
- **Direct execution** — Running existing bytecode skips source compilation and native linking

---

## Superloop Fusion

Superloop is a bytecode optimization for recognized integer loop patterns, including eligible empty counting loops, sums and array fill/sum loops. It replaces repeated interpreter dispatch with specialized loop instructions while preserving supported loop semantics.

Fusion is enabled by default. Use `--no-superloop` when compiling source to compare the general lowering, and inspect `--listing`/disassembly to see the chosen instructions. The flag does not supply Async/Await support, create a task scheduler, switch threading models or make the VM freestanding. Current C KNC rejects general Await and IO.Async.Run.

---

## Standalone kinalvm Tool

`kinalvm` is a standalone KinalVM executable that can run or inspect `.knc` directly:

```bash
kinalvm <file.knc>
kinalvm --disasm <file.knc>
```

---

## KinalVM vs. Native Compilation

| Feature | KinalVM (`.knc`) | Native Compilation |
|---------|-----------------|-------------------|
| Execution | Bytecode interpreter; no documented JIT | Compiled machine code |
| Startup | Loads bytecode into the VM | Loads an already linked executable |
| Portability | Compatible VM/runtime required | Rebuild for the target OS/ABI |
| Deployment requirements | Matching VM or packed template, plus host dependencies | Target runtime/system/package dependencies |
| Use case | Development, scripting, embedding | Production deployment |

---

## See Also

- [CLI Overview](compiler.md) — Overview of all subcommands
- [Async Programming](../language/async.md) — Superloop and coroutines
- [Freestanding](../advanced/freestanding.md) — Using the VM in constrained environments
- [IO.Runtime Constants](../advanced/runtime-environment-constants.md) — Detecting the current execution backend in source code
- [IO.Version Constants](../advanced/version-constants.md) — Reading compiler and VM version information in source code
- [CLI Specification](cli-spec.md) — CLI design specification (developer reference)

See [Backend Support](../advanced/backend-support.md) for language limitations, numeric behavior and deployment requirements. The standalone VM supports `--version` and `--disasm`; use `kinal vm --help` for CLI help.
