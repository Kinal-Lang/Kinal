# IO.Console — 控制台 I/O

`IO.Console` 提供标准控制台的输入/输出能力。

## 导入

```kinal
Get IO.Console;
// 或
Get IO.Console;
```

## 函数参考

### PrintLine

打印一行内容并换行（`\n`）。

**签名：** `PrintLine(any... args) → void`（可变参数，Safety: Safe）

```kinal
IO.Console.PrintLine("Hello, World!");
IO.Console.PrintLine(42);
IO.Console.PrintLine(true);
IO.Console.PrintLine();                         // 输出空行

// 多参数：依次打印，用空格分隔
IO.Console.PrintLine("a=", 1, ", b=", true);
```

输出：
```
Hello, World!
42
true

a= 1 , b= true
```

### Print

打印内容但**不换行**。

**签名：** `Print(any... args) → void`（可变参数，Safety: Safe）

```kinal
IO.Console.Print("Hello");
IO.Console.Print(", ");
IO.Console.Print("World");
IO.Console.PrintLine();  // 手动换行
```

### ReadLine

从控制台读取一行用户输入（阻塞，等待回车）。

**签名：** `ReadLine() → string`（Safety: Safe）

```kinal
IO.Console.Print("请输入你的名字: ");
string name = IO.Console.ReadLine();
IO.Console.PrintLine("你好, " + name);
```

## 完整示例

```kinal
Unit App;

Get IO.Console;

Static Function int Main()
{
    // 基本输出
    IO.Console.PrintLine("欢迎使用 Kinal!");
    IO.Console.PrintLine();

    // 数值输出
    int n = 42;
    float pi = 3.14159;
    IO.Console.PrintLine("整数: ", n);
    IO.Console.PrintLine("浮点: ", pi);

    // 格式化字符串
    string msg = [f]"n={n}, pi={pi}";
    IO.Console.PrintLine(msg);

    // 读取输入
    IO.Console.Print("请输入一个数字: ");
    string input = IO.Console.ReadLine();
    int value = [int](input);
    IO.Console.PrintLine("你输入了: ", value);
    IO.Console.PrintLine("它的平方是: ", value * value);

    Return 0;
}
```

## 注意事项

- `PrintLine` 和 `Print` 会自动将参数转换为字符串（通过 `any.ToString()`）
- 输出 `null` 会打印 `"null"`
- 多参数时各参数通过空格或无分隔符连接（取决于实现）

## 相关

- [IO.Text](text.md) — 字符串处理
- [字符串特性](../language/strings.md)
