# Operators

Kinal provides a complete set of arithmetic, logical, comparison, bitwise, and type-conversion operators.

## Arithmetic Operators

```kinal
int a = 10;
int b = 3;

int sum  = a + b;    // 13
int diff = a - b;    // 7
int prod = a * b;    // 30
int quot = a / b;    // 3 (integer division)
int rem  = a % b;    // 1 (remainder)
```

Floating-point division:

```kinal
float x = 10.0;
float y = 3.0;
float result = x / y;  // 3.333...
```

### String Concatenation

The `+` operator can be used for string concatenation:

```kinal
string s = "Hello" + ", " + "World!";  // "Hello, World!"
```

### Increment/Decrement

```kinal
int i = 0;
i++;   // i = 1 (post-increment)
i--;   // i = 0 (post-decrement)
```

### Compound Assignment Operators

```kinal
int n = 10;
n += 5;   // 15
n -= 3;   // 12
n *= 2;   // 24
n /= 4;   // 6
n %= 4;   // 2
```

## Comparison Operators

All comparison operators return `bool`:

```kinal
int a = 5, b = 3;
bool eq = a == b;   // false
bool ne = a != b;   // true
bool lt = a < b;    // false
bool le = a <= b;   // false
bool gt = a > b;    // true
bool ge = a >= b;   // true
```

String comparison uses `==` and `!=` for content comparison:

```kinal
string s1 = "hello";
string s2 = "hello";
bool same = s1 == s2;  // true
```

## Logical Operators

| Operator | Meaning | Notes |
|----------|---------|-------|
| `&&` | Logical AND | Short-circuit evaluation |
| `\|\|` | Logical OR | Short-circuit evaluation |
| `!` | Logical NOT | Inverts a boolean value |

```kinal
bool a = true, b = false;

bool and_result = a && b;   // false
bool or_result  = a || b;   // true
bool not_result = !a;       // false
```

**Short-circuit evaluation**: `&&` does not evaluate the right side if the left is `false`; `||` does not evaluate the right side if the left is `true`.

```kinal
bool safeCheck(string s)
{
    Return s != null && s.Length() > 0;  // Length() is not called if s is null
}
```

## Bitwise Operators

Applicable to integer types (`int`, `i8`~`i64`, `u8`~`u64`, `byte`):

```kinal
int a = 5;   // 0101
int b = 3;   // 0011

int and = a & b;    // 0001 = 1  (bitwise AND)
int or  = a | b;    // 0111 = 7  (bitwise OR)
int xor = a ^ b;    // 0110 = 6  (bitwise XOR)
int shl = a << 1;   // 1010 = 10 (left shift)
int shr = a >> 1;   // 0010 = 2  (right shift)
int inv = ~a;       // bitwise NOT (result is platform-dependent)
```

Compound bitwise assignment is not currently supported (must be expanded: `n = n & mask`).

## Type Conversion Operator

Kinal uses the `[TargetType](expression)` syntax for explicit type conversion:

```kinal
// Numeric conversions
int i = 65;
char c = [char](i);       // 'A'
float f = [float](i);     // 65.0
u8 u = [u8](i);            // 65

// string and numeric conversions
string s = [string](42);  // "42"
int n = [int]("100");     // 100
bool b = [bool]("1");     // true

// any unboxing
any a = 3.14;
float pi = [float](a);

// string to char array
char[] chars = [c]("abc");   // ['a', 'b', 'c']
```

Using `IO.Type.*` prefix is also available for conversions (equivalent to `[type](...)`):

```kinal
string s = IO.Type.string(42);   // "42"
int n = IO.Type.int("100");      // 100
```

## The `Is` Type Check Operator

```kinal
Animal a = New Dog();
bool isDog = a Is Dog;      // true
bool isCat = a Is Cat;      // false
```

Combined with `If` for pattern matching:

```kinal
If (a Is Dog d)


{
    d.Bark();   // d has type Dog within this block
}
```

## Operator Precedence

From highest to lowest (same level is left-to-right):

| Precedence | Operators |
|------------|-----------|
| 1 (highest) | `!`, `~`, `-` (unary minus) |
| 2 | `*`, `/`, `%` |
| 3 | `+`, `-` |
| 4 | `<<`, `>>` |
| 5 | `<`, `<=`, `>`, `>=` |
| 6 | `==`, `!=` |
| 7 | `&` |
| 8 | `^` |
| 9 | `\|` |
| 10 | `&&` |
| 11 | `\|\|` |
| 12 (lowest) | `=`, `+=`, `-=`, `*=`, `/=`, `%=` |

Using parentheses to clarify precedence is recommended:

```kinal
int result = (a + b) * (c - d);
bool check = (x > 0) && (y < 100);
```

## Next Steps

- [Control Flow](control-flow.md)
- [Type System](types.md)
