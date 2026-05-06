# IO.Cat

`IO.Cat` 是一个小型示例包，暴露 `IO.Cat.Rommy` 类。

## 导入

```kinal
Get IO.Cat;
```

## Rommy

`IO.Cat.Rommy` 是一个轻量示例类，带有受保护的构造函数，以及几个返回文本的交互方法。

### 构造函数

```kinal
IO.Cat.Rommy rommy = New IO.Cat.Rommy(true, true);
```

如果两个构造参数中任意一个为 `false`，这个包会抛出 `IO.Error`。

### 方法

| 方法 | 签名 | 说明 |
|------|------|------|
| `Speak(something)` | `string → string` | 返回一段响应文本 |
| `Touch()` | `→ string` | 返回一段简短交互文本 |
| `Feed()` | `→ string` | 返回一段简短交互文本 |
| `Play()` | `→ string` | 返回一段简短交互文本 |

## 示例

```kinal
Unit App.CatDemo;

Get Console By IO.Console;
Get IO.Cat;

Static Function int Main()
{
    IO.Cat.Rommy rommy = New IO.Cat.Rommy(true, true);
    Console.PrintLine(rommy.Speak("hello"));
    Console.PrintLine(rommy.Touch());
    Return 0;
}
```

## 说明

- `IO.Cat` 是示例包，不属于核心运行时依赖。
- 构造函数会先校验两个显式确认参数，再创建实例。

## 相关

- [标准库概览](overview.md)
- [IO.Console](console.md)
