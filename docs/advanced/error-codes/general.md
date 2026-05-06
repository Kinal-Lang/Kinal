# General and Generic Diagnostics

General rules covering constructors, constants, array length, generic dispatch, and similar behavior.

The examples in this document are minimal reproductions of the most common way each diagnostic code is triggered. A few diagnostics that are more implementation-oriented use the closest surface-level form that user code can express.

---

## Diagnostic Code Index

| Code | Title | Default Detail |
|------|------|----------|
| `E-GEN-00001` | Abstract Method | Class must implement abstract base method |
| `E-GEN-00002` | Array Length | Initializer has too many elements |
| `E-GEN-00003` | Byte Overflow | byte range is 0..255 |
| `E-GEN-00004` | Const Assignment | Cannot assign to const variable |
| `E-GEN-00005` | Const Assignment | Cannot modify const variable |
| `E-GEN-00006` | Const Requires Init | Const variable must have initializer |
| `E-GEN-00007` | Constructor | Constructor not found |
| `E-GEN-00008` | Conversion Syntax Changed | Type conversion now use [type](value) syntax instead of type(value). For example: [int]("123") instead of int("123") |
| `E-GEN-00009` | Conversion Syntax Changed | Type conversion now use [type](value) syntax instead of type(value).  |
| `E-GEN-00010` | Generic Arity | Wrong number of generic type arguments |
| `E-GEN-00011` | Generic Function | Failed to resolve instantiated generic function |
| `E-GEN-00012` | Generic Function | Generic call requires explicit type arguments |
| `E-GEN-00013` | Generic Function | Generic function reference requires explicit type arguments |
| `E-GEN-00014` | Interface Method | Class must implement interface method |
| `E-GEN-00015` | Interface Method | Interface implementation must be public |
| `E-GEN-00016` | Scoped Symbol Required | Imported symbol must be called with module or alias qualifier |
| `E-GEN-00017` | Unsupported Generic Method | Generic methods are not supported |
| `E-GEN-00018` | Unsupported Generic Local Function | Generic local named functions are not supported yet |
| `E-GEN-00019` | Unsupported Generic Method | Generic methods are not supported yet |

---

## E-GEN-00001 — Abstract Method

- Severity: Error
- Default Detail: Class must implement abstract base method
- Description: The derived class does not fully implement the methods required by the abstract base class.

Incorrect Example:

```kinal
Abstract Class BaseType
{
    Public Abstract Function int Value();
}

Class Derived By BaseType
{
}
```

Correct Example:

```kinal
Abstract Class BaseType
{
    Public Abstract Function int Value();
}

Class Derived By BaseType
{
    Public Override Function int Value()
    {
        Return 1;
    }
}
```

Reason: As long as the derived class remains an instantiable ordinary class, all abstract contracts of the abstract base class must be honored.

Fix: Implement the missing abstract member, or declare the derived class as `Abstract Class` as well.

---

## E-GEN-00002 — Array Length

- Severity: Error
- Default Detail: Initializer has too many elements
- Description: When initializing a fixed-length array, the number of elements cannot exceed the declared length.

Incorrect Example:

```kinal
int values[2] = { 1, 2, 3 };
```

Correct Example:

```kinal
int values[2] = { 1, 2 };
```

Reason: The length of the array is fixed in the type, and it will go out of bounds when the initializer has more elements.

Fix: Reduce the initializer elements, or change the array length to a value that can accommodate these elements.

---

## E-GEN-00003 — Byte Overflow

- Severity: Error
- Default Detail: byte range is 0..255
- Description: `byte` literals must fall within the `0..255` range.

Incorrect Example:

```kinal
byte value = 300;
```

Correct Example:

```kinal
byte value = 255;
```

Reason: `byte` is only 8 bits, and values out of range cannot be represented losslessly.

Fix: Change it to a value within the legal range, or use a larger integer type.

---

## E-GEN-00004 — Const Assignment

- Severity: Error
- Default Detail: Cannot assign to const variable
- Description: `const` variables cannot be reassigned.

Incorrect Example:

```kinal
Const int limit = 10;
limit = 20;
```

Correct Example:

```kinal
Const int limit = 10;
int next = limit + 10;
```

Reason: The semantics of `const` is that no new value can be bound after a single initialization.

Fix: If the variable needs to be reassigned, change it to a normal variable; otherwise, just read it and don't write it again.

---

