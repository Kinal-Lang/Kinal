# `kinal fmt`: Code Formatter

`fmt` is built into the C compiler tool. It formats source text; it does not replace parsing, semantic checking, or runtime tests.

## Commands and Exit Status

```bash
kinal fmt main.kn
kinal fmt src/
kinal fmt main.kn helpers.kn
kinal fmt --check src/
kinal fmt --stdout main.kn
```

Normal file mode replaces changed files in place. Directory arguments recursively collect `.kn` files; pass other supported source-file spellings explicitly when needed. `--check` leaves files unchanged and returns nonzero when formatting differs. Invalid options, read failures and other errors can also return nonzero.

| Option | Meaning |
|--------|---------|
| `--check` | Compare against formatted text without writing files |
| `--stdin` | Read source from standard input |
| `--stdout` | Write formatted text to standard output |
| `--stdin-filepath <path>` | Virtual filename supplied by editor integrations |
| `-h`, `--help` | Show formatter help |

`--stdin` emits formatted text unless `--check` suppresses it; `--stdout` explicitly requests output. In file mode, `--stdout` previews instead of overwriting. With multiple files the outputs are concatenated; use one file for an editor replacement. `--stdin-filepath` is context metadata, not a project-selection or output option.

## Standard Input

```bash
# POSIX shell
kinal fmt --stdin --stdout --stdin-filepath src/Main.kn < src/Main.kn
```

```powershell
# PowerShell
Get-Content -Raw -Encoding utf8 src/Main.kn | kinal fmt --stdin --stdout --stdin-filepath src/Main.kn
```

For an integration that already has a text buffer, pass that buffer directly to the subprocess stdin. The VS Code extension uses this path instead of writing the source file merely to request formatting.

## Formatting Style

The current formatter uses four-space indentation, spaces around ordinary binary operators, spaces after commas, and opening block braces on their own line:

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}
```

It does not rename symbols or convert localized alias tokens into canonical spellings. Naming conventions such as PascalCase function names are project guidance, not automatic formatter rewrites. Keep comments and string contents meaningful and review changes to unusual syntax as part of normal source review.

## CI and Editor Setup

```bash
kinal fmt --check src/
kinal build --project . --emit check -o out/project.kcheck
```

The first command checks formatting; the second checks language semantics. Neither proves runtime correctness. Reformat locally, review the diff, and then repeat the non-mutating check in CI.

For VS Code, place this JSON in `.vscode/settings.json`:

```json
{
    "[kinal]": {
        "editor.formatOnSave": true,
        "editor.defaultFormatter": "kinal.kinal"
    }
}
```

Set `kinal.path` when the compiler is not on PATH. This formatter is registered for Kinal source documents, not for `.knproj` configuration files. Compiler and LSP diagnostic language settings are separate from format-on-save.

## See Also

- [Compiler CLI](compiler.md)
- [Project Files](project-files.md)
- [CLI Design Specification](cli-spec.md)
