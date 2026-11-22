# IO.Cat

`IO.Cat` is a small sample package that exposes the `IO.Cat.Rommy` class.

## Import

```kinal
Get IO.Cat;
```

## Rommy

`IO.Cat.Rommy` is a light example class with a guarded constructor and a few text-returning interaction methods.

### Constructor

```kinal
IO.Cat.Rommy rommy = New IO.Cat.Rommy(true, true);
```

If either constructor argument is `false`, the package throws `IO.Error`.

### Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `Speak(something)` | `string → string` | Returns a response string |
| `Touch()` | `→ string` | Returns a short interaction string |
| `Feed()` | `→ string` | Returns a short interaction string |
| `Play()` | `→ string` | Returns a short interaction string |

## Example

```kinal
Unit App.CatDemo;

Get Console By IO.Console;
Get IO.Cat;

Static Function int Main()
{
    IO.Cat.Rommy rommy = New IO.Cat.Rommy(true, true);
    Console.PrintLine(rommy.Speak("hello"));
    Console.PrintLine(rommy.Touch());
    Return 0;
}
```

## Notes

- `IO.Cat` is an example package rather than a core runtime dependency.
- The constructor intentionally validates the two opt-in arguments before creating the instance.

## See Also

- [Standard Library Overview](overview.md)
- [IO.Console](console.md)

## Package Identity

The current source package version is 2020.8.31 and its Unit is IO.Cat. Package and compiler versions are independent. Rommy is a teaching class; its constructor, properties and printing methods are listed below. Other packages have their own lifetime and error contracts.

## Public Source Signatures

The tables list public fields, constructors, properties and methods from the current package source. Inherited members are listed under their declaring base type. Get/Set identify property accessors; signatures include declared defaults. Native handles and Host/Win32 helpers require their ABI/lifetime contracts; ordinary applications should start with the package high-level objects.

### IO.Cat.Rommy

| Member | Declaration | Accessors |
|--------|-------------|-----------|
| `Constructor` | `Public Constructor(bool i_know_this_is_just_a_clone_of_Rommy,bool i_will_be_good_to_Rommy)` | — |
| `Speak` | `Public Function string Speak(string something)` | — |
| `Touch` | `Public Function string Touch()` | — |
| `Feed` | `Public Function string Feed()` | — |
| `Play` | `Public Function string Play()` | — |
