# Type System Diagnostics

Diagnostics related to type inference, type matching, array types, and conversions.

The examples in this document are minimal reproductions of the most common way each diagnostic code is triggered. A few diagnostics that are more implementation-oriented use the closest surface-level form that user code can express.

---

## Diagnostic Code Index

| Code | Title | Default Detail |
|------|------|----------|
| `E-TYP-00001` | Invalid Cast | Cannot cast Object values; use Object conversions instead |
| `E-TYP-00002` | Invalid Cast | Cannot cast to Object types; use Object conversions instead |
| `E-TYP-00003` | Invalid Type | Array element name missing |
| `E-TYP-00004` | Invalid Type | Array element type unknown |
| `E-TYP-00005` | Invalid Type | Type not specified |
| `E-TYP-00006` | Invalid Type | Void not allowed here |
| `E-TYP-00007` | Invalid Type | any[] requires explicit length |
| `E-TYP-00008` | Type Inference Failed | Array element type requires initializer |
| `E-TYP-00009` | Type Inference Failed | Cannot infer element type from empty array |
| `E-TYP-00010` | Type Inference Failed | Var requires initializer |
| `E-TYP-00011` | Type Mismatch | Type mismatch |
| `E-TYP-00012` | Unsupported Type | Type not supported yet |

---

## E-TYP-00001 — Invalid Cast

- Severity: Error
- Default Detail: Cannot cast Object values; use Object conversions instead
- Description: An object hierarchy cannot be directly converted to a base type by ordinary numeric cast rules.

Incorrect Example:

```kinal
IO.Type.Object.Function fn = IO.Console.PrintLine;
int value = [int](fn);
```

Correct Example:

```kinal
IO.Type.Object.Function fn = IO.Console.PrintLine;
IO.Type.Object.Class obj = fn;
string text = obj.ToString();
```

Reason: `IO.Type.Object.*` has independent object semantics and does not apply to ordinary scalar conversions.

Fix: Use object API or object layer allowed assignment/up-transition for objects.

---

## E-TYP-00002 — Invalid Cast

- Severity: Error
- Default Detail: Cannot cast to Object types; use Object conversions instead
- Description: Arbitrary values cannot be changed directly into object schema types with ordinary casts.

Incorrect Example:

```kinal
IO.Type.Object.Class obj = [IO.Type.Object.Class](42);
```

Correct Example:

```kinal
IO.Type.Object.Function fn = IO.Console.PrintLine;
IO.Type.Object.Class obj = fn;
```

Reason: The object layer type is not the landing point of a normal cast, but is part of the runtime object system.

Fix: Assign the object type through a legal object value, rather than casting the base value directly.

---

## E-TYP-00003 — Invalid Type

- Severity: Error
- Default Detail: Array element name missing
- Description: Array declaration is missing a variable name or element name.

Incorrect Example:

```kinal
int[] = { 1, 2, 3 };
```

Correct Example:

```kinal
int[] values = { 1, 2, 3 };
```

Reason: The array declaration is still a variable declaration, and the type must be followed by a name.

Fix: Add the variable name after the array type.

---

## E-TYP-00004 — Invalid Type

- Severity: Error
- Default Detail: Array element type unknown
- Description: Array element type could not be resolved.

Incorrect Example:

```kinal
MissingType[] values = {};
```

Correct Example:

```kinal
int[] values = {};
```

Reason: Array types depend on their element type being valid first.

Fix: Define the element type first, or change it to an existing type name.

---

## E-TYP-00005 — Invalid Type

- Severity: Error
- Default Detail: Type not specified
- Description: The current location requires an explicit type, but it is not given.

Incorrect Example:

```kinal
Function Add(a, b)
{
    Return a + b;
}
```

Correct Example:

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}
```

Reason: Kinal will not fill in the type for you in all claim positions.

Fix: Fill in the type name where an explicit type is required.

---

## E-TYP-00006 — Invalid Type

- Severity: Error
- Default Detail: Void not allowed here
- Description: The current context cannot use `void` as a value type.

Incorrect Example:

```kinal
void value;
```

Correct Example:

```kinal
int value;
```

Reason: `void` means "no value", so it cannot be used to declare normal variables or positions that require actual values.

Fix: Change to a specific data type, or change the location to a declaration that no value is required.

---

## E-TYP-00007 — Invalid Type

- Severity: Error
- Default Detail: any[] requires explicit length
- Description: Uninitialized `any[]` requires an explicit length.

Incorrect Example:

```kinal
any[] values;
```

Correct Example:

```kinal
any values[2];
```

Reason: Without an element initializer, the compiler cannot infer how much storage `any[]` should allocate.

Fix: Change it to a fixed-length `any[n]`, or provide the initializer directly.

---

## E-TYP-00008 — Type Inference Failed

- Severity: Error
- Default Detail: Array element type requires initializer
- Description: Array element type inference depends on the initializer.

Incorrect Example:

```kinal
Var values[];
```

Correct Example:

```kinal
Var values[] = { 1, 2, 3 };
```

Reason: Without an initializer, `Var` has no way to know the element type of the array.

Fix: Provide an initializer for the `Var` array, or write out the element type directly.

---

## E-TYP-00009 — Type Inference Failed

- Severity: Error
- Default Detail: Cannot infer element type from empty array
- Description: An empty array literal alone is not sufficient to infer the element type.

Incorrect Example:

```kinal
Var values = {};
```

Correct Example:

```kinal
int[] values = {};
```

Reason: An empty array does not have any elements that can provide type cues.

Fix: Explicitly declare the array type.

---

## E-TYP-00010 — Type Inference Failed

- Severity: Error
- Default Detail: Var requires initializer
- Description: `Var` must be accompanied by an initializer so its type can be inferred.

Incorrect Example:

```kinal
Var value;
```

Correct Example:

```kinal
Var value = 42;
```

Reason: The type of `Var` comes entirely from the initialization expression on the right.

Fix: Provide an initializer for `Var`, or use an explicit type declaration instead.

---

## E-TYP-00011 — Type Mismatch

- Severity: Error
- Default Detail: Type mismatch
- Description: The actual type in the assignment, return, or call is incompatible with the expected type.

Incorrect Example:

```kinal
int value = "42";
```

Correct Example:

```kinal
int value = [int]("42");
```

Reason: The compiler found that the source type cannot be directly assigned to the target type when the type is checked.

Fix: Modify the expression type, adjust the target type, or explicitly do a legal conversion.

---

## E-TYP-00012 — Unsupported Type

- Severity: Error
- Default Detail: Type not supported yet
- Description: The current type form is not yet supported by the compiler.

Incorrect Example:

```kinal
// This example uses a type form that is not supported yet.
Unsupported<T> value;
```

Correct Example:

```kinal
Struct Pair
{
    int Left;
    int Right;
}

Pair value;
```

Reason: This type of orientation may be reserved in the language design, but the current implementation is not yet fully supported.

Fix: Switch to a combination of supported types at this stage, or wait for that type of capability to land.
