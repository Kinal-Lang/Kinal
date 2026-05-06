# Argument Diagnostics

Diagnostics related to functions, methods, constructors, and named arguments.

The examples in this document are minimal reproductions of the most common way each diagnostic code is triggered. A few diagnostics that are more implementation-oriented use the closest surface-level form that user code can express.

---

## Diagnostic Code Index

| Code | Title | Default Detail |
|------|------|----------|
| `E-ARG-00001` | Argument Count | Wrong number of arguments |
| `E-ARG-00002` | Invalid Named Argument | Positional argument cannot follow named argument |
| `E-ARG-00003` | Invalid Named Argument | Named arguments are not supported for this callable |
| `E-ARG-00004` | Invalid Named Argument | Named arguments are not supported for constructors |
| `E-ARG-00005` | Duplicate Named Argument | Duplicate named argument |

---

## E-ARG-00001 — Argument Count

- Severity: Error
- Default Detail: Wrong number of arguments
- Description: The number of parameters passed in at the call site is inconsistent with the signature.

Incorrect Example:

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}

int value = Add(1);
```

Correct Example:

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}

int value = Add(1, 2);
```

Reason: The compiler found the target function after overload resolution, but the argument quantities provided by the call site still did not match.

Fix: Verify function, method or constructor signatures, complete missing parameters, remove redundant parameters, or provide default values for missing parameters.

---

## E-ARG-00002 — Invalid Named Argument

- Severity: Error
- Default Detail: Positional argument cannot follow named argument
- Description: Once you start using named parameters, subsequent arguments must continue to use named forms.

Incorrect Example:

```kinal
Function string Format(string text, int indent = 0, bool uppercase = false)
{
    Return text;
}

Format(text = "hello", true);
```

Correct Example:

```kinal
Function string Format(string text, int indent = 0, bool uppercase = false)
{
    Return text;
}

Format(text = "hello", uppercase = true);
```

Reason: Named parameters change the parameter matching rules; if you return to positional parameters later, the compiler cannot reliably determine which formal parameter each value is bound to.

Fix: Either use all positional parameters, or change all named parameters after the first named parameter.

---

## E-ARG-00003 — Invalid Named Argument

- Severity: Error
- Default Detail: Named arguments are not supported for this callable
- Description: The current callable does not support named parameter calls.

Incorrect Example:

```kinal
Function int Identity(int value)
{
    Return value;
}

int result = Identity(value = 42);
```

Correct Example:

```kinal
Function int Identity(int value)
{
    Return value;
}

int result = Identity(42);
```

Reason: Not all call targets expose parameter information that can be bound by name; some built-in calls, function objects, or specific call paths only accept positional arguments.

Fix: Use positional parameters instead; if you really need named calls, change to a normal function signature that supports named parameters.

---

## E-ARG-00004 — Invalid Named Argument

- Severity: Error
- Default Detail: Named arguments are not supported for constructors
- Description: Constructor calls currently do not accept named parameters.

Incorrect Example:

```kinal
Class Pair
{
    Public Constructor(int left, int right)
    {
    }
}

Pair p = New Pair(left = 1, right = 2);
```

Correct Example:

```kinal
Class Pair
{
    Public Constructor(int left, int right)
    {
    }
}

Pair p = New Pair(1, 2);
```

Reason: The parameter binding rules in the construction phase are different from those of ordinary functions. The current front-end does not extend named parameter distribution to the `New` calling path.

Fix: Pass in the positional parameters in the order of the constructor parameters; if you need a more readable construction method, you can provide a static factory function.

---

## E-ARG-00005 — Duplicate Named Argument

- Severity: Error
- Default Detail: Duplicate named argument
- Description: The same named parameter can appear only once in a call.

Incorrect Example:

```kinal
Function string Format(string text, int indent = 0)
{
    Return text;
}

Format(text = "hello", text = "world");
```

Correct Example:

```kinal
Function string Format(string text, int indent = 0)
{
    Return text;
}

Format(text = "hello", indent = 2);
```

Reason: Repeated assignment of the same formal parameter will make the call semantics unclear, and the compiler cannot determine which value should be used in the end.

Fix: Make sure each named parameter appears at most once; if you just want to override the default value, keep only one binding.
