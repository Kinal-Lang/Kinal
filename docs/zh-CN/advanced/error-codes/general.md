# 通用与泛型诊断

构造函数、常量、数组长度、泛型分发等通用规则。

本文中的示例都是最小复现片段，用来说明该错误码最常见的触发方式；少数更偏内部的诊断会使用最接近用户代码的表面写法来说明。

---

## 错误码目录

| 错误码 | 标题 | 默认详情 |
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

- 级别：错误
- 默认详情：Class must implement abstract base method
- 说明：派生类没有把抽象基类要求的方法实现完整。

错误示例：

```kinal
Abstract Class BaseType
{
    Public Abstract Function int Value();
}

Class Derived By BaseType
{
}
```

正确示例：

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

原因：只要派生类仍然是可实例化的普通类，就必须兑现抽象基类的全部抽象契约。

修复方式：实现缺失的抽象成员，或者也把派生类声明为 `Abstract Class`。

---

## E-GEN-00002 — Array Length

- 级别：错误
- 默认详情：Initializer has too many elements
- 说明：定长数组初始化时，元素个数不能超过声明长度。

错误示例：

```kinal
int values[2] = { 1, 2, 3 };
```

正确示例：

```kinal
int values[2] = { 1, 2 };
```

原因：数组长度在类型里已经固定，初始化器元素更多时会越界。

修复方式：减少初始化器元素，或把数组长度改成能容纳这些元素的值。

---

## E-GEN-00003 — Byte Overflow

- 级别：错误
- 默认详情：byte range is 0..255
- 说明：`byte` 字面量必须落在 `0..255` 范围内。

错误示例：

```kinal
byte value = 300;
```

正确示例：

```kinal
byte value = 255;
```

原因：`byte` 只有 8 位，超出范围的值无法无损表示。

修复方式：改成合法范围内的值，或使用更大的整数类型。

---

## E-GEN-00004 — Const Assignment

- 级别：错误
- 默认详情：Cannot assign to const variable
- 说明：`const` 变量不能被重新赋值。

错误示例：

```kinal
Const int limit = 10;
limit = 20;
```

正确示例：

```kinal
Const int limit = 10;
int next = limit + 10;
```

原因：`const` 的语义是单次初始化后不可再绑定新值。

修复方式：如果变量需要重新赋值，把它改成普通变量；否则只读取它，不要再写入。

---

## E-GEN-00005 — Const Assignment

- 级别：错误
- 默认详情：Cannot modify const variable
- 说明：常量值也不能通过自增、自减或复合操作被修改。

错误示例：

```kinal
Const int step = 1;
step++;
```

正确示例：

```kinal
Const int step = 1;
int next = step + 1;
```

原因：这类操作本质上还是写回原变量，因此同样违反常量不可变规则。

修复方式：对常量使用派生值而不是原地修改。

---

## E-GEN-00006 — Const Requires Init

- 级别：错误
- 默认详情：Const variable must have initializer
- 说明：`const` 声明时必须立即初始化。

错误示例：

```kinal
Const int limit;
```

正确示例：

```kinal
Const int limit = 10;
```

原因：常量一旦声明就必须有确定值，否则后续无法保证它真正不可变。

修复方式：在声明处直接提供初始化器。

---

## E-GEN-00007 — Constructor

- 级别：错误
- 默认详情：Constructor not found
- 说明：`New` 调用没有找到匹配的构造函数签名。

错误示例：

```kinal
Class Counter
{
    Public Constructor(int start)
    {
    }
}

Counter c = New Counter();
```

正确示例：

```kinal
Class Counter
{
    Public Constructor(int start)
    {
    }
}

Counter c = New Counter(1);
```

原因：构造函数解析和普通函数解析一样，实参类型、个数或默认值都必须能对上。

修复方式：传入正确的参数，或者补充对应签名的构造函数。

---

## E-GEN-00008 — Conversion Syntax Changed

- 级别：错误
- 默认详情：Type conversion now use [type](value) syntax instead of type(value). For example: [int]("123") instead of int("123")
- 说明：旧式 `type(value)` 转换语法已经废弃。

错误示例：

```kinal
int value = int("123");
```

正确示例：

```kinal
int value = [int]("123");
```

原因：Kinal 现在把显式类型转换统一成 `[type](expr)` 形式，以免和普通函数调用混淆。

修复方式：把旧写法批量替换成方括号转换语法。

---

## E-GEN-00009 — Conversion Syntax Changed

- 级别：错误
- 默认详情：Type conversion now use [type](value) syntax instead of type(value). 
- 说明：另一条兼容诊断，含义与旧转换语法变更相同。

错误示例：

```kinal
float value = float("1.5");
```

正确示例：

```kinal
float value = [float]("1.5");
```

原因：旧式调用式转换会和函数调用解析冲突，因此被统一替换。

修复方式：一律改成 `[type](value)`。

---

## E-GEN-00010 — Generic Arity

- 级别：错误
- 默认详情：Wrong number of generic type arguments
- 说明：泛型实参数量必须和定义中的类型参数数量一致。

错误示例：

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

