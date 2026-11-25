# IO.UI 2.0 API 参考

本页适用于 Windows x64 托管环境中的 `IO.UI@2.0.0`。签名中的名字相对于 `IO.UI`。例如，声明 `Get UI By IO.UI;` 后可以写 `New UI.Button("Save")`。入门和完整例子分别见[使用指南](ui.md)与[自定义控件教程](ui-custom-controls.md)。

## 构造函数

`title` 和 `text` 是字符串，`mode` 是 `LayoutMode`，坐标和尺寸使用 `int`。`ComboBox` 和 `ProgressBar` 从位置参数开始。要为组合框设置未选中时显示的文字，请给 `Text` 属性赋值。

| 通过 `New` 调用 |
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

`Panel` 构造后的期望尺寸为 240 × 180，可以继续通过属性设置宽高。

## 窗口

| 成员 | 行为 |
|---|---|
| `Title` | 创建前后均可设置。窗口已打开时，标题立即更新。 |
| `Width`、`Height`、`X`、`Y`、`UseClientSize`、`DarkTitle` | 在 `Create()`、`Show()` 或 `Run()` 前设置。默认宽高指初始外部尺寸。`UseClientSize = true` 时指初始客户区尺寸。窗口打开后，给这些几何属性赋值只更新保存值。 |
| `ClientWidth`、`ClientHeight` | 布局完成后的实际客户区宽高，使用逻辑单位。可在 `OnResize` 中读取。 |
| `Create() → byte*`、`Handle` | 创建或取得共享窗口句柄。触发 `OnClose` 前会清空 `Handle`。 |
| `Show()`、`Run() → int`、`Close()` | 显示窗口、进入消息循环或请求关闭。`OnClosing` 可以拦截关闭，也适用于 `Close()` 发起的请求。 |
| `BackColor`、`ForeColor`、`Font`、`Palette` | 窗口背景、可继承的文字和字体默认值，以及预设控件使用的调色板。显式设置 `BackColor` 会覆盖调色板的客户区背景色。 |
| `Add(component) → Window`、`Remove(component) → bool`、`Components`、`ChildCount`、`ChildAt(index)` | 管理或查询顶层控件。`Remove` 返回是否移除了直属控件。 |
| `FindControl(name) → Component`、`HitTest(x, y) → Component` | 按名称递归查找，或按客户区坐标查找最上层的命中控件。找不到时返回 `null`。需要按名称查找时，为控件分配唯一名称。 |
| `FocusControl(component)` | 把键盘焦点移到本窗口的控件。传 `null` 清除焦点。 |
| `Arrange(width, height)`、`DrawInto(context)` | 按指定客户区大小排版，再画入有效的 `DrawingContext`。预览可用这两个方法把未开窗的控件树画到另一窗口。 |
| `RequestLayout()`、`LayoutChildren()`、`Invalidate()` | 排队请求布局、用当前原生客户区尺寸立即布局、请求重绘。属性的设置方法已经会发出相应请求。纯布局测试可以直接调用 `Arrange`。 |

## 控件与容器

| 成员 | 行为 |
|---|---|
| `Name`、`Text`、`Owner`、`Parent`、`HostHandle` | 查找名称、显示文本、所属窗口、直属父控件和宿主窗口句柄。顶层控件的 `Parent = null`。 |
| `X`、`Y`、`Width`、`Height` | 请求的位置和尺寸。由父布局决定使用哪些值。 |
| `ActualX`、`ActualY`、`ActualWidth`、`ActualHeight` | 排版后相对于父级的位置和实际尺寸。布局可能排队执行，请在排版后读取。绘制时从本地原点 `(0, 0)` 开始，使用实际宽高。 |
| `FillWindow`、`AbsolutePosition` | `FillWindow` 作用于 `Window` 的直属控件。`AbsolutePosition` 让 `Panel` 的子控件脱离常规排列。 |
| `Margin`、`Grow`、`MinWidth`、`MinHeight`、`MaxWidth`、`MaxHeight` | 子控件间距和尺寸约束。`Grow` 分配行或列的剩余空间。最大值为零表示无上限。 |
| `Row`、`Column`、`RowSpan`、`ColumnSpan` | 网格行列索引及跨度。跨度为一表示占用单个轨道。 |
| `Hovered`、`Pressed`、`HasFocus`、`AcceptsTab` | 输入与焦点状态。`AcceptsTab` 让已聚焦的控件自行接收 Tab 键。它不会把控件从 Tab 导航顺序中排除。 |
| `Invalidate()`、`InvalidateLayout()` | 请求重绘或重新分配边界。自定义属性改变了 `MeasureWidth` 或 `MeasureHeight` 的结果时，调用后者。 |
| `GroupBox.Add(child)`、`Remove(child)`、`Children`、`ChildCount`、`ChildAt(index)` | 管理直属控件。组内坐标相对于分组原点。安排位置时请给分组标题留空间。 |
| `Panel.Mode`、`Padding`、`Gap`、`Rows`、`Columns` | 设置面板排列方式。修改网格轨道后，通过属性重新赋值轨道数组来请求布局。轨道由 `Track.Fixed(size)`、`Track.Auto()` 和 `Track.Fraction(weight = 1)` 创建。 |

