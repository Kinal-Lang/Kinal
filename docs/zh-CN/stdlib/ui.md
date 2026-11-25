# IO.UI

IO.UI 2.0 用 Kinal 编写 Windows 桌面窗体。每个窗口拥有一个 Direct3D 11 绘图表面，控件组成一棵树，由 Direct2D 和 DirectWrite 绘制。窗体、布局和事件代码可以编译为托管环境下的 Native 可执行文件，也可以由配套的 Windows 桌面版 KinalVM 执行。

新项目可以在 `Packages.Pins` 列表中固定 `IO.UI@2.0.0`。Windows x64 SDK 同时装有两个版本时，未固定版本的构建默认选择 2.0。需要把每个子控件的窗口句柄传给 Win32 的旧 Native 项目，请固定 `IO.UI@1.0.0`。在 2.0 中，`component.HostHandle` 指向控件所属的**窗体**。本页覆盖 Windows x64 桌面环境。

要制作能在多个窗体中复用的组件，可直接阅读[自定义控件与 Studio 设计器指南](ui-custom-controls.md)。

## 用一个文件写出可调整大小的窗体

把代码保存为 `Greeting.kn`。输入名字并点击 **Greet**，弹窗会显示问候语。拉宽窗口时，输入框获得多出的宽度，按钮保持 96 个布局单位宽。

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

在 Windows 桌面 SDK 上可以直接运行源码，也可以先生成字节码再运行。

```powershell
kinal run Greeting.kn
kinal vm build Greeting.kn -o Greeting.knc
kinal vm run Greeting.knc
```

`UseClientSize = true` 将 440 × 72 作为初始**客户区**大小。`Show()` 创建并显示窗口，`Run()` 还会进入消息循环。需要两个窗体时，可对第一个调用 `Show()`，对第二个调用 `Run()`。最后一个窗体关闭后，消息循环退出。耗时的点击处理函数会推迟输入和重绘。模态对话框打开期间仍可能分发窗口消息，原处理函数此时尚未返回。

## 布局与控件归属

`Width` 和 `Height` 表示期望尺寸。布局结束后，`ActualX`、`ActualY`、`ActualWidth`、`ActualHeight` 才是实际分配的边界，使用相对于父容器的逻辑单位。绘制控件时读取实际宽高。要让布局随客户区调整，在窗口的顶层 `Panel` 上设置 `FillWindow = true`。不同布局可以通过嵌套面板组合。

| `LayoutMode` | 子控件如何排列 |
|---|---|
| `Absolute` | 按子控件的 `X`、`Y` 定位，坐标从面板内边距起算，再加上子控件的外边距。 |
| `Row` / `Column` | 按添加顺序横排或竖排。`Grow = 0` 的控件占用测量尺寸。扣除这些控件、间隔和外边距后，按正的 `Grow` 权重分配主轴余量。交叉轴在尺寸约束内撑满。 |
| `Wrap` | 按测量宽度从左到右排列，下一个控件连同外边距放不下时换行。 |
| `Grid` | 用从零开始的 `Row`、`Column` 指定单元格，用 `RowSpan`、`ColumnSpan` 跨越相邻轨道。 |
| `Overlay` | 每个子控件占用扣除外边距后的面板内部区域。后添加的控件绘制在上层，指针命中测试也优先检查它。 |

`Padding` 为面板四边留白，`Margin` 为单个子控件四边留白，`Gap` 控制相邻控件或网格轨道的间隔。在任一布局中给子控件设置 `AbsolutePosition = true`，就能让它退出常规排列，独立使用 `X`、`Y` 定位。使用 `MinWidth`、`MaxWidth`、`MinHeight`、`MaxHeight` 约束尺寸。最大值为零表示该方向不限上限。

### 固定标签列，让输入框随窗口伸缩

在上例的 `Main` 中创建 `window` 后，用这段代码替换原来的行布局。`Fixed` 预留固定尺寸。`Auto` 取该轨道上单轨控件的最大测量尺寸，包含外边距，跨轨控件不参与这一步。`Fraction` 按权重分配余量。示例中，输入框决定第一行的高度，状态文字独占第二行并横跨两列。

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

`Add` 将控件挂到一个容器。移动控件时，先对原父容器调用 `Remove(child)`，再对新容器调用 `Add(child)`。移除会清理子树持有的焦点、指针捕获和动画帧订阅。`Children`、`Components`、`Events` 返回快照，修改控件树要用容器方法。给控件设置 `Name` 后，可通过 `window.FindControl(name)` 递归查找。

