# Semantic Diagnostics

Diagnostics emitted during semantic checking after syntax analysis succeeds.

The examples in this document are minimal reproductions of the most common way each diagnostic code is triggered. A few diagnostics that are more implementation-oriented use the closest surface-level form that user code can express.

---

## Diagnostic Code Index

| Code | Title | Default Detail |
|------|------|----------|
| `E-SEM-00001` | Access Violation | Private member not accessible |
| `E-SEM-00002` | Access Violation | Protected member not accessible |
| `E-SEM-00003` | Invalid Abstract | Abstract method requires abstract class |
| `E-SEM-00004` | Invalid Address | Address-of array not supported |
| `E-SEM-00005` | Invalid Address | Address-of requires an assignable expression |
| `E-SEM-00006` | Invalid Align | Align must be power of two |
| `E-SEM-00007` | Invalid Assignment | Cannot assign to enum constant |
| `E-SEM-00008` | Invalid Assignment | Cannot modify enum constant |
| `E-SEM-00009` | Invalid Assignment | Increment/decrement target must be assignable |
| `E-SEM-00010` | Invalid Assignment | Left-hand side must be assignable |
| `E-SEM-00011` | Invalid Assignment | Target is not assignable |
| `E-SEM-00012` | Invalid Attribute | Attributes are not supported on global variables |
| `E-SEM-00013` | Invalid Base | Base is not available in static context |
| `E-SEM-00014` | Invalid Base | Class has no base type |
| `E-SEM-00015` | Invalid Call | Block object must use Run/Jump APIs |
| `E-SEM-00016` | Invalid Call | Call target is not a function object |
| `E-SEM-00017` | Invalid Call | Static method requires class name |
| `E-SEM-00018` | Invalid Catch | Catch parameter must be string or IO.Error |
| `E-SEM-00019` | Invalid Constructor | Constructor cannot be static/virtual/override/abstract/sealed |
| `E-SEM-00020` | Invalid Enum | Enum underlying type must be integer |
| `E-SEM-00021` | Invalid Generic Call | Builtin function does not accept generic type arguments |
| `E-SEM-00022` | Invalid Generic Call | Function object invocation does not accept generic type arguments |
| `E-SEM-00023` | Invalid Generic Call | Type arguments are only valid for generic function calls |
| `E-SEM-00024` | Invalid Generic Call | Type arguments can only be used with generic functions |
| `E-SEM-00025` | Invalid Get Position | Get must appear at top-level after Unit and before declarations |
| `E-SEM-00026` | Invalid Global | Only variable declarations are allowed at global scope |
| `E-SEM-00027` | Invalid Index | Indexing requires array, string or pointer |
| `E-SEM-00028` | Invalid Inheritance | Cannot inherit from sealed class |
| `E-SEM-00029` | Invalid Inheritance | Class cannot inherit from itself |
| `E-SEM-00030` | Invalid Interface | Interface methods must be abstract |
| `E-SEM-00031` | Invalid Member | Interface members not supported yet |
| `E-SEM-00032` | Invalid Member | Member access requires class instance |
| `E-SEM-00033` | Invalid Member | Member call requires class instance |
| `E-SEM-00034` | Invalid Modifier | Sealed is only valid on override methods |
| `E-SEM-00035` | Invalid Modifier | Static methods cannot be virtual/override/abstract |
| `E-SEM-00036` | Invalid New | Cannot instantiate abstract class |
| `E-SEM-00037` | Invalid Operator | Unknown binary operator |
| `E-SEM-00038` | Invalid Operator | Unknown unary operator |
| `E-SEM-00039` | Invalid Override | Base method is sealed |
| `E-SEM-00040` | Invalid Override | No virtual base method to override |
| `E-SEM-00041` | Invalid This | This is not available in static context |
| `E-SEM-00042` | Missing Entry | Main function not found |
| `E-SEM-00043` | Missing Get | Standard module not imported; add a Get statement |
| `E-SEM-00044` | Missing Override | Method hides a virtual base method |
| `E-SEM-00045` | Unsafe Block | Record inside nested control flow requires Unsafe Block |
| `E-SEM-00046` | Unsafe Call | Safe function cannot call Unsafe |
| `E-SEM-00047` | Invalid Generic Call | Generic type arguments are only supported on named function calls |
| `E-SEM-00048` | Invalid Generic Function | Extern function cannot be generic |
| `E-SEM-00049` | Invalid Generic Member | Generic type arguments on member access require a call |
| `E-SEM-00050` | Invalid Generic Type | Expected a type argument |

