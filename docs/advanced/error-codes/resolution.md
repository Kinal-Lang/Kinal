# Name Resolution Diagnostics

Triggered when symbols, types, members, or referenced targets cannot be found.

The examples in this document are minimal reproductions of the most common way each diagnostic code is triggered. A few diagnostics that are more implementation-oriented use the closest surface-level form that user code can express.

---

## Diagnostic Code Index

| Code | Title | Default Detail |
|------|------|----------|
| `E-RES-00001` | Ambiguous Function | Multiple functions match; use qualified name |
| `E-RES-00002` | Unknown Base | Base class not found |
| `E-RES-00003` | Unknown Base | Base class or interface not found |
| `E-RES-00004` | Unknown Enum Member | Enum member not found |
| `E-RES-00005` | Unknown Expression | Unsupported expression |
| `E-RES-00006` | Unknown Extern Binding | Supported bindings are C and System |
| `E-RES-00007` | Unknown Field | Field not found |
| `E-RES-00008` | Unknown Function | Builtin function reference not found |
| `E-RES-00009` | Unknown Function | Function not found |
| `E-RES-00010` | Unknown Interface | Interface not found |
| `E-RES-00011` | Unknown Method | Interface method not found |
| `E-RES-00012` | Unknown Method | Method not found |
| `E-RES-00013` | Unknown Method | Static method not found |
| `E-RES-00014` | Unknown Specifier | Unsupported struct specifier |
| `E-RES-00015` | Unknown Symbol | Symbol not found in module |
| `E-RES-00016` | Unknown Type | Cast target type is unknown |
| `E-RES-00017` | Unknown Type | Class not found |
| `E-RES-00018` | Unknown Type | Class or interface not found |
| `E-RES-00019` | Unknown Type | Enum not found |
| `E-RES-00020` | Unknown Type | Struct not found |
| `E-RES-00021` | Unknown Variable | Assignment target not found |
| `E-RES-00022` | Unknown Variable | Variable not defined |
| `E-RES-00023` | Unknown Parameter | Unknown named argument |

---

## E-RES-00001 — Ambiguous Function

- Severity: Error
- Default Detail: Multiple functions match; use qualified name
- Description: The current call site matches multiple equally feasible function candidates.

Incorrect Example:

```kinal
Get A By App.MathA;
Get B By App.MathB;

int value = Add(1, 2);
```

Correct Example:

```kinal
Get A By App.MathA;
Get B By App.MathB;

int value = A.Add(1, 2);
```

Reason: There are multiple candidate functions with the same name and the same adaptation in the current scope, and the compiler cannot arbitrarily choose one for you.

Fix: Give a module qualified name, an alias qualified name, or adjust the argument type to make the target unique.

---

## E-RES-00002 — Unknown Base

- Severity: Error
- Default Detail: Base class not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
Class Derived By MissingBase
{
}
```

Correct Example:

```kinal
Class BaseType
{
}

Class Derived By BaseType
{
}
```

Reason: The base class or interface referenced by the inheritance list has not been defined, or the name is written incorrectly.

Fix: Define the base class/interface first, or change it to an existing type name.

---

## E-RES-00003 — Unknown Base

- Severity: Error
- Default Detail: Base class or interface not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
Class Derived By MissingBase
{
}
```

Correct Example:

```kinal
Class BaseType
{
}

Class Derived By BaseType
{
}
```

Reason: The base class or interface referenced by the inheritance list has not been defined, or the name is written incorrectly.

Fix: Define the base class/interface first, or change it to an existing type name.

---

## E-RES-00004 — Unknown Enum Member

- Severity: Error
- Default Detail: Enum member not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
Enum State
{
    Ready
}

State value = State.Done;
```

Correct Example:

```kinal
Enum State
{
    Ready,
    Done
}

