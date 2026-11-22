# IO.UI

`IO.UI` 提供原生桌面窗口与控件层。

Windows x64 SDK 默认选择 IO.UI 2.0。在命令行或 Studio 中均可直接 `Get IO.UI;`。原生程序和桌面 KinalVM 都使用 Direct2D 和 DirectWrite，在共享的 Direct3D 11 表面上绘制控件。需要访问各控件 Win32 句柄的原生工程可添加 `Packages { Pins = ["IO.UI@1.0.0"]; }`。

在 2.0 的自定义画布中，调用 `context.Gradient(x, y, width, height, fromColor, toColor)` 可以绘制从左向右的线性渐变。最后一个参数传入 `true` 时，方向为从上向下。两个颜色均使用 `0xAARRGGBB`。例如 `context.Gradient(0, 0, 160, 100, 0x00000000, 0xff000000, true)` 会让下方已有的图形自上而下逐渐变暗。

Windows 桌面 KinalVM 支持更新后的 IO.UI 2.0 包。使用配套自举编译器生成字节码即可运行。Windows 消息同步调用字节码事件处理器，绘图使用与 Native 相同的 DX11 窗口。

在 IO.UI 2.0 中，`Viewport3D.SetMesh(points, indices, colors)` 接受索引三角网格。`Yaw`、`Pitch`、`Zoom`、`PanX`、`PanY` 控制视角、缩放与平移，`Orthographic`、`Wireframe`、`ShowEdges` 控制投影、线框与棱线。按住中键可平移，调用 `ResetCamera()` 可恢复初始视角、缩放与中心。Studio 示例的 Mocha 控件位于各自项目的 `src/Controls`。

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

## 运行与对象生命周期

示例使用 Windows hosted Native，编译为 exe 后运行。Window.Run 进入消息循环，窗口关闭后继续执行后续代码。Add 将组件加入容器；组件、事件和捕获变量必须在窗口使用期间保持有效。事件处理函数不要执行长时间阻塞任务，否则会阻塞 UI 消息处理。

控件坐标和尺寸由当前 Win32 实现解释；未提供自动响应式布局保证。其他系统上的成功前端检查不代表控件具有可运行后端。Host／Win32 接口暴露原生细节，普通应用应优先使用 Window、Component 与具体控件。

## 公开源码签名

下表逐项列出当前包源码的公开字段、构造函数、属性和方法；继承成员在基类下列出。Get/Set 表示属性访问器，函数行的参数包含声明默认值。原生句柄、Host／Win32 等低层接口需要相应 ABI 与生命周期知识，普通应用优先使用包的高级对象。

### 枚举

- `IO.UI.FontWeight`: `Normal = 400`, `Bold = 700`
- `IO.UI.EventKind`: `OnClick = 1`, `OnClose = 2`, `OnShown = 3`, `OnResize = 4`, `OnFocus = 5`, `OnBlur = 6`, `OnTextChanged = 7`, `OnCheckedChanged = 8`, `OnSelectedChanged = 9`, `OnDoubleClick = 10`, `OnKeyDown = 11`, `OnKeyUp = 12`, `OnMouseEnter = 13`, `OnMouseLeave = 14`, `OnMouseDown = 15`, `OnMouseMove = 16`, `OnMouseUp = 17`, `OnMove = 18`
- `IO.UI.MessageBoxButtons`: `OK = 0`, `OKCancel = 1`, `YesNo = 4`
- `IO.UI.MessageBoxIcon`: `None = 0`, `Error = 0x10`, `Question = 0x20`, `Warning = 0x30`, `Info = 0x40`

### IO.UI.Component

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(string text = "", int x = 0, int y = 0, int width = 120, int height = 24)` | — |
| `Text` | `Public Property string Text` | Get/Set |
| `X` | `Public Property int X` | Get/Set |
| `Y` | `Public Property int Y` | Get/Set |
| `Width` | `Public Property int Width` | Get/Set |
| `Height` | `Public Property int Height` | Get/Set |
| `BackColor` | `Public Property Color BackColor` | Get/Set |
| `ForeColor` | `Public Property Color ForeColor` | Get/Set |
| `Font` | `Public Property Font Font` | Get/Set |
| `Handle` | `Public Property byte* Handle` | Get/Set |
| `NativeBrush` | `Public Property byte* NativeBrush` | Get/Set |
| `NativeFont` | `Public Property byte* NativeFont` | Get/Set |
| `NativeProc` | `Public Property byte* NativeProc` | Get/Set |
| `Owner` | `Public Property Window Owner` | Get/Set |
| `Events` | `Public Property Event[] Events` | Get/Set |
| `SupportsEvent` | `Public Virtual Safe Function bool SupportsEvent(EventKind kind)` | — |
| `HasEvent` | `Public Safe Function bool HasEvent(EventKind kind)` | — |
| `KindName` | `Public Virtual Safe Function string KindName()` | — |
| `EffectiveBackColor` | `Public Trusted Function Color EffectiveBackColor()` | — |
| `EffectiveForeColor` | `Public Trusted Function Color EffectiveForeColor()` | — |
| `EffectiveFont` | `Public Trusted Function Font EffectiveFont()` | — |
| `SetBackColor` | `Public Function Component SetBackColor(Color color)` | — |
| `SetForeColor` | `Public Function Component SetForeColor(Color color)` | — |
| `SetFont` | `Public Function Component SetFont(Font font)` | — |
| `AddEvent` | `Public Function Component AddEvent(Event event)` | — |
| `RemoveEvent` | `Public Function Component RemoveEvent(Event event)` | — |
| `TriggerEvent` | `Public Trusted Function void TriggerEvent(EventKind kind, usize wParam = 0, isize lParam = 0)` | — |

### IO.UI.Label

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(string text, int x, int y, int width = 160, int height = 24)` | — |
| `KindName` | `Public Override Safe Function string KindName()` | — |
| `CreateNative` | `Public Trusted Function byte* CreateNative(byte* parent, byte* instance)` | — |

