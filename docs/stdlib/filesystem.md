# IO.File and IO.Directory

These IO.Core units provide text-file operations and directory discovery. Import each Unit you use. Their source implementations are Trusted, and Safe callers may use their public interface. KinalVM follows Native semantics for file append and optional reads, including a null result from `ReadIfExists` when the file is missing.

## Complete IO.File API

All paths are string parameters. Text operations use NUL-terminated text, not an arbitrary binary-buffer interface.

| Function | Result | Behavior |
|----------|--------|----------|
| `Exists(string path)` | `bool` | Check file existence |
| `Create(string path)` | `bool` | Create/truncate |
| `Touch(string path)` | `bool` | Create if needed without truncating existing content |
| `ReadAllText(string path)` | `string` | Read text; current failure fallback is empty text |
| `ReadFirstLine(string path)` | `string` | Read through the first LF, NUL, or end of file. Discard the terminator and one trailing CR. Returns an empty string on failure. |
| `ReadIfExists(string path)` | `string` | Return null when absent; otherwise read |
| `ReadOrDefault(string path, string fallback)` | `string` | Use fallback when absent |
| `ReadOr(string path, string fallback)` | `string` | Alias of ReadOrDefault |
| `WriteAllText(string path, string text)` | `bool` | Overwrite with text |
| `AppendAllText(string path, string text)` | `bool` | Append at end of file, creating it if missing. Null and empty text append zero bytes. |
| `AppendLine(string path, string text)` | `bool` | Append the text followed by one LF. Null and empty text each append a bare LF. |
| `Delete(string path)` | `bool` | Attempt file deletion |
| `DeleteIfExists(string path)` | `bool` | True if absent or deletion succeeds |
| `Size(string path)` | `int` | Byte size; -1 on failure |
| `IsEmpty(string path)` | `bool` | Whether Size is exactly zero |
| `Copy(string source, string destination)` | `bool` | Copy a file |
| `Move(string source, string destination)` | `bool` | Move/rename a file |
| `ReplaceText(string path, string from, string to)` | `bool` | Replace text and rewrite; false if source file is absent |
| `ReadLines(string path)` | `list` | Read text and apply IO.Text.Lines |
| `WriteLines(string path, list lines)` | `bool` | Join with LF and write; no implicit final LF |

Check bool results; returning false is not a caught exception. `ReadAllText` returning empty does not distinguish a valid empty file from every failure. `ReadIfExists`/ReadOr only guard existence, not permissions or races after the check. `Size == -1` is not an empty file.

`ReadFirstLine` reads in blocks of up to 4 KiB. Its memory use follows the length of the first line, even when a large file follows it. An interior CR and an initial UTF-8 BOM are preserved. On a pipe, the call can return at LF or NUL while the writer is still open. Bytes following the terminator in the same block have already been consumed from the pipe.

Native and KinalVM use the operating system's append mode. An append preserves every existing byte, including embedded NULs, and needs write access only. Its memory use is independent of the existing file size. On local filesystems that support concurrent append, the OS places each write at the current end of the file. A large payload can require several writes, allowing another writer's data between chunks. A write failure can leave a partial append. To create missing parent directories, call `IO.Directory.Ensure` first.

## Complete IO.Directory API

Native and KinalVM support all six directory operations. `Files(path, recursive)` returns a sorted list of file paths. Pass `true` to include files in subdirectories. Windows directory queries accept UTF-8 paths and drive roots.

```kinal
Get IO.Directory;
```

| Function | Result | Behavior |
|----------|--------|----------|
| `Exists(string path)` | `bool` | Check directory existence |
| `Create(string path)` | `bool` | Create one directory level |
| `Ensure(string path)` | `bool` | Create the directory and missing parents |
| `Delete(string path)` | `bool` | Delete an empty directory |
| `DeleteIfExists(string path)` | `bool` | True if absent or empty-directory deletion succeeds |
| `Files(string path, bool recursive)` | `list` | Regular file paths in stable ordinal order |

Files returns an empty list if the directory cannot be opened. False recursion includes direct regular files; true descends through ordinary subdirectories. Directory symlinks/reparse points are not followed. Empty/unreadable discovery is not a permission-diagnostic API. Delete does not recursively delete a nonempty tree.

## Complete Text Round Trip

This example uses a dedicated scratch directory in the working directory. Run it where creating these example files is appropriate:

```kinal
Unit Docs.Files;
Get IO.Console;
Get IO.File;
Get IO.Directory;
Get IO.Path;

Static Function int Main()
{
    string directory = "docs_files_example";
    If (!IO.Directory.Ensure(directory)) { Return 1; }
    string path = IO.Path.Combine(directory, "message.txt");
    If (!IO.File.WriteAllText(path, "Hello, Kinal!\n")) { Return 2; }

    IO.Console.PrintLine(IO.File.ReadFirstLine(path));
    IO.Console.PrintLine(IO.File.Size(path) > 0);
    IO.Console.PrintLine(IO.Directory.Files(directory, false).Count());
    IO.Console.PrintLine(IO.File.DeleteIfExists(path));
    IO.Directory.Delete(directory);
    Return 0;
}
```

On a fresh scratch directory:

```text
Hello, Kinal!
true
1
true
```

For a text table, ReadLines/Split can help with simple delimiter-separated data. Splitting on commas alone is not a complete CSV parser: quoted commas, escaped quotes and embedded newlines need a real parser.

## Paths, Encoding and Backend Limits

Relative paths are relative to the program's working directory, not automatically to its source file or `.knproj`. Use IO.Path for target path policy. Native Windows host boundaries currently use ANSI-facing APIs in several paths; UTF-8 source literals do not establish universal Unicode-path support.

In KinalVM, pass the result of `IO.File.ReadLines(path)` directly to `IO.Text.Join(lines, separator)`, or write a list of lines with `IO.File.WriteLines(path, lines)`.

## See Also

- [IO.Path](path.md)
- [IO.Text](text.md)
- [IO.System](system.md)
- [Memory and Lifetimes](../advanced/memory-management.md)