State value = State.Done;
```

Reason: The enumeration member to the right of the dot is not in the enumeration's member table.

Fix: Check the enumeration name and member name, and fill in missing members if necessary.

---

## E-RES-00005 — Unknown Expression

- Severity: Error
- Default Detail: Unsupported expression
- Description: The current expression form is not an expression type supported by the compiler.

Incorrect Example:

```kinal
Var pair = (1, 2);
```

Correct Example:

```kinal
Struct Pair
{
    int Left;
    int Right;
}

Pair pair;
pair.Left = 1;
pair.Right = 2;
```

Reason: Kinal does not define everything that looks like an "expression" as legal syntax; some conventions in the language do not exist here.

Fix: Use Kinal's defined expressions or data structures instead.

---

## E-RES-00006 — Unknown Extern Binding

- Severity: Error
- Default Detail: Supported bindings are C and System
- Description: `Extern` currently only supports the built-in binding names `C` and `System`.

Incorrect Example:

```kinal
Extern Function int puts(string text) By Native;
```

Correct Example:

```kinal
Extern Function int puts(string text) By C;
```

Reason: The compiler only knows a fixed set of external binding backends; unregistered binding keywords cannot participate in ABI selection.

Fix: Change the binding name to `C` or `System`, or first extend the compiler's support for the new binding.

---

## E-RES-00007 — Unknown Field

- Severity: Error
- Default Detail: Field not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
Struct Pair
{
    int Left;
}

Pair p;
int value = p.Right;
```

Correct Example:

```kinal
Struct Pair
{
    int Left;
    int Right;
}

Pair p;
int value = p.Right;
```

Reason: The type exists, but there is no corresponding field in the member table.

Fix: Change it to a defined field, or add the field in the type definition.

---

## E-RES-00008 — Unknown Function

- Severity: Error
- Default Detail: Builtin function reference not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
IO.Type.Object.Function fn = IO.Console.PrintLn;
```

Correct Example:

```kinal
IO.Type.Object.Function fn = IO.Console.PrintLine;
```

Reason: Built-in function reference uses wrong target name.

Fix: Check the real names of standard library built-in functions.

---

## E-RES-00009 — Unknown Function

- Severity: Error
- Default Detail: Function not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
int value = MakeUser();
```

Correct Example:

```kinal
Function int MakeUser()
{
    Return 1;
}

int value = MakeUser();
```

Reason: There are no bindable function declarations in the current scope.

Fix: Define the function first, import the correct module, or fix the spelling.

---

## E-RES-00010 — Unknown Interface

- Severity: Error
- Default Detail: Interface not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
Class User By IMissing
{
}
```

Correct Example:

```kinal
Interface IUser
{
    Function void Run();
}

Class User By IUser
{
    Public Function void Run()
    {
    }
}
```

Reason: The interface in the inheritance list does not exist.

Fix: Define the interface first, or quote the correct interface name.

---

## E-RES-00011 — Unknown Method

- Severity: Error
- Default Detail: Interface method not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
Interface IUser
{
    Function void Run();
}

Class User By IUser
{
    Public Function void Start()
    {
    }
}
```

Correct Example:

```kinal
Interface IUser
{
    Function void Run();
}

Class User By IUser
{
    Public Function void Run()
    {
    }
}
```

Reason: The method name provided by the implementation class is inconsistent with the interface contract.

Fix: Change the implementation method to the method name and signature declared in the interface.

---

## E-RES-00012 — Unknown Method

- Severity: Error
- Default Detail: Method not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
Class User
{
    Public Function void Run()
    {
    }
}

User u = New User();
u.Start();
```

Correct Example:

```kinal
Class User
{
    Public Function void Run()
    {
    }
}

User u = New User();
u.Run();
```

Reason: The instance type exists, but the requested method does not exist on the instance.

Fix: Use the real method name instead, or supplement the method definition.

---

## E-RES-00013 — Unknown Method

- Severity: Error
- Default Detail: Static method not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
Class Math
{
    Public Static Function int Add(int a, int b)
    {
        Return a + b;
    }
}

int value = Math.Sum(1, 2);
```

Correct Example:

```kinal
Class Math
{
    Public Static Function int Add(int a, int b)
    {
        Return a + b;
    }
}

int value = Math.Add(1, 2);
```

