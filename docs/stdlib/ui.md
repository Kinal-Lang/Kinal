# IO.UI

IO.UI 2.0 builds Windows desktop forms in Kinal. Each window owns one Direct3D 11 surface. Controls form a tree drawn with Direct2D and DirectWrite. The same window, layout and event code runs as a hosted Native executable or as bytecode with the matching Windows desktop KinalVM.

For a new project, pin `IO.UI@2.0.0` in its `Packages.Pins` list. With both versions installed, an unpinned Windows x64 SDK build selects 2.0. A Native project that calls Win32 with individual child control handles needs `IO.UI@1.0.0`. In 2.0, `component.HostHandle` identifies the owning **window**. Windows x64 is the desktop target covered here.

To build a widget for reuse in several forms, see the [custom control and Studio guide](ui-custom-controls.md).

## A resizable form in one file

Save this as `Greeting.kn`. Type a name, then click **Greet**. Widen the window and the input takes the extra width while the button stays 96 units wide.

```kinal
Unit App.Greeting;
Get UI By IO.UI;

Static Function int Main()
{
    UI.Window window = New UI.Window("Greeting", 440, 72);
    window.UseClientSize = true;
    UI.Panel row = New UI.Panel(UI.LayoutMode.Row);
    row.FillWindow = true;
    row.Padding = 16;
    row.Gap = 8;

    UI.Input name = New UI.Input("", 0, 0, 160, 32);
    name.PlaceholderText = "Your name";
    name.Grow = 1;
    UI.Button greet = New UI.Button("Greet", 0, 0, 96, 32);
    greet.AddEvent(New UI.Event(UI.EventKind.OnClick,
        Function void(IO.Type.Object.Class sender, UI.EventArgs args) {
            UI.MessageBox.ShowInfo("Hello, " + name.Text, "Greeting");
        }
    ));
    row.Add(name);
    row.Add(greet);
    window.Add(row);
    Return window.Run();
}
```

Run the source directly, or build and run its bytecode with the Windows desktop SDK.

```powershell
kinal run Greeting.kn
kinal vm build Greeting.kn -o Greeting.knc
kinal vm run Greeting.knc
```

`UseClientSize = true` makes 440 × 72 the initial *client* size. `Show()` creates and displays a window. `Run()` also starts the message loop. To display two windows, call `Show()` on one and `Run()` on the other. Closing the last window exits the loop. A long-running click handler delays input and painting. Modal dialogs can dispatch further messages before the initiating handler returns.

## Layout and ownership

`Width` and `Height` express preferred size. During arrangement each control receives `ActualX`, `ActualY`, `ActualWidth` and `ActualHeight` in logical units relative to its parent. Inspect the actual bounds after layout, and use them when drawing. A top-level `Panel` with `FillWindow = true` follows the client area. Nest panels to combine modes.

| `LayoutMode` | How it arranges the children |
|---|---|
| `Absolute` | Places each child at its own `X` and `Y` inside the panel padding, with its margin added. |
| `Row` / `Column` | Places children in insertion order. Children with `Grow = 0` take their measured extent. Positive `Grow` weights divide the remaining main-axis space after fixed children, gaps and margins. The cross-axis fills the available space within size limits. |
| `Wrap` | Places children left to right at measured sizes. Starts a new line when the next child and its margins no longer fit. |
| `Grid` | Assigns cells by zero-based `Row` and `Column`. `RowSpan` and `ColumnSpan` cover adjacent tracks. |
| `Overlay` | Gives each child the panel's inner rectangle minus its margins. Later children draw over earlier children and are tested first for pointer input. |

`Padding` applies on all four edges of a panel, `Margin` on all four edges of a child. `Gap` separates adjacent tracks or flow items. Set `AbsolutePosition = true` on an individual child to take it out of normal layout and position it with `X` and `Y`. Use `MinWidth`, `MaxWidth`, `MinHeight` and `MaxHeight` to constrain the result. A zero maximum leaves that dimension uncapped.

### Grid with an expanding field

Use the following in `Main` after creating `window`, in place of the row in the first example. `Fixed` reserves its size. `Auto` measures the largest single-track child in that track, including its margins. The auto measurement uses only single-track children. `Fraction` distributes the remaining space by weight. Here the input determines the first row's height and the status spans both columns.

```kinal
UI.Panel grid = New UI.Panel(UI.LayoutMode.Grid);
grid.FillWindow = true;
grid.Padding = 12;
grid.Gap = 8;
grid.Columns = { UI.Track.Fixed(96), UI.Track.Fraction(1) };
grid.Rows = { UI.Track.Auto(), UI.Track.Fixed(32) };

UI.Label caption = New UI.Label("Address", 0, 0, 80, 28);
UI.Input address = New UI.Input("", 0, 0, 160, 32);
address.Column = 1;
UI.Label status = New UI.Label("Ready", 0, 0, 100, 24);
status.Row = 1;
status.ColumnSpan = 2;
grid.Add(caption);
grid.Add(address);
grid.Add(status);
window.Add(grid);
```

