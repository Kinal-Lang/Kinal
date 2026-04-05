# 运算符

Kinal 提供完整的算术、逻辑、比较、位运算以及类型转换运算符。

## 算术运算符

```kinal
int a = 10;
int b = 3;

int sum  = a + b;    // 13
int diff = a - b;    // 7
int prod = a * b;    // 30
int quot = a / b;    // 3（整数除法）
int rem  = a % b;    // 1（取余）
```

浮点数除法：

```kinal
float x = 10.0;
float y = 3.0;
float result = x / y;  // 3.333...
```

### 字符串拼接

`+` 运算符可用于字符串拼接：

```kinal
string s = "Hello" + ", " + "World!";  // "Hello, World!"
```

### 自增/自减

```kinal
int i = 0;
i++;   // i = 1（后置自增）
i--;   // i = 0（后置自减）
```

### 复合赋值运算符

```kinal
int n = 10;
n += 5;   // 15
n -= 3;   // 12
n *= 2;   // 24
n /= 4;   // 6
n %= 4;   // 2
```

## 比较运算符

所有比较运算符返回 `bool`：

```kinal
int a = 5, b = 3;
bool eq = a == b;   // false
bool ne = a != b;   // true
bool lt = a < b;    // false
bool le = a <= b;   // false
bool gt = a > b;    // true
bool ge = a >= b;   // true
```

字符串比较使用 `==` 和 `!=` 进行内容比较：

```kinal
string s1 = "hello";
string s2 = "hello";
bool same = s1 == s2;  // true
```

## 逻辑运算符

| 运算符 | 含义 | 说明 |
|--------|------|------|
| `&&` | 逻辑与 | 短路求值 |
| `\|\|` | 逻辑或 | 短路求值 |
| `!` | 逻辑非 | 翻转布尔值 |

```kinal
bool a = true, b = false;

bool and_result = a && b;   // false
bool or_result  = a || b;   // true
bool not_result = !a;       // false
```

**短路求值**：`&&` 若左侧为 `false` 则不求值右侧；`||` 若左侧为 `true` 则不求值右侧。

```kinal
bool safeCheck(string s)
{
    Return s != null && s.Length() > 0;  // s 为 null 时不会调用 Length()
}
```

## 位运算符

适用于整数类型（`int`、`i8`~`i64`、`u8`~`u64`、`byte`）：

```kinal
int a = 5;   // 0101
int b = 3;   // 0011

int and = a & b;    // 0001 = 1（按位与）
int or  = a | b;    // 0111 = 7（按位或）
int xor = a ^ b;    // 0110 = 6（按位异或）
int shl = a << 1;   // 1010 = 10（左移）
int shr = a >> 1;   // 0010 = 2（右移）
int inv = ~a;       // 按位取反（结果依平台）
```

位运算复合赋值暂不支持（需展开写：`n = n & mask`）。

## 类型转换运算符

Kinal 使用 `[目标类型](表达式)` 语法进行显式类型转换：

```kinal
// 数值类型间转换
int i = 65;
char c = [char](i);       // 'A'
float f = [float](i);     // 65.0
u8 u = [u8](i);            // 65

// string 与数值互转
string s = [string](42);  // "42"
int n = [int]("100");     // 100
bool b = [bool]("1");     // true

// any 解包
any a = 3.14;
float pi = [float](a);

// 字符串到字符数组
char[] chars = [c]("abc");   // ['a', 'b', 'c']
```

使用 `IO.Type.*` 前缀也可用于转换（等价于 `[type](...)`）：

```kinal
string s = IO.Type.string(42);   // "42"
int n = IO.Type.int("100");      // 100
```

## Is 类型检查运算符

```kinal
Animal a = New Dog();
bool isDog = a Is Dog;      // true
bool isCat = a Is Cat;      // false
```

结合 `If` 实现模式匹配：

```kinal
If (a Is Dog d)


{
    d.Bark();   // d 类型为 Dog
}
```

## 运算符优先级

从高到低（同级从左到右）：

| 优先级 | 运算符 |
|--------|--------|
| 1（最高）| `!`、`~`、`-`（一元负号） |
| 2 | `*`、`/`、`%` |
| 3 | `+`、`-` |
| 4 | `<<`、`>>` |
| 5 | `<`、`<=`、`>`、`>=` |
| 6 | `==`、`!=` |
| 7 | `&` |
| 8 | `^` |
| 9 | `\|` |
| 10 | `&&` |
| 11 | `\|\|` |
| 12（最低） | `=`、`+=`、`-=`、`*=`、`/=`、`%=` |

建议使用括号明确优先级：

```kinal
int result = (a + b) * (c - d);
bool check = (x > 0) && (y < 100);
```

## 下一步

- [控制流](control-flow.md)
- [类型系统](types.md)
