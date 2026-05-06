# Syntax Diagnostics

Triggered when keywords, separators, parentheses, identifiers, or structural boundaries are missing.

The examples in this document are minimal reproductions of the most common way each diagnostic code is triggered. A few diagnostics that are more implementation-oriented use the closest surface-level form that user code can express.

---

## Diagnostic Code Index

| Code | Title | Default Detail |
|------|------|----------|
| `E-SYN-00001` | Expected '/>' | Missing '/>' after block body |
| `E-SYN-00002` | Unexpected Token | Unexpected token in expression |
| `W-SYN-00001` | Legacy Array Syntax | Use 'Type[] name' instead of 'Type name[]' |
| `W-SYN-00002` | Legacy Constructor Syntax | Use 'Constructor(...)' instead of 'Function ClassName(...)' |
| `E-SYN-00003` | Expected '(' | Missing '(' |
| `E-SYN-00004` | Expected '(' | Missing '(' after Align |
| `E-SYN-00005` | Expected '(' | Missing '(' after Constructor |
| `E-SYN-00006` | Expected '(' | Missing '(' after If |
| `E-SYN-00007` | Expected '(' | Missing '(' after cast type |
| `E-SYN-00008` | Expected '(' | Missing '(' after class name |
| `E-SYN-00009` | Expected '(' | Missing '(' after function name |
| `E-SYN-00010` | Expected '(' | Missing '(' after function return type |
| `E-SYN-00011` | Expected '(' | Missing '(' after method name |
| `E-SYN-00012` | Expected ')' | Missing ')' |
| `E-SYN-00013` | Expected ')' | Missing ')' after Align |
| `E-SYN-00014` | Expected ')' | Missing ')' after If condition |
| `E-SYN-00015` | Expected ')' | Missing ')' after catch parameter |
| `E-SYN-00016` | Expected ')' | Missing ')' after for clause |
| `E-SYN-00017` | Expected ')' | Missing ')' after parameters |
| `E-SYN-00018` | Expected ')' | Missing ')' in attribute |
| `E-SYN-00019` | Expected ')' | Missing ')' in call |
| `E-SYN-00020` | Expected ')' | Missing ')' in cast expression |
| `E-SYN-00021` | Expected ')' | Missing ')' in constructor call |
| `E-SYN-00022` | Expected ')' | Missing ')' in member call |
| `E-SYN-00023` | Expected '/' | Missing '/>' after block body |
| `E-SYN-00024` | Expected '/' | Missing '</' before block body |
| `E-SYN-00025` | Expected ':' | Missing ':' in conditional expression |
| `E-SYN-00026` | Expected '<' | Missing '</' before block body |
| `E-SYN-00027` | Expected '<' | Missing '<' before generic type arguments |
| `E-SYN-00028` | Expected '>' | Missing '/>' after block body |
| `E-SYN-00029` | Expected '>' | Missing '>' after generic type arguments |
| `E-SYN-00030` | Expected '>' | Missing '>' after generic type parameter list |
| `E-SYN-00031` | Expected '[' | Missing '[' in cast expression |
| `E-SYN-00032` | Expected ']' | Missing ']' |
| `E-SYN-00033` | Expected ']' | Missing ']' after attribute |
| `E-SYN-00034` | Expected ']' | Missing ']' after index |
| `E-SYN-00035` | Expected ']' | Missing ']' in cast expression |
| `E-SYN-00036` | Expected '{' | Missing '{' |
| `E-SYN-00037` | Expected '{' | Missing '{' in Else expression |
| `E-SYN-00038` | Expected '{' | Missing '{' in If expression |
| `E-SYN-00039` | Expected '{' | Missing '{' in class |
| `E-SYN-00040` | Expected '{' | Missing '{' in enum |
| `E-SYN-00041` | Expected '{' | Missing '{' in interface |
| `E-SYN-00042` | Expected '{' | Missing '{' in struct |
| `E-SYN-00043` | Expected '}' | Missing '}' |
| `E-SYN-00044` | Expected '}' | Missing '}' in Else expression |
| `E-SYN-00045` | Expected '}' | Missing '}' in If expression |
| `E-SYN-00046` | Expected '}' | Missing '}' in array literal |
| `E-SYN-00047` | Expected '}' | Missing '}' in enum |
| `E-SYN-00048` | Unexpected End Of File | Unexpected end of file while parsing expression |
| `E-SYN-00049` | Unexpected End Of File | Unexpected end of file while parsing variable declaration |
| `E-SYN-00050` | Unterminated String Literal | String literal is missing a closing quote |
| `E-SYN-00051` | Expected Block | Expected 'Block' keyword |
| `E-SYN-00052` | Expected Catch | Missing 'Catch' after Try block |
| `E-SYN-00053` | Expected Class | Expected 'Class' keyword |
| `E-SYN-00054` | Expected Constructor | Expected 'Constructor' keyword |
| `E-SYN-00055` | Expected Else | If expression requires Else branch |
| `E-SYN-00056` | Expected Enum | Expected 'Enum' keyword |
| `E-SYN-00057` | Expected For | Expected 'For' keyword |
| `E-SYN-00058` | Expected Function | Expected 'Function' keyword |
| `E-SYN-00059` | Expected Get | Expected 'Get' keyword |
| `E-SYN-00060` | Expected Identifier | Expected block name |
| `E-SYN-00061` | Expected Identifier | Expected block variable name |
| `E-SYN-00062` | Expected Identifier | Expected catch variable name |
| `E-SYN-00063` | Expected Identifier | Expected class name |
| `E-SYN-00064` | Expected Identifier | Expected enum member |
| `E-SYN-00065` | Expected Identifier | Expected enum name |
| `E-SYN-00066` | Expected Identifier | Expected field name |
| `E-SYN-00067` | Expected Identifier | Expected function name |
| `E-SYN-00068` | Expected Identifier | Expected generic type parameter name |
| `E-SYN-00069` | Expected Identifier | Expected interface name |
| `E-SYN-00070` | Expected Identifier | Expected method name |
| `E-SYN-00071` | Expected Identifier | Expected name |
| `E-SYN-00072` | Expected Identifier | Expected parameter name |
| `E-SYN-00073` | Expected Identifier | Expected record name |
| `E-SYN-00074` | Expected Identifier | Expected remote name |
| `E-SYN-00075` | Expected Identifier | Expected specifier |
| `E-SYN-00076` | Expected Identifier | Expected struct name |
| `E-SYN-00077` | Expected Identifier | Expected variable name |
| `E-SYN-00078` | Expected Identifier | Extern binding requires an identifier (C/System) |
| `E-SYN-00079` | Expected If | Expected 'If' keyword |
| `E-SYN-00080` | Expected If | Expected 'If' keyword in expression |
| `E-SYN-00081` | Expected Interface | Expected 'Interface' keyword |
| `E-SYN-00082` | Expected Jump | Expected 'Jump' keyword |
| `E-SYN-00083` | Expected Number | Align requires a number |
| `E-SYN-00084` | Expected Number | Enum value requires a number |
| `E-SYN-00085` | Expected Record | Expected 'Record' keyword |
| `E-SYN-00086` | Expected Semicolon | Abstract method requires ';' |
| `E-SYN-00087` | Expected Semicolon | Extern function requires ';' |
| `E-SYN-00088` | Expected Semicolon | Missing ';' after Get |
| `E-SYN-00089` | Expected Semicolon | Missing ';' after Jump |
| `E-SYN-00090` | Expected Semicolon | Missing ';' after Throw |
| `E-SYN-00091` | Expected Semicolon | Missing ';' after Unit |
| `E-SYN-00092` | Expected Semicolon | Missing ';' after break |
| `E-SYN-00093` | Expected Semicolon | Missing ';' after continue |
| `E-SYN-00094` | Expected Semicolon | Missing ';' after declaration |
| `E-SYN-00095` | Expected Semicolon | Missing ';' after expression |
| `E-SYN-00096` | Expected Semicolon | Missing ';' after field |
| `E-SYN-00097` | Expected Semicolon | Missing ';' after for condition |
| `E-SYN-00098` | Expected Semicolon | Missing ';' after return |
| `E-SYN-00099` | Expected Semicolon | Missing ';' after variable declaration |
| `E-SYN-00100` | Expected Semicolon | Missing ';' in for initializer |
| `E-SYN-00101` | Expected Struct | Expected 'Struct' keyword |
| `E-SYN-00102` | Expected Throw | Expected 'Throw' keyword |
| `E-SYN-00103` | Expected Try | Expected 'Try' keyword |
| `E-SYN-00104` | Expected While | Expected 'While' keyword |
| `E-SYN-00105` | Expected '}' | Unexpected end of file while parsing block |
| `E-SYN-00106` | Unexpected Top-Level Token | Expected a top-level declaration (Function/Class/Struct/Enum/Interface/variable declaration) |
| `E-SYN-00107` | Invalid Global Modifier | Top-level global variables cannot use function modifiers like Static/Extern/Delegate/Safe/Trusted/Unsafe/Async |

