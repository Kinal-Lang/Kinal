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

## 包身份

当前源码包版本是 2020.8.31，Unit 为 IO.Cat。包版本与编译器版本无关。Rommy 是教学类；构造参数、属性与打印方法的完整签名见下表，不能据此推导其他包具有相同生命周期或错误契约。

## 公开源码签名

下表逐项列出当前包源码的公开字段、构造函数、属性和方法；继承成员在基类下列出。Get/Set 表示属性访问器，函数行的参数包含声明默认值。原生句柄、Host／Win32 等低层接口需要相应 ABI 与生命周期知识，普通应用优先使用包的高级对象。

### IO.Cat.Rommy

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(bool i_know_this_is_just_a_clone_of_Rommy,bool i_will_be_good_to_Rommy)` | — |
| `Speak` | `Public Function string Speak(string something)` | — |
| `Touch` | `Public Function string Touch()` | — |
| `Feed` | `Public Function string Feed()` | — |
| `Play` | `Public Function string Play()` | — |
