# Block — Schedulable Code Block Objects

`Block` is one of Kinal's core features. It encapsulates a section of code as a **first-class, schedulable, jump-capable object** that supports precise range execution, internal jumps, capturing outer variables, and can be assigned to variables, passed around, and stored in collections just like ordinary objects.

The type of a `Block` is `IO.Type.Object.Block`, which inherits from `IO.Type.Object.Class`.

---

## 1. Declaration Syntax

### Named Block Declaration (Recommended)

Use `Block name </.../>` directly at a statement position:

```kinal
Block MyBlock
</
    IO.Console.PrintLine("hello from block");
/>
```

This is equivalent to declaring a local variable named `MyBlock` of type `IO.Type.Object.Block`. The Block body does not execute automatically.

### Assigning to a Variable

```kinal
IO.Type.Object.Block b = Block MyBlock
</
    IO.Console.PrintLine("hello");
/>;
```

### Anonymous Block

A Block without a name, typically assigned directly to a variable:

```kinal
IO.Type.Object.Block b = Block
</
    IO.Console.PrintLine("anonymous");
/>;
```

### Safety Modifiers

Block declarations can be prefixed with a safety level (defaults to `Safe`):

```kinal
Safe Block   MyBlock </.../>    // default, Safe can be omitted
Trusted Block TBlock </.../>
Unsafe Block  UBlock </.../>
```

---

## 2. Execution API

Blocks cannot be called like functions with `()`; they must be executed through the following four APIs.

### `Run()` — Execute from Start to End

Executes from IP=0 (block entry) to the end:

```kinal
Block Greet
</
    IO.Console.PrintLine("step-A");
    IO.Console.PrintLine("step-B");
/>

Greet.Run();
// Output:
// step-A
// step-B
```

### `Jump(ip)` — Execute from a Position to the End

Starts execution at the position with IP `ip` and runs to the end:

```kinal
Block Seg
</
    IO.Console.PrintLine("0");
    Record A;
    IO.Console.PrintLine("1");
    Record B;
    IO.Console.PrintLine("2");
/>

Seg.Jump(Seg.A);
// Executes from A to the end
// Output:
// 1
// 2
```

### `RunUntil(ip)` — Execute from Start to Before a Position

Starts from IP=0 and stops before IP=ip (the code at the ip position is not included):

```kinal
Seg.RunUntil(Seg.B);
// Executes the [0, B) interval, i.e., "0" and "1"
// Output:
// 0
// 1
```

### `JumpAndRunUntil(startIp, endIp)` — Execute a Specified Range

Starts from `startIp` and stops before `endIp`:

```kinal
Seg.JumpAndRunUntil(Seg.A, Seg.B);
// Executes only the code between [A, B)
// Output:
// 1
```

### Summary of the Four APIs

| API | Execution Range |
|-----|----------------|
| `Run()` | `[0, end]` |
| `Jump(ip)` | `[ip, end]` |
| `RunUntil(ip)` | `[0, ip)` |
| `JumpAndRunUntil(s, e)` | `[s, e)` |

---

## 3. Record — Position Labels

`Record name;` declares a position label inside a Block body; IPs increment from 1.

```kinal
Block Demo
</
    IO.Console.PrintLine("before-R1");
    Record R1;
    IO.Console.PrintLine("after-R1");
    Record R2;
    IO.Console.PrintLine("after-R2");
    Record R3;
    IO.Console.PrintLine("after-R3");
/>

IO.Console.PrintLine(Demo.R1);   // Output: 1
IO.Console.PrintLine(Demo.R2);   // Output: 2
IO.Console.PrintLine(Demo.R3);   // Output: 3
```

- `Demo.R1`, `Demo.R2`, `Demo.R3` are read-only fields of type `int`
- IP=0 is the block entry (before the first line of code)
- Integer IPs can be used directly: `Demo.Jump(2)` is equivalent to `Demo.Jump(Demo.R2)`

---

## 4. The Jump Statement — Internal Jumps

`Jump target;` is an unconditional jump statement **used exclusively inside Block bodies**, supporting both forward and backward jumps.

```kinal
Block Skipper
</
    IO.Console.PrintLine("start");
    Jump End;
    IO.Console.PrintLine("SKIPPED");    // will not execute
    Record Middle;
    IO.Console.PrintLine("middle");     // will not execute (jumped over)
    Record End;
    IO.Console.PrintLine("end");
/>

Skipper.Run();
// Output:
// start
// end
```

Jump targets can be:
- A Record name: `Jump MyLabel;`
- An integer expression: `Jump 3;`

---

## 5. IP Range Execution in Practice

```kinal
Unit App.BlockRegion;
Get IO.Console;

Static Function int Main()


{
    Block Pipeline
    </
        IO.Console.PrintLine("init");
        Record Step1;
        IO.Console.PrintLine("step1: load");
        Record Step2;
        IO.Console.PrintLine("step2: parse");
        Record Step3;
        IO.Console.PrintLine("step3: emit");
        Record Done;
        IO.Console.PrintLine("done");
    />

    // Execute only initialization
    Pipeline.RunUntil(Pipeline.Step1);
    IO.Console.PrintLine("---");

    // From Step2 to Step3
    Pipeline.JumpAndRunUntil(Pipeline.Step2, Pipeline.Step3);
    IO.Console.PrintLine("---");

    // Execute from Step1 to the end
    Pipeline.Jump(Pipeline.Step1);

    Return 0;
}
```

