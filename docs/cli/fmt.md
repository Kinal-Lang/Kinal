# kinal fmt — Code Formatter

`kinal fmt` is the official code formatting subcommand for the Kinal language. It is built into the `kinal` toolchain and requires no separate installation.

## Usage

```bash
kinal fmt <file.kn>
kinal fmt <dir/>
kinal fmt <file1.kn> <file2.kn> ...
```

Formatting **modifies files in-place**, replacing file contents with Kinal-compliant formatting.

---

## Options

| Option | Description |
|--------|-------------|
| `--check` | Only check whether files are already formatted; do not modify them |
| `--stdin` | Read source code from standard input |
| `--stdout` | Write formatted output to standard output (do not modify files) |
| `--stdin-filepath <path>` | Provide a file path context for stdin input |
| `-h, --help` | Show fmt help |

---

## Examples

### Format a single file

```bash
kinal fmt main.kn
```

### Format all .kn files in a directory

```bash
kinal fmt src/
```

### Format multiple files

```bash
kinal fmt main.kn utils.kn helpers.kn
```

---

## Kinal Code Style

`kinal fmt` follows these formatting rules:

### Indentation

- Use **4 spaces** for indentation (no tabs)
- Each nesting level adds 4 spaces

### Parentheses and Braces

- The opening brace `{` for function bodies, class bodies, and control flow statements is on the same line as the statement
- The closing brace `}` is on its own line

```kinal
// Correct
Function int Add(int a, int b)
{
    Return a + b;
}
```

### Keywords and Identifiers

- Keywords use initial capital letters (`If`, `While`, `Function`, etc.) as enforced by the compiler; `fmt` does not recase keywords
- Class names use PascalCase (`MyClass`)
- Function names use PascalCase (`GetValue`)
- Variable names use camelCase (`myValue`)

### Spacing

- One space before and after operators: `a + b`, `x == y`
- One space after commas: `Function int Add(int a, int b)`
- No spaces inside parentheses: `Add(a, b)` not `Add( a, b )`

### Blank Lines

- One blank line between top-level declarations
- Blank lines between logical sections inside a function are allowed but limited to one

---

## Checking Formatting in CI

Use `--check` to verify formatting compliance in CI:

```bash
kinal fmt --check src/
```

`--check` mode does not modify files; it exits with a non-zero status code only when formatting differences exist.

You can also use the `git diff` approach:

```bash
kinal fmt src/
git diff --exit-code
```

---

## Editor Integration

### VS Code

After installing the Kinal language extension, configure `kinal fmt` as the format-on-save formatter:

```json
// .vscode/settings.json
{
    "[kinal]":
    {
        "editor.formatOnSave": true,
        "editor.defaultFormatter": "kinal-lang.kinal"
    }
}
```

---

## See Also

- [CLI Overview](compiler.md) — Overview of all subcommands
- [Project Structure](../getting-started/project-structure.md) — Recommended directory layout
- [CLI Specification](cli-spec.md) — CLI design specification (developer reference)