### IO.UI.Button

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(string text, int x, int y, int width = 120, int height = 28)` | — |
| `Id` | `Public Property int Id` | Get/Set |
| `SupportsEvent` | `Public Override Safe Function bool SupportsEvent(EventKind kind)` | — |
| `KindName` | `Public Override Safe Function string KindName()` | — |
| `CreateNative` | `Public Trusted Function byte* CreateNative(byte* parent, byte* instance)` | — |
| `TriggerClick` | `Public Trusted Function void TriggerClick()` | — |

### IO.UI.Input

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(string text = "", int x = 0, int y = 0, int width = 200, int height = 24)` | — |
| `Id` | `Public Property int Id` | Get/Set |
| `Multiline` | `Public Property bool Multiline` | Get/Set |
| `ReadOnly` | `Public Property bool ReadOnly` | Get/Set |
| `Password` | `Public Property bool Password` | Get/Set |
| `MaxLength` | `Public Property int MaxLength` | Get/Set |
| `PlaceholderText` | `Public Property string PlaceholderText` | Get/Set |
| `SupportsEvent` | `Public Override Safe Function bool SupportsEvent(EventKind kind)` | — |
| `KindName` | `Public Override Safe Function string KindName()` | — |
| `SetMultiline` | `Public Function Input SetMultiline(bool multiline)` | — |
| `SetReadOnly` | `Public Function Input SetReadOnly(bool readOnly)` | — |
| `SetPassword` | `Public Function Input SetPassword(bool password)` | — |
| `SetMaxLength` | `Public Function Input SetMaxLength(int maxLength)` | — |
| `CreateNative` | `Public Trusted Function byte* CreateNative(byte* parent, byte* instance)` | — |

### IO.UI.CheckBox

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(string text, int x, int y, int width = 160, int height = 24)` | — |
| `Id` | `Public Property int Id` | Get/Set |
| `Checked` | `Public Property bool Checked` | Get/Set |
| `SupportsEvent` | `Public Override Safe Function bool SupportsEvent(EventKind kind)` | — |
| `KindName` | `Public Override Safe Function string KindName()` | — |
| `CreateNative` | `Public Trusted Function byte* CreateNative(byte* parent, byte* instance)` | — |

### IO.UI.RadioButton

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(string text, int x, int y, int width = 160, int height = 24)` | — |
| `Id` | `Public Property int Id` | Get/Set |
| `Checked` | `Public Property bool Checked` | Get/Set |
| `GroupName` | `Public Property string GroupName` | Get/Set |
| `SupportsEvent` | `Public Override Safe Function bool SupportsEvent(EventKind kind)` | — |
| `KindName` | `Public Override Safe Function string KindName()` | — |
| `CreateNative` | `Public Trusted Function byte* CreateNative(byte* parent, byte* instance)` | — |

### IO.UI.ComboBox

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(int x, int y, int width = 160, int height = 200)` | — |
| `Id` | `Public Property int Id` | Get/Set |
| `Items` | `Public Property string[] Items` | Get/Set |
| `SelectedIndex` | `Public Property int SelectedIndex` | Get/Set |
| `Editable` | `Public Property bool Editable` | Get/Set |
| `AddItem` | `Public Function ComboBox AddItem(string item)` | — |
| `ClearItems` | `Public Function ComboBox ClearItems()` | — |
| `SupportsEvent` | `Public Override Safe Function bool SupportsEvent(EventKind kind)` | — |
| `KindName` | `Public Override Safe Function string KindName()` | — |
| `CreateNative` | `Public Trusted Function byte* CreateNative(byte* parent, byte* instance)` | — |

### IO.UI.ProgressBar

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(int x, int y, int width = 200, int height = 24)` | — |
| `Minimum` | `Public Property int Minimum` | Get/Set |
| `Maximum` | `Public Property int Maximum` | Get/Set |
| `Value` | `Public Property int Value` | Get/Set |
| `Step` | `Public Property int Step` | Get/Set |
| `PerformStep` | `Public Trusted Function void PerformStep()` | — |
| `KindName` | `Public Override Safe Function string KindName()` | — |
| `CreateNative` | `Public Trusted Function byte* CreateNative(byte* parent, byte* instance)` | — |

