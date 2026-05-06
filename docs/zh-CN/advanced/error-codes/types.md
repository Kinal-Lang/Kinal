# 类型系统诊断

类型推断、类型匹配、数组类型和转换相关诊断。

本文中的示例都是最小复现片段，用来说明该错误码最常见的触发方式；少数更偏内部的诊断会使用最接近用户代码的表面写法来说明。

---

## 错误码目录

| 错误码 | 标题 | 默认详情 |
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

- 级别：错误
- 默认详情：Cannot cast Object values; use Object conversions instead
- 说明：对象体系不能按普通数值 cast 规则直接转换成基础类型。

错误示例：

```kinal
IO.Type.Object.Function fn = IO.Console.PrintLine;
int value = [int](fn);
```

正确示例：

```kinal
IO.Type.Object.Function fn = IO.Console.PrintLine;
IO.Type.Object.Class obj = fn;
string text = obj.ToString();
```

原因：`IO.Type.Object.*` 有独立的对象语义，不适用普通标量转换。

修复方式：对对象使用对象 API 或对象层允许的赋值 / 向上转型。

---

## E-TYP-00002 — Invalid Cast

- 级别：错误
- 默认详情：Cannot cast to Object types; use Object conversions instead
- 说明：不能把任意值用普通 cast 直接变成对象体系类型。

错误示例：

```kinal
IO.Type.Object.Class obj = [IO.Type.Object.Class](42);
```

正确示例：

```kinal
IO.Type.Object.Function fn = IO.Console.PrintLine;
IO.Type.Object.Class obj = fn;
```

原因：对象层类型不是普通 cast 的落点，而是运行时对象系统的一部分。

修复方式：通过合法对象值赋给对象类型，而不是直接 cast 基础值。

---

## E-TYP-00003 — Invalid Type

- 级别：错误
- 默认详情：Array element name missing
- 说明：数组声明缺少变量名或元素名。

错误示例：

```kinal
int[] = { 1, 2, 3 };
```

正确示例：

```kinal
int[] values = { 1, 2, 3 };
```

原因：数组声明仍然是变量声明，类型后面必须有名称。

修复方式：在数组类型后补上变量名。

---

## E-TYP-00004 — Invalid Type

- 级别：错误
- 默认详情：Array element type unknown
- 说明：数组元素类型无法解析。

错误示例：

```kinal
MissingType[] values = {};
```

正确示例：

```kinal
int[] values = {};
```

原因：数组类型依赖元素类型先合法存在。

修复方式：先定义元素类型，或改成已存在的类型名。

---

## E-TYP-00005 — Invalid Type

- 级别：错误
- 默认详情：Type not specified
- 说明：当前位置需要一个明确类型，但没有给出。

错误示例：

```kinal
Function Add(a, b)
{
    Return a + b;
}
```

正确示例：

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}
```

原因：Kinal 不会在所有声明位置都替你补全类型。

修复方式：在要求显式类型的地方补写类型名。

---

## E-TYP-00006 — Invalid Type

- 级别：错误
- 默认详情：Void not allowed here
- 说明：当前上下文不能使用 `void` 作为值类型。

错误示例：

```kinal
void value;
```

正确示例：

```kinal
int value;
```

原因：`void` 表示“没有值”，因此不能用来声明普通变量或需要实际值的位置。

修复方式：改成具体数据类型，或把该位置改成不需要值的声明。

---

## E-TYP-00007 — Invalid Type

- 级别：错误
- 默认详情：any[] requires explicit length
- 说明：未初始化的 `any[]` 需要显式长度。

错误示例：

```kinal
any[] values;
```

正确示例：

```kinal
any values[2];
```

原因：`any[]` 没有元素初始化器时，编译器无法推导出需要分配多大存储。

修复方式：改成定长 `any[n]`，或直接提供初始化器。

---

## E-TYP-00008 — Type Inference Failed

- 级别：错误
- 默认详情：Array element type requires initializer
- 说明：数组元素类型推断依赖初始化器。

错误示例：

```kinal
Var values[];
```

正确示例：

```kinal
Var values[] = { 1, 2, 3 };
```

原因：没有初始化器时，`Var` 无法知道数组元素是什么类型。

修复方式：为 `Var` 数组提供初始化器，或直接写出元素类型。

---

## E-TYP-00009 — Type Inference Failed

- 级别：错误
- 默认详情：Cannot infer element type from empty array
- 说明：空数组字面量本身不足以推断元素类型。

错误示例：

```kinal
Var values = {};
```

正确示例：

```kinal
int[] values = {};
```

原因：空数组没有任何元素能提供类型线索。

修复方式：显式声明数组类型。

---

## E-TYP-00010 — Type Inference Failed

- 级别：错误
- 默认详情：Var requires initializer
- 说明：`Var` 必须配合初始化器才能推断类型。

错误示例：

```kinal
Var value;
```

正确示例：

```kinal
Var value = 42;
```

原因：`Var` 的类型完全来自右侧初始化表达式。

修复方式：给 `Var` 提供初始化器，或改用显式类型声明。

---

## E-TYP-00011 — Type Mismatch

- 级别：错误
- 默认详情：Type mismatch
- 说明：赋值、返回或调用中的实际类型和期望类型不兼容。

错误示例：

```kinal
int value = "42";
```

正确示例：

```kinal
int value = [int]("42");
```

原因：编译器在类型检查时发现源类型不能直接赋给目标类型。

修复方式：修改表达式类型、调整目标类型，或显式做合法转换。

---

## E-TYP-00012 — Unsupported Type

- 级别：错误
- 默认详情：Type not supported yet
- 说明：当前类型形式尚未被编译器支持。

错误示例：

```kinal
// 这里使用了当前版本尚未支持的类型形式
Unsupported<T> value;
```

正确示例：

```kinal
Struct Pair
{
    int Left;
    int Right;
}

Pair value;
```

原因：语言设计里可能预留了该类型方向，但当前实现还没有完整支持。

修复方式：改用现阶段已支持的类型组合，或等待该类型能力落地。
