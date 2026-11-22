# IO.UI

`IO.UI` provides a native desktop window and controls layer.

The Windows x64 SDK selects IO.UI 2.0 by default. `Get IO.UI;` loads it for command-line and Studio builds. Native and the desktop KinalVM draw the controls on a shared Direct3D 11 surface using Direct2D and DirectWrite. For a Native application that accesses individual Win32 control handles, add `Packages { Pins = ["IO.UI@1.0.0"]; }` to its project.

The Windows desktop KinalVM runs the updated IO.UI 2.0 package. Use the matching selfhost compiler to build the bytecode. Window messages can invoke bytecode event handlers synchronously, including while a modal message box is open. Rendering uses the same DX11 surface as a native build.

In a 2.0 custom canvas, use `context.Gradient(x, y, width, height, fromColor, toColor)` to fill a rectangle from left to right. Pass `true` as the last argument for top to bottom. Both colors use `0xAARRGGBB`. For example, `context.Gradient(0, 0, 160, 100, 0x00000000, 0xff000000, true)` darkens the drawing beneath it towards the bottom.

In IO.UI 2.0, `Viewport3D` accepts indexed triangles via `SetMesh(points, indices, colors)`. Set `Yaw`, `Pitch`, `Zoom`, `PanX` and `PanY` to position the view. `Orthographic`, `Wireframe` and `ShowEdges` control projection and rendering. A middle-button drag pans the view. `ResetCamera()` restores the initial orientation, scale and center. The Studio examples supply their Mocha widgets in their own `src/Controls` directories.

## Import

```kinal
Get IO.UI;
```

## Main Types

### Window

`IO.UI.Window` is the top-level container.

Important members:

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

### Common Controls

The package currently includes:

- `Label`
- `Button`
- `Input`
- `CheckBox`
- `RadioButton`
- `ComboBox`
- `ProgressBar`
- `GroupBox`

All controls derive from `IO.UI.Component` and share common properties such as `Text`, `X`, `Y`, `Width`, `Height`, `BackColor`, `ForeColor`, and `Font`.

### Styling Types

- `Color`
- `Font`
- `FontWeight`

### Events

`IO.UI.Event` binds a handler to a control or window.

Common event kinds:

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

`IO.UI.MessageBox` provides small modal dialogs:

- `Show(text, caption = "", buttons = MessageBoxButtons.OK, icon = MessageBoxIcon.None)`
- `ShowInfo(text, caption = "")`
- `ShowError(text, caption = "")`

## Example

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

## Notes

- `IO.UI` is intended for hosted desktop builds.
- The current implementation is native-host backed and not part of the VM surface.
- Message boxes are useful for compile-only or minimal host checks.

## See Also

- [Standard Library Overview](overview.md)
- [IO.System](system.md)
- [IO.Web](web.md) — Hosted application surface on the server side

## Execution and Object Lifetime

The example targets Windows hosted Native; compile an exe and run it. Window.Run enters the message loop and returns after the window closes. Add attaches components to the container. Keep components, events and captured state alive while the window uses them. Long blocking event handlers also block UI message processing.

Control coordinates and dimensions follow the current Win32 implementation; automatic responsive layout is not promised. Frontend acceptance on another system does not establish an executable control backend. Host/Win32 APIs expose native details; ordinary applications should start with Window, Component and concrete controls.

## Public Source Signatures

The tables list public fields, constructors, properties and methods from the current package source. Inherited members are listed under their declaring base type. Get/Set identify property accessors; signatures include declared defaults. Native handles and Host/Win32 helpers require their ABI/lifetime contracts; ordinary applications should start with the package high-level objects.

### Enums

- `IO.UI.FontWeight`: `Normal = 400`, `Bold = 700`
- `IO.UI.EventKind`: `OnClick = 1`, `OnClose = 2`, `OnShown = 3`, `OnResize = 4`, `OnFocus = 5`, `OnBlur = 6`, `OnTextChanged = 7`, `OnCheckedChanged = 8`, `OnSelectedChanged = 9`, `OnDoubleClick = 10`, `OnKeyDown = 11`, `OnKeyUp = 12`, `OnMouseEnter = 13`, `OnMouseLeave = 14`, `OnMouseDown = 15`, `OnMouseMove = 16`, `OnMouseUp = 17`, `OnMove = 18`
- `IO.UI.MessageBoxButtons`: `OK = 0`, `OKCancel = 1`, `YesNo = 4`
- `IO.UI.MessageBoxIcon`: `None = 0`, `Error = 0x10`, `Question = 0x20`, `Warning = 0x30`, `Info = 0x40`

### IO.UI.Component

