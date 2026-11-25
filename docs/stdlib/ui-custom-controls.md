# Custom controls in IO.UI 2.0

A styled button, an animated meter and a Studio toolbox item all use the same control tree. Start with a preset when you want its behavior. Choose `UI.Canvas` when your widget needs its own drawing and input model. For an animated button with built-in disabled and keyboard states, customize `UI.TemplateButton.Template`. To group existing widgets in code, add them to a `UI.Panel`.

## Extend a button and keep its input behavior

Save this file as `src/Controls/AccentButton.kn` in a desktop Studio project. The first line registers the class with Studio. The runtime class inherits pointer and keyboard activation and still supports an ordinary `OnClick` handler.

```kinal
// @studio-control AccentButton Button Accent button
Unit StudioForm;
Get UI By IO.UI;

Public Class AccentButton By UI.Button
{
    Public Constructor(string text, int x, int y, int width, int height)
    {
        _Text = text;
        _X = x;
        _Y = y;
        _Width = width;
        _Height = height;
        _Events = {};
        CornerRadius = 6;
    }

    Public Override Function void Draw(UI.DrawingContext g)
    {
        Base.Draw(g);
        UI.ControlPalette p = EffectivePalette();
        g.Box(5, ActualHeight / 4, 3, ActualHeight / 2, p.Accent, 1);
        If (HasFocus) g.Box(1, 1, ActualWidth - 2, ActualHeight - 2, p.Accent, 6, 1);
    }
}
```

`Base.Draw(g)` draws the themed button and its caption. The two additional boxes draw an accent and a focus outline over it. The drawing origin is the top left of this button. Adding `X` or `ActualX` here would apply its horizontal offset twice. Use the assigned size so that both decorations follow resizes.

The five constructor arguments match the call emitted by Studio. This constructor initializes the protected geometry, caption and event list in the same way as Studio's custom-button template. When deriving from a more specialized control, also initialize the state it needs, such as a combo's item list and initial selected index.

## Build an interactive canvas

Save this class as `LevelMeter.kn`. Clicking or dragging sets a percentage. With keyboard focus, Left and Right adjust it by five points. The label shows the requested level immediately while a short transition moves the bar to it.

```kinal
// @studio-control LevelMeter Label Level meter
Unit StudioForm;
Get UI By IO.UI;

Public Class LevelMeter By UI.Canvas
{
    Private UI.Transition _fill;
    Public int Level;

    Public Constructor(string text, int x, int y, int width, int height)
    {
        _Text = text;
        _X = x;
        _Y = y;
        _Width = width;
        _Height = height;
        _Events = {};
        Level = 40;
        _fill = New UI.Transition(0.4, 140, UI.Easing.EaseOut);
    }

    Public Function void SetLevel(int value)
    {
        Level = value < 0 ? 0 : (value > 100 ? 100 : value);
        _fill.To([float](Level) / 100.0);
        RequestFrames(_fill.Running);
        Invalidate();
    }

    Public Override Function void OnSurfaceEvent(UI.SurfaceInput input)
    {
        Base.OnSurfaceEvent(input);
        UI.SurfaceEvent k = input.Kind;
        If (k == UI.SurfaceEvent.Frame) {
            Invalidate();
            Return;
        }
        If ((k == UI.SurfaceEvent.PointerDown ||
             (k == UI.SurfaceEvent.PointerMove && Pressed)) && ActualWidth > 0) {
            SetLevel(input.First * 100 / ActualWidth);
        } Else If (k == UI.SurfaceEvent.KeyDown && input.First == 37) {
            SetLevel(Level - 5);
        } Else If (k == UI.SurfaceEvent.KeyDown && input.First == 39) {
            SetLevel(Level + 5);
        } Else If (k == UI.SurfaceEvent.Focus || k == UI.SurfaceEvent.Blur) {
            Invalidate();
        }
    }

    Public Override Function void Draw(UI.DrawingContext g)
    {
        UI.ControlPalette p = EffectivePalette();
        int w = ActualWidth;
        int h = ActualHeight;
        int filled = [int](_fill.Current() * [float](w));
        g.Box(0, 0, w, h, StyleFill(p.Surface, p.Surface, p.Surface), 5);
        If (filled > 0) g.Box(0, h - 6, filled, 6, p.Accent, 3);
        g.Write(Text + "  " + [string](Level) + "%", 6, 0, w - 12, h - 6,
                StyleText(p.Text), 14, 600);
        If (HasFocus) g.Box(0, 0, w, h, p.Accent, 5, 1);
        If (!_fill.Running) RequestFrames(false);
    }
}
```

