# IO.UI 2.0 reference

This page covers hosted Windows x64 with `IO.UI@2.0.0`. In the signatures, names are relative to `IO.UI`. For example, `Get UI By IO.UI;` lets you write `New UI.Button("Save")`. See the tutorials for [a first form](ui.md) and [custom controls](ui-custom-controls.md).

## Constructors

`title` and `text` are strings, `mode` is a `LayoutMode` and geometry uses `int`. `ComboBox` and `ProgressBar` start with geometry arguments. To set a combo's placeholder caption, assign its `Text` property.

| Call with `New` |
|---|
| `Window(title = "Kinal Window", width = 640, height = 480, x = 100, y = 100)` |
| `Label(text, x = 0, y = 0, width = 160, height = 24)` |
| `Button(text, x = 0, y = 0, width = 120, height = 28)` |
| `Input(text = "", x = 0, y = 0, width = 200, height = 24)` |
| `CheckBox(text, x = 0, y = 0, width = 160, height = 24)` |
| `RadioButton(text, x = 0, y = 0, width = 160, height = 24)` |
| `ComboBox(x = 0, y = 0, width = 160, height = 200)` |
| `ProgressBar(x = 0, y = 0, width = 200, height = 24)` |
| `GroupBox(text, x = 0, y = 0, width = 300, height = 200)` |
| `Panel(mode = LayoutMode.Absolute)` |
| `Canvas(x = 0, y = 0, width = 320, height = 240)` |
| `TemplateButton(text, x = 0, y = 0, width = 100, height = 32)` |
| `Viewport3D(title = "3D", x = 0, y = 0, width = 280, height = 190)` |

Set a `Panel`'s preferred width and height through its properties after construction. Its initial values are 240 × 180.

## Window

| Members | Behavior |
|---|---|
| `Title` | Set before or after creation. The caption of an open window updates immediately. |
| `Width`, `Height`, `X`, `Y`, `UseClientSize`, `DarkTitle` | Set before `Create()`, `Show()` or `Run()`. Width and height describe the initial outer size unless `UseClientSize = true`. Assigning these geometry fields while a window is open only changes the stored values. |
| `ClientWidth`, `ClientHeight` | Actual client size in logical units after arrangement. Read these in `OnResize`. |
| `Create() → byte*`, `Handle` | Create or retrieve the shared window handle. `Handle` is cleared before `OnClose`. |
| `Show()`, `Run() → int`, `Close()` | Show the window, enter the message loop, or request closing. `OnClosing` can veto a close, including a call to `Close()`. |
| `BackColor`, `ForeColor`, `Font`, `Palette` | Window background, inherited text/font defaults and the palette used by presets. Setting `BackColor` explicitly overrides the palette's client background. |
| `Add(component) → Window`, `Remove(component) → bool`, `Components`, `ChildCount`, `ChildAt(index)` | Manage or inspect top-level controls. `Remove` returns whether the control was a direct child. |
| `FindControl(name) → Component`, `HitTest(x, y) → Component` | Search by name through nested containers or find the topmost component at a point in client coordinates. Both return `null` on a miss. Give controls unique names if looking them up by name. |
| `FocusControl(component)` | Move keyboard focus to a component belonging to this window. Pass `null` to clear it. |
| `Arrange(width, height)`, `DrawInto(context)` | Lay out the tree at a specified client size and paint it into a live `DrawingContext`. A detached preview can use these without opening another window. |
| `RequestLayout()`, `LayoutChildren()`, `Invalidate()` | Queue layout, lay out using the current native client extent immediately, or request paint. Property setters already request the relevant update. Use `Arrange` directly for a detached layout test. |

## Component and containers

