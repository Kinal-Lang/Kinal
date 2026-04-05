# Block — 可调度代码块对象

`Block` 是 Kinal 的核心特性之一。它将一段代码封装为一个**可调度、可跳转的一等公民对象**，支持精确的区间执行、内部跳转、捕获外层变量，并可像普通对象一样赋值、传递、存入集合。

`Block` 的类型为 `IO.Type.Object.Block`，继承自 `IO.Type.Object.Class`。

---

## 1. 声明语法

### 命名 Block 声明（推荐）

在语句位置直接使用 `Block 名称 </.../>` 声明：

```kinal
Block MyBlock
</
    IO.Console.PrintLine("hello from block");
/>
```

这等价于声明一个名为 `MyBlock` 的 `IO.Type.Object.Block` 局部变量，Block 体不会自动执行。

### 赋值给变量

```kinal
IO.Type.Object.Block b = Block MyBlock
</
    IO.Console.PrintLine("hello");
/>;
```

### 匿名 Block

不提供名称的 Block，通常直接赋值给变量：

```kinal
IO.Type.Object.Block b = Block
</
    IO.Console.PrintLine("anonymous");
/>;
```

### 安全修饰

Block 声明可以添加安全级别前缀（默认为 `Safe`）：

```kinal
Safe Block   MyBlock </.../>    // 默认，可以省略 Safe
Trusted Block TBlock </.../>
Unsafe Block  UBlock </.../>
```

---

## 2. 执行 API

Block 不能像函数一样用 `()` 调用，必须通过以下四个 API 执行。

### `Run()` — 从头执行到尾

从 IP=0（块入口）执行到末尾：

```kinal
Block Greet
</
    IO.Console.PrintLine("step-A");
    IO.Console.PrintLine("step-B");
/>

Greet.Run();
// 输出:
// step-A
// step-B
```

### `Jump(ip)` — 从指定位置执行到尾

从 IP 为 `ip` 的位置开始，执行到末尾：

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
// 从 A 执行到末尾
// 输出:
// 1
// 2
```

### `RunUntil(ip)` — 从头执行到指定位置前

从 IP=0 开始，执行到 IP=ip 之前停止（不含 ip 区域的代码）：

```kinal
Seg.RunUntil(Seg.B);
// 执行 [0, B) 区间，即"0"和"1"
// 输出:
// 0
// 1
```

### `JumpAndRunUntil(startIp, endIp)` — 执行指定区间

从 `startIp` 开始，执行到 `endIp` 之前停止：

```kinal
Seg.JumpAndRunUntil(Seg.A, Seg.B);
// 仅执行 [A, B) 之间的代码
// 输出:
// 1
```

### 四个 API 总结

| API | 执行范围 |
|-----|---------|
| `Run()` | `[0, 末尾]` |
| `Jump(ip)` | `[ip, 末尾]` |
| `RunUntil(ip)` | `[0, ip)` |
| `JumpAndRunUntil(s, e)` | `[s, e)` |

---

## 3. Record — 位置标签

`Record 名称;` 在 Block 体内声明一个位置标签，IP 从 1 开始依次递增。

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

IO.Console.PrintLine(Demo.R1);   // 输出: 1
IO.Console.PrintLine(Demo.R2);   // 输出: 2
IO.Console.PrintLine(Demo.R3);   // 输出: 3
```

- `Demo.R1`、`Demo.R2`、`Demo.R3` 是 `int` 类型的只读字段
- IP=0 为块入口（第一行代码之前）
- 也可以直接使用整数 IP：`Demo.Jump(2)` 等同于 `Demo.Jump(Demo.R2)`

---

## 4. Jump 语句 — 内部跳转

`Jump 目标;` 是 Block **体内专用**的无条件跳转语句，可以向前或向后跳转。

```kinal
Block Skipper
</
    IO.Console.PrintLine("start");
    Jump End;
    IO.Console.PrintLine("SKIPPED");    // 不会执行
    Record Middle;
    IO.Console.PrintLine("middle");     // 不会执行（被跳过）
    Record End;
    IO.Console.PrintLine("end");
/>

Skipper.Run();
// 输出:
// start
// end
```

`Jump` 目标可以是：
- Record 名称：`Jump MyLabel;`
- 整数表达式：`Jump 3;`

---