控件树决定布局、绘制顺序和命中测试。移动到另一个容器时，先 `Remove`，再 `Add`。同一容器里较晚添加的控件绘制在上层，命中测试也优先检查它。父级边界会裁剪整个子树。

## 预设控件的状态与操作

| 控件 | 属性与方法 |
|---|---|
| `Label` | `Text` 和继承的外观属性。 |
| `Button` | `TriggerClick()` 触发 `OnClick`，`WParam` 为按钮的 `Id`。 |
| `Input` | `Text`、`PlaceholderText`、`ReadOnly`、`Password`、`MaxLength`、`Multiline`、`Caret`、`Anchor`、`ScrollX`。`MaxLength` 为零表示不限长度。 |
| `CheckBox` | 通过 `Checked` 取得当前状态。用户激活时，先更新选中状态，再触发 `OnCheckedChanged` 和 `OnClick`。 |
| `RadioButton` | `Checked` 和 `GroupName`。选中一个按钮会取消同组兄弟的选中状态。已挂到窗口的兄弟按钮会收到 `OnCheckedChanged`。 |
| `ComboBox` | `Items` 保存选项。用 `AddItem(text)`、`ClearItems()`、`SelectedIndex` 操作。`ClearItems()` 同时把索引设为 -1。 |
| `ProgressBar` | `Minimum`、`Maximum`、`Value`、`Step` 默认为 0、100、0、10。`PerformStep()` 递增数值，并以 `Maximum` 为上限。直接赋值前可自行限制范围，使应用状态与画面一致。 |
| `TemplateButton` | `Enabled` 禁用激活并切换外观。`Template` 提供颜色、焦点框、圆角、文字与过渡配置。运行中修改已有模板的字段后，调用 `RefreshVisual()`。 |
| `Viewport3D` | `SetMesh(points, indices, colors)`、`TriangleCount`、`ResetMesh()`、`ResetCamera()`。`Yaw` 和 `Pitch` 使用角度（`Pitch` 限制在 ±85°），`Zoom` 使用百分比。`PanX` 和 `PanY` 保存偏移量。`Orthographic`、`Wireframe`、`ShowEdges` 控制投影或画法。 |

### 输入与选区