---

## E-SEM-00001 — Access Violation

- Severity: Error
- Default Detail: Private member not accessible
- Description: An attempt was made outside the class to access member `Private`.

Incorrect Example:

```kinal
Class Account
{
    Private int balance;
}

Account a = New Account();
int value = a.balance;
```

Correct Example:

```kinal
Class Account
{
    Private int balance;

    Public Function int Balance()
    {
        Return This.balance;
    }
}

Account a = New Account();
int value = a.Balance();
```

Reason: The `Private` member can only be accessed within the class in which it is declared.

Fix: Use public methods to encapsulate access instead, or move member access inside the class.

---

## E-SEM-00002 — Access Violation

- Severity: Error
- Default Detail: Protected member not accessible
- Description: The `Protected` member was accessed by a non-derived class context.

Incorrect Example:

```kinal
Class Base
{
    Protected int value;
}

Base b = New Base();
int v = b.value;
```

Correct Example:

```kinal
Class Base
{
    Protected int value;
}

Class Derived By Base
{
    Public Function int Value()
    {
        Return This.value;
    }
}
```

Reason: `Protected` is accessible only to the class itself and its derived classes.

Fix: Access it in a derived class, or use a public API instead to expose the required information.

---

## E-SEM-00003 — Invalid Abstract

- Severity: Error
- Default Detail: Abstract method requires abstract class
- Description: Only abstract classes can declare abstract methods.

Incorrect Example:

```kinal
Class Base
{
    Public Abstract Function int Value();
}
```

Correct Example:

```kinal
Abstract Class Base
{
    Public Abstract Function int Value();
}
```

Reason: Abstract methods mean that the type itself cannot be fully instantiated directly.

Fix: Change the class to `Abstract Class`, or provide a concrete implementation for the method.

---

## E-SEM-00004 — Invalid Address

- Severity: Error
- Default Detail: Address-of array not supported
- Description: Currently it is not possible to directly address the entire array.

Incorrect Example:

```kinal
int values[3];
int* p = &values;
```

Correct Example:

```kinal
int values[3];
int* p = &values[0];
```

Reason: The array itself is not a scalar lvalue that can be addressed directly according to the current rules.

Fix: Get the address of a specific element, or use a pointer variable to carry the target address.

---

## E-SEM-00005 — Invalid Address

- Severity: Error
- Default Detail: Address-of requires an assignable expression
- Description: Only assignable lvalues can take addresses.

Incorrect Example:

```kinal
int a = 1;
int b = 2;
int* p = &(a + b);
```

Correct Example:

```kinal
int a = 1;
int* p = &a;
```

Reason: Temporary expressions have no stable storage location and cannot be safely retrieved.

Fix: First save the result to a variable, and then get the address of the variable.

---

## E-SEM-00006 — Invalid Align

- Severity: Error
- Default Detail: Align must be power of two
- Description: `Align` can only use powers of 2.

Incorrect Example:

```kinal
Struct Pair By Align(3)
{
    int Left;
    int Right;
}
```

Correct Example:

```kinal
Struct Pair By Align(8)
{
    int Left;
    int Right;
}
```

Reason: Data alignment typically relies on bit boundaries; non-power-of-2 alignments have no consistent layout semantics.

Fix: Change `Align(n)` to legal values such as `1`, `2`, `4`, `8`, `16`.

---

## E-SEM-00007 — Invalid Assignment

- Severity: Error
- Default Detail: Cannot assign to enum constant
- Description: Enumeration members are constants and cannot be assigned values.

Incorrect Example:

```kinal
Enum State
{
    Ready, Done
}

State.Ready = State.Done;
```

Correct Example:

```kinal
Enum State
{
    Ready, Done
}

State value = State.Done;
```

Reason: Enumeration members are fixed constants, not writable variables.

Fix: Write the value to a separate variable rather than trying to modify the enumeration member itself.

---

## E-SEM-00008 — Invalid Assignment

- Severity: Error
- Default Detail: Cannot modify enum constant
- Description: Enumeration members are constants and cannot be assigned values.

Incorrect Example:

```kinal
Enum State
{
    Ready, Done
}

State.Ready = State.Done;
```

Correct Example:

```kinal
Enum State
{
    Ready, Done
}

State value = State.Done;
```

Reason: Enumeration members are fixed constants, not writable variables.

Fix: Write the value to a separate variable rather than trying to modify the enumeration member itself.

---

## E-SEM-00009 — Invalid Assignment

- Severity: Error
- Default Detail: Increment/decrement target must be assignable
- Description: The target of `++` / `--` must be a writable variable, field, index, or dereference result.

Incorrect Example:

```kinal
int a = 1;
int b = 2;
++(a + b);
```

Correct Example:

```kinal
int a = 1;
++a;
```

Reason: Auto-increment and auto-decrement will write the result back to the original position, and there is no position for temporary expressions to be written back.

Fix: Only use `++` / `--` for truly assignable lvalues.

---

## E-SEM-00010 — Invalid Assignment

- Severity: Error
- Default Detail: Left-hand side must be assignable
- Description: The left side of the assignment must be a writable target.

Incorrect Example:

```kinal
1 = 2;
```

Correct Example:

```kinal
int value = 1;
value = 2;
```

Reason: Literals, temporary values, and read-only expressions cannot accept writes.

Fix: Make sure the left-hand side is a variable, field, index expression, or other legal lvalue.

---

## E-SEM-00011 — Invalid Assignment

- Severity: Error
- Default Detail: Target is not assignable
- Description: The left side of the assignment must be a writable target.

Incorrect Example:

```kinal
1 = 2;
```

Correct Example:

```kinal
int value = 1;
value = 2;
```

Reason: Literals, temporary values, and read-only expressions cannot accept writes.

Fix: Make sure the left-hand side is a variable, field, index expression, or other legal lvalue.

---

## E-SEM-00012 — Invalid Attribute

- Severity: Error
- Default Detail: Attributes are not supported on global variables
- Description: Global variables currently do not support attribute annotations.

Incorrect Example:

```kinal
[My.Tag]
int value = 1;
```

Correct Example:

```kinal
[My.Tag]
Function int Value()
{
    Return 1;
}
```

Reason: The current property system is primarily oriented toward types, methods, and other supported declarations, and does not extend to top-level variables.

Fix: Put the attribute on a supported declaration, or remove the attribute.

---

## E-SEM-00013 — Invalid Base

- Severity: Error
- Default Detail: Base is not available in static context
- Description: `Base` cannot be used in static methods.

Incorrect Example:

```kinal
Class Derived By BaseType
{
    Public Static Function string Kind()
    {
        Return Base.Kind();
    }
}
```

Correct Example:

```kinal
Class Derived By BaseType
{
    Public Override Function string Kind()
    {
        Return Base.Kind();
    }
}
```

Reason: `Base` relies on the virtual calling context of a specific instance, and static methods do not have `This` / `Base` counterparts.

Fix: Put the logic into instance methods, or call static members directly through the class name.

---

## E-SEM-00014 — Invalid Base

- Severity: Error
- Default Detail: Class has no base type
- Description: The current class has no base class, but uses `Base`.

Incorrect Example:

```kinal
Class User
{
    Public Function string Kind()
    {
        Return Base.ToString();
    }
}
```

Correct Example:

```kinal
Class BaseType
{
    Public Virtual Function string Kind()
    {
        Return "base";
    }
}

Class User By BaseType
{
    Public Override Function string Kind()
    {
        Return Base.Kind();
    }
}
```

