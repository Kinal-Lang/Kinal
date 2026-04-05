# IO.Collection — 集合类型

Kinal 内置三种集合类型：`list`（有序列表）、`dict`（键值字典）、`set`（无重复集合）。它们均为引用类型，需要先创建再使用。

语言层面提供了 `list`、`dict`、`set` 作为对应模块的内置别名，可直接作为类型名使用。

## 导入

集合类型由编译器内置注入，无需手动导入。也可以显式引入命名空间：

```kinal
Get IO.Collection;
```

---

## list — 有序列表

`list` 是动态大小的有序集合，等价于其他语言的数组列表（ArrayList / Vec）。

### 创建

```kinal
list names = IO.Collection.list.Create();
```

或字面量方式（如果语言版本支持）：

```kinal
list nums = [1, 2, 3];
```

### 函数参考

| 函数 | 签名 | 说明 |
|------|------|------|
| `Create()` | `→ list` | 创建空列表 |
| `Add(l, value)` | `list, any → void` | 末尾追加元素 |
| `Fetch(l, index)` | `list, int → any` | 按索引获取（越界抛出异常） |
| `TryFetch(l, index)` | `list, int → any` | 按索引获取（越界返回 `null`） |
| `Set(l, index, value)` | `list, int, any → void` | 按索引设置值 |
| `Insert(l, index, value)` | `list, int, any → void` | 在指定位置插入 |
| `RemoveAt(l, index)` | `list, int → void` | 删除指定位置元素 |
| `Pop(l)` | `list → any` | 移除并返回最后一个元素 |
| `Peek(l)` | `list → any` | 返回最后一个元素（不移除） |
| `Contains(l, value)` | `list, any → bool` | 是否包含指定值 |
| `IndexOf(l, value)` | `list, any → int` | 首次出现索引（-1 表示不存在） |
| `Count(l)` | `list → int` | 元素数量 |
| `IsEmpty(l)` | `list → bool` | 是否为空 |
| `Clear(l)` | `list → void` | 清空列表 |

### 示例

```kinal
list fruits = IO.Collection.list.Create();
IO.Collection.list.Add(fruits, "apple");
IO.Collection.list.Add(fruits, "banana");
IO.Collection.list.Add(fruits, "cherry");

int count = IO.Collection.list.Count(fruits);  // 3

string first = [string](IO.Collection.list.Fetch(fruits, 0));  // "apple"

IO.Collection.list.Set(fruits, 1, "blueberry");
// fruits = ["apple", "blueberry", "cherry"]

IO.Collection.list.RemoveAt(fruits, 0);
// fruits = ["blueberry", "cherry"]

bool has = IO.Collection.list.Contains(fruits, "cherry");  // true
```

---

## dict — 键值字典

`dict` 是哈希映射，存储键值对，键和值均为 `any` 类型。

### 创建

```kinal
dict config = IO.Collection.dict.Create();
```

### 函数参考

| 函数 | 签名 | 说明 |
|------|------|------|
| `Create()` | `→ dict` | 创建空字典 |
| `Set(d, key, value)` | `dict, any, any → void` | 设置键值（不存在则新增，已存在则更新） |
| `Fetch(d, key)` | `dict, any → any` | 获取指定键的值（键不存在时抛出异常） |
| `TryFetch(d, key)` | `dict, any → any` | 获取值（键不存在时返回 `null`） |
| `Contains(d, key)` | `dict, any → bool` | 是否包含指定键 |
| `Remove(d, key)` | `dict, any → void` | 删除指定键 |
| `Count(d)` | `dict → int` | 条目数量 |
| `IsEmpty(d)` | `dict → bool` | 是否为空 |
| `Keys(d)` | `dict → list` | 所有键的列表 |
| `Values(d)` | `dict → list` | 所有值的列表 |

### 示例

```kinal
dict scores = IO.Collection.dict.Create();
IO.Collection.dict.Set(scores, "Alice", 95);
IO.Collection.dict.Set(scores, "Bob",   82);
IO.Collection.dict.Set(scores, "Carol", 90);

int alice = [int](IO.Collection.dict.Fetch(scores, "Alice"));  // 95

bool hasDave = IO.Collection.dict.Contains(scores, "Dave");  // false

any bob = IO.Collection.dict.TryFetch(scores, "Bob");  // 82 or null

IO.Collection.dict.Remove(scores, "Carol");

list keys = IO.Collection.dict.Keys(scores);
// ["Alice", "Bob"]
```

---

## set — 无重复集合

`set` 存储唯一元素，重复添加同一值不会产生重复项。

### 创建

```kinal
set tags = IO.Collection.set.Create();
```

### 函数参考

| 函数 | 签名 | 说明 |
|------|------|------|
| `Create()` | `→ set` | 创建空集合 |
| `Add(s, value)` | `set, any → void` | 添加元素（已存在则忽略） |
| `Contains(s, value)` | `set, any → bool` | 是否包含指定值 |
| `Remove(s, value)` | `set, any → void` | 删除指定元素 |
| `Count(s)` | `set → int` | 元素数量 |
| `IsEmpty(s)` | `set → bool` | 是否为空 |
| `Clear(s)` | `set → void` | 清空集合 |
| `Values(s)` | `set → list` | 返回所有元素的列表 |

### 示例

```kinal
set seen = IO.Collection.set.Create();
IO.Collection.set.Add(seen, "apple");
IO.Collection.set.Add(seen, "banana");
IO.Collection.set.Add(seen, "apple");   // 重复，被忽略

int count = IO.Collection.set.Count(seen);  // 2

bool hasApple = IO.Collection.set.Contains(seen, "apple");  // true

list all = IO.Collection.set.Values(seen);  // ["apple", "banana"]
```

---

## 综合示例

```kinal
Unit App.CollectionDemo;

Get IO.Console;
Get IO.Collection;

Static Function int Main()
{
    // list：统计单词
    list words = IO.Collection.list.Create();
    IO.Collection.list.Add(words, "hello");
    IO.Collection.list.Add(words, "world");
    IO.Collection.list.Add(words, "hello");

    IO.Console.PrintLine(IO.Collection.list.Count(words));  // 3

    // dict：单词计数
    dict counts = IO.Collection.dict.Create();
    int i = 0;
    While (i < IO.Collection.list.Count(words))


    {
        string w = [string](IO.Collection.list.Fetch(words, i));
        If (IO.Collection.dict.Contains(counts, w))


        {
            int c = [int](IO.Collection.dict.Fetch(counts, w));
            IO.Collection.dict.Set(counts, w, c + 1);
        } else
        {
            IO.Collection.dict.Set(counts, w, 1);
        }
        i = i + 1;
    }
    IO.Console.PrintLine(IO.Collection.dict.Fetch(counts, "hello"));  // 2

    // set：去重
    set unique = IO.Collection.set.Create();
    int j = 0;
    While (j < IO.Collection.list.Count(words))


    {
        IO.Collection.set.Add(unique, IO.Collection.list.Fetch(words, j));
        j = j + 1;
    }
    IO.Console.PrintLine(IO.Collection.set.Count(unique));  // 2

    Return 0;
}
```

## 相关

- [类型系统](../language/types.md) — `list`/`dict`/`set` 作为内置类型别名
- [异常处理](../language/exceptions.md) — `Fetch` 越界和键不存在时抛出异常
