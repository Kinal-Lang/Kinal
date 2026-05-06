# IO.UI

`IO.UI` 提供原生桌面窗口与控件层。

当前官方包基于 Win32 host 实现，现有测试也按 Windows 场景使用它。

## 导入

```kinal
Get IO.UI;
```

## 主要类型

### Window

`IO.UI.Window` 是顶层容器。

常用成员：

- `Title`
- `Width`
- `Height`
- `X`
- `Y`
- `BackColor`
- `ForeColor`
- `Font`
- `Add(component)`
- `AddEvent(event)`
- `Show()`
- `Run()`

### 常用控件

当前包包含：

- `Label`
- `Button`
- `Input`
- `CheckBox`
- `RadioButton`
- `ComboBox`
- `ProgressBar`
- `GroupBox`

所有控件都继承自 `IO.UI.Component`，共享 `Text`、`X`、`Y`、`Width`、`Height`、`BackColor`、`ForeColor`、`Font` 等通用属性。

### 样式类型

- `Color`
- `Font`
- `FontWeight`

### 事件

`IO.UI.Event` 用于把处理函数绑定到控件或窗口上。

常见事件种类：

- `OnClick`
- `OnClose`
- `OnShown`
- `OnResize`
- `OnFocus`
- `OnBlur`
- `OnTextChanged`
- `OnCheckedChanged`
- `OnSelectedChanged`

### MessageBox

`IO.UI.MessageBox` 提供简单的模态对话框：

- `Show(text, caption = "", buttons = MessageBoxButtons.OK, icon = MessageBoxIcon.None)`
- `ShowInfo(text, caption = "")`
- `ShowError(text, caption = "")`

## 示例

```kinal
Unit App.UiDemo;

Get IO.UI;

Static Function int Main()
{
    IO.UI.Window window = New IO.UI.Window("Kinal UI", 640, 480);
    IO.UI.Label label = New IO.UI.Label("Name:", 20, 20, 60, 24);
    IO.UI.Input input = New IO.UI.Input("", 90, 20, 200, 24);
    IO.UI.Button button = New IO.UI.Button("Submit", 20, 60, 120, 28);

    button.AddEvent(New IO.UI.Event(IO.UI.EventKind.OnClick, Function void()
    {
        button.Text = "Submitted";
    }
    ));

    window.Add(label);
    window.Add(input);
    window.Add(button);
    Return window.Run();
}
```

## 说明

- `IO.UI` 面向 hosted 桌面程序。
- 当前实现依赖原生 host，不属于 VM 运行时接口的一部分。
- `MessageBox` 很适合做 compile-only 或最小 host 校验。

## 相关

- [标准库概览](overview.md)
- [IO.System](system.md)
- [IO.Web](web.md) — 服务器侧的 hosted 应用接口