`Caret` 和 `Anchor` 是相对于 `Text` 的 **UTF-8 字节偏移**。两者相等表示选区为空。替换文本并定位光标时，应同时设置这两个值。`ScrollX` 是水平滚动的逻辑距离。`MaxLength` 按 UTF-16 代码单元检查用户编辑，因此基本多文种平面以外的表情计为两个单元。粘贴也经过这项检查，代码直接设置 `Text` 时由调用方检查长度。偏移与绘图位置之间的转换见下文[文字位置与命中测试](#文字位置与命中测试)。

`ReadOnly` 会阻止用户修改，但仍可选中文字。`Password` 会遮罩显示，并禁止把选区复制到剪贴板。从 `Text` 读出的是原始明文。`Multiline` 允许粘贴中的换行保留，内置编辑路径仍以单行为主。回车不插入换行，光标移动和滚动都按水平方向处理。要做完整的多行编辑，可以用自定义控件。

### 组合框选项

用 `SelectedIndex = -1` 表示未选择。取选项前检查 `0 ≤ SelectedIndex < Items.Length()`。索引无效时画面回退到 `Text`。代码设置 `SelectedIndex` 会重绘。选择事件由用户输入触发，所以填充完选项后如需同步其他字段，应主动刷新。上下方向键到达边界后仍可能产生通知，业务需要识别真正变化时可比较新旧索引。

`Editable` 保存配置值。当前预设实现的是带弹出列表的选择器，有输入需求时可以搭配独立的 `Input`。列表为每个选项排一行，绘制在窗口客户区内，请让选项数量适合窗口尺寸。

## 事件分发

将 `Function void(IO.Type.Object.Class sender, UI.EventArgs args)` 类型的处理函数传给 `New UI.Event(kind, handler)`。下表列出内置输入路径和窗口流程实际触发的事件。

| 类型 | 事件 |
|---|---|
| `Window` | `OnShown`, `OnClosing`, `OnClose`, `OnResize`, `OnMove` |
| `Button` | `OnClick`, `OnMouseEnter`, `OnMouseLeave`, `OnMouseDown`, `OnMouseMove`, `OnMouseUp` |
| `Input` | `OnTextChanged`, `OnKeyDown`, `OnKeyUp`, `OnFocus`, `OnBlur` |
| `CheckBox` / `RadioButton` | `OnCheckedChanged`, `OnClick`, `OnFocus`, `OnBlur` |
| `ComboBox` | `OnSelectedChanged`, `OnFocus`, `OnBlur` |
| `Canvas` / `Viewport3D` | `OnMouseEnter`, `OnMouseLeave`, `OnMouseMove`, `OnMouseDown`, `OnMouseUp`, `OnKeyDown`, `OnKeyUp`, `OnFocus`, `OnBlur` |
| `TemplateButton` | `OnClick`, `OnMouseEnter`, `OnMouseLeave`, `OnMouseDown`, `OnMouseUp`, `OnFocus`, `OnBlur` |

`Label`、`ProgressBar`、`GroupBox`、`Panel` 用于展示或排版。事件绑定在可交互的子控件上。`Window.SupportsEvent` 还接受键盘事件类型，但键盘消息实际转发给聚焦控件。自定义组件需要双击、中键和右键上下文操作时，通过 `OnSurfaceEvent` 处理。

匹配的处理函数会按顺序收到同一个 `EventArgs`。窗口的关闭流程使用 `Handled`，它不会截断后续处理函数。参数随 `Kind` 变化。

| 事件 | `WParam` / `LParam` |
|---|---|
| 窗口 `OnResize` | 客户区逻辑宽高。此时已经完成布局。 |
| `Input`、`Canvas` 的 `OnKeyDown`、`OnKeyUp` | Windows 虚拟键码和零。 |
| `Button.OnClick` | 按钮 `Id` 和零。 |
| `Button.OnMouseMove`，`Canvas` 的指针移动／按下／松开 | 本地 X 和 Y。捕获期间如需得到带符号的 X，请转为 `[int]`。 |
| 选择、文本、选中态改变 | 从 `sender` 读取新状态。 |

需要在控件存活期间替换处理函数时，保存注册时的 `Event` 对象，并先用 `RemoveEvent` 移除该对象。`HasEvent(kind)` 可查询控件是否已注册这一类事件。

## 绘制、样式与底层输入

### 样式解析

| 成员 | 实际绘制时如何使用 |
|---|---|
| `BackColor`、`HoverBackColor`、`PressedBackColor` | `StyleFill(normal, hover, pressed)` 按按下、悬停、常态的优先顺序选择状态，并取该状态的配置色或传入的后备色。 |
| `ForeColor`、`HoverForeColor`、`PressedForeColor` | `StyleText(normal)` 先解析包含继承的常态前景色，再应用悬停或按下的文字颜色。 |
| `BorderColor`、`HoverBorderColor`、`PressedBorderColor` | `StyleBorder(normal)` 先解析常态边框，再应用当前状态的覆盖色。`BorderWidth = 0` 时预设不绘制边框。 |
| `CornerRadius` | `StyleRadius(fallback)` 优先使用控件的设置。显式设置为零可得到直角。 |
| `Font`、`EffectiveFont()` | 从控件、父级和窗口中依次查找字体。当前预设把字号和字重传给 DirectWrite，共享表面使用 Segoe UI 或 Consolas。`Font.Family`、`Italic`、`Underline` 和 `StrikeOut` 目前不改变预设控件的绘制。 |
| `Window.Palette`、`EffectivePalette()` | 调色板有 `Canvas`、`Surface`、`Field`、`Hover`、`Border`、`Subtle`、`Text`、`Accent`、`Caret`、`OnAccent`、`Selection`。子类可重写 `EffectivePalette()`，为本控件提供独立配色，组合框的弹出列表也会使用这套配色。 |

`Color.FromRgb(r, g, b)` 创建不透明颜色。需要 Alpha 时用 `New UI.Color(r, g, b, a)`。组件样式接收该对象。绘图函数和调色板字段使用 `u32` 格式的 ARGB。

### DrawingContext 绘制函数

坐标和尺寸都以逻辑单位表示，原点由当前上下文决定。上下文应在绘制回调中使用。网格的深度和变换状态由控件保存。

| 调用 | 默认值和效果 |
|---|---|
| `Box(x, y, width, height, argb, radius = 0, stroke = 0)` | 默认填充矩形。`stroke` 为正时绘制轮廓。 |
| `Line(x, y, endX, endY, color, thickness = 1)` | 画线段。 |
| `Triangle(ax, ay, bx, by, cx, cy, color)` | 填充三角形。 |
| `Gradient(x, y, width, height, fromColor, toColor, vertical = false)` | 两色渐变。可从左到右或从上到下。 |
| `Write(text, x, y, width, height, argb, size = 13, weight = 400, mono = false, alignment = TextAlignment.Start)` | 文字在矩形内垂直居中，横向可选 `Start`、`Center`、`End`。文字不会自动折行，请为内容留出足够空间。 |
| `View(x, y, zoom = 100) → DrawingContext` | 相对于当前原点平移，并将所请求的缩放与当前比例相乘。缩放用百分比表示。 |
| `PushClip(x, y, width, height)`、`PopClip()` | 收窄并恢复裁剪区域。在同一次绘制中成对调用。 |

### 文字位置与命中测试

从合法边界出发，用 `NextChar(text, offset)` 和 `PreviousChar(text, offset)` 按 UTF-8 码点移动。以组合字符为单位的操作需要另行处理。`Utf16Column(text, byteOffset)` 返回相应的 UTF-16 位置。`Utf8Column(text, units)` 转成字节偏移，如果位置在代理对内部，则停在代理对之前。`CharacterText(code)` 把字符事件中的有效 Unicode 标量转换为字符串。

`TextPosition(handle, text, size, mono, offset, hit = false)` 在有效表面上把 UTF-16 位置转为逻辑横坐标。`hit = true` 时传入逻辑 X，得到命中的 UTF-16 位置。自定义文本控件可以继续转成 UTF-8 字节偏移，再切分字符串。

### SurfaceInput 参数

控件可重写 `OnSurfaceEvent(input)`。窗口负责指针命中测试，以及将窗口坐标转换为控件本地坐标。指针坐标带符号，捕获期间可能为负。

| `Kind` | `First` / `Second` |
|---|---|
| `PointerDown`、`PointerMove`、`PointerUp`、`MiddleDown`、`MiddleUp`、`DoubleClick`、`ContextMenu` | 控件本地 X、Y。Windows 在右键松开时发送 `ContextMenu`。 |
| `Wheel` | 带符号的滚轮增量和 Windows 按键／按钮状态掩码。优先发给悬停控件，否则走键盘路由。 |
| `KeyDown`、`KeyUp` | 虚拟键码和原生按键消息标志。文本输入单独走 `Character`。 |
| `Character` | `First` 中的 Unicode 标量。用 `CharacterText` 得到字符串。 |
| `Focus`、`Blur`、`PointerLeave`、`CaptureLost`、`Frame` | 通过 `Kind` 更新输入状态或请求重绘。 |
| `Resize` | 窗口客户区宽高。控件自身尺寸从实际布局边界读取。 |

## 动画与停靠

- `Transition(initial = 0, duration = 140, easing = Easing.EaseOut)` 提供 `Duration`、`Delay`、`Curve`、`RespectSystemMotion`。`To(target)`、`Current()` 使用单调时钟。`StartAt(target, now)`、`Sample(now)` 允许传入明确的毫秒时间戳。`Snap(value)` 立即设置起点和终点。取样后读取 `Running`，为假时可取消帧订阅。`BlendColor(a, b, amount)` 按 0 到 1 的比例插值 ARGB 通道。
- `DockLayout` 使用稳定的字符串 ID 和整数面板索引。`Add(id, title, area)` 会创建面板，并把它设为所在停靠区的活动项。`Find(id)`、`Active(area)` 找不到时返回 -1。`DockPane.Area` 是当前区域，`Home` 记录浮动前离开的停靠区。`X`、`Y`、`Width`、`Height` 为宿主管理的浮动窗口保存位置和尺寸。浮动面板没有停靠区的活动标签。

## 常见问题

| 现象 | 排查方式 |
|---|---|
| 旧窗体有的成员在新构建中找不到 | 检查 `Packages.Pins`。2.0 用 `HostHandle` 做窗体级集成，通过 `Component` 接口操作控件。旧版签名可查阅单独的参考页。[IO.UI 1.0 参考](ui-legacy.md) |
| 首帧尺寸不对 | 在创建前设置窗体尺寸。自动布局使用窗口直属的 `FillWindow` 面板，在布局后读取控件实际边界。 |
| 绘制或拖动出现坐标偏移 | 绘制使用本地原点，`SurfaceInput` 指针坐标已转换到本地。也可检查父级裁剪和同级绘制顺序。 |
| 运行操作时输入和动画停住 | 消息循环正忙于处理函数。缩短处理函数的执行时间，在窗体线程应用界面状态。 |
| 处理函数没有运行或绑定时抛出异常 | 先查事件表和 `SupportsEvent`，需要值时从发送者读取。代码直接修改 `Input.Text` 等属性后，显式同步业务状态。 |
| 工具箱能显示自定义类，但设计器里的外观不同 | 注册注释中的预览类型决定设计时占位外观。运行窗体即可检查自定义绘制与交互。[自定义控件与 Studio 注册](ui-custom-controls.md) |
