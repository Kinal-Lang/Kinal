# IO.UI 2.0 自定义控件

要保留按钮原有的键盘和鼠标行为，可以继承预设控件，只重写绘制。要做仪表、图表或视口一类需要自行解释输入的组件，可以继承 `UI.Canvas`。带动画、禁用态和键盘激活的按钮可使用 `UI.TemplateButton`，并设置它的 `Template`。如果只是把多个现有控件组合起来，在代码中用 `UI.Panel` 容纳它们即可。

## 继承按钮，保留原有交互

把下面的文件保存为 Studio 桌面项目的 `src/Controls/AccentButton.kn`。第一行用于让设计器发现控件。运行时的类继承自按钮，支持鼠标、键盘激活，也能像普通按钮一样订阅 `OnClick`。

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

`Base.Draw(g)` 先绘制按钮底色、边框和标题。后面的两个矩形分别绘制强调色条和键盘焦点框。绘图原点已经在按钮左上角。如果再加一次 `X` 或 `ActualX`，色条就会偏移。使用布局分配的实际宽高，装饰会随按钮大小一起变化。

构造函数的五个参数对应 Studio 生成的实例化调用。示例按 Studio 自定义按钮模板的方式初始化位置、尺寸、文本和事件列表。如果改为继承更专门的控件，还要初始化它需要的状态，例如组合框的选项数组和初始选中索引。

## 从画布制作可交互的数值条

把下面的类保存为 `LevelMeter.kn`。点击或拖动可设置百分比。控件获得键盘焦点后，左右方向键每次调整五个百分点。文字立即显示目标值，色条在短暂过渡中移动到对应位置。

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

- **输入。** 指针事件的 `SurfaceInput.First`、`Second` 分别是控件内的 X、Y。按下时窗口会捕获命中的控件，所以拖动可能超出边界。示例会将数值限制在有效范围。如果另设了拖动标志，应在 `CaptureLost` 时清除。画布基类负责更新 `Pressed`、`Hovered`，并触发支持的焦点、键盘和鼠标事件。保留 `Base.OnSurfaceEvent(input)` 即可继续使用这些行为。
- **重绘。** 在 `SetLevel` 更新业务状态并调用 `Invalidate()`，由 `Draw` 在绘制时读取最新状态。即使窗口被遮挡后再次显示，也能按当前数值重新绘制。在业务代码中也应调用 `SetLevel` 更新数值。
- **动画。** `Transition.To` 从当前显示值开始过渡，拖动中途改变目标也能平滑衔接。`Current()` 按时钟取样，到达目标时会结束过渡。`RequestFrames(true)` 让所属窗口向该画布发送 `SurfaceEvent.Frame`。要在窗体已经创建、具有句柄后启动逐帧动画。取样到终点就取消订阅。持续时间为零或系统关闭动画时，过渡可能立即完成，所以即使 `Running` 为假也应请求一次重绘。
- **测试。** 验证动画时间时，可在测试用的过渡对象上设置 `RespectSystemMotion = false`，用 `StartAt(target, startMs)` 和 `Sample(nowMs)` 传入明确的毫秒时间戳。`Snap(value)` 会直接定位到指定值并停止过渡。

### 脱离 Studio 运行两个控件

把两个控件类和下面的 `Main.kn` 放在同一目录，在带有 IO.UI 2.0 的 Windows SDK 中运行 `kinal run Main.kn`。Kinal 会发现同一 Unit 的其他文件。按下文使用设计器时，沿用 Studio Desktop 项目已有的入口。

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

## 用面板组合现有控件

如果子项需要保留各自的焦点、编辑和事件，可以继承容器。下面把输入框和按钮组合成一个可复用的行控件。窗体通过 `Value` 读写文本，通过 `BindApply` 注册回调。注册委托给内部按钮，所以回调的 `sender` 是实际按钮对象。

保存为 `AddressEntry.kn` 后执行 `kinal run AddressEntry.kn`。输入内容后按 Tab 移到 **Apply**，再按空格提交。调节窗口宽度时，输入框会随之伸缩。

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