Reason: `Base` only makes sense if there is a real inheritance chain.

Fix: Verify that the current class indeed inherits from a base class, or remove the `Base` call.

---

## E-SEM-00015 — Invalid Call

- Severity: Error
- Default Detail: Block object must use Run/Jump APIs
- Description: Block objects cannot be called directly like ordinary functions.

Incorrect Example:

```kinal
IO.Type.Object.Block flow = Block Demo</
    Record A;
/>;

flow();
```

Correct Example:

```kinal
IO.Type.Object.Block flow = Block Demo</
    Record A;
/>;

flow.Run();
```

Reason: Block's execution model relies on the `Run` / `Jump` / `RunUntil` set of explicit APIs.

Fix: Use the specialized methods provided by the Block object instead.

---

## E-SEM-00016 — Invalid Call

- Severity: Error
- Default Detail: Call target is not a function object
- Description: The calling target is not a function object.

Incorrect Example:

```kinal
int value = 42;
value();
```

Correct Example:

```kinal
Function int Identity(int value)
{
    Return value;
}

int result = Identity(42);
```

Reason: Only functions, methods, constructors, and function objects can have calling brackets.

Fix: Verify that the type of the called expression is a function object, or remove the incorrect calling parentheses.

---

## E-SEM-00017 — Invalid Call

- Severity: Error
- Default Detail: Static method requires class name
- Description: The calling target is not a function object.

Incorrect Example:

```kinal
int value = 42;
value();
```

Correct Example:

```kinal
Function int Identity(int value)
{
    Return value;
}

int result = Identity(42);
```

Reason: Only functions, methods, constructors, and function objects can have calling brackets.

Fix: Verify that the type of the called expression is a function object, or remove the incorrect calling parentheses.

---

## E-SEM-00018 — Invalid Catch

- Severity: Error
- Default Detail: Catch parameter must be string or IO.Error
- Description: The `Catch` parameter type can only be `string` or `IO.Error`.

Incorrect Example:

```kinal
Try
{
    Throw "boom";
}
Catch (int code)
{
}
```

Correct Example:

```kinal
Try
{
    Throw New IO.Error("Config", "missing key");
}
Catch (IO.Error err)
{
}
```

Reason: The current exception model only ensures that these two catch forms have stable semantics.

Fix: Change the catch parameter type to `string` or `IO.Error`.

---

## E-SEM-00019 — Invalid Constructor

- Severity: Error
- Default Detail: Constructor cannot be static/virtual/override/abstract/sealed
- Description: The constructor cannot be modified with `Static`, `Virtual`, `Override`, `Abstract` or `Sealed`.

Incorrect Example:

```kinal
Class Counter
{
    Public Static Constructor(int value)
    {
    }
}
```

Correct Example:

```kinal
Class Counter
{
    Public Constructor(int value)
    {
    }
}
```

Reason: The constructor is responsible for initializing the instance. It does not participate in virtual distribution and is not a static member.

Fix: Remove disallowed modifiers, leaving only available visibility and security modifiers.

---

## E-SEM-00020 — Invalid Enum

- Severity: Error
- Default Detail: Enum underlying type must be integer
- Description: The underlying type of the enumeration must be an integer type.

Incorrect Example:

```kinal
Enum Status By string
{
    Ok, Fail
}
```

Correct Example:

```kinal
Enum Status By u8
{
    Ok, Fail
}
```

Reason: The underlying value of the enumeration needs to be stable, discrete, and comparable. Currently, only integers are supported.

Fix: Change the underlying type to an integer type such as `u8`, `i32`.

---

## E-SEM-00021 — Invalid Generic Call

- Severity: Error
- Default Detail: Builtin function does not accept generic type arguments
- Description: Built-in functions do not accept explicit generic parameters.

Incorrect Example:

```kinal
IO.Console.PrintLine<int>(1);
```

Correct Example:

```kinal
IO.Console.PrintLine(1);
```

Reason: Built-in functions are not generic function templates and cannot be instantiated.

Fix: Remove `<...>`.