`Add` attaches a control to one container. To move it, call `Remove(child)` on its current parent, then `Add(child)` on the destination. Removal clears focus, capture and frame subscriptions for the detached subtree. `Children`, `Components` and `Events` return snapshots. Use the container methods to modify the tree. Give a control a `Name` and call `window.FindControl(name)` to look it up recursively.

For a layout test you can call `window.Arrange(440, 72)` before opening the window, then read actual bounds or call `HitTest(x, y)`. For a preview, `DrawInto(context)` renders the arranged tree into an existing drawing context.

## Controls and change notifications

| Control | Common task | Event to handle |
|---|---|---|
| `Label` / `Button` | Set `Text` to change a caption. A button can be activated with the pointer, Enter or Space. | `UI.EventKind.OnClick` |
| `Input` | Set `PlaceholderText`, `ReadOnly`, `Password` or `MaxLength`. Read the value from `Text`. Password mode masks the display and suppresses copying the selected text. | `OnTextChanged` for user edits, `OnKeyDown` / `OnKeyUp`, `OnFocus` / `OnBlur` |
| `CheckBox` / `RadioButton` | Read `Checked`. Radio buttons with the same `GroupName` under the same parent are mutually exclusive. | `OnCheckedChanged` for a state change, `OnClick` for activation |
| `ComboBox` | Call `ClearItems()` and `AddItem(text)` to populate the list. `SelectedIndex` is zero-based. Check it against `Items.Length()` before indexing. | `OnSelectedChanged` for a user selection |
| `ProgressBar` | Set `Minimum`, `Maximum` and `Value` for known progress, or set `Step` and call `PerformStep()`. | Update it from the action whose progress it displays. |
| `GroupBox` / `Panel` | A titled group holds children at their own positions. Use a `Panel` for a layout mode. | Bind to the interactive children. |
| `TemplateButton` | Use `Enabled` and a `ButtonTemplate` to control disabled, hover, pressed and focus visuals. | `OnClick`, pointer enter/leave/down/up, focus/blur |

Changes in application code can have different notification semantics from user input. Setting `input.Text` or `combo.SelectedIndex` repaints the control. To update dependent fields after populating a form, call your update function explicitly.

### Event lifetime, focus and closing

`AddEvent` takes a `UI.Event`. Keep the event object if you will unsubscribe it later with `RemoveEvent`. For a dynamically chosen widget, check `SupportsEvent(kind)` before subscribing. An unsupported registration throws. In a callback, use `sender Is UI.Button button` to access the concrete control. `Tab` and `Shift+Tab` move focus through eligible controls in tree order. Use `window.FocusControl(input)` for explicit focus.

Use `OnShown` for work needing a displayed window, `OnResize` for client size changes, `OnClosing` for a close decision and `OnClose` for cleanup after destruction. In an `OnClosing` callback set `args.Handled = true` to keep the window open. `MessageBox.ShowInfo` and `ShowError` display a message. For a decision use `MessageBox.Show` with `MessageBoxButtons.YesNo` and `MessageBoxIcon.Question`. The Windows Yes result is `6`.

## Theme a window and its controls

Insert this before `window.Run()` in the greeting example for a Catppuccin Mocha palette. `Canvas` colors the window background, `Field` is used for input surfaces and `Subtle` for secondary text such as placeholders. `Accent`, `OnAccent`, `Caret` and `Selection` keep focus and editing visible. A combo uses its effective palette for its popup too. Set `DarkTitle` before creating the window.

```kinal
UI.ControlPalette p = New UI.ControlPalette();
p.Canvas = 0xff1e1e2e;
p.Surface = 0xff313244;
p.Field = 0xff181825;
p.Hover = 0xff45475a;
p.Border = 0xff585b70;
p.Subtle = 0xffa6adc8;
p.Text = 0xffcdd6f4;
p.Accent = 0xff89b4fa;
p.Caret = p.Accent;
p.OnAccent = 0xff11111b;
p.Selection = 0xff45475a;
window.Palette = p;
window.DarkTitle = true;

greet.BackColor = UI.Color.FromRgb(49, 50, 68);
greet.HoverBackColor = UI.Color.FromRgb(69, 71, 90);
greet.PressedBackColor = UI.Color.FromRgb(88, 91, 112);
greet.CornerRadius = 6;
```

Palette and drawing colors are `u32` in `0xAARRGGBB` order. Individual style properties take `UI.Color`. A component can override normal, hover and pressed fills, text and borders with `BackColor`, `ForeColor`, `BorderColor` and their `Hover…` / `Pressed…` counterparts. Use `BorderWidth` for stroke thickness and `Font = UI.Font.FromSize(16, 600)` for size and weight. An unset foreground or font can come from a parent or the window. When changing fields on an existing palette after showing the form, call `window.Invalidate()` to repaint.

### Animate a button with a template

