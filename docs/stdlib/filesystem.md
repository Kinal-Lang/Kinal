# IO.File — File Operations

`IO.File` provides basic operations for reading, writing, creating, deleting, and copying files. All path parameters are strings.

## Import

```kinal
Get IO.File;
```

---

## Function Reference

### Checking and Metadata

| Function | Signature | Description |
|----------|-----------|-------------|
| `Exists(path)` | `string → bool` | Whether the file exists |
| `Size(path)` | `string → int` | File size in bytes |
| `IsEmpty(path)` | `string → bool` | Whether the file content is empty |

```kinal
If (IO.File.Exists("config.json"))
{
    int size = IO.File.Size("config.json");
    IO.Console.PrintLine(size);
}
```

### Creating

| Function | Signature | Description |
|----------|-----------|-------------|
| `Create(path)` | `string → void` | Create a file (clears it if it already exists) |
| `Touch(path)` | `string → void` | Create a file (does not modify content if it already exists) |

```kinal
IO.File.Create("output.txt");        // Clear or create new
IO.File.Touch("log.txt");            // Just ensure the file exists
```

### Reading

| Function | Signature | Description |
|----------|-----------|-------------|
| `ReadAllText(path)` | `string → string` | Read all content |
| `ReadFirstLine(path)` | `string → string` | Read the first line |
| `ReadIfExists(path)` | `string → string` | Read if file exists, otherwise return `""` |
| `ReadOrDefault(path, default)` | `string, string → string` | Read if file exists, otherwise return the default value |
| `ReadOr(path, default)` | `string, string → string` | Same as `ReadOrDefault` (alias) |
| `ReadLines(path)` | `string → list` | Read by lines, returns a `list` |

```kinal
string content = IO.File.ReadAllText("hello.txt");

string first = IO.File.ReadFirstLine("data.csv");

string cfg = IO.File.ReadOrDefault("config.json", "{}");

list lines = IO.File.ReadLines("names.txt");
int lineCount = IO.Collection.list.Count(lines);
```

### Writing

| Function | Signature | Description |
|----------|-----------|-------------|
| `WriteAllText(path, text)` | `string, string → void` | Overwrite and write all content |
| `AppendAllText(path, text)` | `string, string → void` | Append text to end of file |
| `AppendLine(path, line)` | `string, string → void` | Append a line (automatically adds a newline) |
| `WriteLines(path, lines)` | `string, list → void` | Write each element in the list as a line |

```kinal
IO.File.WriteAllText("out.txt", "Hello, Kinal!\n");

IO.File.AppendLine("log.txt", "New event occurred");

list lines = IO.Collection.list.Create();
IO.Collection.list.Add(lines, "line1");
IO.Collection.list.Add(lines, "line2");
IO.File.WriteLines("data.txt", lines);
```

### Management Operations

| Function | Signature | Description |
|----------|-----------|-------------|
| `Delete(path)` | `string → void` | Delete a file (throws an exception if it does not exist) |
| `DeleteIfExists(path)` | `string → void` | Delete the file if it exists (silent if it does not) |
| `Copy(src, dst)` | `string, string → void` | Copy a file |
| `Move(src, dst)` | `string, string → void` | Move/rename a file |

```kinal
IO.File.Copy("template.txt", "output.txt");
IO.File.Move("old_name.txt", "new_name.txt");
IO.File.DeleteIfExists("temp.tmp");
```

---

## Complete Examples

### Read and Process CSV

```kinal
Unit App.FileDemo;

Get IO.Console;
Get IO.File;
Get IO.Text;
Get IO.Collection;

Static Function int Main()
{
    string csvPath = "data.csv";

    If (!IO.File.Exists(csvPath))
    {
        IO.Console.PrintLine("File not found");
        Return 1;
    }

    list lines = IO.File.ReadLines(csvPath);
    int total = IO.Collection.list.Count(lines);
    IO.Console.PrintLine([string](total) + " rows of data");

    int i = 0;
    While (i < total)
    {
        string line = [string](IO.Collection.list.Fetch(lines, i));
        list fields = IO.Text.Split(line, ",");
        IO.Console.PrintLine(IO.Collection.list.Fetch(fields, 0));
        i = i + 1;
    }

    Return 0;
}
```

### Write a Log File

```kinal
Unit App.Logger;

Get IO.File;
Get IO.Time;

Static Function void Log(string message)
{
    string ts = IO.Time.Format(IO.Time.Now(), "yyyy-MM-dd HH:mm:ss");
    IO.File.AppendLine("app.log", "[" + ts + "] " + message);
}
```

---

## Exceptions

The following conditions throw an `IO.Error` exception:

- `ReadAllText`/`ReadLines` etc.: File does not exist
- `Delete`: File does not exist
- `WriteAllText`/`AppendLine` etc.: Path is not writable (insufficient permissions, directory does not exist)

### Defensive Pattern

```kinal
Try
{
    string content = IO.File.ReadAllText("data.txt");
    IO.Console.PrintLine(content);
} Catch IO.Error e
{
    IO.Console.PrintLine("Read failed: " + e.Message);
}
```

---

## See Also

- [IO.Path](path.md) — Path joining and operations
- [IO.Text](text.md) — Text content processing
- [Exception Handling](../language/exceptions.md) — Try/Catch