- **Input.** `SurfaceInput.First` and `Second` carry local X and Y for pointer events. A press captures the hit control, so a drag can move outside its bounds. Clamp the value, and handle `CaptureLost` if you keep a drag flag of your own. The canvas base handler maintains `Pressed` and `Hovered` and raises the supported focus, key and mouse events. Calling it preserves these behaviors.
- **Repaint.** Change logical state in `SetLevel` and call `Invalidate()`. `Draw` reads the latest state on a paint. Keeping state changes outside drawing makes repainting after uncovering the window predictable. Use `SetLevel` when updating this control from application code.
- **Animation.** `Transition.To` starts at the currently displayed value, including when a drag changes the target mid-transition. `Current()` samples the clock and marks a completed transition as stopped. `RequestFrames(true)` asks the owning window to deliver `SurfaceEvent.Frame`. Start a frame-driven animation once the window has a handle. Unsubscribe as soon as sampling reaches the target. Zero duration or the system's reduced-motion setting can finish the transition immediately, so request a repaint even if `Running` is false.
- **Testing.** For a deterministic test of timing, set `RespectSystemMotion = false` on the test transition, call `StartAt(target, startMs)` and `Sample(nowMs)` with explicit millisecond timestamps. `Snap(value)` immediately sets the displayed value and stops the transition.

### Run both widgets without Studio

Place the two class files and this `Main.kn` in the same directory. Run `kinal run Main.kn` in a Windows SDK with IO.UI 2.0. Kinal discovers the other files in the same unit. Use the Studio Desktop project's existing entry point if following the designer steps below.

```kinal
Unit StudioForm;
Get UI By IO.UI;

Static Function int Main()
{
    UI.Window window = New UI.Window("Level", 360, 120);
    window.UseClientSize = true;
    LevelMeter meter = New LevelMeter("Volume", 16, 12, 328, 40);
    AccentButton reset = New AccentButton("Reset", 16, 68, 96, 34);
    reset.AddEvent(New UI.Event(UI.EventKind.OnClick,
        Function void(IO.Type.Object.Class sender, UI.EventArgs args) {
            meter.SetLevel(40);
        }
    ));
    window.Add(meter);
    window.Add(reset);
    Return window.Run();
}
```

## Compose a control from a panel

Use a container subclass when the pieces should keep their own focus, editing and events. This example groups an input and a button in a reusable row. The form reads and writes the text through `Value` and subscribes through `BindApply`. The callback receives the actual button as `sender` because the composite delegates registration to it.

Save this as `AddressEntry.kn` and run `kinal run AddressEntry.kn`. Try typing a value, pressing Tab to reach **Apply**, then pressing Space. Resizing the window changes the space allocated to the text field.

```kinal
Unit App.AddressEntry;
Get UI By IO.UI;

Public Class AddressField By UI.Panel
{
    Private UI.Input _entry;
    Private UI.Button _apply;

    Public Constructor()
    {
        _Children = {};
        _Events = {};
        Rows = {};
        Columns = {};
        Mode = UI.LayoutMode.Row;
        Padding = 8;
        Gap = 8;
        Width = 340;
        Height = 48;

        _entry = New UI.Input();
        _entry.PlaceholderText = "Server address";
        _entry.Grow = 1;
        _apply = New UI.Button("Apply", 0, 0, 84, 28);
        Add(_entry);
        Add(_apply);
    }

    Public Property string Value
    {
        Get { Return _entry.Text; }
        Set { _entry.Text = value; }
    }

    Public Function void BindApply(IO.Type.Object.Function handler)
    {
        _apply.AddEvent(New UI.Event(UI.EventKind.OnClick, handler));
    }
}

Static Function int Main()
{
    UI.Window window = New UI.Window("Address entry", 380, 108);
    window.UseClientSize = true;

    UI.Panel column = New UI.Panel(UI.LayoutMode.Column);
    column.Padding = 8;
    column.Gap = 8;
    column.FillWindow = true;
    AddressField field = New AddressField();
    UI.Label status = New UI.Label("Ready");

    field.Value = "localhost";
    field.BindApply(Function void(IO.Type.Object.Class sender, UI.EventArgs args) {
        status.Text = "Selected: " + field.Value;
    });
    column.Add(field);
    column.Add(status);
    window.Add(column);
    Return window.Run();
}
```