Expected output:

```
init
---
step2: parse
---
step1: load
step2: parse
step3: emit
done
```

---

## 6. Closure Capture (Capture by Reference)

Blocks capture variables from the outer scope **by reference**. This means the Block sees the **current value** of the variable when it executes, not a snapshot at the time of declaration.

```kinal
Unit App.BlockCapture;
Get IO.Console;

Static Function int Main()


{
    Var state = "idle";

    IO.Type.Object.Block b = Block
    </
        IO.Console.PrintLine(state);
    />;

    b.Run();         // Output: idle

    state = "running";
    b.Run();         // Output: running

    state = "done";
    b.Run();         // Output: done

    Return 0;
}
```

Blocks can be executed repeatedly; each execution reflects the latest value of the variable.

---

## 7. Internal Local Declarations

The following local members can be declared inside a Block body; their scope is limited to within the Block:

| Declarable Content | Description |
|-------------------|-------------|
| Local variables | `Var x = 1;` |
| Local functions | `Function int Add(int a, int b) {...}` |
| Local classes | `Class TempClass {...}` |
| Local structs | `Struct Pair { int A; int B; }` |
| Local enums | `Enum Flag { Off, On }` |
| Local interfaces | `Interface ITemp { ... }` |
| Nested Blocks | `Block Inner </.../>` |

```kinal
Unit App.BlockLocal;
Get IO.Console;

Static Function int Main()


{
    Block WithLocalFunc
    </
        Function int Multiply(int a, int b)
        {
            Return a * b;
        }

        int result = Multiply(6, 7);
        IO.Console.PrintLine(result);    // Output: 42
    />

    WithLocalFunc.Run();
    Return 0;
}
```

---

## 8. Block as an Object

A Block is an object of type `IO.Type.Object.Block`, which can be:

### Assigned to a Variable

```kinal
IO.Type.Object.Block a = Block </  IO.Console.PrintLine("A"); />;
IO.Type.Object.Block b = a;      // share the same Block object
b.Run();                          // Output: A
```

### Stored in an Array

```kinal
IO.Type.Object.Block[] blocks =
{
    Block </ IO.Console.PrintLine("first"); />,
    Block </ IO.Console.PrintLine("second"); />,
    Block </ IO.Console.PrintLine("third"); />
};

int i = 0;
While (i < blocks.Length())


{
    blocks[i].Run();
    i = i + 1;
}
```

### Upcast to Class

```kinal
IO.Type.Object.Block b = Block </ IO.Console.PrintLine("hi"); />;
IO.Type.Object.Class c = b;     // valid, upcast
```

---

## 9. Unsafe Block — Record in Nested Control Flow

In `Safe`/`Trusted` Blocks, `Record` can only be placed at the **top-level** statement positions of the Block body; it cannot be placed inside control flow blocks such as `If`/`While`/`For`.

To use `Record` inside nested control flow, the Block must be declared as `Unsafe Block`:

```kinal
// Error: Safe Block does not allow Record inside nested control flow
Block Bad
</
    If (true)


    {
        Record A;    // compile error: E-SEM-00045 Unsafe Block
    }
/>
```

```kinal
// Correct: use Unsafe Block
Unsafe Block Good
</
    IO.Console.PrintLine("before");
    If (true)


    {
        Record Inner;
        IO.Console.PrintLine("inner");
    }
    IO.Console.PrintLine("after");
/>

Good.Run();
// Output:
// before
// inner
// after
```

---

## 10. Block Limitations

| Limitation | Description |
|------------|-------------|
| Cannot be called with `()` | Must use `Run()`/`Jump()` etc. |
| Cannot use `Return` | `Return` statements are not allowed inside Block bodies |
| `Record` inside nested blocks requires `Unsafe` | Not allowed in `Safe`/`Trusted` Blocks |
| No return value | Blocks do not return values |

---

## 11. Complete Comprehensive Example

The following example demonstrates the use of Block in a state machine pattern:

```kinal
Unit App.StateMachine;
Get IO.Console;

Static Function int Main()


{
    Var phase = 0;

    Block StateMachine
    </
        Record Idle;
        IO.Console.PrintLine("State: Idle");
        Record Running;
        IO.Console.PrintLine("State: Running");
        Record Paused;
        IO.Console.PrintLine("State: Paused");
        Record Stopped;
        IO.Console.PrintLine("State: Stopped");
    />

    // Jump directly to the Running state
    StateMachine.Jump(StateMachine.Running);
    IO.Console.PrintLine("---");

    // Execute only the Running to Paused range
    StateMachine.JumpAndRunUntil(StateMachine.Running, StateMachine.Paused);
    IO.Console.PrintLine("---");

    // Execute from Paused to the end
    StateMachine.Jump(StateMachine.Paused);

    Return 0;
}
```

Output:

```
State: Running
State: Paused
State: Stopped
---
State: Running
---
State: Paused
State: Stopped
```

---

## Related

- [Meta (Metadata Annotations)](meta.md)
- [Control Flow](control-flow.md)
- [Async Programming](async.md)
