# Naming and Duplicate Definition Diagnostics

Problems such as duplicate declarations, scope conflicts, and type-name conflicts.

The examples in this document are minimal reproductions of the most common way each diagnostic code is triggered. A few diagnostics that are more implementation-oriented use the closest surface-level form that user code can express.

---

## Diagnostic Code Index

| Code | Title | Default Detail |
|------|------|----------|
| `E-NAM-00001` | Duplicate Class | Class already defined |
| `E-NAM-00002` | Duplicate Constructor | Constructor already defined |
| `E-NAM-00003` | Duplicate Enum | Enum already defined |
| `E-NAM-00004` | Duplicate Enum | Enum member already defined |
| `E-NAM-00005` | Duplicate Field | Field already defined |
| `E-NAM-00006` | Duplicate Function | Function already defined |
| `E-NAM-00007` | Duplicate Interface | Interface already defined |
| `E-NAM-00008` | Duplicate Interface | Interface already listed |
| `E-NAM-00009` | Duplicate Method | Interface method already defined |
| `E-NAM-00010` | Duplicate Method | Method already defined |
| `E-NAM-00011` | Duplicate Struct | Struct already defined |
| `E-NAM-00012` | Duplicate Symbol | Variable already defined in this scope |
| `E-NAM-00013` | Name Conflict | Class conflicts with enum name |
| `E-NAM-00014` | Name Conflict | Class conflicts with interface name |
| `E-NAM-00015` | Name Conflict | Class conflicts with struct name |
| `E-NAM-00016` | Name Conflict | Interface conflicts with enum name |
| `E-NAM-00017` | Name Conflict | Interface conflicts with struct name |
| `E-NAM-00018` | Name Conflict | Struct conflicts with enum name |

---

## E-NAM-00001 — Duplicate Class

- Severity: Error
- Default Detail: Class already defined
- Description: Duplicate definitions occur within the same namespace.

Incorrect Example:

```kinal
Class Demo
{
}

Class Demo
{
}
```

Correct Example:

```kinal
Class Demo
{
}

Class DemoExtra
{
}
```

Reason: Entities with the same name will cause subsequent references to resolve to multiple candidate targets, and the compiler can no longer guarantee binding stability.

Fix: Rename the duplicate entity, or change it to a legal overload with a different parameter list when overloading is really needed.

---

## E-NAM-00002 — Duplicate Constructor

- Severity: Error
- Default Detail: Constructor already defined
- Description: Duplicate definitions occur within the same namespace.

Incorrect Example:

```kinal
Class Demo
{
    Public Constructor(int value)
    {
    }

    Public Constructor(int value)
    {
    }
}
```

Correct Example:

```kinal
Class Demo
{
    Public Constructor(int value)
    {
    }

    Public Constructor(string text)
    {
    }
}
```

Reason: Entities with the same name will cause subsequent references to resolve to multiple candidate targets, and the compiler can no longer guarantee binding stability.

Fix: Rename the duplicate entity, or change it to a legal overload with a different parameter list when overloading is really needed.

---

## E-NAM-00003 — Duplicate Enum

- Severity: Error
- Default Detail: Enum already defined
- Description: Duplicate definitions occur within the same namespace.

Incorrect Example:

```kinal
Enum State
{
    Ready
}

Enum State
{
    Done
}
```

Correct Example:

```kinal
Enum State
{
    Ready
}

Enum Phase
{
    Done
}
```

Reason: Entities with the same name will cause subsequent references to resolve to multiple candidate targets, and the compiler can no longer guarantee binding stability.

Fix: Rename the duplicate entity, or change it to a legal overload with a different parameter list when overloading is really needed.

---

## E-NAM-00004 — Duplicate Enum

- Severity: Error
- Default Detail: Enum member already defined
- Description: Duplicate enumeration member name.

Incorrect Example:

```kinal
Enum State
{
    Ready,
    Ready
}
```

Correct Example:

```kinal
Enum State
{
    Ready,
    Done
}
```

Reason: Member names within the same enumeration must be unique, otherwise dot access cannot distinguish them.

Fix: Rename duplicate members.

---

## E-NAM-00005 — Duplicate Field