The constructor initializes the inherited child and event lists and the grid-track arrays. The container's normal layout code arranges its children. Use the same `Add` and `Remove` rules as for any other panel. This example composes controls in code. Studio places a registered custom control as one item, with the chosen preview's property and event rows. Its generated source doesn't create designer entries for children constructed inside your class.

## Register a control with Kinal Studio

```text
// @studio-control <class> <preview-kind> <toolbox label>
```

Place that comment on the first line of a `.kn` file inside the project source set. Use the same `Unit StudioForm;` as the generated Desktop form and give the public class a constructor taking `(string text, int x, int y, int width, int height)`. The toolbox label may contain spaces.

The preview kind selects a designer representation and its property and event rows. `AccentButton Button` exposes the button preview and its click binding. `LevelMeter Label` exposes a label-shaped placeholder. At runtime, Studio constructs `AccentButton` and `LevelMeter` themselves, so their overrides run when you launch the form. Use a preview kind whose fields and events the real component supports. Access custom methods and fields such as `SetLevel` from code.

1. Save the class files, then open the project in Studio. The custom entries appear below the toolbox separator.
2. Drop `LevelMeter` and `AccentButton` on the form. Set their names to `LevelMeter1` and `ResetButton`. Name the visible button **Reset**. Set the meter's text to **Volume**.
3. In the button's event rows, set its click handler to `ResetLevel`. You can use the lightning button to generate the handler first, then put the following method body in the generated stub. If adding the whole method manually, use this signature.

```kinal
// Add this method inside MainForm in src/Main.kn.
Public Function void ResetLevel()
{
    LevelMeter1.SetLevel(40);
}
```

Save the form and run it. Drag the meter, then press **Reset**. The event handler accesses the generated `LevelMeter1` member on its own `MainForm` instance.

`MainForm.kui` stores the design. Before building, Studio generates `src/MainForm.Designer.kn` with the typed fields, construction code and event bindings. Keep application logic in `src/Main.kn` and custom classes in separate source files. If a control does not show up in the toolbox, check the first-line comment and the project's source set. If the designer shows its placeholder and the build fails, check the class name, constructor and event handler signature.

## Drawing and input reference

| API | Use |
|---|---|
| `g.Box(x, y, w, h, color, radius, stroke)` | Draw a filled rounded rectangle when `stroke = 0`, or an outline with positive stroke. |
| `g.Line(...)` / `g.Triangle(...)` | Draw rules and filled triangles. |
| `g.Gradient(x, y, w, h, from, to, vertical)` | Fill from left to right by default, or from top to bottom when `vertical = true`. |
| `g.Write(...)` | Draw text with size, weight, optional monospaced font and `TextAlignment.Start`, `Center` or `End`. |
| `g.View(x, y, zoom)` | Create a child drawing context translated from the current one. Zoom uses percent with 100 as normal size. |
| `g.PushClip(...)` / `g.PopClip()` | Limit a drawing region. Pair each push with a pop in the same draw call. |
| `MeasureWidth` / `MeasureHeight` | Override these to offer a preferred size to a parent layout. Draw using the size ultimately assigned. |
| `SupportsEventCode(int code)` | Override when a subclass exposes additional existing `EventKind` values through `AddEvent`. Combine with the base check and raise the event from the corresponding input path. |

For a custom-drawn control, `StyleFill(normal, hover, pressed)`, `StyleText(normal)`, `StyleBorder(normal)` and `StyleRadius(normal)` keep per-control and inspector styles effective. All drawing colors use `0xAARRGGBB`. The renderer clips each control and its children to the control's bounds. Draw during `Draw` and use `Invalidate()` when external state changes. Avoid disk, network and other blocking work inside paint and input callbacks.

- [IO.UI layout, controls, palettes, and Viewport3D](ui.md)
- [IO.UI 2.0 API, SurfaceInput payloads, and text hit testing](ui-reference.md)