| Member | Declaration | Accessors |
|--------|-------------|-----------|
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

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(string text, int x, int y, int width = 160, int height = 24)` | — |
| `KindName` | `Public Override Safe Function string KindName()` | — |
| `CreateNative` | `Public Trusted Function byte* CreateNative(byte* parent, byte* instance)` | — |

### IO.UI.Button

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(string text, int x, int y, int width = 120, int height = 28)` | — |
| `Id` | `Public Property int Id` | Get/Set |
| `SupportsEvent` | `Public Override Safe Function bool SupportsEvent(EventKind kind)` | — |
| `KindName` | `Public Override Safe Function string KindName()` | — |
| `CreateNative` | `Public Trusted Function byte* CreateNative(byte* parent, byte* instance)` | — |
| `TriggerClick` | `Public Trusted Function void TriggerClick()` | — |

### IO.UI.Input

| Member | Declaration | Accessors |
|--------|-------------|-----------|
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

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(string text, int x, int y, int width = 160, int height = 24)` | — |
| `Id` | `Public Property int Id` | Get/Set |
| `Checked` | `Public Property bool Checked` | Get/Set |
| `SupportsEvent` | `Public Override Safe Function bool SupportsEvent(EventKind kind)` | — |
| `KindName` | `Public Override Safe Function string KindName()` | — |
| `CreateNative` | `Public Trusted Function byte* CreateNative(byte* parent, byte* instance)` | — |

### IO.UI.RadioButton

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(string text, int x, int y, int width = 160, int height = 24)` | — |
| `Id` | `Public Property int Id` | Get/Set |
| `Checked` | `Public Property bool Checked` | Get/Set |
| `GroupName` | `Public Property string GroupName` | Get/Set |
| `SupportsEvent` | `Public Override Safe Function bool SupportsEvent(EventKind kind)` | — |
| `KindName` | `Public Override Safe Function string KindName()` | — |
| `CreateNative` | `Public Trusted Function byte* CreateNative(byte* parent, byte* instance)` | — |

### IO.UI.ComboBox

| Member | Declaration | Accessors |
|--------|-------------|-----------|
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

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(int x, int y, int width = 200, int height = 24)` | — |
| `Minimum` | `Public Property int Minimum` | Get/Set |
| `Maximum` | `Public Property int Maximum` | Get/Set |
| `Value` | `Public Property int Value` | Get/Set |
| `Step` | `Public Property int Step` | Get/Set |
| `PerformStep` | `Public Trusted Function void PerformStep()` | — |
| `KindName` | `Public Override Safe Function string KindName()` | — |
| `CreateNative` | `Public Trusted Function byte* CreateNative(byte* parent, byte* instance)` | — |

### IO.UI.GroupBox

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(string text, int x, int y, int width = 300, int height = 200)` | — |
| `Children` | `Public Property Component[] Children` | Get/Set |
| `Add` | `Public Function GroupBox Add(Component child)` | — |
| `KindName` | `Public Override Safe Function string KindName()` | — |
| `CreateNative` | `Public Trusted Function byte* CreateNative(byte* parent, byte* instance)` | — |

### IO.UI.Color

| Member | Declaration | Accessors |
|--------|-------------|-----------|
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

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(string family = "Segoe UI", int size = 16, FontWeight weight = FontWeight.Normal, bool italic = false, bool underline = false, bool strikeOut = false)` | — |
| `Family` | `Public Property string Family` | Get/Set |
| `Size` | `Public Property int Size` | Get/Set |
| `Weight` | `Public Property FontWeight Weight` | Get/Set |
| `Italic` | `Public Property bool Italic` | Get/Set |
| `Underline` | `Public Property bool Underline` | Get/Set |
| `StrikeOut` | `Public Property bool StrikeOut` | Get/Set |

### IO.UI.EventArgs

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(EventKind kind = EventKind.OnClick, usize wParam = 0, isize lParam = 0)` | — |
| `Kind` | `Public Property EventKind Kind` | Get/Set |
| `WParam` | `Public Property usize WParam` | Get/Set |
| `LParam` | `Public Property isize LParam` | Get/Set |
| `Handled` | `Public Property bool Handled` | Get/Set |

### IO.UI.Event

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(EventKind kind, IO.Type.Object.Function handler)` | — |
| `Kind` | `Public Property EventKind Kind` | Get/Set |
| `Handler` | `Public Property IO.Type.Object.Function Handler` | Get/Set |
| `Invoke` | `Public Trusted Function void Invoke(IO.Type.Object.Class sender, EventArgs args)` | — |

### IO.UI.MessageBox

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Show` | `Public Trusted Function int Show(string text, string caption = "", MessageBoxButtons buttons = MessageBoxButtons.OK, MessageBoxIcon icon = MessageBoxIcon.None)` | — |
| `ShowInfo` | `Public Trusted Function int ShowInfo(string text, string caption = "")` | — |
| `ShowError` | `Public Trusted Function int ShowError(string text, string caption = "")` | — |

### IO.UI.Window

| Member | Declaration | Accessors |
|--------|-------------|-----------|
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