Reason: The static method search successfully locates the type, but the specific method name does not exist.

Fix: Check static member names.

---

## E-RES-00014 — Unknown Specifier

- Severity: Error
- Default Detail: Unsupported struct specifier
- Description: Struct decorators currently only accept supported built-in specifiers.

Incorrect Example:

```kinal
Struct Packet By Packed(2)
{
    int Value;
}
```

Correct Example:

```kinal
Struct Packet By Align(8)
{
    int Value;
}
```

Reason: The structure specifier does not work with any identifier, the compiler only accepts fixed sets.

Fix: Change to a struct specifier explicitly supported in the documentation, such as `Align(...)`.

---

## E-RES-00015 — Unknown Symbol

- Severity: Error
- Default Detail: Symbol not found in module
- Description: The module import is successful, but the target symbol itself does not exist.

Incorrect Example:

```kinal
Get PrintLn By IO.Console.PrintLn;
```

Correct Example:

```kinal
Get PrintLine By IO.Console.PrintLine;
```

Reason: After successful module path resolution, the compiler continues to look for member names; it will fail at this level if it is misspelled or if the target does not exist.

Fix: Check the fully qualified name and member spelling.

---

## E-RES-00016 — Unknown Type

- Severity: Error
- Default Detail: Cast target type is unknown
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
Var value = [MissingType](1);
```

Correct Example:

```kinal
int value = [int](1);
```

Reason: Explicit conversion target type itself does not exist.

Fix: Change it to an existing type name.

---

## E-RES-00017 — Unknown Type

- Severity: Error
- Default Detail: Class not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
MissingType value;
```

Correct Example:

```kinal
Class User
{
}

User value;
```

Reason: A non-existent class is referenced.

Fix: Define the class first, or correct the class name.

---

## E-RES-00018 — Unknown Type

- Severity: Error
- Default Detail: Class or interface not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
Class Derived By MissingBase
{
}
```

Correct Example:

```kinal
Interface IValue
{
    Function int Value();
}

Class Derived By IValue
{
    Public Function int Value()
    {
        Return 1;
    }
}
```

Reason: The entity required here can only be a class or interface, but the lookup failed.

Fix: Use an existing class or interface name instead.

---

## E-RES-00019 — Unknown Type

- Severity: Error
- Default Detail: Enum not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
MissingEnum value;
```

Correct Example:

```kinal
Enum State
{
    Ready
}

State value;
```

Reason: A reference to an enumeration type that does not exist.

Fix: Define the enumeration or fix the type name.

---

## E-RES-00020 — Unknown Type

- Severity: Error
- Default Detail: Struct not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
MissingStruct value;
```

Correct Example:

```kinal
Struct Pair
{
    int Left;
}

Pair value;
```

Reason: A non-existent structure type is referenced.

Fix: Define the structure or fix the type name.

---

## E-RES-00021 — Unknown Variable

- Severity: Error
- Default Detail: Assignment target not found
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
missing = 1;
```

Correct Example:

```kinal
int value = 0;
value = 1;
```

Reason: The left side of the assignment statement is not bound to an existing variable.

Fix: Declare the variable first, then write the assignment.

---

## E-RES-00022 — Unknown Variable

- Severity: Error
- Default Detail: Variable not defined
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
int value = missing;
```

Correct Example:

```kinal
int missing = 1;
int value = missing;
```

Reason: A variable that does not exist in the current scope was read.

Fix: Declare the variable first, or correct the reference name.

---

## E-RES-00023 — Unknown Parameter

- Severity: Error
- Default Detail: Unknown named argument
- Description: The current reference target does not exist within visible range.

Incorrect Example:

```kinal
Function string Format(string text, int indent = 0)
{
    Return text;
}

Format(text = "hello", depth = 2);
```

Correct Example:

```kinal
Function string Format(string text, int indent = 0)
{
    Return text;
}

Format(text = "hello", indent = 2);
```

Reason: The named parameter name does not match any formal parameter.

Fix: Check the spelling of named parameters and only use actual parameter names.