---

## E-SEM-00022 — Invalid Generic Call

- Severity: Error
- Default Detail: Function object invocation does not accept generic type arguments
- Description: Explicit generic parameters can no longer be appended when called via a function object.

Incorrect Example:

```kinal
IO.Type.Object.Function fn = IO.Console.PrintLine;
fn<int>(1);
```

Correct Example:

```kinal
IO.Type.Object.Function fn = IO.Console.PrintLine;
fn(1);
```

Reason: The function object is already the specific value of a callable entity, and the generic template layer is no longer retained.

Fix: Call the function object directly; if generic instantiation is required, instantiate it before getting the function object.

---

## E-SEM-00023 — Invalid Generic Call

- Severity: Error
- Default Detail: Type arguments are only valid for generic function calls
- Description: The current call target is not a generic function, but type parameters are appended.

Incorrect Example:

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}

int value = Add<int>(1, 2);
```

Correct Example:

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}

int value = Add(1, 2);
```

Reason: Type parameters are only meaningful for targets that actually declare `<T>`.

Fix: Remove type parameters or change them to corresponding generic functions.

---

## E-SEM-00024 — Invalid Generic Call

- Severity: Error
- Default Detail: Type arguments can only be used with generic functions
- Description: The current call target is not a generic function, but type parameters are appended.

Incorrect Example:

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}

int value = Add<int>(1, 2);
```

Correct Example:

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}

int value = Add(1, 2);
```

Reason: Type parameters are only meaningful for targets that actually declare `<T>`.

Fix: Remove type parameters or change them to corresponding generic functions.

---

## E-SEM-00025 — Invalid Get Position

- Severity: Error
- Default Detail: Get must appear at top-level after Unit and before declarations
- Description: `Get` must be placed at the top level, after `Unit` and before other declarations.

Incorrect Example:

```kinal
Static Function int Main()
{
    Get IO.Console;
    Return 0;
}
```

Correct Example:

```kinal
Get IO.Console;

Static Function int Main()
{
    Return 0;
}
```

Reason: Imports are declarations that take effect at the compilation unit level, not runtime statements.

Fix: Move `Get` to the appropriate location at the top of the file.

---

## E-SEM-00026 — Invalid Global

- Severity: Error
- Default Detail: Only variable declarations are allowed at global scope
- Description: Only variable declarations are allowed at the top level, and direct execution statements are not allowed.

Incorrect Example:

```kinal
If (true)
{
}
```

Correct Example:

```kinal
int flag = 1;

Static Function int Main()
{
    If (flag == 1)
    {
    }
    Return 0;
}
```

Reason: The top-level area is the declaration area, not the execution area.

Fix: Move execution statements into functions, methods, or Blocks.

---

## E-SEM-00027 — Invalid Index

- Severity: Error
- Default Detail: Indexing requires array, string or pointer
- Description: Only arrays, strings, or pointers can use indexing.

Incorrect Example:

```kinal
int value = 42;
int first = value[0];
```

Correct Example:

```kinal
int[] values = { 42 };
int first = values[0];
```

Reason: Indexing operations require that the underlying object has "access elements by position" semantics.

Fix: Change the target to an array, string, or pointer, or use an access method truly supported by that type.

---

## E-SEM-00028 — Invalid Inheritance

- Severity: Error
- Default Detail: Cannot inherit from sealed class
- Description: `Sealed Class` can no longer be inherited.

Incorrect Example:

```kinal
Sealed Class Final
{
}

Class Child By Final
{
}
```

Correct Example:

```kinal
Class Base
{
}

Class Child By Base
{
}
```

Reason: `Sealed` explicitly indicates that the inheritance chain ends here.

Fix: Change to inheriting from a non-sealed base class, or remove inheritance from this type.

---

## E-SEM-00029 — Invalid Inheritance

- Severity: Error
- Default Detail: Class cannot inherit from itself
- Description: Classes cannot inherit from themselves.

Incorrect Example:

```kinal
Class Loop By Loop
{
}
```

Correct Example:

```kinal
Class Base
{
}

Class Derived By Base
{
}
```

Reason: Self-inheritance creates an infinitely recursive type hierarchy.

Fix: Change to a real base class, or remove the inheritance clause.

---

## E-SEM-00030 — Invalid Interface

- Severity: Error
- Default Detail: Interface methods must be abstract
- Description: Interface methods must remain abstract declarations and do not provide method bodies.

Incorrect Example:

```kinal
Interface IValue
{
    Function int Value()
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
```

Reason: The interface only defines the contract and does not carry the implementation.

Fix: Remove the method body and keep the abstract signature.

---

## E-SEM-00031 — Invalid Member

- Severity: Error
- Default Detail: Interface members not supported yet
- Description: The interface does not currently support non-method members such as fields.

Incorrect Example:

```kinal
Interface IValue
{
    int Value;
}
```

Correct Example:

```kinal
Interface IValue
{
    Function int Value();
}
```

Reason: The current interface system only models callable contracts and does not extend to the field level.

Fix: Change the members into method form, or put the state into a concrete class.

---

## E-SEM-00032 — Invalid Member

- Severity: Error
- Default Detail: Member access requires class instance
- Description: The left side of a member access or member call must be a class instance.

Incorrect Example:

```kinal
int value = 1;
value.Count();
```

Correct Example:

```kinal
Class Counter
{
    Public Function int Count()
    {
        Return 1;
    }
}

Counter counter = New Counter();
int value = counter.Count();
```

Reason: Member tables only exist on class instances or other objects that actually support member access.

Fix: Make sure the left side of the dot is the correct instance type.

---

## E-SEM-00033 — Invalid Member

- Severity: Error
- Default Detail: Member call requires class instance
- Description: The left side of a member access or member call must be a class instance.

Incorrect Example:

```kinal
int value = 1;
value.Count();
```

Correct Example:

```kinal
Class Counter
{
    Public Function int Count()
    {
        Return 1;
    }
}

Counter counter = New Counter();
int value = counter.Count();
```

Reason: Member tables only exist on class instances or other objects that actually support member access.

Fix: Make sure the left side of the dot is the correct instance type.

---

## E-SEM-00034 — Invalid Modifier

- Severity: Error
- Default Detail: Sealed is only valid on override methods
- Description: `Sealed` can only modify override methods.

Incorrect Example:

```kinal
Class User
{
    Public Sealed Function void Run()
    {
    }
}
```

Correct Example:

```kinal
Class Base
{
    Public Virtual Function void Run()
    {
    }
}

Class User By Base
{
    Public Override Sealed Function void Run()
    {
    }
}
```

Reason: The meaning of `Sealed` is "the override should be closed here", and it is inseparable from the override relationship.

Fix: Only use `Sealed` on override methods.

---

## E-SEM-00035 — Invalid Modifier

- Severity: Error
- Default Detail: Static methods cannot be virtual/override/abstract
- Description: Static methods can no longer superimpose virtual distribution related modifications.

Incorrect Example:

```kinal
Class User
{
    Public Static Virtual Function void Run()
    {
    }
}
```

Correct Example:

```kinal
Class User
{
    Public Static Function void Run()
    {
    }
}
```

Reason: Static methods do not use instance virtual tables, so `Virtual` / `Override` / `Abstract` have no semantic space.

Fix: Remove these modifiers, or change the method to an instance method.

---

## E-SEM-00036 — Invalid New

- Severity: Error
- Default Detail: Cannot instantiate abstract class
- Description: Abstract classes cannot be instantiated directly.

Incorrect Example:

```kinal
Abstract Class Base
{
}

Base value = New Base();
```

Correct Example:

```kinal
Abstract Class Base
{
    Public Abstract Function int Value();
}

Class Derived By Base
{
    Public Override Function int Value()
    {
        Return 1;
    }
}

Base value = New Derived();
```

Reason: Abstract classes only describe interfaces and shared implementations, and do not represent a complete constructible object.

Fix: Instantiate a concrete derived class.

---

## E-SEM-00037 — Invalid Operator