- Severity: Error
- Default Detail: Field already defined
- Description: Duplicate definitions occur within the same namespace.

Incorrect Example:

```kinal
Class Counter
{
    Public int Count;
    Public int Count;
}
```

Correct Example:

```kinal
Class Counter
{
    Public int Count;
    Public int Total;
}
```

Reason: Entities with the same name will cause subsequent references to resolve to multiple candidate targets, and the compiler can no longer guarantee binding stability.

Fix: Rename the duplicate entity, or change it to a legal overload with a different parameter list when overloading is really needed.

---

## E-NAM-00006 — Duplicate Function

- Severity: Error
- Default Detail: Function already defined
- Description: Duplicate definitions occur within the same namespace.

Incorrect Example:

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}

Function int Add(int a, int b)
{
    Return a - b;
}
```

Correct Example:

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}

Function int Sub(int a, int b)
{
    Return a - b;
}
```

Reason: Entities with the same name will cause subsequent references to resolve to multiple candidate targets, and the compiler can no longer guarantee binding stability.

Fix: Rename the duplicate entity, or change it to a legal overload with a different parameter list when overloading is really needed.

---

## E-NAM-00007 — Duplicate Interface

- Severity: Error
- Default Detail: Interface already defined
- Description: Duplicate definitions occur within the same namespace.

Incorrect Example:

```kinal
Interface IDemo
{
    Function void Run();
}

Interface IDemo
{
    Function void Stop();
}
```

Correct Example:

```kinal
Interface IRunner
{
    Function void Run();
}

Interface IStopper
{
    Function void Stop();
}
```

Reason: Entities with the same name will cause subsequent references to resolve to multiple candidate targets, and the compiler can no longer guarantee binding stability.

Fix: Rename the duplicate entity, or change it to a legal overload with a different parameter list when overloading is really needed.

---

## E-NAM-00008 — Duplicate Interface

- Severity: Error
- Default Detail: Interface already listed
- Description: The same interface is written repeatedly in the inheritance list.

Incorrect Example:

```kinal
Interface IValue
{
    Function int Value();
}

Class Counter By IValue, IValue
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
}
```

Reason: Listing the same interface repeatedly does not increase capabilities and only makes inheritance relationships redundant.

Fix: Write each interface only once in the list.

---

## E-NAM-00009 — Duplicate Method

- Severity: Error
- Default Detail: Interface method already defined
- Description: Duplicate definitions occur within the same namespace.

Incorrect Example:

```kinal
Class Counter
{
    Public Function int Value()
    {
        Return 1;
    }

    Public Function int Value()
    {
        Return 2;
    }
}
```

Correct Example:

```kinal
Class Counter
{
    Public Function int Value()
    {
        Return 1;
    }

    Public Function int ValuePlusOne()
    {
        Return 2;
    }
}
```

Reason: Entities with the same name will cause subsequent references to resolve to multiple candidate targets, and the compiler can no longer guarantee binding stability.

Fix: Rename the duplicate entity, or change it to a legal overload with a different parameter list when overloading is really needed.

---

## E-NAM-00010 — Duplicate Method

- Severity: Error
- Default Detail: Method already defined
- Description: Duplicate definitions occur within the same namespace.

Incorrect Example:

```kinal
Class Counter
{
    Public Function int Value()
    {
        Return 1;
    }

    Public Function int Value()
    {
        Return 2;
    }
}
```

Correct Example:

```kinal
Class Counter
{
    Public Function int Value()
    {
        Return 1;
    }

    Public Function int ValuePlusOne()
    {
        Return 2;
    }
}
```

Reason: Entities with the same name will cause subsequent references to resolve to multiple candidate targets, and the compiler can no longer guarantee binding stability.

Fix: Rename the duplicate entity, or change it to a legal overload with a different parameter list when overloading is really needed.

---

## E-NAM-00011 — Duplicate Struct

- Severity: Error
- Default Detail: Struct already defined
- Description: Duplicate definitions occur within the same namespace.

Incorrect Example:

```kinal
Struct Pair
{
    int Left;
}

Struct Pair
{
    int Right;
}
```

Correct Example:

```kinal
Struct Pair
{
    int Left;
}

Struct Size
{
    int Right;
}
```

Reason: Entities with the same name will cause subsequent references to resolve to multiple candidate targets, and the compiler can no longer guarantee binding stability.

Fix: Rename the duplicate entity, or change it to a legal overload with a different parameter list when overloading is really needed.

---

## E-NAM-00012 — Duplicate Symbol

- Severity: Error
- Default Detail: Variable already defined in this scope
- Description: Variables with the same name are declared repeatedly in the same scope.

Incorrect Example:

```kinal
Static Function int Main()
{
    int value = 1;
    int value = 2;
    Return value;
}
```

Correct Example:

```kinal
Static Function int Main()
{
    int left = 1;
    int right = 2;
    Return left + right;
}
```

Reason: Local variables with the same name will obscure or conflict, preventing subsequent references from being uniquely bound.

Fix: Rename variables, or combine them into a single declaration.

---

## E-NAM-00013 — Name Conflict

- Severity: Error
- Default Detail: Class conflicts with enum name
- Description: Different types of entities use the same name.

Incorrect Example:

```kinal
Class Value
{
}

Enum Value
{
    A, B
}
```

Correct Example:

```kinal
Class ValueBox
{
}

Enum ValueKind
{
    A, B
}
```

Reason: Type names share the same namespace; classes, interfaces, structures, and enumerations cannot coexist with the same name.

Fix: Rename conflicting entities to keep each public type name globally unique.

---

## E-NAM-00014 — Name Conflict

- Severity: Error
- Default Detail: Class conflicts with interface name
- Description: Different types of entities use the same name.

Incorrect Example:

```kinal
Class Value
{
}

Enum Value
{
    A, B
}
```

Correct Example:

```kinal
Class ValueBox
{
}

Enum ValueKind
{
    A, B
}
```

Reason: Type names share the same namespace; classes, interfaces, structures, and enumerations cannot coexist with the same name.

Fix: Rename conflicting entities to keep each public type name globally unique.

---

## E-NAM-00015 — Name Conflict

- Severity: Error
- Default Detail: Class conflicts with struct name
- Description: Different types of entities use the same name.

Incorrect Example:

```kinal
Class Value
{
}

Enum Value
{
    A, B
}
```

Correct Example:

```kinal
Class ValueBox
{
}

Enum ValueKind
{
    A, B
}
```

Reason: Type names share the same namespace; classes, interfaces, structures, and enumerations cannot coexist with the same name.

Fix: Rename conflicting entities to keep each public type name globally unique.

---

## E-NAM-00016 — Name Conflict

- Severity: Error
- Default Detail: Interface conflicts with enum name
- Description: Different types of entities use the same name.

Incorrect Example:

```kinal
Class Value
{
}

Enum Value
{
    A, B
}
```

Correct Example:

```kinal
Class ValueBox
{
}

Enum ValueKind
{
    A, B
}
```

Reason: Type names share the same namespace; classes, interfaces, structures, and enumerations cannot coexist with the same name.

Fix: Rename conflicting entities to keep each public type name globally unique.

---

## E-NAM-00017 — Name Conflict

- Severity: Error
- Default Detail: Interface conflicts with struct name
- Description: Different types of entities use the same name.

Incorrect Example:

```kinal
Class Value
{
}

Enum Value
{
    A, B
}
```

Correct Example:

```kinal
Class ValueBox
{
}

Enum ValueKind
{
    A, B
}
```

Reason: Type names share the same namespace; classes, interfaces, structures, and enumerations cannot coexist with the same name.

Fix: Rename conflicting entities to keep each public type name globally unique.

---

## E-NAM-00018 — Name Conflict

- Severity: Error
- Default Detail: Struct conflicts with enum name
- Description: Different types of entities use the same name.

Incorrect Example:

```kinal
Class Value
{
}

Enum Value
{
    A, B
}
```

Correct Example:

```kinal
Class ValueBox
{
}

Enum ValueKind
{
    A, B
}
```

Reason: Type names share the same namespace; classes, interfaces, structures, and enumerations cannot coexist with the same name.

Fix: Rename conflicting entities to keep each public type name globally unique.
