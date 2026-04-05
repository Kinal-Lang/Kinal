# Kinal CLI Design Specification

This document defines the design conventions for the `kinal` command-line tool. All new CLI features must follow this specification.

---

## 1. Overall Architecture

```
kinal <subcommand> [options] [positional-args...]
```

`kinal` uses a **subcommand** architecture. Each top-level operation maps to a subcommand; subcommands may have second-level subcommands.

### Subcommand Tree

```
kinal
├── build       Compile to native outputs
├── run         Compile and run a temporary native executable
├── fmt         Code formatting
├── vm          Bytecode operations
│   ├── build   Compile to .knc
│   ├── run     Execute .kn or .knc via KinalVM
│   └── pack    Bundle into a vm-exe
└── pkg         Package management
    ├── build   Build .klib
    ├── info    View .klib metadata
    └── unpack  Unpack .klib
```

### Standalone Tools

| Tool | Purpose |
|------|---------|
| `kinalvm` | Standalone KinalVM executable; runs `.knc` directly |

Standalone tools are not subcommands; they are separate executables.

---

## 2. Naming Rules

### 2.1 Subcommand Names

- Use **lowercase English words** with no hyphens or underscores
- Must be a **verb** or **noun** (representing the target of the operation)
- Length: 2–8 characters
- Prefer community-standard names (`build`, `run`, `fmt`, `test`)

```
✅ build, run, fmt, pkg, vm, test, init, clean
❌ Build, compile-and-link, format_code, do-build
```

### 2.2 Option Names

- Use `--` prefix + **lowercase kebab-case**
- Short options use `-` + **a single letter**
- Boolean options are negated with a `--no-` prefix

```
✅ --output, --emit, --target, --link-file, --no-superloop
❌ --Output, --link_file, --linkFile, --nosuperloop
```

### 2.3 Option Values

- Enum values use **lowercase**, with multi-word values hyphenated
- File paths are passed through as-is without case conversion

```
✅ --emit obj, --kind static, --linker lld
❌ --emit OBJ, --kind Static
```

---

## 3. Option Categories and Conventions

### 3.1 Common Options

The following options are available in **all subcommands** (processed before subcommand parsing):

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help |
| `--color <auto\|always\|never>` | Color output mode |
| `--lang <en\|zh>` | Diagnostic language |
| `--locale-file <file>` | Custom locale file |

### 3.2 Output Options

All subcommands that produce file output use a unified output option:

| Option | Description |
|--------|-------------|
| `-o, --output <file>` | Specify output path |

> **Note:** The long form is always `--output`; do not use `--out`.

### 3.3 Boolean Options

- Options that default to `true` are disabled via `--no-<name>`
- Options that default to `false` are enabled via `--<name>`
- Do not use the `--<name>=true/false` form

```
--superloop          ← Enabled by default
--no-superloop       ← Explicitly disabled

--keep-temps         ← Disabled by default; explicitly enabled
```

### 3.4 Multi-Value Options

Repeatable options are accumulated by passing them multiple times; do not use comma separation:

```bash
# Correct
kinal build main.kn -L ./lib1 -L ./lib2 -l foo -l bar

# Incorrect
kinal build main.kn -L ./lib1,./lib2
```

---

## 4. Subcommand Design Principles

### 4.1 Single Responsibility

Each subcommand does one thing. When an operation involves multiple steps:
- Provide a composite subcommand (e.g. `kinal run` = compile + execute)
- Do not pack unrelated functionality into a single subcommand

### 4.2 Explicit over Implicit

- Do not rely on the order of positional arguments to change semantics
- Do not guess user intent from file extensions (`.knc` input to `build` should error, not silently switch to VM mode)
- Each operation has a clear subcommand entry point

### 4.3 Subcommand Groups

When operations share the same target object, use **second-level subcommands**:

```
kinal vm build    ← Operations around VM bytecode
kinal vm run
kinal vm pack

kinal pkg build   ← Operations around packages
kinal pkg info
kinal pkg unpack
```

