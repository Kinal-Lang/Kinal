# Formatting and diagnostic controls

The selfhost formatter uses Kinal's lexer and a Kinal-owned formatting state
machine. It invokes neither stage0 nor an external formatting process. The
only native operations are raw file/console I/O leaves.

```sh
kinal-selfhost fmt source.kn src/
kinal-selfhost fmt --check src/
kinal-selfhost fmt --stdout source.kn
kinal-selfhost fmt --stdin --stdin-filepath editor-buffer.kn
```

Directories are scanned recursively for case-sensitive `.kn` suffixes. Explicit
files may have other extensions. Files are rewritten only when the result
changes; `--stdout` leaves them untouched. `--check` leaves files untouched,
returns 1 when any input differs, and otherwise returns 0. Like stage0,
`--check --stdout` suppresses file output, but emits stdin output when `--stdin`
is selected. Stdin without `--check` always writes the formatted bytes to stdout.
`--stdin-filepath` supplies the diagnostic path for editor-buffer errors.

Formatting preserves UTF-8 bytes, a leading UTF-8 BOM, literal spellings,
comments, and the first LF/CRLF newline convention. Inputs without a newline
use CRLF, matching stage0. Layout uses four-space indentation, Allman braces,
and the existing stage0 whitespace rules. Alias, Foreach, and In keep their
keyword boundaries. Formatted output is idempotent.

Lexically malformed input (unknown characters, invalid/unterminated literals,
unterminated block comments, embedded NUL) returns 1 with an error on stderr,
without rewriting that input or producing partial formatted stdout. Formatting
is not a syntax or semantic check. For multiple inputs, earlier successfully
processed files can have been written before a later failure, as with stage0.

## Diagnostics

The modern build, run, and VM build driver accepts:

- `--color auto|always|never`: auto enables ANSI color only for a terminal;
  always enables it even with redirected output
- `--lang en|zh|zh-CN`: select English or built-in Simplified Chinese messages
- `--locale-file path.json`: load an external locale map, with explicit entries
  taking precedence over the selected built-in language
- `--Werror`: make enabled warnings fail compilation before artifact generation
- `--warn-level n`: select a nonnegative warning level; zero suppresses warnings,
  including their promotion by Werror; current selfhost warnings are level 1
- `--dump-locale-en path.json`: export the English diagnostic catalog, also
  accepted without a source input

The established selfhost diagnostic envelope and stdout channel are retained.
These controls do not claim byte-identical stage0 diagnostics or add new lint
warnings. An English message remains the fallback when no translation exists.
Diagnostic title/detail payloads such as symbol names and types are retained
rather than replaced with a generic catalog detail.

External locale files use stage0's object-map shape:

```json
{
  "ui.stage.parser": { "text": "Parser" },
  "ui.severity.warning": { "text": "Warning" },
  "ui.diag.location": { "text": "in <{0}> at {1}:{2}" },
  "W-SYN-00001": {
    "title": "Legacy Array Syntax",
    "detail": "Use 'Type[] name' instead of 'Type name[]'"
  },
  "selfhost.Parser.Expected ';'": { "title": "Semicolon required" }
}
```

Stage0 registry codes are reused for matching title/detail templates. A
selfhost-specific message can be addressed by `selfhost.<Stage>.<Title>`;
this explicit key takes precedence over a registry entry. Unspecified or empty
fields retain their fallback. The location template uses `{0}` for the path,
`{1}` for the line, and `{2}` for the column. Invalid or missing locale files
are reported as CLI errors instead of silently ignored.

The built-in catalog is generated from `apps/kinal/src/kn_diag_registry.c` and
`infra/assets/locales/zh-CN.json`:

```sh
python infra/scripts/sync_selfhost_diagnostic_catalog.py
python infra/scripts/sync_selfhost_diagnostic_catalog.py --check
python tests/selfhost/check_format_diagnostics.py --compiler <selfhost> \
  --stage0 <stage0> --output <test-output-directory>
KINAL_TEST_COMPILER=<compiler> python tests/test_formatter_keywords.py -v
```

The contract compares the formatter with stage0 on real compiler sources and
fixtures, repeats formatting for idempotence, covers file/stdin/check/stdout
exit policy, and verifies Unicode/BOM/newline preservation, malformed-input
non-destruction, warning promotion/suppression, color, external locales,
Chinese output, invalid option values, and English catalog round trips.