测试布局时可以在开窗前调用 `window.Arrange(440, 72)`，随后读取实际边界或用 `HitTest(x, y)` 检查某个位置命中了谁。制作预览时，`DrawInto(context)` 可以把排好的控件树画进已有的绘图上下文。

## 常用控件与变更通知

| 控件 | 常用操作 | 可订阅的事件 |
|---|---|---|
| `Label` / `Button` | 修改 `Text` 更新标题。按钮支持指针点击、回车和空格激活。 | 按钮点击用 `UI.EventKind.OnClick` |
| `Input` | 用 `PlaceholderText` 设置占位文本，用 `ReadOnly`、`Password`、`MaxLength` 控制输入。值从 `Text` 读取。密码模式遮罩显示，并阻止复制选中的文字。 | 用户编辑触发 `OnTextChanged`，另有 `OnKeyDown`、`OnKeyUp`、`OnFocus`、`OnBlur` |
| `CheckBox` / `RadioButton` | 通过 `Checked` 读写选中状态。同一父容器下，`GroupName` 相同的单选按钮互斥。 | 状态改变用 `OnCheckedChanged`，激活用 `OnClick` |
| `ComboBox` | 用 `ClearItems()` 和 `AddItem(text)` 装载选项。`SelectedIndex` 从零开始，取 `Items` 元素前要检查索引范围。 | 用户选择触发 `OnSelectedChanged` |
| `ProgressBar` | 有准确进度时设置 `Minimum`、`Maximum`、`Value`。按步推进时设置 `Step` 并调用 `PerformStep()`。 | 由执行业务操作的代码更新进度。 |
| `GroupBox` / `Panel` | 带标题的分组按子控件自己的位置排列。需要行列或网格时使用 `Panel`。 | 事件通常绑定在组内可交互的控件上。 |
| `TemplateButton` | 用 `Enabled` 和 `ButtonTemplate` 设置禁用、悬停、按下和焦点外观。 | `OnClick`、指针进入／离开／按下／松开，以及焦点事件 |

程序赋值和用户输入的通知时机有所区别。例如，给 `input.Text` 或 `combo.SelectedIndex` 赋值会触发重绘。加载窗体数据后，如果还需要刷新其他字段，请显式调用更新函数。

### 事件的生命周期、焦点和关闭

`AddEvent` 接受一个 `UI.Event` 对象。如果以后要取消订阅，请保留该对象，传给 `RemoveEvent`。为运行时选出的控件绑定事件前，可用 `SupportsEvent(kind)` 查询。注册不支持的事件会抛出异常。回调中可用 `sender Is UI.Button button` 取得具体类型的控件。`Tab` 和 `Shift+Tab` 按控件树顺序切换可聚焦的控件，也可通过 `window.FocusControl(input)` 主动设置焦点。

需要窗口已经显示时用 `OnShown`，客户区大小改变时用 `OnResize`，关闭前的确认用 `OnClosing`，窗口销毁后的清理用 `OnClose`。在 `OnClosing` 回调中设置 `args.Handled = true` 会保留窗口。消息提示可用 `MessageBox.ShowInfo` 或 `ShowError`。需要用户作决定时，用 `MessageBox.Show` 搭配 `MessageBoxButtons.YesNo` 和 `MessageBoxIcon.Question`，Windows 的“是”返回 `6`。

## 为窗口和控件设置主题

把下面的代码插在问候示例的 `window.Run()` 之前，就能得到 Catppuccin Mocha 配色。`Canvas` 控制窗体背景，`Field` 用于输入区域，`Subtle` 用于占位符等次要文字。`Accent`、`OnAccent`、`Caret`、`Selection` 分别处理强调色、强调色上的文字、插入光标和选区。组合框的弹出列表也使用该控件的有效调色板。`DarkTitle` 应在创建窗口之前设置。

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

调色板和绘图接口使用 `0xAARRGGBB` 顺序的 `u32`，单个样式属性则接收 `UI.Color`。控件可以通过 `BackColor`、`ForeColor`、`BorderColor` 和对应的 `Hover…`、`Pressed…` 属性分别设置常态、悬停和按下时的填充、文字和边框。`BorderWidth` 控制边框粗细，`Font = UI.Font.FromSize(16, 600)` 设置字号和字重。未显式设置的文字颜色或字体可以继承自父容器或窗口。如果在窗体显示后直接修改调色板字段，请调用 `window.Invalidate()` 使新配色生效。