### IO.UI.GroupBox

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(string text, int x, int y, int width = 300, int height = 200)` | — |
| `Children` | `Public Property Component[] Children` | Get/Set |
| `Add` | `Public Function GroupBox Add(Component child)` | — |
| `KindName` | `Public Override Safe Function string KindName()` | — |
| `CreateNative` | `Public Trusted Function byte* CreateNative(byte* parent, byte* instance)` | — |

### IO.UI.Color

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(u8 r = 0, u8 g = 0, u8 b = 0, u8 a = 255)` | — |
| `R` | `Public Property u8 R` | Get/Set |
| `G` | `Public Property u8 G` | Get/Set |
| `B` | `Public Property u8 B` | Get/Set |
| `A` | `Public Property u8 A` | Get/Set |
| `ToColorRef` | `Public Trusted Function u32 ToColorRef()` | — |
| `FromRgb` | `Public Static Function Color FromRgb(u8 r, u8 g, u8 b)` | — |
| `Black` | `Public Static Function Color Black()` | — |
| `White` | `Public Static Function Color White()` | — |
| `Gray` | `Public Static Function Color Gray()` | — |
| `Blue` | `Public Static Function Color Blue()` | — |

### IO.UI.Font

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(string family = "Segoe UI", int size = 16, FontWeight weight = FontWeight.Normal, bool italic = false, bool underline = false, bool strikeOut = false)` | — |
| `Family` | `Public Property string Family` | Get/Set |
| `Size` | `Public Property int Size` | Get/Set |
| `Weight` | `Public Property FontWeight Weight` | Get/Set |
| `Italic` | `Public Property bool Italic` | Get/Set |
| `Underline` | `Public Property bool Underline` | Get/Set |
| `StrikeOut` | `Public Property bool StrikeOut` | Get/Set |

### IO.UI.EventArgs

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(EventKind kind = EventKind.OnClick, usize wParam = 0, isize lParam = 0)` | — |
| `Kind` | `Public Property EventKind Kind` | Get/Set |
| `WParam` | `Public Property usize WParam` | Get/Set |
| `LParam` | `Public Property isize LParam` | Get/Set |
| `Handled` | `Public Property bool Handled` | Get/Set |

### IO.UI.Event

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(EventKind kind, IO.Type.Object.Function handler)` | — |
| `Kind` | `Public Property EventKind Kind` | Get/Set |
| `Handler` | `Public Property IO.Type.Object.Function Handler` | Get/Set |
| `Invoke` | `Public Trusted Function void Invoke(IO.Type.Object.Class sender, EventArgs args)` | — |

### IO.UI.MessageBox

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Show` | `Public Trusted Function int Show(string text, string caption = "", MessageBoxButtons buttons = MessageBoxButtons.OK, MessageBoxIcon icon = MessageBoxIcon.None)` | — |
| `ShowInfo` | `Public Trusted Function int ShowInfo(string text, string caption = "")` | — |
| `ShowError` | `Public Trusted Function int ShowError(string text, string caption = "")` | — |

### IO.UI.Window

| 成员 | 声明 | 访问器 |
|------|------|--------|
| `Constructor` | `Public Constructor(string title = "Kinal Window", int width = 640, int height = 480, int x = 100, int y = 100)` | — |
| `Title` | `Public Property string Title` | Get/Set |
| `Width` | `Public Property int Width` | Get/Set |
| `Height` | `Public Property int Height` | Get/Set |
| `X` | `Public Property int X` | Get/Set |
| `Y` | `Public Property int Y` | Get/Set |
| `BackColor` | `Public Property Color BackColor` | Get/Set |
| `ForeColor` | `Public Property Color ForeColor` | Get/Set |
| `Font` | `Public Property Font Font` | Get/Set |
| `SetBackColor` | `Public Function Window SetBackColor(Color color)` | — |
| `SetForeColor` | `Public Function Window SetForeColor(Color color)` | — |
| `SetFont` | `Public Function Window SetFont(Font font)` | — |
| `Add` | `Public Function Window Add(Component component)` | — |
| `SupportsEvent` | `Public Safe Function bool SupportsEvent(EventKind kind)` | — |
| `AddEvent` | `Public Function Window AddEvent(Event event)` | — |
| `RemoveEvent` | `Public Function Window RemoveEvent(Event event)` | — |
| `TriggerEvent` | `Public Trusted Function void TriggerEvent(EventKind kind, usize wParam = 0, isize lParam = 0)` | — |
| `Create` | `Public Trusted Function byte* Create()` | — |
| `Show` | `Public Trusted Function void Show()` | — |
| `Run` | `Public Trusted Function int Run()` | — |
| `Handle` | `Public byte* Handle;` | field |
| `Components` | `Public Component[] Components;` | field |
| `Events` | `Public Event[] Events;` | field |
| `NativeBrush` | `Public byte* NativeBrush;` | field |