---

## E-SYN-00001 — Expected '/>'

- Severity: Error
- Default Detail: Missing '/>' after block body
- Description: Block closing tag is incomplete.

Incorrect Example:

```kinal
Block Demo</
    Record A;
>
```

Correct Example:

```kinal
Block Demo</
    Record A;
/>
```

Reason: Block uses a pair of special boundaries `</` and `/>` to wrap the body.

Fix: Fill the end boundary with `/>`.

---

## E-SYN-00002 — Unexpected Token

- Severity: Error
- Default Detail: Unexpected token in expression
- Description: A token that the parser does not expect appears in the expression.

Incorrect Example:

```kinal
int value = +* 1;
```

Correct Example:

```kinal
int value = -1;
```

Reason: The current token sequence cannot form a legal expression.

Fix: Check whether the nearest operator, parentheses, and delimiters are misspelled.

---

## W-SYN-00001 — Legacy Array Syntax

- Severity: Warning
- Default Detail: Use 'Type[] name' instead of 'Type name[]'
- Description: Dynamic array parameters or variables still use the old `Type name[]` notation.

Incorrect Example:

```kinal
Static Function int Main(string args[])
{
    Return 0;
}
```

Correct Example:

```kinal
Static Function int Main(string[] args)
{
    Return 0;
}
```

Reason: The language has unified the writing method of dynamic array types to `Type[] name`.

Fix: Bulk change all old style `Type name[]` to `Type[] name`.

---

## W-SYN-00002 — Legacy Constructor Syntax

- Severity: Warning
- Default Detail: Use 'Constructor(...)' instead of 'Function ClassName(...)'
- Description: The old way of writing named constructors is still recognized, but is no longer recommended.

Incorrect Example:

```kinal
Class Counter
{
    Public Function Counter(int start)
    {
    }
}
```

Correct Example:

```kinal
Class Counter
{
    Public Constructor(int start)
    {
    }
}
```

Reason: The `Constructor(...)` keyword form is clearer and less likely to be confused with ordinary functions.

Fix: Change the old-style function constructor of the same name to `Constructor`.

---

## E-SYN-00003 — Expected '('

- Severity: Error
- Default Detail: Missing '('
- Description: The token sequence at the current position does not comply with the current grammar rules.

Incorrect Example:

```kinal
// This form does not produce valid syntax.
???
```

Correct Example:

```kinal
// Change it to a syntax form defined by the language.
Function int Main()
{
    Return 0;
}
```

Reason: The parser found no legal syntax in the current context on which to proceed with the reduction.

Fix: Prioritizes checking of nearest keywords, parentheses, curly braces, angle brackets, and semicolons.

---

## E-SYN-00004 — Expected '('

- Severity: Error
- Default Detail: Missing '(' after Align
- Description: Missing left bracket after `Align`.

Incorrect Example:

```kinal
Struct Pair By Align 8)
{
    int Left;
}
```

Correct Example:

```kinal
Struct Pair By Align(8)
{
    int Left;
}
```

Reason: `Align` needs to be written as `Align(n)` in functional parameter form.

Fix: Append the complete pair of parentheses after `Align`.

---

## E-SYN-00005 — Expected '('

- Severity: Error
- Default Detail: Missing '(' after Constructor
- Description: Missing left parenthesis before constructor parameter list.

Incorrect Example:

```kinal
Public Constructor int value)
{
}
```

Correct Example:

```kinal
Public Constructor(int value)
{
}
```

Reason: The parameter portion of the constructor signature must be enclosed in parentheses.

Fix: Add `(` after the constructor name.

---

## E-SYN-00006 — Expected '('

- Severity: Error
- Default Detail: Missing '(' after If
- Description: `If` Missing left parenthesis before condition.

Incorrect Example:

```kinal
If true)
{
    Return 1;
}
```

Correct Example:

```kinal
If (true)
{
    Return 1;
}
```

Reason: `If` Conditional expressions must be written within parentheses.

Fix: Add `(` and ensure that the conditional expression is complete.

---

## E-SYN-00007 — Expected '('

- Severity: Error
- Default Detail: Missing '(' after cast type
- Description: Explicit conversion is missing an opening bracket after the closing type square bracket.

Incorrect Example:

```kinal
int value = [int]42);
```

Correct Example:

```kinal
int value = [int](42);
```

Reason: Kinal's cast syntax is fixed to `[type](expr)`.

Fix: Enclose the expression to be converted in parentheses.

---

## E-SYN-00008 — Expected '('

- Severity: Error
- Default Detail: Missing '(' after class name
- Description: The constructor call is missing an opening bracket after the class name.