int value = Identity<int, string>(1);
```

正确示例：

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

int value = Identity<int>(1);
```

原因：泛型实例化时，每个类型参数都需要一一对应，多一个或少一个都无法形成合法实例。

修复方式：核对泛型函数或类型定义，传入完全一致的类型参数数量。

---

## E-GEN-00011 — Generic Function

- 级别：错误
- 默认详情：Failed to resolve instantiated generic function
- 说明：显式实例化后的泛型函数没有成功解析出可调用目标。

错误示例：

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

int value = Identity<string>(1);
```

正确示例：

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

int value = Identity<int>(1);
```

原因：实例化后的签名和调用点实参不兼容，或者推导结果无法落到一个合法实现上。

修复方式：检查显式类型参数和实参类型是否匹配，必要时改成正确的显式类型列表。

---

## E-GEN-00012 — Generic Function

- 级别：错误
- 默认详情：Generic call requires explicit type arguments
- 说明：某些泛型调用无法完全依靠实参推导，必须显式给出类型参数。

错误示例：

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

Var fn = Identity;
int value = fn(1);
```

正确示例：

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

int value = Identity<int>(1);
```

原因：当调用点缺少足够上下文时，编译器无法可靠推出 `T`。

修复方式：显式写出 `<T>`，或调整代码让实参类型能唯一决定泛型参数。

---

## E-GEN-00013 — Generic Function

- 级别：错误
- 默认详情：Generic function reference requires explicit type arguments
- 说明：获取泛型函数引用时，也需要先显式实例化它。

错误示例：

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

IO.Type.Object.Function fn = Identity;
```

正确示例：

```kinal
Function T Identity<T>(T value)
{
    Return value;
}

IO.Type.Object.Function fn = Identity<int>;
```

原因：函数对象必须指向一个已经确定参数和返回类型的具体实例，而不是抽象的泛型模板。

修复方式：在取引用时显式写出类型参数。

---

## E-GEN-00014 — Interface Method

- 级别：错误
- 默认详情：Class must implement interface method
- 说明：类实现接口时，必须补齐接口要求的方法。

错误示例：

```kinal
Interface IValue
{
    Function int Value();
}

Class Counter By IValue
{
}
```

正确示例：

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

原因：接口承诺是可检查的编译期契约，缺失任何方法都会让实现不完整。

修复方式：为接口中的每个方法提供兼容签名的公开实现。

---

## E-GEN-00015 — Interface Method

- 级别：错误
- 默认详情：Interface implementation must be public
- 说明：接口实现的方法必须是 `Public`。

错误示例：

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

正确示例：

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

原因：接口方法需要对外可见，否则通过接口类型调用时无法满足可访问性要求。

修复方式：把接口实现改成 `Public`。

---

## E-GEN-00016 — Scoped Symbol Required

- 级别：错误
- 默认详情：Imported symbol must be called with module or alias qualifier
- 说明：通过模块或别名导入的符号必须带限定名调用。

错误示例：

```kinal
Unit App.Main;

Get App.Util;

Static Function int Main(string[] args)
{
    Return Add(40, 2);
}
```

正确示例：

```kinal
Unit App.Main;

Get U By App.Util;

Static Function int Main(string[] args)
{
    Return U.Add(40, 2);
}
```

原因：模块导入不会把所有成员直接铺平到当前作用域；这样做是为了避免跨模块名称污染。

修复方式：使用完整模块名或别名限定调用目标。

---

## E-GEN-00017 — Unsupported Generic Method

- 级别：错误
- 默认详情：Generic methods are not supported
- 说明：当前编译器不支持泛型方法。

错误示例：

```kinal
Class Box
{
    Public Function T Map<T>(T value)
    {
        Return value;
    }
}
```

正确示例：

```kinal
Function T Map<T>(T value)
{
    Return value;
}
```

原因：当前泛型实现只覆盖顶层泛型函数，方法级泛型还没有落地到完整后端。

修复方式：把泛型逻辑提到顶层泛型函数，或改成非泛型实例方法。

---

## E-GEN-00018 — Unsupported Generic Local Function

- 级别：错误
- 默认详情：Generic local named functions are not supported yet
- 说明：局部命名函数当前不支持泛型。

错误示例：

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

正确示例：

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

原因：局部命名函数本身就比顶层函数多一层闭包环境；泛型再叠加上去时，当前实现还没有完整支持。

修复方式：把泛型局部函数提升到顶层，或者改成普通非泛型局部函数。

---

## E-GEN-00019 — Unsupported Generic Method

- 级别：错误
- 默认详情：Generic methods are not supported yet
- 说明：另一条更明确的提示：泛型方法暂未支持。

错误示例：

```kinal
Class Box
{
    Public Function T Echo<T>(T value)
    {
        Return value;
    }
}
```

正确示例：

```kinal
Class Box
{
    Public Function int Echo(int value)
    {
        Return value;
    }
}
```

原因：方法级泛型和虚调用、重载、实例分发的组合还没有被当前编译器完全实现。

修复方式：改用顶层泛型函数，或为需要的类型提供普通重载方法。