## E-GEN-00005 — Const Assignment

- Severity: Error
- Default Detail: Cannot modify const variable
- Description: Constant values cannot be modified through increment, decrement, or compound operations.

Incorrect Example:

```kinal
Const int step = 1;
step++;
```

Correct Example:

```kinal
Const int step = 1;
int next = step + 1;
```

Reason: This type of operation essentially writes back to the original variable, so it also violates the rules of constant immutability.

Fix: Use derived values for constants instead of modifying them in place.

---

## E-GEN-00006 — Const Requires Init

- Severity: Error
- Default Detail: Const variable must have initializer
- Description: `const` must be initialized immediately when declared.

Incorrect Example:

```kinal
Const int limit;
```

Correct Example:

```kinal
Const int limit = 10;
```

Reason: Once a constant is declared, it must have a definite value, otherwise it cannot be guaranteed to be truly immutable later.

Fix: Provide the initializer directly at the declaration.

---

## E-GEN-00007 — Constructor

- Severity: Error
- Default Detail: Constructor not found
- Description: The call to `New` found no matching constructor signature.

Incorrect Example:

```kinal
Class Counter
{
    Public Constructor(int start)
    {
    }
}

Counter c = New Counter();
```

Correct Example:

```kinal
Class Counter
{
    Public Constructor(int start)
    {
    }
}

Counter c = New Counter(1);
```

Reason: Constructor parsing is the same as ordinary function parsing. The actual parameter types, numbers or default values must match.

Fix: Pass in the correct parameters, or supplement the constructor with the corresponding signature.

---

## E-GEN-00008 — Conversion Syntax Changed

- Severity: Error
- Default Detail: Type conversion now use [type](value) syntax instead of type(value). For example: [int]("123") instead of int("123")
- Description: The old-style `type(value)` conversion syntax has been deprecated.

Incorrect Example:

```kinal
int value = int("123");
```

Correct Example:

```kinal
int value = [int]("123");
```

Reason: Kinal now unifies explicit type conversions into the `[type](expr)` form to avoid confusion with ordinary function calls.

Fix: Batch replace the old writing methods with square bracket conversion syntax.

---

## E-GEN-00009 — Conversion Syntax Changed

- Severity: Error
- Default Detail: Type conversion now use [type](value) syntax instead of type(value). 
- Description: Another compatibility diagnostic with the same meaning as the old conversion syntax change.

Incorrect Example:

```kinal
float value = float("1.5");
```

Correct Example:

```kinal
float value = [float]("1.5");
```

Reason: The old-style call conversion conflicts with function call parsing, so it has been replaced uniformly.

Fix: Always change to `[type](value)`.

---

## E-GEN-00010 — Generic Arity

- Severity: Error
- Default Detail: Wrong number of generic type arguments
- Description: The number of generic argument parameters must match the number of type parameters in the definition.

Incorrect Example:

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

int value = Identity<int, string>(1);
```

Correct Example:

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

int value = Identity<int>(1);
```

Reason: When instantiating a generic, each type parameter needs to have a one-to-one correspondence. One more or one less parameter cannot form a legal instance.

Fix: Check the generic function or type definition and pass in the exact same number of type parameters.

---

## E-GEN-00011 — Generic Function

- Severity: Error
- Default Detail: Failed to resolve instantiated generic function
- Description: The explicitly instantiated generic function did not successfully resolve the callable.

Incorrect Example:

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

int value = Identity<string>(1);
```

Correct Example:

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

int value = Identity<int>(1);
```

Reason: The instantiated signature is incompatible with the call site actual parameters, or the derivation result cannot fall into a legal implementation.

Fix: Check that the explicit type parameter and actual parameter types match, changing to the correct explicit type list if necessary.

---

## E-GEN-00012 — Generic Function

- Severity: Error
- Default Detail: Generic call requires explicit type arguments
- Description: Some generic calls cannot rely entirely on argument deduction, and type parameters must be given explicitly.

Incorrect Example:

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

Var fn = Identity;
int value = fn(1);
```

Correct Example:

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

int value = Identity<int>(1);
```

Reason: The compiler cannot reliably derive `T` when the call site lacks sufficient context.

Fix: Write `<T>` explicitly, or adjust the code so that the argument type uniquely determines the generic parameter.

---

## E-GEN-00013 — Generic Function