| Members | Behavior |
|---|---|
| `Name`, `Text`, `Owner`, `Parent`, `HostHandle` | Stable lookup name, visible text, owning window, immediate parent component and host window handle. Top-level components have `Parent = null`. |
| `X`, `Y`, `Width`, `Height` | Requested position and size. The parent layout decides which values to use. |
| `ActualX`, `ActualY`, `ActualWidth`, `ActualHeight` | Arranged position relative to the parent and arranged size. Layout can be queued, so read these after arrangement. Draw at local origin `(0, 0)` using the actual width and height. |
| `FillWindow`, `AbsolutePosition` | `FillWindow` applies to a direct child of `Window`. `AbsolutePosition` takes a child of `Panel` out of its normal flow. |
| `Margin`, `Grow`, `MinWidth`, `MinHeight`, `MaxWidth`, `MaxHeight` | Per-child spacing and size constraints. `Grow` distributes leftover space in rows and columns. Zero maximum means open-ended. |
| `Row`, `Column`, `RowSpan`, `ColumnSpan` | Grid indices and spans. A span of one occupies a single track. |
| `Hovered`, `Pressed`, `HasFocus`, `AcceptsTab` | Input and focus state. `AcceptsTab` lets the focused component receive the Tab key itself. It does not remove a component from Tab navigation. |
| `Invalidate()`, `InvalidateLayout()` | Repaint or request new bounds. In a custom class, call the latter when a property changes the value returned by `MeasureWidth` or `MeasureHeight`. |
| `GroupBox.Add(child)`, `Remove(child)`, `Children`, `ChildCount`, `ChildAt(index)` | Manage direct children. A group positions them relative to its origin. Allow space for its caption when choosing coordinates. |
| `Panel.Mode`, `Padding`, `Gap`, `Rows`, `Columns` | Configure the panel's arrangement. To update grid tracks, reassign the track array through its property so layout is queued. Use `Track.Fixed(size)`, `Track.Auto()` and `Track.Fraction(weight = 1)`. |

The containment tree owns layout, drawing order and hit testing. Reparent with `Remove` followed by `Add`. A later child in the same container paints on top and has priority in hit testing. The parent clips the child tree to its bounds.

## Control state and editing

| Control | Properties and operations |
|---|---|
| `Label` | `Text` and the inherited appearance properties. |
| `Button` | `TriggerClick()` dispatches `OnClick` with the button's `Id` in `WParam`. |
| `Input` | `Text`, `PlaceholderText`, `ReadOnly`, `Password`, `MaxLength`, `Multiline`, `Caret`, `Anchor`, `ScrollX`. Zero `MaxLength` means unlimited. |
| `CheckBox` | `Checked` gives the current state. For a user activation, the checked state changes before `OnCheckedChanged` and `OnClick` run. |
| `RadioButton` | `Checked` and `GroupName`. Selecting one clears checked siblings in the same group. A cleared sibling attached to a window receives `OnCheckedChanged`. |
| `ComboBox` | `Items` holds the choices. Use `AddItem(text)`, `ClearItems()` and `SelectedIndex`. `ClearItems()` also sets the index to -1. |
| `ProgressBar` | `Minimum`, `Maximum`, `Value` and `Step` default to 0, 100, 0 and 10. `PerformStep()` increments the value and caps it at `Maximum`. To keep application state within the range, clamp values before assigning them. |
| `TemplateButton` | `Enabled` disables activation and switches appearance. `Template` supplies colors, focus ring, radius, text and transition settings. Call `RefreshVisual()` after changing fields on an existing template at runtime. |
| `Viewport3D` | `SetMesh(points, indices, colors)`, `TriangleCount`, `ResetMesh()` and `ResetCamera()`. `Yaw` and `Pitch` are angles in degrees (`Pitch` is limited to ±85°). `Zoom` is a percentage. `PanX` and `PanY` store view offsets. `Orthographic`, `Wireframe` and `ShowEdges` control projection or drawing. |

### Input text and selection

