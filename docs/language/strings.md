# String Features

Kinal strings support multiple literal prefixes, format strings, and char array conversion.

## Basic Strings

```kinal
string s = "Hello, World!";
string path = "C:\\Users\\test";  // using escape sequences
```

Standard escape sequences:

| Escape | Meaning |
|--------|---------|
| `\n` | Newline |
| `\r` | Carriage return |
| `\t` | Tab |
| `\\` | Backslash |
| `\"` | Double quote |
| `\'` | Single quote |
| `\0` | Null character |

## String Prefixes

### `[r]` — Raw String

In a raw string, backslashes are not treated as escape characters:

```kinal
string raw = [r]"C:\Users\test\n";
// raw has the literal value "C:\Users\test\n" (6 characters, no newline)
Console.PrintLine(raw.Length());  // 18 (actual character count)
```

Equivalent alias: `[raw]`

### `[f]`/`[format]` — Format String (String Interpolation)

Insert expressions inside double curly braces:

```kinal
int value = 42;
string name = "Kinal";
string msg = [f]"value={value}, name={name}";
Console.PrintLine(msg);  // value=42, name=Kinal
```

Expression interpolation:

```kinal
int x = 5;
Console.PrintLine([f]"x squared = {x * x}");  // x squared = 25
Console.PrintLine([format]"sum={1 + 2 + 3}");  // sum=6
```

Brace escaping (double braces output a single brace):

```kinal
Console.PrintLine([f]"{{{value}}}");  // {42}
```

Equivalent alias: `[format]`

### `[c]`/`[chars]` — Char Array

Converts a string literal to a `char[]` array:

```kinal
char[] chars = [c]"abc";
Console.PrintLine(chars.Length());  // 3
Console.PrintLine(chars[0]);        // a
Console.PrintLine(chars[2]);        // c
```

Equivalent alias: `[chars]`

### Combining Prefixes

Prefixes can be combined; they are applied left to right:

```kinal
int val = 7;
// First format, then convert to char array
Var chars = [f][c]"z={val}";
// Equivalent to [f]"z={val}" → "z=7", then [c]"z=7" → ['z','=','7']
Console.PrintLine(chars.Length());  // 3

// First apply raw, then convert to char array
Var rawChars = [chars][raw]"\n";
// [raw]"\n" → "\n" (two characters: \ and n), then [chars] → ['\','n']
Console.PrintLine(rawChars.Length()); // 2
```

## String Concatenation

Use `+` to concatenate strings:

```kinal
string greeting = "Hello" + ", " + "World" + "!";
```

Convert numeric to string, then concatenate:

```kinal
int n = 42;
string msg = "The number is: " + [string](n);
```

Or use a format string:

```kinal
string msg = [f]"The number is: {n}";
```

## String Methods

String methods are provided through the `IO.Text` module (see [IO.Text](../stdlib/text.md)):

```kinal
Get IO.Text;

string s = "Hello, World!";

IO.Text.Length(s);              // 13
IO.Text.Contains(s, "World");   // true
IO.Text.StartsWith(s, "Hello"); // true
IO.Text.ToUpper(s);             // "HELLO, WORLD!"
IO.Text.Slice(s, 7, 5);         // "World"
IO.Text.Replace(s, "World", "Kinal"); // "Hello, Kinal!"
IO.Text.Split(s, ", ");         // list: ["Hello", "World!"]
```

Built-in string methods (call directly, no `IO.Text` needed):

```kinal
s.Length();    // character length
s.Equals(t);  // content comparison
s.ToString(); // returns self (any interface compatibility)
```

## String Comparison

```kinal
string a = "hello";
string b = "hello";
bool same = a == b;   // true (content comparison)
bool diff = a != b;   // false
```

## Char and String Conversion

```kinal
// char to string
char c = 'A';
string s = [string](c);    // "A" (single-character string)

// int → char → string
int code = 65;
char ch = [char](code);    // 'A'

// string to char array
char[] arr = IO.Type.string.ToChars("abc");  // ['a', 'b', 'c']
// or
char[] arr2 = [c]"abc";
```

## char* and string Interoperability

In Unsafe contexts, `string` can be converted to `char*` or `byte*` (C string):

```kinal
Unsafe Function void Demo(string s)
{
    byte* ptr = [byte*](s);   // get the underlying C string pointer
}
```

There are restrictions on concatenating `char*` with `string`: you cannot directly concatenate a `char*` with a `string` (the compiler will report an error); you must first convert back to `string`.

## String to Numeric

```kinal
int n = [int]("42");       // 42
float f = [float]("3.14"); // 3.14
bool b = [bool]("1");      // true
bool b2 = [bool]("true");  // true
```

## Next Steps

- [IO.Text Standard Library](../stdlib/text.md)
- [Type System](types.md)