- Severity: Error
- Default Detail: Generic function reference requires explicit type arguments
- Description: When obtaining a reference to a generic function, you also need to explicitly instantiate it first.

Incorrect Example:

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

IO.Type.Object.Function fn = Identity;
```

Correct Example:

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

IO.Type.Object.Function fn = Identity<int>;
```

Reason: The function object must point to a concrete instance with determined parameters and return types, rather than an abstract generic template.

Fix: Explicitly write out type parameters when taking a reference.

---

## E-GEN-00014 — Interface Method

- Severity: Error
- Default Detail: Class must implement interface method
- Description: When a class implements an interface, it must complete the methods required by the interface.

Incorrect Example:

```kinal
Interface IValue
{
    Function int Value();
}

Class Counter By IValue
{
}
```

Correct Example:

```kinal
Interface IValue
{
    Function int Value();
}

Class Counter By IValue
{
    Public Function int Value()
    {
        Return 1;
    }
}
```

Reason: Interface promises are checkable compile-time contracts, and the absence of any method makes the implementation incomplete.

Fix: Provide a signature-compatible public implementation for every method in the interface.

---

## E-GEN-00015 — Interface Method

- Severity: Error
- Default Detail: Interface implementation must be public
- Description: The method implemented by the interface must be `Public`.

Incorrect Example:

```kinal
Interface IValue
{
    Function int Value();
}

Class Counter By IValue
{
    Private Function int Value()
    {
        Return 1;
    }
}
```

Correct Example:

```kinal
Interface IValue
{
    Function int Value();
}

Class Counter By IValue
{
    Public Function int Value()
    {
        Return 1;
    }
}
```

Reason: Interface methods need to be visible to the outside world, otherwise accessibility requirements cannot be met when called through the interface type.

Fix: Change the interface implementation to `Public`.

---

## E-GEN-00016 — Scoped Symbol Required

- Severity: Error
- Default Detail: Imported symbol must be called with module or alias qualifier
- Description: Symbols imported through modules or aliases must be called with qualified names.

Incorrect Example:

```kinal
Unit App.Main;

Get App.Util;

Static Function int Main(string[] args)
{
    Return Add(40, 2);
}
```

Correct Example:

```kinal
Unit App.Main;

Get U By App.Util;

Static Function int Main(string[] args)
{
    Return U.Add(40, 2);
}
```

Reason: Module imports do not flatten all members directly into the current scope; this is done to avoid cross-module name pollution.

Fix: Qualify the call target using the full module name or an alias.

---

## E-GEN-00017 — Unsupported Generic Method

- Severity: Error
- Default Detail: Generic methods are not supported
- Description: The current compiler does not support generic methods.

Incorrect Example:

```kinal
Class Box
{
    Public Function T Map<T>(T value)
    {
        Return value;
    }
}
```

Correct Example:

```kinal
Function T Map<T>(T value)
{
    Return value;
}
```

Reason: The current generic implementation only covers top-level generic functions, and method-level generics have not yet been implemented in the complete backend.

Fix: Move generic logic to top-level generic functions, or change it to non-generic instance methods.

---

## E-GEN-00018 — Unsupported Generic Local Function

- Severity: Error
- Default Detail: Generic local named functions are not supported yet
- Description: Generics are not currently supported for local named functions.

Incorrect Example:

```kinal
Static Function int Main()
{
    Function T Local<T>(T value)
    {
        Return value;
    }
    Return 0;
}
```

Correct Example:

```kinal
Function T Local<T>(T value)
{
    Return value;
}

Static Function int Main()
{
    Return 0;
}
```

Reason: The local named function itself has one more layer of closure environment than the top-level function; when generics are superimposed on top, the current implementation does not yet fully support it.

Fix: Promote generic local functions to the top level, or change them to ordinary non-generic local functions.

---

## E-GEN-00019 — Unsupported Generic Method

- Severity: Error
- Default Detail: Generic methods are not supported yet
- Description: Another more clear tip: Generic methods are not supported yet.

Incorrect Example:

```kinal
Class Box
{
    Public Function T Echo<T>(T value)
    {
        Return value;
    }
}
```

Correct Example:

```kinal
Class Box
{
    Public Function int Echo(int value)
    {
        Return value;
    }
}
```

Reason: The combination of method-level generics and virtual calls, overloading, and instance dispatch is not yet fully implemented by current compilers.

Fix: Use top-level generic functions instead, or provide plain overloaded methods for the types you need.