`Caret` and `Anchor` are UTF-8 **byte offsets** into `Text`. Equal offsets mean an empty selection. Set both when replacing text and positioning the caret. `ScrollX` is the logical horizontal scroll offset. `MaxLength` checks user edits in UTF-16 code units, so an emoji outside the Basic Multilingual Plane counts as two. The check also applies when pasting. Application assignment to `Text` bypasses that edit check. Use the [text geometry helpers](#text-geometry) when mapping offsets to drawing positions.

`ReadOnly` blocks user edits and permits selecting text. `Password` masks the display and suppresses clipboard copy of the selected text. Treat the value read from `Text` as the original plaintext. `Multiline` allows pasted line breaks through the edit path, but the built-in editor is single-line oriented. Enter does not insert a new line, and movement and scrolling are horizontal. Implement a custom editor for multiline editing.

### Combo selection

Use `SelectedIndex = -1` for no selection. Check `0 ≤ SelectedIndex < Items.Length()` before reading the item. The current renderer uses `Text` as the fallback caption when the index is invalid. Setting `SelectedIndex` in code repaints. Update dependent state explicitly, since the user-selection notification is dispatched by the input route. Keyboard Up/Down can dispatch even at a selection boundary, so compare indices if your callback needs a genuine change.

`Editable` stores a configuration value. The current preset implements a picker with a popup, so pair it with a separate `Input` if you need typed values. The popup lays out one row per item and uses the window's client region. Keep choice lists short enough to fit.

## Event dispatch

Use a `Function void(IO.Type.Object.Class sender, UI.EventArgs args)` with `New UI.Event(kind, handler)`. The table lists events that the built-in input and window routes deliver.

| Type | Events |
|---|---|
| `Window` | `OnShown`, `OnClosing`, `OnClose`, `OnResize`, `OnMove` |
| `Button` | `OnClick`, `OnMouseEnter`, `OnMouseLeave`, `OnMouseDown`, `OnMouseMove`, `OnMouseUp` |
| `Input` | `OnTextChanged`, `OnKeyDown`, `OnKeyUp`, `OnFocus`, `OnBlur` |
| `CheckBox` / `RadioButton` | `OnCheckedChanged`, `OnClick`, `OnFocus`, `OnBlur` |
| `ComboBox` | `OnSelectedChanged`, `OnFocus`, `OnBlur` |
| `Canvas` / `Viewport3D` | `OnMouseEnter`, `OnMouseLeave`, `OnMouseMove`, `OnMouseDown`, `OnMouseUp`, `OnKeyDown`, `OnKeyUp`, `OnFocus`, `OnBlur` |
| `TemplateButton` | `OnClick`, `OnMouseEnter`, `OnMouseLeave`, `OnMouseDown`, `OnMouseUp`, `OnFocus`, `OnBlur` |

`Label`, `ProgressBar`, `GroupBox` and `Panel` are display or layout controls. Bind events to their interactive children. `Window.SupportsEvent` additionally accepts key event kinds, but keyboard messages are routed to the focused component rather than to a window-level listener. For double-click, middle-button and context input on a custom control, use `OnSurfaceEvent`.

The same `EventArgs` instance goes to matching handlers in order. `Handled` is used by the window close path and does not stop the remaining handlers. The payload depends on `Kind`.

| Event | `WParam` / `LParam` |
|---|---|
| Window `OnResize` | Client width and height in logical units. Arrangement has already run. |
| `OnKeyDown` and `OnKeyUp` on `Input` or `Canvas` | Windows virtual-key code and zero. |
| `Button.OnClick` | Button `Id` and zero. |
| `Button.OnMouseMove`, `Canvas` pointer move/down/up | Local X and Y. Cast X to `[int]` if you need the signed position during capture. |
| Selection, text and check changes | Read the new state from `sender`. |

To observe an event across the component lifetime, store its `Event` instance. Remove that instance with `RemoveEvent` before replacing the handler. `HasEvent(kind)` answers whether a component already has a subscription of that kind.

## Drawing, style and raw input

### Style resolution

| Member | What the renderer uses |
|---|---|
| `BackColor`, `HoverBackColor`, `PressedBackColor` | `StyleFill(normal, hover, pressed)` chooses the state in order pressed, hover, normal and uses that state's configured color or the supplied fallback. |
| `ForeColor`, `HoverForeColor`, `PressedForeColor` | `StyleText(normal)` resolves the inherited normal foreground first. State-specific foregrounds override it. |
| `BorderColor`, `HoverBorderColor`, `PressedBorderColor` | `StyleBorder(normal)` resolves a base border and then applies state-specific overrides. `BorderWidth = 0` suppresses the preset border. |
| `CornerRadius` | `StyleRadius(fallback)` uses the control's setting when present. Explicit radius zero produces square corners. |
| `Font`, `EffectiveFont()` | Picks the nearest set font from the component, its parents or the window. Current presets pass size and weight to DirectWrite. The shared-surface renderer uses Segoe UI or Consolas. The `Font.Family`, `Italic`, `Underline` and `StrikeOut` fields do not change preset drawing. |
| `Window.Palette`, `EffectivePalette()` | The built-in palette roles are `Canvas`, `Surface`, `Field`, `Hover`, `Border`, `Subtle`, `Text`, `Accent`, `Caret`, `OnAccent` and `Selection`. Override `EffectivePalette()` in a subclass to provide a different palette for that widget, including a combo's popup. |

`Color.FromRgb(r, g, b)` creates an opaque color. Use `New UI.Color(r, g, b, a)` for an explicit alpha channel. Components take this object type. Drawing primitives and palette fields take `u32` ARGB.

### DrawingContext calls

Coordinates and sizes are logical units with the origin established by the current context. Use the context during the paint callback.

| Call | Defaults and effect |
|---|---|
| `Box(x, y, width, height, argb, radius = 0, stroke = 0)` | Filled rectangle. Positive `stroke` draws an outline. |
| `Line(x, y, endX, endY, color, thickness = 1)` | Line segment. |
| `Triangle(ax, ay, bx, by, cx, cy, color)` | Filled triangle. |
| `Gradient(x, y, width, height, fromColor, toColor, vertical = false)` | Two-color gradient, left to right or top to bottom. |
| `Write(text, x, y, width, height, argb, size = 13, weight = 400, mono = false, alignment = TextAlignment.Start)` | Draw text centered vertically, with horizontal alignment `Start`, `Center` or `End`. Choose a big enough text rectangle for the content. Word wrapping is disabled. |
| `View(x, y, zoom = 100) → DrawingContext` | Translate relative to the current origin and combine the requested zoom with the current scale. Zoom is a percentage. |
| `PushClip(x, y, width, height)`, `PopClip()` | Narrow and then restore the clip. Balance the calls within the same draw operation. |

### Text geometry

Use `NextChar(text, offset)` and `PreviousChar(text, offset)` to walk UTF-8 code points from a valid boundary. They move by code point rather than by grapheme cluster. `Utf16Column(text, byteOffset)` gives the corresponding UTF-16 position. `Utf8Column(text, units)` maps back to a byte offset and stops before a surrogate pair when the position lands inside it. `CharacterText(code)` encodes a valid Unicode scalar from a character event.

`TextPosition(handle, text, size, mono, offset, hit = false)` on a live surface converts a UTF-16 position to a logical horizontal position. With `hit = true`, pass a logical X and receive a UTF-16 hit position. A typical custom text control converts that result to a UTF-8 byte offset before slicing its string.

### SurfaceInput payloads

Override `OnSurfaceEvent(input)` on the component. Pointer coordinates are signed, so they can be negative when captured. The window performs hit testing and local-coordinate conversion for pointer events.

| `Kind` | `First` / `Second` |
|---|---|
| `PointerDown`, `PointerMove`, `PointerUp`, `MiddleDown`, `MiddleUp`, `DoubleClick`, `ContextMenu` | Control-local X and Y. Windows sends `ContextMenu` from right-button release. |
| `Wheel` | Signed wheel delta and the Windows key/button-state mask. Routed to the hovered component when present, otherwise to the keyboard route. |
| `KeyDown`, `KeyUp` | Virtual key and native key-message flags. `Character` is the separate event for text. |
| `Character` | Unicode scalar in `First`. Use `CharacterText` to obtain the string. |
| `Focus`, `Blur`, `PointerLeave`, `CaptureLost`, `Frame` | Use `Kind` to change input state or schedule a repaint. |
| `Resize` | Window client width and height. Use the component's actual bounds for its size. |

## Animation and docking

- `Transition(initial = 0, duration = 140, easing = Easing.EaseOut)` has `Duration`, `Delay`, `Curve` and `RespectSystemMotion`. `To(target)` and `Current()` use the monotonic clock. `StartAt(target, now)` and `Sample(now)` use explicit millisecond timestamps. `Snap(value)` sets both ends immediately. Read `Running` after sampling to know when to stop requesting frames. `BlendColor(a, b, amount)` interpolates ARGB channels for an amount from 0 to 1.
- `DockLayout` uses stable string IDs and integer pane indices. `Add(id, title, area)` creates a pane and activates it in that docked area. `Find(id)` and `Active(area)` return -1 for a miss. `DockPane.Area` names the current area. `Home` records the area left before floating. `X`, `Y`, `Width` and `Height` provide geometry for host-managed floating panes. A `Floating` pane has no active dock tab.

## Diagnose common problems

| Symptom | Check |
|---|---|
| A property exists on a legacy form but is missing in a new build | Check `Packages.Pins`. In 2.0, use `HostHandle` for window integration and `Component` methods for widgets. The 1.0 signatures are still available. [IO.UI 1.0 reference](ui-legacy.md) |
| The first paint uses the wrong size | Set window geometry before creation. For automatic layout use a direct `FillWindow` panel and read actual control bounds after arrangement. |
| Paint or drag calculations are offset | Draw in local coordinates and use `SurfaceInput`'s already-local pointer coordinates. Check ancestor clipping and sibling order. |
| Animations or typing stop during an operation | The message loop is busy running a handler. Keep handlers brief and apply UI state on the window thread. |
| A callback never runs or binding throws | Check the event table and `SupportsEvent`. Read data from the sender where appropriate. Programmatic assignments such as `Input.Text` need an explicit app-level update. |
| A custom class appears in the toolbox but its drawing differs in the designer | The registration's preview kind selects the design-time placeholder. Inspect the true drawing and behavior in the launched form. [Custom controls and Studio registration](ui-custom-controls.md) |