Incorrect Example:

```kinal
User user = New User;
```

Correct Example:

```kinal
User user = New User();
```

Reason: The parameter list for `New Type(...)` must appear explicitly.

Fix: Add parameter brackets after the class name.

---

## E-SYN-00009 — Expected '('

- Severity: Error
- Default Detail: Missing '(' after function name
- Description: The function or method call is missing an opening parenthesis.

Incorrect Example:

```kinal
PrintLine "hello");
```

Correct Example:

```kinal
PrintLine("hello");
```

Reason: Calling an expression requires enclosing the argument list in parentheses.

Fix: Add `(` after the transferred target.

---

## E-SYN-00010 — Expected '('

- Severity: Error
- Default Detail: Missing '(' after function return type
- Description: Function declaration is missing parameter list left bracket after return type.

Incorrect Example:

```kinal
Function int Add int a, int b)
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

Reason: Function signatures require that the argument list begins immediately after the function name.

Fix: Append the left parenthesis to the parameter list.

---

## E-SYN-00011 — Expected '('

- Severity: Error
- Default Detail: Missing '(' after method name
- Description: The function or method call is missing an opening parenthesis.

Incorrect Example:

```kinal
PrintLine "hello");
```

Correct Example:

```kinal
PrintLine("hello");
```

Reason: Calling an expression requires enclosing the argument list in parentheses.

Fix: Add `(` after the transferred target.

---

## E-SYN-00012 — Expected ')'

- Severity: Error
- Default Detail: Missing ')'
- Description: The token sequence at the current position does not comply with the current grammar rules.

Incorrect Example:

```kinal
// This form does not produce valid syntax.
???
```

Correct Example:

```kinal
// Change it to a syntax form defined by the language.
Function int Main()
{
    Return 0;
}
```

Reason: The parser found no legal syntax in the current context on which to proceed with the reduction.

Fix: Prioritizes checking of nearest keywords, parentheses, curly braces, angle brackets, and semicolons.

---

## E-SYN-00013 — Expected ')'

- Severity: Error
- Default Detail: Missing ')' after Align
- Description: `Align` The parameter list is missing a closing parenthesis.

Incorrect Example:

```kinal
Struct Pair By Align(8
{
    int Left;
}
```

Correct Example:

```kinal
Struct Pair By Align(8)
{
    int Left;
}
```

Reason: `Align` must form the complete `Align(n)` structure.

Fix: Fill in the missing closing bracket.

---

## E-SYN-00014 — Expected ')'

- Severity: Error
- Default Detail: Missing ')' after If condition
- Description: `If` Missing closing bracket at the end of the condition.

Incorrect Example:

```kinal
If (value > 0
{
    Return 1;
}
```

Correct Example:

```kinal
If (value > 0)
{
    Return 1;
}
```

Reason: The conditional expression is not completely closed.

Fix: Add `)` at the end of the condition.

---

## E-SYN-00015 — Expected ')'

- Severity: Error
- Default Detail: Missing ')' after catch parameter
- Description: `Catch` The parameter list is missing a closing parenthesis.

Incorrect Example:

```kinal
Catch (IO.Error err
{
}
```

Correct Example:

```kinal
Catch (IO.Error err)
{
}
```

Reason: The catch parameter declaration must be completely closed before entering the statement block.

Fix: Add `)` at the end of `Catch (...)`.

---

## E-SYN-00016 — Expected ')'

- Severity: Error
- Default Detail: Missing ')' after for clause
- Description: `For` Missing closing bracket in header.

Incorrect Example:

```kinal
For (int i = 0; i < 10; i = i + 1
{
}
```

Correct Example:

```kinal
For (int i = 0; i < 10; i = i + 1)
{
}
```

Reason: The loop head is not completely closed.

Fix: Add `)` at the end of `For (...)`.

---

## E-SYN-00017 — Expected ')'

- Severity: Error
- Default Detail: Missing ')' after parameters
- Description: The formal parameter list is missing a closing parenthesis.

Incorrect Example:

```kinal
Function int Add(int a, int b
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

Reason: The parameter list is not fully closed.

Fix: Add `)` after the last parameter.

---

## E-SYN-00018 — Expected ')'

- Severity: Error
- Default Detail: Missing ')' in attribute
- Description: Property parameter list is missing closing bracket.

Incorrect Example:

```kinal
[Route("/ping"]
```

Correct Example:

```kinal
[Route("/ping")]
```

Reason: Property calls require a complete pair of parentheses just like normal calls.

Fix: Complete the closing parenthesis of the attribute parameter list.

---

## E-SYN-00019 — Expected ')'

- Severity: Error
- Default Detail: Missing ')' in call
- Description: The calling parameter list is missing a closing parenthesis.

Incorrect Example:

```kinal
PrintLine("hello";
```

Correct Example:

```kinal
PrintLine("hello");
```

Reason: The calling expression is not fully closed.

Fix: Append `)` at the end of the parameter list.

---

## E-SYN-00020 — Expected ')'

- Severity: Error
- Default Detail: Missing ')' in cast expression
- Description: Explicit conversion missing closing bracket.

Incorrect Example:

```kinal
int value = [int]("42";
```

Correct Example:

```kinal
int value = [int]("42");
```

Reason: The cast syntax must fully conform to `[type](expr)`.

Fix: Append `)` at the end of the conversion expression.

---

## E-SYN-00021 — Expected ')'

- Severity: Error
- Default Detail: Missing ')' in constructor call
- Description: The calling parameter list is missing a closing parenthesis.

Incorrect Example:

```kinal
PrintLine("hello";
```

Correct Example:

```kinal
PrintLine("hello");
```

Reason: The calling expression is not fully closed.

Fix: Append `)` at the end of the parameter list.

---

## E-SYN-00022 — Expected ')'

- Severity: Error
- Default Detail: Missing ')' in member call
- Description: The calling parameter list is missing a closing parenthesis.

Incorrect Example:

```kinal
PrintLine("hello";
```

Correct Example:

```kinal
PrintLine("hello");
```

Reason: The calling expression is not fully closed.

Fix: Append `)` at the end of the parameter list.

---

## E-SYN-00023 — Expected '/'

- Severity: Error
- Default Detail: Missing '/>' after block body
- Description: Block closing tag is incomplete.

Incorrect Example:

```kinal
Block Demo</
    Record A;
>
```

Correct Example:

```kinal
Block Demo</
    Record A;
/>
```

Reason: Block uses a pair of special boundaries `</` and `/>` to wrap the body.

Fix: Fill the end boundary with `/>`.

---

## E-SYN-00024 — Expected '/'

- Severity: Error
- Default Detail: Missing '</' before block body
- Description: Block starting boundary is incomplete.

Incorrect Example:

```kinal
Block Demo
    Record A;
/>
```

Correct Example:

```kinal
Block Demo</
    Record A;
/>
```

Reason: The block separator must be entered before the Block body.

Fix: Add `</` after the Block name.

---

## E-SYN-00025 — Expected ':'

- Severity: Error
- Default Detail: Missing ':' in conditional expression
- Description: The conditional expression is missing `:` to separate true and false branches.

Incorrect Example:

```kinal
int value = ok ? 1 0;
```

Correct Example:

```kinal
int value = ok ? 1 : 0;
```

Reason: Ternary conditional expressions require `?` and `:` to appear in pairs.

Fix: Add `:` between the true and false branches.

---

## E-SYN-00026 — Expected '<'

- Severity: Error
- Default Detail: Missing '</' before block body
- Description: Block starting boundary is incomplete.

Incorrect Example:

```kinal
Block Demo
    Record A;
/>
```

Correct Example:

```kinal
Block Demo</
    Record A;
/>
```

Reason: The block separator must be entered before the Block body.

Fix: Add `</` after the Block name.

---

## E-SYN-00027 — Expected '<'

- Severity: Error
- Default Detail: Missing '<' before generic type arguments
- Description: Generic call is missing left angle bracket.

Incorrect Example:

```kinal
Identity int>(1);
```

Correct Example:

```kinal
Identity<int>(1);
```

Reason: Explicit type parameters must be completely enclosed in angle brackets.

Fix: Prepend `<` before the type parameter list.

---

## E-SYN-00028 — Expected '>'

- Severity: Error
- Default Detail: Missing '/>' after block body
- Description: Block closing tag is incomplete.

Incorrect Example:

```kinal
Block Demo</
    Record A;
>
```

Correct Example:

```kinal
Block Demo</
    Record A;
/>
```

Reason: Block uses a pair of special boundaries `</` and `/>` to wrap the body.

Fix: Fill the end boundary with `/>`.

---

## E-SYN-00029 — Expected '>'

- Severity: Error
- Default Detail: Missing '>' after generic type arguments
- Description: Generic type parameter list is missing a closing angle bracket.

Incorrect Example:

```kinal
Identity<int(1);
```

Correct Example:

```kinal
Identity<int>(1);
```

Reason: Angle brackets must be closed in pairs.

Fix: Append `>` to the end of the type parameter list.

---

## E-SYN-00030 — Expected '>'

- Severity: Error
- Default Detail: Missing '>' after generic type parameter list
- Description: Generic type parameter list is missing a closing angle bracket.

Incorrect Example:

```kinal
Identity<int(1);
```

Correct Example:

```kinal
Identity<int>(1);
```

Reason: Angle brackets must be closed in pairs.

Fix: Append `>` to the end of the type parameter list.

---

## E-SYN-00031 — Expected '['

- Severity: Error
- Default Detail: Missing '[' in cast expression
- Description: Explicit conversion missing left bracket.

Incorrect Example:

```kinal
int value = int](1);
```

Correct Example:

```kinal
int value = [int](1);
```

Reason: Kinal encloses the conversion target type in square brackets.

Fix: Add `[` before the type.

---

## E-SYN-00032 — Expected ']'

- Severity: Error
- Default Detail: Missing ']'
- Description: The current bracket structure is missing a closing bracket.

Incorrect Example:

```kinal
int[] values = { 1, 2;
```

Correct Example:

```kinal
int[] values = { 1, 2 };
```

Reason: The parser detected an unclosed square bracket structure.

Fix: Fill in the missing `]` or correct the entire structure.

---

## E-SYN-00033 — Expected ']'

- Severity: Error
- Default Detail: Missing ']' after attribute
- Description: Property is missing closing bracket.

Incorrect Example:

```kinal
[Route("/ping")
```

Correct Example:

```kinal
[Route("/ping")]
```

Reason: Property declarations must be wrapped in `[` and `]`.

Fix: Add `]` at the end of the attribute.

---

## E-SYN-00034 — Expected ']'

- Severity: Error
- Default Detail: Missing ']' after index
- Description: The index expression is missing a closing square bracket.

Incorrect Example:

```kinal
int first = values[0;
```

Correct Example:

```kinal
int first = values[0];
```

Reason: Index accesses must form the complete `expr[index]`.

Fix: Append `]` at the end of the index.

---

## E-SYN-00035 — Expected ']'

- Severity: Error
- Default Detail: Missing ']' in cast expression
- Description: Explicit conversion missing closing bracket.

Incorrect Example:

```kinal
int value = [int(1);
```

Correct Example:

```kinal
int value = [int](1);
```

Reason: The type part of cast is not properly closed.

Fix: Add `]` after the type.

---

## E-SYN-00036 — Expected '{'

- Severity: Error
- Default Detail: Missing '{'
- Description: The opening curly brace is missing at the current position.

Incorrect Example:

```kinal
If (true)
    Return 1;
```

Correct Example:

```kinal
If (true)
{
    Return 1;
}
```

Reason: This structure requires curly braces to be used to wrap the body.

Fix: Complete `{` and complete the block.

---

## E-SYN-00037 — Expected '{'

- Severity: Error
- Default Detail: Missing '{' in Else expression
- Description: The Else branch of the If expression is missing an opening brace.

Incorrect Example:

```kinal
int value = If (ok) { 1 } Else 0 };
```

Correct Example:

```kinal
int value = If (ok) { 1 } Else { 0 };
```

Reason: Each branch of the If expression must be a block.

Fix: Add `{` for the Else branch.

---

## E-SYN-00038 — Expected '{'

- Severity: Error
- Default Detail: Missing '{' in If expression
- Description: The true branch of the If expression is missing an opening brace.

Incorrect Example:

```kinal
int value = If (ok) 1 } Else { 0 };
```

Correct Example:

```kinal
int value = If (ok) { 1 } Else { 0 };
```

Reason: If expressions require block syntax for both true and false branches.

Fix: Add `{` for the true branch.

---

## E-SYN-00039 — Expected '{'

- Severity: Error
- Default Detail: Missing '{' in class
- Description: Class declaration is missing opening curly brace.

Incorrect Example:

```kinal
Class User
    Public int Age;
}
```

Correct Example:

```kinal
Class User
{
    Public int Age;
}
```

Reason: The class body must be enclosed in curly braces.

Fix: Add `{` after the class name.

---

## E-SYN-00040 — Expected '{'

- Severity: Error
- Default Detail: Missing '{' in enum
- Description: Enumeration declaration is missing opening curly brace.

Incorrect Example:

```kinal
Enum State
    Ready
}
```

Correct Example:

```kinal
Enum State
{
    Ready
}
```

Reason: The list of enumeration members must be written within curly braces.

Fix: Add `{` after the enumeration name.

---

## E-SYN-00041 — Expected '{'

- Severity: Error
- Default Detail: Missing '{' in interface
- Description: The interface declaration is missing an opening curly brace.

Incorrect Example:

```kinal
Interface IValue
    Function int Value();
}
```

Correct Example:

```kinal
Interface IValue
{
    Function int Value();
}
```

Reason: The list of interface members must be enclosed in curly braces.

Fix: Add `{` after the interface name.

---

## E-SYN-00042 — Expected '{'

- Severity: Error
- Default Detail: Missing '{' in struct
- Description: The structure declaration is missing an opening brace.

Incorrect Example:

```kinal
Struct Pair
    int Left;
}
```

Correct Example:

```kinal
Struct Pair
{
    int Left;
}
```

Reason: Structure fields must be written within curly braces.

Fix: Add `{` after the structure name.

---

## E-SYN-00043 — Expected '}'

- Severity: Error
- Default Detail: Missing '}'
- Description: The closing curly brace is missing at the current position.

Incorrect Example:

```kinal
If (true)
{
    Return 1;
```

Correct Example:

```kinal
If (true)
{
    Return 1;
}
```

Reason: The block does not close properly.

Fix: Append `}` at the end of the block.

---

## E-SYN-00044 — Expected '}'

- Severity: Error
- Default Detail: Missing '}' in Else expression
- Description: One of the branches in the If expression is missing a closing curly brace.

Incorrect Example:

```kinal
int value = If (ok) { 1 Else { 0 };
```

Correct Example:

```kinal
int value = If (ok) { 1 } Else { 0 };
```

Reason: The branch block is not closed.

Fix: Add `}` to this branch.

---

## E-SYN-00045 — Expected '}'

- Severity: Error
- Default Detail: Missing '}' in If expression
- Description: One of the branches in the If expression is missing a closing curly brace.

Incorrect Example:

```kinal
int value = If (ok) { 1 Else { 0 };
```

Correct Example:

```kinal
int value = If (ok) { 1 } Else { 0 };
```

Reason: The branch block is not closed.

Fix: Add `}` to this branch.

---

## E-SYN-00046 — Expected '}'

- Severity: Error
- Default Detail: Missing '}' in array literal
- Description: Array literal missing closing curly brace.

Incorrect Example:

```kinal
int[] values = { 1, 2;
```

Correct Example:

```kinal
int[] values = { 1, 2 };
```

Reason: Array literals must be enclosed in pairs of curly braces.

Fix: Append `}` to the end of the array literal.

---

## E-SYN-00047 — Expected '}'

- Severity: Error
- Default Detail: Missing '}' in enum
- Description: The enumeration is missing a closing curly brace.

Incorrect Example:

```kinal
Enum State
{
    Ready,
    Done
```

Correct Example:

```kinal
Enum State
{
    Ready,
    Done
}
```

Reason: The enumeration is not closed.

Fix: Add `}` at the end of the enumeration.

---

## E-SYN-00048 — Unexpected End Of File

- Severity: Error
- Default Detail: Unexpected end of file while parsing expression
- Description: The file ended prematurely before the expression ended.

Incorrect Example:

```kinal
int value = (1 +
```

Correct Example:

```kinal
int value = (1 + 2);
```

Reason: The parser is still waiting for the remainder of the expression.

Fix: Complete missing operands, separators, or closing symbols.

---

## E-SYN-00049 — Unexpected End Of File

- Severity: Error
- Default Detail: Unexpected end of file while parsing variable declaration
- Description: The file ends when the variable declaration has not been completely written.

Incorrect Example:

```kinal
int value =
```

Correct Example:

```kinal
int value = 1;
```

Reason: The declaration is missing an initializer, semicolon, or subsequent structure.

Fix: Complete the variable declaration.

---

## E-SYN-00050 — Unterminated String Literal

- Severity: Error
- Default Detail: String literal is missing a closing quote
- Description: String literal missing closing quote.

Incorrect Example:

```kinal
string text = "hello;
```

Correct Example:

```kinal
string text = "hello";
```

Reason: The lexical analysis phase did not find the closing quote for the string.

Fix: Add the closing quote and check for escaping if necessary.

---

## E-SYN-00051 — Expected Block

- Severity: Error
- Default Detail: Expected 'Block' keyword
- Description: The `Block` keyword should be written here.

Incorrect Example:

```kinal
Demo</
    Record A;
/>
```

Correct Example:

```kinal
Block Demo</
    Record A;
/>
```

Reason: The current syntax position requires explicitly starting a Block declaration.

Fix: Add `Block`.

---

## E-SYN-00052 — Expected Catch

- Severity: Error
- Default Detail: Missing 'Catch' after Try block
- Description: `Try` must be followed by at least one `Catch`.

Incorrect Example:

```kinal
Try
{
    Throw "boom";
}
```

Correct Example:

```kinal
Try
{
    Throw "boom";
}
Catch (string msg)
{
}
```

Reason: The exception handling structure must give a capture path.

Fix: Add legal `Catch` after `Try`.

---

## E-SYN-00053 — Expected Class

- Severity: Error
- Default Detail: Expected 'Class' keyword
- Description: This should be the class declaration.

Incorrect Example:

```kinal
Public User
{
}
```

Correct Example:

```kinal
Public Class User
{
}
```

Reason: The parser is waiting for a class declaration start keyword.

Fix: Add `Class`.

---

## E-SYN-00054 — Expected Constructor

- Severity: Error
- Default Detail: Expected 'Constructor' keyword
- Description: Here it should be written `Constructor`.

Incorrect Example:

```kinal
Public Build(int value)
{
}
```

Correct Example:

```kinal
Public Constructor(int value)
{
}
```

Reason: The parser is expecting the constructor keyword form at this point.

Fix: Change to `Constructor(...)`.

---

## E-SYN-00055 — Expected Else

- Severity: Error
- Default Detail: If expression requires Else branch
- Description: If expressions must provide an Else branch.

Incorrect Example:

```kinal
int value = If (ok) { 1 };
```

Correct Example:

```kinal
int value = If (ok) { 1 } Else { 0 };
```

Reason: The expression form If must yield values on all paths.

Fix: Add the `Else` branch.

---

## E-SYN-00056 — Expected Enum

- Severity: Error
- Default Detail: Expected 'Enum' keyword
- Description: Here it should be written `Enum`.

Incorrect Example:

```kinal
State
{
    Ready
}
```

Correct Example:

```kinal
Enum State
{
    Ready
}
```

Reason: The parser is waiting for an enumeration declaration.

Fix: Add `Enum`.

---

## E-SYN-00057 — Expected For

- Severity: Error
- Default Detail: Expected 'For' keyword
- Description: Here it should be written `For`.

Incorrect Example:

```kinal
(int i = 0; i < 10; i = i + 1)
{
}
```

Correct Example:

```kinal
For (int i = 0; i < 10; i = i + 1)
{
}
```

Reason: The current location requires the beginning of a `For` statement.

Fix: Add `For`.

---

## E-SYN-00058 — Expected Function

- Severity: Error
- Default Detail: Expected 'Function' keyword
- Description: Here it should be written `Function`.

Incorrect Example:

```kinal
int Add(int a, int b)
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

Reason: Function declarations must explicitly begin with `Function`.

Fix: Add `Function` before the statement.

---

## E-SYN-00059 — Expected Get

- Severity: Error
- Default Detail: Expected 'Get' keyword
- Description: The `Get` import should be written here.

Incorrect Example:

```kinal
IO.Console;
```

Correct Example:

```kinal
Get IO.Console;
```

Reason: The import statement must begin with `Get`.

Fix: Change to a complete `Get ...;` statement.

---

## E-SYN-00060 — Expected Identifier

- Severity: Error
- Default Detail: Expected block name
- Description: Block statement is missing a name.

Incorrect Example:

```kinal
Block </
    Record A;
/>
```

Correct Example:

```kinal
Block Demo</
    Record A;
/>
```

Reason: Naming a Block requires an explicit identifier.

Fix: Add the name after `Block`.

---

## E-SYN-00061 — Expected Identifier

- Severity: Error
- Default Detail: Expected block variable name
- Description: Block variable declaration is missing a variable name.

Incorrect Example:

```kinal
IO.Type.Object.Block = Block</
    Record A;
/>;
```

Correct Example:

```kinal
IO.Type.Object.Block flow = Block</
    Record A;
/>;
```

Reason: Variable declarations must have target identifiers.

Fix: Add the variable name after the type.

---

## E-SYN-00062 — Expected Identifier

- Severity: Error
- Default Detail: Expected catch variable name
- Description: Catch parameter is missing a variable name.

Incorrect Example:

```kinal
Catch (IO.Error)
{
}
```

Correct Example:

```kinal
Catch (IO.Error err)
{
}
```

Reason: The exception parameter requires a local name to be referenced by the capture block.

Fix: Add the variable name after the catch type.

---

## E-SYN-00063 — Expected Identifier

- Severity: Error
- Default Detail: Expected class name
- Description: Class declaration is missing class name.

Incorrect Example:

```kinal
Class
{
}
```

Correct Example:

```kinal
Class User
{
}
```

Reason: The type declaration must first give the identifier.

Fix: Add the class name after `Class`.

---

## E-SYN-00064 — Expected Identifier

- Severity: Error
- Default Detail: Expected enum member
- Description: Member name is missing from the enumeration.

Incorrect Example:

```kinal
Enum State
{
    ,
    Done
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

Reason: The commas between them must be real member identifiers.

Fix: Fill in the legal enumeration member names.

---

## E-SYN-00065 — Expected Identifier

- Severity: Error
- Default Detail: Expected enum name
- Description: The enumeration declaration is missing a name.

Incorrect Example:

```kinal
Enum
{
    Ready
}
```

Correct Example:

```kinal
Enum State
{
    Ready
}
```

Reason: Enumerations are named types and must be given a name first.

Fix: Add the name after `Enum`.

---

## E-SYN-00066 — Expected Identifier

- Severity: Error
- Default Detail: Expected field name
- Description: Field declaration is missing field name.

Incorrect Example:

```kinal
Class Pair
{
    int;
}
```

Correct Example:

```kinal
Class Pair
{
    int Left;
}
```

Reason: A field declaration requires at least a type and field name.

Fix: Add the name after the field type.

---

## E-SYN-00067 — Expected Identifier

- Severity: Error
- Default Detail: Expected function name
- Description: Function declaration is missing function name.

Incorrect Example:

```kinal
Function int (int a, int b)
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

Reason: There must be a referenceable function identifier in the function signature.

Fix: Add the function name after the return type.

---

## E-SYN-00068 — Expected Identifier

- Severity: Error
- Default Detail: Expected generic type parameter name
- Description: Generic type parameter is missing a name.

Incorrect Example:

```kinal
Function T Identity<>(T value)
{
    Return value;
}
```

Correct Example:

```kinal
Function T Identity<T>(T value)
{
    Return value;
}
```

Reason: Each item within the angle brackets must be a type parameter name.

Fix: Complete the type parameter identifier within angle brackets.

---

## E-SYN-00069 — Expected Identifier

- Severity: Error
- Default Detail: Expected interface name
- Description: The interface declaration is missing an interface name.

Incorrect Example:

```kinal
Interface
{
    Function int Value();
}
```

Correct Example:

```kinal
Interface IValue
{
    Function int Value();
}
```

Reason: Interfaces are named types.

Fix: Add the interface name after `Interface`.

---

## E-SYN-00070 — Expected Identifier

- Severity: Error
- Default Detail: Expected method name
- Description: Method declaration is missing method name.

Incorrect Example:

```kinal
Class User
{
    Public Function int (int value)
    {
        Return value;
    }
}
```

Correct Example:

```kinal
Class User
{
    Public Function int Value(int value)
    {
        Return value;
    }
}
```

Reason: The method signature requires a name that can be referenced by the call site.

Fix: Add the method name after the return type.

---

## E-SYN-00071 — Expected Identifier

- Severity: Error
- Default Detail: Expected name
- Description: The current location is missing an identifier.

Incorrect Example:

```kinal
int = 1;
```

Correct Example:

```kinal
int value = 1;
```

Reason: The parser needs a legal name here to continue constructing the grammar node.

Fix: Complete the variable, type or member name.

---

## E-SYN-00072 — Expected Identifier

- Severity: Error
- Default Detail: Expected parameter name
- Description: The parameter name is missing.

Incorrect Example:

```kinal
Function int Add(int, int b)
{
    Return b;
}
```

Correct Example:

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}
```

Reason: Formal parameter declarations require both type and name.

Fix: Add the parameter name after the formal parameter type.

---

## E-SYN-00073 — Expected Identifier

- Severity: Error
- Default Detail: Expected record name
- Description: `Record` Missing tag name.

Incorrect Example:

```kinal
Record;
```

Correct Example:

```kinal
Record Exit;
```

Reason: Block tags must be named.

Fix: Add the label name after `Record`.

---

## E-SYN-00074 — Expected Identifier

- Severity: Error
- Default Detail: Expected remote name
- Description: The remote name or rebind name is missing an identifier.

Incorrect Example:

```kinal
Get Console By IO.Console.;
```

Correct Example:

```kinal
Get Console By IO.Console.PrintLine;
```

Reason: The last segment of the qualified import must be a valid name.

Fix: Complete the missing target name.

---

## E-SYN-00075 — Expected Identifier

- Severity: Error
- Default Detail: Expected specifier
- Description: Specifier name missing after structure attribute `By`.

Incorrect Example:

```kinal
Struct Pair By (8)
{
    int Left;
}
```

Correct Example:

```kinal
Struct Pair By Align(8)
{
    int Left;
}
```

Reason: `By` must be followed by a specific specifier.

Fix: Add a valid specifier name such as `Align`.

---

## E-SYN-00076 — Expected Identifier

- Severity: Error
- Default Detail: Expected struct name
- Description: Structure declaration is missing a name.

Incorrect Example:

```kinal
Struct
{
    int Left;
}
```

Correct Example:

```kinal
Struct Pair
{
    int Left;
}
```

Reason: Structures are named types.

Fix: Add the type name after `Struct`.

---

## E-SYN-00077 — Expected Identifier

- Severity: Error
- Default Detail: Expected variable name
- Description: Variable declaration is missing variable name.

Incorrect Example:

```kinal
int = 1;
```

Correct Example:

```kinal
int value = 1;
```

Reason: The type must be followed by an identifier in a variable declaration.

Fix: Add the variable name after the type.

---

## E-SYN-00078 — Expected Identifier

- Severity: Error
- Default Detail: Extern binding requires an identifier (C/System)
- Description: Missing binding identifier after `Extern ... By`.

Incorrect Example:

```kinal
Extern Function int puts(string text) By ;
```

Correct Example:

```kinal
Extern Function int puts(string text) By C;
```

Reason: External binding backends must be specified explicitly.

Fix: Add `C` or `System` after `By`.

---

## E-SYN-00079 — Expected If

- Severity: Error
- Default Detail: Expected 'If' keyword
- Description: Here it should be written `If`.

Incorrect Example:

```kinal
(ok)
{
    Return 1;
}
```

Correct Example:

```kinal
If (ok)
{
    Return 1;
}
```

Reason: The parser now expects the conditional statement keyword.

Fix: Add `If`.

---

## E-SYN-00080 — Expected If

- Severity: Error
- Default Detail: Expected 'If' keyword in expression
- Description: What is expected here is an If expression.

Incorrect Example:

```kinal
int value = (ok) { 1 } Else { 0 };
```

Correct Example:

```kinal
int value = If (ok) { 1 } Else { 0 };
```

Reason: In expression context, the conditional expression must start with `If`.

Fix: Add `If` at the beginning of the expression.

---

## E-SYN-00081 — Expected Interface

- Severity: Error
- Default Detail: Expected 'Interface' keyword
- Description: Here it should be written `Interface`.

Incorrect Example:

```kinal
IValue
{
    Function int Value();
}
```

Correct Example:

```kinal
Interface IValue
{
    Function int Value();
}
```

Reason: The current location is parsing the interface declaration.

Fix: Add `Interface`.

---

## E-SYN-00082 — Expected Jump

- Severity: Error
- Default Detail: Expected 'Jump' keyword
- Description: Here it should be written `Jump`.

Incorrect Example:

```kinal
Record Exit;
Go Exit;
```

Correct Example:

```kinal
Record Exit;
Jump Exit;
```

Reason: Block jump semantics only recognize `Jump`.

Fix: Fill in the correct keywords.

---

## E-SYN-00083 — Expected Number

- Severity: Error
- Default Detail: Align requires a number
- Description: Numbers must be written in `Align(...)`.

Incorrect Example:

```kinal
Struct Pair By Align(big)
{
    int Left;
}
```

Correct Example:

```kinal
Struct Pair By Align(8)
{
    int Left;
}
```

Reason: Alignment values must be syntactically numeric literals.

Fix: Change `Align(...)` to a number.

---

## E-SYN-00084 — Expected Number

- Severity: Error
- Default Detail: Enum value requires a number
- Description: Explicit enumeration values must be written as numbers.

Incorrect Example:

```kinal
Enum State
{
    Ready = low
}
```

Correct Example:

```kinal
Enum State
{
    Ready = 0
}
```

Reason: The current enum value syntax only accepts numeric literals.

Fix: Change enumeration member values to numbers.

---

## E-SYN-00085 — Expected Record

- Severity: Error
- Default Detail: Expected 'Record' keyword
- Description: Here it should be written `Record`.

Incorrect Example:

```kinal
Mark A;
```

Correct Example:

```kinal
Record A;
```

Reason: Label declarations within Block must use `Record`.

Fix: Add `Record`.

---

## E-SYN-00086 — Expected Semicolon

- Severity: Error
- Default Detail: Abstract method requires ';'
- Description: The current statement is missing a trailing semicolon.

Incorrect Example:

```kinal
Class Base
{
    Public Abstract Function int Value()
}
```

Correct Example:

```kinal
Class Base
{
    Public Abstract Function int Value();
}
```

Reason: Abstract methods only have signatures and no method bodies, so they must end with a semicolon.

Fix: Add `;` at the end of the abstract method declaration.

---

## E-SYN-00087 — Expected Semicolon

- Severity: Error
- Default Detail: Extern function requires ';'
- Description: The current statement is missing a trailing semicolon.

Incorrect Example:

```kinal
Extern Function int puts(string text) By C
```

Correct Example:

```kinal
Extern Function int puts(string text) By C;
```

Reason: External function declarations do not have a method body, so they need to be terminated with a semicolon.

Fix: Add `;` at the end of the extern declaration.

---

## E-SYN-00088 — Expected Semicolon

- Severity: Error
- Default Detail: Missing ';' after Get
- Description: The current statement is missing a trailing semicolon.

Incorrect Example:

```kinal
Get IO.Console
```

Correct Example:

```kinal
Get IO.Console;
```

Reason: Import statements are syntactically complete declarations and must be terminated with a semicolon.

Fix: Add `;` to the end of the `Get` statement.

---

## E-SYN-00089 — Expected Semicolon

- Severity: Error
- Default Detail: Missing ';' after Jump
- Description: The current statement is missing a trailing semicolon.

Incorrect Example:

```kinal
Jump Exit
```

Correct Example:

```kinal
Jump Exit;
```

Reason: Jump statements require an explicit terminator just like normal statements.

Fix: Add `;` after the `Jump` statement.

---

## E-SYN-00090 — Expected Semicolon

- Severity: Error
- Default Detail: Missing ';' after Throw
- Description: The current statement is missing a trailing semicolon.

Incorrect Example:

```kinal
Throw "boom"
```

Correct Example:

```kinal
Throw "boom";
```

Reason: Throw statements must be terminated explicitly.

Fix: Add `;` after the `Throw` statement.

---

## E-SYN-00091 — Expected Semicolon

- Severity: Error
- Default Detail: Missing ';' after Unit
- Description: The current statement is missing a trailing semicolon.

Incorrect Example:

```kinal
Unit App.Main
```

Correct Example:

```kinal
Unit App.Main;
```

Reason: Compilation unit declarations must end with a semicolon.

Fix: Add `;` after the `Unit` declaration.

---

## E-SYN-00092 — Expected Semicolon

- Severity: Error
- Default Detail: Missing ';' after break
- Description: The current statement is missing a trailing semicolon.

Incorrect Example:

```kinal
Break
```

Correct Example:

```kinal
Break;
```

Reason: `Break` is a complete statement and requires a terminator.

Fix: Add `;` after `Break`.

---

## E-SYN-00093 — Expected Semicolon

- Severity: Error
- Default Detail: Missing ';' after continue
- Description: The current statement is missing a trailing semicolon.

Incorrect Example:

```kinal
Continue
```

Correct Example:

```kinal
Continue;
```

Reason: `Continue` is a complete statement and requires a terminator.

Fix: Add `;` after `Continue`.

---

## E-SYN-00094 — Expected Semicolon

- Severity: Error
- Default Detail: Missing ';' after declaration
- Description: The current statement is missing a trailing semicolon.

Incorrect Example:

```kinal
int value = 1
```

Correct Example:

```kinal
int value = 1;
```

Reason: The declaration statement is missing a terminating semicolon.

Fix: Add `;` at the end of the declaration.

---

## E-SYN-00095 — Expected Semicolon

- Severity: Error
- Default Detail: Missing ';' after expression
- Description: The current statement is missing a trailing semicolon.

Incorrect Example:

```kinal
PrintLine("hello")
```

Correct Example:

```kinal
PrintLine("hello");
```

Reason: Expression statements must be terminated with a semicolon.

Fix: Add `;` after the expression statement.

---

## E-SYN-00096 — Expected Semicolon

- Severity: Error
- Default Detail: Missing ';' after field
- Description: The current statement is missing a trailing semicolon.

Incorrect Example:

```kinal
Class User
{
    int Age
}
```

Correct Example:

```kinal
Class User
{
    int Age;
}
```

Reason: Field declaration is missing a semicolon.

Fix: Append `;` at the end of the field.

---

## E-SYN-00097 — Expected Semicolon

- Severity: Error
- Default Detail: Missing ';' after for condition
- Description: The current statement is missing a trailing semicolon.

Incorrect Example:

```kinal
For (i = 0; i < 10 i = i + 1)
{
}
```

Correct Example:

```kinal
For (i = 0; i < 10; i = i + 1)
{
}
```

Reason: The three paragraphs of for must be separated by semicolons.

Fix: Add `;` after the conditional section.

---

## E-SYN-00098 — Expected Semicolon

- Severity: Error
- Default Detail: Missing ';' after return
- Description: The current statement is missing a trailing semicolon.

Incorrect Example:

```kinal
Return 0
```

Correct Example:

```kinal
Return 0;
```

Reason: The Return statement is missing a terminating semicolon.

Fix: Add `;` after `Return`.

---

## E-SYN-00099 — Expected Semicolon

- Severity: Error
- Default Detail: Missing ';' after variable declaration
- Description: The current statement is missing a trailing semicolon.

Incorrect Example:

```kinal
Var value = 1
```

Correct Example:

```kinal
Var value = 1;
```

Reason: The variable declaration statement is missing a semicolon.

Fix: Add `;` at the end of the variable declaration.

---

## E-SYN-00100 — Expected Semicolon

- Severity: Error
- Default Detail: Missing ';' in for initializer
- Description: The token sequence at the current position does not comply with the current grammar rules.

Incorrect Example:

```kinal
// This form does not produce valid syntax.
???
```

Correct Example:

```kinal
// Change it to a syntax form defined by the language.
Function int Main()
{
    Return 0;
}
```

Reason: The parser found no legal syntax in the current context on which to proceed with the reduction.

Fix: Prioritizes checking of nearest keywords, parentheses, curly braces, angle brackets, and semicolons.

---

## E-SYN-00101 — Expected Struct

- Severity: Error
- Default Detail: Expected 'Struct' keyword
- Description: Here it should be written `Struct`.

Incorrect Example:

```kinal
Pair
{
    int Left;
}
```

Correct Example:

```kinal
Struct Pair
{
    int Left;
}
```

Reason: The structure declaration is being parsed at the current location.

Fix: Add `Struct`.

---

## E-SYN-00102 — Expected Throw

- Severity: Error
- Default Detail: Expected 'Throw' keyword
- Description: Here it should be written `Throw`.

Incorrect Example:

```kinal
Raise "boom";
```

Correct Example:

```kinal
Throw "boom";
```

Reason: The keyword for the exception throw statement is `Throw`.

Fix: Change the keyword to `Throw`.

---

## E-SYN-00103 — Expected Try

- Severity: Error
- Default Detail: Expected 'Try' keyword
- Description: Here it should be written `Try`.

Incorrect Example:

```kinal
Test
{
    Throw "boom";
}
Catch (string msg)
{
}
```

Correct Example:

```kinal
Try
{
    Throw "boom";
}
Catch (string msg)
{
}
```

Reason: Exception handling blocks must start with `Try`.

Fix: Add `Try`.

---

## E-SYN-00104 — Expected While

- Severity: Error
- Default Detail: Expected 'While' keyword
- Description: Here it should be written `While`.

Incorrect Example:

```kinal
(true)
{
    Break;
}
```

Correct Example:

```kinal
While (true)
{
    Break;
}
```

Reason: The loop statement is being resolved at the current location.

Fix: Add `While`.

---

## E-SYN-00105 — Expected '}'

- Severity: Error
- Default Detail: Unexpected end of file while parsing block
- Description: The file ends before the block is closed.

Incorrect Example:

```kinal
Static Function int Main()
{
    Return 0;
```

Correct Example:

```kinal
Static Function int Main()
{
    Return 0;
}
```

Reason: When the parser scans to the end of the file, there are still unclosed blocks.

Fix: Check that the most recent `{` / `}` and `</` / `/>` pairs are balanced.

---

## E-SYN-00106 — Unexpected Top-Level Token

- Severity: Error
- Default Detail: Expected a top-level declaration (Function/Class/Struct/Enum/Interface/variable declaration)
- Description: A token that does not belong to the declaration area appears at the top level of the file.

Incorrect Example:

```kinal
42;
```

Correct Example:

```kinal
Function int Main()
{
    Return 0;
}
```

Reason: The top level can only contain declarations, not bare expressions or unrelated tokens.

Fix: Change the code to a legal top-level declaration, or move it into the body of the function.

---

## E-SYN-00107 — Invalid Global Modifier

- Severity: Error
- Default Detail: Top-level global variables cannot use function modifiers like Static/Extern/Delegate/Safe/Trusted/Unsafe/Async
- Description: Top-level global variables cannot have function modifiers.

Incorrect Example:

```kinal
Static int counter = 0;
```

Correct Example:

```kinal
int counter = 0;
```

Reason: Modifiers such as `Static`, `Extern`, and `Safe` are function-level semantics and do not apply to top-level variables.

Fix: Remove these modifiers.