构造函数初始化了继承的子控件列表、事件列表和网格轨道数组。子控件继续由容器的常规布局管理，添加和移除规则与其他面板相同。这个例子在代码中组合子控件。Studio 将注册后的复合控件视为一项，面板中显示所选预览类型的属性和事件。类内部创建的子控件由类自己管理，也由类的公开接口提供给窗体使用。

## 在 Kinal Studio 中注册自定义控件

```text
// @studio-control <class> <preview-kind> <toolbox label>
```

把注册注释放在项目源码集内 `.kn` 文件的第一行。Desktop 窗体生成器使用 `Unit StudioForm;`，自定义类也放在同一个 Unit，声明为公开类，并提供签名为 `(string text, int x, int y, int width, int height)` 的构造函数。工具箱显示名称可以包含空格。

预览类型决定设计器画出的替身，以及属性和事件面板提供的条目。`AccentButton Button` 使用按钮预览并提供点击绑定。`LevelMeter Label` 使用标签作占位。运行窗体时，Studio 会生成真正的 `AccentButton`、`LevelMeter` 实例，执行自定义绘制和交互。选择预览类型时，应确保实际控件支持设计器要写入的属性和要绑定的事件。`SetLevel` 这样的自定义方法或字段需要从代码访问。

1. 保存两个类文件，在 Studio 中打开项目。自定义控件出现在工具箱分割线下方。
2. 把 `LevelMeter` 和 `AccentButton` 拖进窗体，分别命名为 `LevelMeter1` 和 `ResetButton`。把按钮文本设为 **Reset**，数值条文本设为 **Volume**。
3. 在按钮的事件行将点击事件绑定到 `ResetLevel`。可以点闪电按钮生成事件处理函数，再填入下面的函数体。也可以手工添加完整的方法。

```kinal
// Add this method inside MainForm in src/Main.kn.
Public Function void ResetLevel()
{
    LevelMeter1.SetLevel(40);
}
```

保存并运行窗体。拖动数值条，再点击 **Reset**。事件处理函数通过当前 `MainForm` 实例上的 `LevelMeter1` 字段操作控件。

`MainForm.kui` 保存布局。构建前，Studio 生成 `src/MainForm.Designer.kn`，其中包含具体类型的字段、控件构造和事件绑定。业务逻辑写在 `src/Main.kn`，自定义类单独保存。工具箱找不到控件时，检查首行注释和项目的源码集。设计器能显示占位元素但构建失败时，核对类名、构造函数和事件签名。

## 绘制与输入速查

| API | 用法 |
|---|---|
| `g.Box(x, y, w, h, color, radius, stroke)` | `stroke = 0` 绘制填充圆角矩形，正数绘制轮廓。 |
| `g.Line(...)` / `g.Triangle(...)` | 绘制线段和填充三角形。 |
| `g.Gradient(x, y, w, h, from, to, vertical)` | 默认从左到右渐变，`vertical = true` 时从上到下。 |
| `g.Write(...)` | 控制字号、字重、等宽字体，以及 `TextAlignment.Start`、`Center`、`End` 对齐。 |
| `g.View(x, y, zoom)` | 以当前绘图上下文为基准创建带平移的子上下文。缩放为百分比，100 表示原尺寸。 |
| `g.PushClip(...)` / `g.PopClip()` | 限制绘制区域。在同一次绘制中成对调用。 |
| `MeasureWidth` / `MeasureHeight` | 可以重写，向父布局提供期望尺寸。绘制使用最终分配的实际尺寸。 |
| `SupportsEventCode(int code)` | 子类要通过 `AddEvent` 开放额外的现有 `EventKind` 时，可以重写此方法。保留基类所支持的事件，并在对应输入到来时触发事件。 |

自绘控件可以用 `StyleFill(normal, hover, pressed)`、`StyleText(normal)`、`StyleBorder(normal)`、`StyleRadius(normal)` 读取最终样式，使属性面板和代码设置的颜色继续生效。绘图颜色使用 `0xAARRGGBB`。渲染时控件及其子树会裁剪在控件边界内。在 `Draw` 中发出绘图命令，外部状态变化后调用 `Invalidate()`。应避免在绘制和输入回调中执行会阻塞的磁盘、网络等操作。

- [IO.UI 布局、控件、主题与三维视图](ui.md)
- [IO.UI 2.0 API、SurfaceInput 参数与文字命中测试](ui-reference.md)