## 5. IP 区间执行实战

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

    // 仅执行初始化
    Pipeline.RunUntil(Pipeline.Step1);
    IO.Console.PrintLine("---");

    // 从 Step2 到 Step3
    Pipeline.JumpAndRunUntil(Pipeline.Step2, Pipeline.Step3);
    IO.Console.PrintLine("---");

    // 执行 Step1 到末尾
    Pipeline.Jump(Pipeline.Step1);

    Return 0;
}
```

编译验证输出：

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

## 6. 闭包捕获（引用捕获）

Block 以**引用方式**捕获外层作用域的变量。这意味着 Block 执行时看到的是变量的**当前值**，而非声明时的快照。

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

    b.Run();         // 输出: idle

    state = "running";
    b.Run();         // 输出: running

    state = "done";
    b.Run();         // 输出: done

    Return 0;
}
```

Block 可以重复执行，每次都反映变量的最新值。

---

## 7. 内部局部声明

Block 体中可以声明以下局部成员，它们的作用域限于 Block 内部：

| 可声明内容 | 说明 |
|-----------|------|
| 局部变量 | `Var x = 1;` |
| 局部函数 | `Function int Add(int a, int b) {...}` |
| 局部类 | `Class TempClass {...}` |
| 局部结构体 | `Struct Pair { int A; int B; }` |
| 局部枚举 | `Enum Flag { Off, On }` |
| 局部接口 | `Interface ITemp { ... }` |
| 嵌套 Block | `Block Inner </.../>` |

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
        IO.Console.PrintLine(result);    // 输出: 42
    />

    WithLocalFunc.Run();
    Return 0;
}
```

---

## 8. Block 作为对象

Block 是 `IO.Type.Object.Block` 类型的对象，可以：

### 赋值给变量

```kinal
IO.Type.Object.Block a = Block </  IO.Console.PrintLine("A"); />;
IO.Type.Object.Block b = a;      // 共享同一 Block 对象
b.Run();                          // 输出: A
```

### 存入数组

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

### 向上转型为 Class

```kinal
IO.Type.Object.Block b = Block </ IO.Console.PrintLine("hi"); />;
IO.Type.Object.Class c = b;     // 合法，向上转型
```

---

## 9. Unsafe Block — 嵌套控制流中的 Record

在 `Safe`/`Trusted` Block 中，`Record` 只能放置在 Block 的**顶层**语句位置，不能放在 `If`/`While`/`For` 等控制流块内部。

如果需要在嵌套控制流内部使用 `Record`，必须声明为 `Unsafe Block`：

```kinal
// 错误：Safe Block 不允许 Record 在嵌套控制流内
Block Bad
</
    If (true)


    {
        Record A;    // 编译错误: E-SEM-00045 Unsafe Block
    }
/>
```

```kinal
// 正确：使用 Unsafe Block
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
// 输出:
// before
// inner
// after
```

---

## 10. Block 的限制

| 限制 | 说明 |
|------|------|
| 不能用 `()` 调用 | 必须使用 `Run()`/`Jump()` 等 API |
| 不能使用 `Return` | Block 体内不允许 `Return` 语句 |
| `Record` 在嵌套块内需 `Unsafe` | `Safe`/`Trusted` Block 不允许 |
| 无返回值 | Block 不返回值 |

---

## 11. 完整综合示例

以下示例演示了 Block 在状态机模式中的应用：

```kinal
Unit App.StateMachine;
Get IO.Console;

Static Function int Main()


{
    Var phase = 0;

    Block StateMachine
    </
        Record Idle;
        IO.Console.PrintLine("状态: 空闲");
        Record Running;
        IO.Console.PrintLine("状态: 运行中");
        Record Paused;
        IO.Console.PrintLine("状态: 已暂停");
        Record Stopped;
        IO.Console.PrintLine("状态: 已停止");
    />

    // 直接跳转到 Running 状态
    StateMachine.Jump(StateMachine.Running);
    IO.Console.PrintLine("---");

    // 只执行 Running 到 Paused 区间
    StateMachine.JumpAndRunUntil(StateMachine.Running, StateMachine.Paused);
    IO.Console.PrintLine("---");

    // 从 Paused 执行到末尾
    StateMachine.Jump(StateMachine.Paused);

    Return 0;
}
```

输出：

```
状态: 运行中
状态: 已暂停
状态: 已停止
---
状态: 运行中
---
状态: 已暂停
状态: 已停止
```

---

## 相关

- [Meta（元数据注解）](meta.md)
- [控制流](control-flow.md)
- [异步编程](async.md)