`TemplateButton.Template` exposes `Background`, `HoverBackground`, `PressedBackground`, `DisabledBackground`, `Foreground`, `DisabledForeground`, `Border` and `FocusRing`. `Duration` and `Delay` use milliseconds. Set `Duration = 0` for immediate feedback. Override the template's `Draw(context, button, fill)` to change the shape or content using the interpolated fill.

Replace the greeting example's `Button` declaration with this block. Keep the event registration and omit the explicit `greet` style properties from the palette example to let the template supply the colors.

```kinal
UI.TemplateButton greet = New UI.TemplateButton("Greet", 0, 0, 96, 32);
UI.ButtonTemplate look = New UI.ButtonTemplate();
look.Background = 0xff313244;
look.HoverBackground = 0xff45475a;
look.PressedBackground = 0xff585b70;
look.DisabledBackground = 0xff181825;
look.Foreground = 0xffcdd6f4;
look.DisabledForeground = 0xffa6adc8;
look.FocusRing = 0xff89b4fa;
look.Radius = 6;
look.Duration = 140;
look.Curve = UI.Easing.EaseOut;
greet.Template = look;
```

Set `greet.Enabled = false` during an operation to show the disabled appearance and suppress activation. Re-enable it when the operation finishes.

## Viewport3D

In a window with room for a 320 × 200 view, add a triangle as follows.

```kinal
UI.Viewport3D view = New UI.Viewport3D("Mesh", 16, 16, 320, 200);
f64[] points = { -1.0,-1.0,0.0, 1.0,-1.0,0.0, 0.0,1.0,0.0 };
int[] indices = { 0, 1, 2 };
u32[] colors = { 0xff89dceb };
view.SetMesh(points, indices, colors);
view.ShowEdges = true;
window.Add(view);
```

`points` stores XYZ triples, `indices` contains three zero-based vertex indices per triangle and `colors` has one ARGB value per face. `SetMesh` validates the lengths, coordinates, extent and index bounds before replacing the current mesh. Valid geometry is centered and normalized for display. `TriangleCount` reports the loaded face count. `ResetMesh()` restores the built-in geometry. `ResetCamera()` restores the initial camera independently.

Drag with the left button to rotate, the middle button to pan, and use the wheel to zoom. With focus, the arrow keys rotate and Home or double-click restores the initial camera. The same settings are available as `Yaw`, `Pitch`, `PanX`, `PanY` and `Zoom` (35–240, initial 100). Set `Orthographic` for parallel projection or `Wireframe` to draw a wire model. `ShowEdges` controls outlines on filled faces.

## Docked tools

`DockLayout` stores pane identity, position and active tabs. For example, `int log = docks.Add("log", "Output", UI.DockArea.Bottom);` on a new `UI.DockLayout` returns an index. Save that index to call `Activate(log)` or `Move(log, UI.DockArea.Floating)`. `Pane(log)` exposes the title, current `Area`, previous docked `Home` and floating rectangle. Restore it with `docks.Move(log, docks.Pane(log).Home)`. `Active(area)` returns the active pane index or `-1`. Use unique IDs so `Find(id)` can find a pane after rearrangement.

The host application draws the tabs, creates floating windows and persists pane positions if required. Kinal Studio uses this model for its tool panes.

## Build and distribute a form

To build the same source for Native and the desktop VM, place the first example at `src/Greeting.kn` and save this as `kinal.knproj`. The two profiles keep the backend and output path explicit. `HideConsole` selects a GUI executable for the Native build.

```kinal
Project Greeting
{
    Packages { Pins = ["IO.UI@2.0.0"]; }
    SourceSet "app" { Roots = ["src"]; Include = ["**/*.kn"]; RequireUnit = true; }

    Profile "native"
    {
        Source { Entry = "src/Greeting.kn"; Sets = ["app"]; }
        Build { Backend = Native; Environment = Hosted; Output = "out/Greeting.exe"; HideConsole = true; }
    }

    Profile "vm"
    {
        Source { Entry = "src/Greeting.kn"; Sets = ["app"]; }
        Build { Backend = VM; Environment = Hosted; Output = "out/Greeting.knc"; }
    }
}
```

Run the following from the project directory. The last command bundles the bytecode with the desktop runner into a separate executable.

```powershell
kinal build --project . --profile native
kinal vm build --project . --profile vm
kinal vm run out/Greeting.knc
kinal vm pack out/Greeting.knc -o out/Greeting-vm.exe
```

Use matching versions of `kinal`, `kinalvm` and the installed IO.UI 2.0 package from a Windows desktop SDK. Keep host-specific calls inside the APIs that your target backend supplies. A custom control using `Canvas` and `DrawingContext` can share its drawing code between the two profiles.

## Related guides

- [Custom controls, animation and Studio registration](ui-custom-controls.md)
- [IO.UI 2.0 API, events, and troubleshooting](ui-reference.md)
- [IO.UI 1.0 Win32 signature reference](ui-legacy.md)
- [Standard library overview](overview.md)
- [Backend support](../advanced/backend-support.md)
- [Project files and package pins](../cli/project-files.md)
