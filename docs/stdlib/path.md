# IO.Path — Path Operations

`IO.Path` provides cross-platform tools for joining, decomposing, and checking file paths. All operations are pure string processing and do not access the file system.

## Import

```kinal
Get IO.Path;
```

---

## Function Reference

### Joining

| Function | Signature | Description |
|----------|-----------|-------------|
| `Combine(a, b)` | `string, string → string` | Join two path segments (also aliased as `Join`) |
| `Join(a, b)` | `string, string → string` | Same as `Combine` |
| `Join3(a, b, c)` | `string, string, string → string` | Join three path segments |
| `Join4(a, b, c, d)` | `string, string, string, string → string` | Join four path segments |

```kinal
string full = IO.Path.Combine("C:/Users/user", "Documents");
// "C:/Users/user/Documents"

string deep = IO.Path.Join3("base", "sub", "file.txt");
// "base/sub/file.txt"
```

### Decomposing

| Function | Signature | Description |
|----------|-----------|-------------|
| `FileName(path)` | `string → string` | File name (including extension) |
| `BaseName(path)` | `string → string` | File name (without extension, also called `Stem`) |
| `Extension(path)` | `string → string` | Extension (including `.`), e.g. `".txt"` |
| `Directory(path)` | `string → string` | Containing directory (also called `Parent`) |

```kinal
string path = "/home/user/docs/report.pdf";

IO.Path.FileName(path);    // "report.pdf"
IO.Path.BaseName(path);    // "report"
IO.Path.Extension(path);   // ".pdf"
IO.Path.Directory(path);   // "/home/user/docs"
```

### Transformation

| Function | Signature | Description |
|----------|-----------|-------------|
| `Normalize(path)` | `string → string` | Normalize the path (remove `.`/`..`, unify separators) |
| `ChangeExtension(path, ext)` | `string, string → string` | Replace the extension |
| `EnsureTrailingSeparator(path)` | `string → string` | Ensure the path ends with a separator |

```kinal
IO.Path.Normalize("a/b/../c/./d");         // "a/c/d"
IO.Path.ChangeExtension("file.txt", ".md"); // "file.md"
IO.Path.EnsureTrailingSeparator("/home");   // "/home/"
```

### Checking

| Function | Signature | Description |
|----------|-----------|-------------|
| `HasExtension(path)` | `string → bool` | Whether the path has an extension |
| `IsAbsolute(path)` | `string → bool` | Whether the path is absolute |
| `IsRelative(path)` | `string → bool` | Whether the path is relative |

```kinal
IO.Path.HasExtension("file.txt");  // true
IO.Path.HasExtension("Makefile");  // false
IO.Path.IsAbsolute("/usr/bin");    // true
IO.Path.IsRelative("../docs");     // true
```

### Environment

| Function | Signature | Description |
|----------|-----------|-------------|
| `Cwd()` | `→ string` | Get the current working directory |

```kinal
string cwd = IO.Path.Cwd();
string output = IO.Path.Combine(cwd, "output");
```

---

## Platform Notes

`IO.Path` automatically adapts to the path separator of the current platform:

- **Windows**: `\` (internally unified; also accepts `/`)
- **Linux / macOS**: `/`

For cross-platform code, it is recommended to always use `IO.Path.Combine` to join paths rather than manual string concatenation.

---

## Complete Example

```kinal
Unit App.PathDemo;

Get IO.Console;
Get IO.Path;
Get IO.File;

Static Function int Main()
{
    // Build output path
    string base  = IO.Path.Cwd();
    string outDir = IO.Path.Combine(base, "output");
    string file   = IO.Path.Combine(outDir, "result.txt");

    IO.Console.PrintLine(file);
    // e.g., /home/user/project/output/result.txt

    // Decompose path info
    IO.Console.PrintLine(IO.Path.FileName(file));   // result.txt
    IO.Console.PrintLine(IO.Path.Extension(file));  // .txt
    IO.Console.PrintLine(IO.Path.Directory(file));  // /home/user/project/output

    // Change extension
    string htmlFile = IO.Path.ChangeExtension(file, ".html");
    IO.Console.PrintLine(htmlFile); // /home/user/project/output/result.html

    Return 0;
}
```

---

## See Also

- [IO.File](filesystem.md) — File reading and writing (using path strings)
- [IO.System](system.md) — `CommandLine`, `FileExists`, and other system interfaces