### 用按钮模板设置动画

`TemplateButton.Template` 提供 `Background`、`HoverBackground`、`PressedBackground`、`DisabledBackground`、`Foreground`、`DisabledForeground`、`Border` 和 `FocusRing`。`Duration` 和 `Delay` 的单位均为毫秒。`Duration = 0` 时立即切换状态。需要替换按钮形状或内容时，可重写模板的 `Draw(context, button, fill)`，其中 `fill` 是当前帧插值得到的填充色。

将问候示例中创建 `Button` 的那一行替换为下面的代码，保留原来的事件绑定。如果也应用了上面的调色板，可以省略给 `greet` 单独设置颜色和圆角的几行，让模板提供这些样式。

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

操作进行中，可通过 `greet.Enabled = false` 显示禁用外观并阻止激活，结束后再恢复。

## Viewport3D 三维视图

在能容纳 320 × 200 视图的窗口中，可以这样添加一个三角形。

```kinal
UI.Viewport3D view = New UI.Viewport3D("Mesh", 16, 16, 320, 200);
f64[] points = { -1.0,-1.0,0.0, 1.0,-1.0,0.0, 0.0,1.0,0.0 };
int[] indices = { 0, 1, 2 };
u32[] colors = { 0xff89dceb };
view.SetMesh(points, indices, colors);
view.ShowEdges = true;
window.Add(view);
```

`points` 按 XYZ 三元组存放顶点坐标，`indices` 每三个从零开始的顶点索引组成一个三角形，`colors` 为每个面提供一个 ARGB 颜色。`SetMesh` 在替换当前网格前检查数组长度、坐标、几何范围和索引边界，随后将有效模型居中并归一化。`TriangleCount` 返回已加载的面数。`ResetMesh()` 恢复内置模型。相机可以单独通过 `ResetCamera()` 复位。

左键拖动旋转，中键拖动平移，滚轮缩放。视图获得焦点后，方向键旋转，Home 复位。双击也可复位。代码中可以设置 `Yaw`、`Pitch`、`PanX`、`PanY` 和 `Zoom`（范围 35～240，初始值 100）。`Orthographic` 切换正交投影，`Wireframe` 切换线框绘制，`ShowEdges` 控制填充面上的轮廓。

## 可停靠工具面板

`DockLayout` 保存面板标识、位置和活动标签。例如，在新建的 `UI.DockLayout` 对象 `docks` 上调用 `int log = docks.Add("log", "输出", UI.DockArea.Bottom);` 会返回面板索引。保留这个索引即可调用 `Activate(log)` 或 `Move(log, UI.DockArea.Floating)`。`Pane(log)` 提供标题、当前 `Area`、上一个停靠位置 `Home` 和浮动矩形。重新停靠可用 `docks.Move(log, docks.Pane(log).Home)`。`Active(area)` 返回区域当前活动的面板索引，空区域返回 `-1`。为每个面板分配唯一 ID，调整位置后仍可用 `Find(id)` 查找。

宿主应用负责绘制标签、创建浮动窗口，并按需保存面板位置。Kinal Studio 的工具面板使用了这个模型。

## 构建和分发窗体

要用同一份源码生成 Native 与桌面 VM 产物，把开头的例子放在 `src/Greeting.kn`，并把下面的配置保存为 `kinal.knproj`。两个配置分别指定后端和输出路径。`HideConsole` 让 Native 构建生成图形界面应用。

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

在项目目录执行以下命令。最后一条把字节码与桌面运行器打包为独立的可执行文件。

```powershell
kinal build --project . --profile native
kinal vm build --project . --profile vm
kinal vm run out/Greeting.knc
kinal vm pack out/Greeting.knc -o out/Greeting-vm.exe
```

使用同一套 Windows 桌面 SDK 中配套的 `kinal`、`kinalvm` 和 IO.UI 2.0 包。需要访问操作系统能力时，使用目标后端提供的接口。基于 `Canvas` 和 `DrawingContext` 的自定义控件可以在两个构建配置中复用绘制代码。

## 延伸阅读

- [自定义控件、动画与 Studio 注册](ui-custom-controls.md)
- [IO.UI 2.0 API、事件与排查手册](ui-reference.md)
- [IO.UI 1.0 Win32 签名参考](ui-legacy.md)
- [标准库概览](overview.md)
- [后端支持范围](../advanced/backend-support.md)
- [项目文件与包版本固定](../cli/project-files.md)