- Severity: Error
- Default Detail: Unknown binary operator
- Description: The current binary operator is not in Kinal's set of defined operators.

Incorrect Example:

```kinal
int value = 1 <=> 2;
```

Correct Example:

```kinal
bool value = 1 < 2;
```

Reason: Not all operators in other languages are supported by Kinal.

Fix: Use operators defined in the documentation instead.

---

## E-SEM-00038 — Invalid Operator

- Severity: Error
- Default Detail: Unknown unary operator
- Description: The current unary operator is not in Kinal's set of defined operators.

Incorrect Example:

```kinal
int value = ^^1;
```

Correct Example:

```kinal
int value = -1;
```

Reason: This unary operator has no semantics defined in the language.

Fix: Change to a supported unary operator or equivalent expression.

---

## E-SEM-00039 — Invalid Override

- Severity: Error
- Default Detail: Base method is sealed
- Description: Base class methods marked `Sealed` can no longer be overridden.

Incorrect Example:

```kinal
Class Base
{
    Public Virtual Function void Run()
    {
    }
}

Class Mid By Base
{
    Public Override Sealed Function void Run()
    {
    }
}

Class Leaf By Mid
{
    Public Override Function void Run()
    {
    }
}
```

Correct Example:

```kinal
Class Leaf By Mid
{
}
```

Reason: `Sealed` override explicitly prohibits subsequent subclasses from overriding the same virtual method slot.

Fix: Delete the override, or move the custom logic to a new method name.

---

## E-SEM-00040 — Invalid Override

- Severity: Error
- Default Detail: No virtual base method to override
- Description: `Override` can only override true virtual methods.

Incorrect Example:

```kinal
Class Base
{
    Public Function void Run()
    {
    }
}

Class Leaf By Base
{
    Public Override Function void Run()
    {
    }
}
```

Correct Example:

```kinal
Class Base
{
    Public Virtual Function void Run()
    {
    }
}

Class Leaf By Base
{
    Public Override Function void Run()
    {
    }
}
```

Reason: Only the `Virtual` method has an overridable slot.

Fix: Add `Virtual` to base class methods, or remove `Override` from subclasses.

---

## E-SEM-00041 — Invalid This

- Severity: Error
- Default Detail: This is not available in static context
- Description: Static contexts have no instances, so `This` cannot be used.

Incorrect Example:

```kinal
Class Counter
{
    Public Static Function int Value()
    {
        Return This.value;
    }
}
```

Correct Example:

```kinal
Class Counter
{
    Private int value;

    Public Function int Value()
    {
        Return This.value;
    }
}
```

Reason: `This` represents the current instance; static members are not attached to any instance.

Fix: Change the logic to an instance method, or pass in the instance object explicitly.

---

## E-SEM-00042 — Missing Entry

- Severity: Error
- Default Detail: Main function not found
- Description: No legal `Main` found in the compilation unit.

Incorrect Example:

```kinal
Function int Start()
{
    Return 0;
}
```

Correct Example:

```kinal
Static Function int Main()
{
    Return 0;
}
```

Reason: By default, the host runner only looks for fully qualified entry names.

Fix: Add a `Main` that meets the signature requirements.

---

## E-SEM-00043 — Missing Get

- Severity: Error
- Default Detail: Standard module not imported; add a Get statement
- Description: A standard library module must be imported before it can be used.

Incorrect Example:

```kinal
Static Function int Main()
{
    IO.Console.PrintLine("hello");
    Return 0;
}
```

Correct Example:

```kinal
Get IO.Console;

Static Function int Main()
{
    IO.Console.PrintLine("hello");
    Return 0;
}
```

Reason: Standard modules do not automatically enter the scope of the current compilation unit.

Fix: Add the correct `Get` statement at the top of the file.

---

## E-SEM-00044 — Missing Override

- Severity: Error
- Default Detail: Method hides a virtual base method
- Description: The subclass method and the virtual base class method have the same name and signature, but `Override` is not explicitly written.

Incorrect Example:

```kinal
Class Base
{
    Public Virtual Function int Value()
    {
        Return 1;
    }
}

Class Derived By Base
{
    Public Function int Value()
    {
        Return 2;
    }
}
```

Correct Example:

```kinal
Class Base
{
    Public Virtual Function int Value()
    {
        Return 1;
    }
}

Class Derived By Base
{
    Public Override Function int Value()
    {
        Return 2;
    }
}
```

Reason: The compiler detects that you are probably overriding a base class virtual method without explicitly declaring your intention.

Fix: If you really want to rewrite, add `Override`; if it just happens to be the same name, change the name to avoid obscuration.

---

## E-SEM-00045 — Unsafe Block

- Severity: Error
- Default Detail: Record inside nested control flow requires Unsafe Block
- Description: `Unsafe Block` must be used when declaring `Record` in nested control flow.

Incorrect Example:

```kinal
Block Bad
</
    If (true)
    {
        Record A;
    }
/>
```

Correct Example:

```kinal
Unsafe Block Good
</
    If (true)
    {
        Record A;
    }
/>
```

Reason: `Record` in nested control flow affects Block's jump graph and is currently considered a low-level capability that requires explicit risk taking.

Fix: Upgrade Block to `Unsafe Block`, or promote `Record` to the top level of Block.

---

## E-SEM-00046 — Unsafe Call

- Severity: Error
- Default Detail: Safe function cannot call Unsafe
- Description: `Safe` functions cannot call `Unsafe` targets directly.

Incorrect Example:

```kinal
Unsafe Function int UnsafeAdd()
{
    Return 1;
}

Static Safe Function int Main(string[] args)
{
    Return UnsafeAdd();
}
```

Correct Example:

```kinal
Unsafe Function int UnsafeAdd()
{
    Return 1;
}

Static Trusted Function int Main(string[] args)
{
    Return UnsafeAdd();
}
```

Reason: Security levels are open in one direction from high to low; `Safe` code cannot directly rely on unprotected dangerous operations.

Fix: Put the call into a `Trusted` / `Unsafe` context, or provide an encapsulated safe API.

---

## E-SEM-00047 — Invalid Generic Call

- Severity: Error
- Default Detail: Generic type arguments are only supported on named function calls
- Description: Explicit generic parameters can only be attached to named function calls.

Incorrect Example:

```kinal
(GetAdder())<int>(1, 2);
```

Correct Example:

```kinal
Add<int>(1, 2);
```

Reason: Only named function call paths can stably carry generic instantiation information.

Fix: Change the target to a named function call, or instantiate first and then get the reference.

---

## E-SEM-00048 — Invalid Generic Function

- Severity: Error
- Default Detail: Extern function cannot be generic
- Description: The `Extern` declaration cannot be a generic function.

Incorrect Example:

```kinal
Extern Function T Identity<T>(T value) By C;
```

Correct Example:

```kinal
Extern Function int Sum(int a, int b) By C;
```

Reason: External symbols at the ABI level do not have generic instantiation information, and the compiler cannot directly map generic templates to external function addresses.

Fix: Write the external interface as a specific signature, and provide generic wrapper functions in the Kinal layer if necessary.

---

## E-SEM-00049 — Invalid Generic Member

- Severity: Error
- Default Detail: Generic type arguments on member access require a call
- Description: When appending generic parameters to a member access, it must be followed by a call.

Incorrect Example:

```kinal
user.Map<int>;
```

Correct Example:

```kinal
user.Map<int>(1);
```

Reason: Only the "member call" syntax path consumes type parameters; individual member access does not.

Fix: If you want to call a member generic method, add the parameter list; if you just want to get the member, remove the type parameter.

---

## E-SEM-00050 — Invalid Generic Type

- Severity: Error
- Default Detail: Expected a type argument
- Description: The real type is missing from the generic type argument list.

Incorrect Example:

```kinal
Identity<>(1);
```

Correct Example:

```kinal
Identity<int>(1);
```

Reason: Each position within the angle brackets must be a legal type name.

Fix: Fill the vacant position with a specific type.
