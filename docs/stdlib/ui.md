# IO.UI

`IO.UI` provides a native desktop window and controls layer.

The current official package is backed by the Win32 host implementation. Existing tests treat it as a Windows-focused package.

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