Do not flatten these into top-level commands:

```
❌ kinal vm-build
❌ kinal pkg-info
```

### 4.4 Option Ownership

Options belong to the subcommand they appear under and do not "inherit" semantics from the parent:

```
kinal build --target win64 ...    ← --target belongs to build
kinal vm pack --target win64 ...  ← --target belongs to vm pack (same meaning, declared independently)
```

---

## 5. Help System

### 5.1 Help Hierarchy

```bash
kinal --help              # Top-level help: list subcommands + build/run/vm/pkg quick ref
kinal build --help        # Subcommand help: list all options for this subcommand
kinal vm --help           # Subcommand group help: list second-level subcommands
kinal vm build --help     # Second-level subcommand help
```

### 5.2 Help Format

```
Usage: kinal <subcommand> [options] [args...]

Subcommands:
  build       Compile source to native outputs
  run         Compile to a temporary native executable and run it
  fmt         Format code
  vm          Bytecode operations
  pkg         Package management

Top-level quick reference:
  -h, --help                  Show this help
  --color <auto|always|never> Color output
  --lang <en|zh>              Diagnostic language
  kinal build ...             Native build entry
  kinal run ...               Native quick-run entry
  kinal vm ...                Bytecode build/run/pack entry
  kinal pkg ...               Package build/info/unpack entry

Use "kinal <subcommand> --help" for detailed subcommand help.
```

### 5.3 Error Suggestions

When encountering an unknown subcommand or option, provide a suggestion:

```
kinal buid main.kn
error: unknown subcommand 'buid', did you mean 'build'?
```

---

## 6. Exit Codes

| Exit Code | Meaning |
|-----------|---------|
| `0` | Success |
| `1` | Compilation/runtime error |
| `2` | CLI argument error (unknown option, missing argument, etc.) |

---

## 7. Canonical Entry Points

The CLI maintains only canonical entry points; no legacy aliases or top-level implicit routing:

- Native builds always use `kinal build`
- Native quick-run always uses `kinal run`
- Bytecode build, run, and pack always use `kinal vm build|run|pack`
- Package operations always use `kinal pkg build|info|unpack`
- Documentation, tests, and build scripts must not use legacy forms such as `kinal main.kn`, `kinal --vm`, or `--emit-klib`

---

## 8. New Subcommand Checklist

When adding a new subcommand, confirm each of the following in order:

- [ ] Subcommand name follows §2.1 naming rules
- [ ] All options follow §2.2 / §2.3 naming rules
- [ ] Output option uses `-o, --output` (§3.2)
- [ ] Boolean options use `--no-` negation (§3.3)
- [ ] Implements `-h, --help` (§5.1)
- [ ] Exit codes follow §6 conventions
- [ ] Registered in `print_help()` top-level help
- [ ] Entry added to subcommand table in `docs/cli/compiler.md`
- [ ] Corresponding `docs/cli/<subcommand>.md` documentation written
- [ ] Subcommand dispatch logic added in `kn_main()`
- [ ] No new compatibility aliases or legacy routing added (§7)
- [ ] `locales/zh-CN.json` help text updated

---

## 9. Example: Adding a `kinal test` Subcommand

Suppose we want to add a subcommand for running project tests:

### 1) Define the Interface

```
kinal test [options] [file-or-dir...]
  --filter <pattern>   Run only matching tests
  --timeout <seconds>  Timeout duration
```

Check: name `test` ✅, options in kebab-case ✅

### 2) Implement Dispatch

```c
// kn_driver_cli.inc — beginning of kn_main()
if (argc >= 2 && kn_strcmp(argv[1], "test") == 0)
    return kn_test_main(argc - 1, argv + 1);
```

### 3) Update Help

Add a `test` entry to the subcommand list in `print_help()`.

### 4) Write Documentation

Create `docs/cli/test.md` and add a row to the subcommand table in `compiler.md`.
