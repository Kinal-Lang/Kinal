# IO.Collection：list、dict 与 set

Native 和 KinalVM 均支持下列全部 `list`、`dict` 和 `set` 操作，包括字典字面量和键值快照。参见[后端支持](../advanced/backend-support.md)。

这些内建引用类型可直接写作 list、dict、set。赋值共享集合，不克隆内容；使用前应创建集合。可以显式 `Get IO.Collection;`，内建别名无需另行导入包。

## 调用形式与创建

```kinal
list names = list.Create();
dict settings = dict.Create();
set tags = set.Create();
```

接收者和命名空间形式等价：`names.Add("Kinal")` 对应 `IO.Collection.list.Add(names, "Kinal")`。命名空间形式的首参数是接收者。以下表格列出接收者形式参数。

数组使用 `{ ... }` 字面量，与 list 不同；`list numbers = [1, 2, 3];` 不是支持的 list 字面量。使用 Create／Add。Dict 字面量采用字符串键和值：

```kinal
dict options = { "name": "Kinal", "enabled": true };
```

## list：有序 any 值

| 方法 | 结果 | 行为 |
|------|------|------|
| `Add(any value)` | `void` | 追加 |
| `Fetch(int index)` | `any` | 读取元素，越界时返回 null。 |
| `TryFetch(int index, any fallback)` | `any` | 读取或返回显式后备值 |
| `Set(int index, any value)` | `void` | 替换已有元素 |
| `Insert(int index, any value)` | `void` | 按索引插入 |
| `RemoveAt(int index)` | `any` | 移除并返回元素 |
| `Pop()` | `any` | 移除并返回末元素 |
| `Peek()` | `any` | 查看末元素，不移除 |
| `Contains(any value)` | `bool` | 使用集合值相等规则查询 |
| `IndexOf(any value)` | `int` | 首个匹配索引，缺失为 -1 |
| `Count()` | `int` | 元素数 |
| `IsEmpty()` | `bool` | 元素数是否为零 |
| `Clear()` | `void` | 清空元素 |

索引从零开始。`Fetch`、`RemoveAt` 越界时返回 null，空列表的 `Pop`、`Peek` 也返回 null。`TryFetch(index, fallback)` 只在索引不存在时返回后备值，已存入的 null 会原样返回。`Set` 更新已有元素，`Add` 和 `Insert` 可以增加元素。

`Clear`、`Pop` 和 `RemoveAt` 会释放列表对已删除元素的引用。当其他引用也消失后，GC 就可以回收该对象。调用方保留的返回值仍然有效。原生后端的列表在 `Clear` 后保留容量，后续添加元素时可以复用。

在 KinalVM 上，向列表末尾追加一个元素的均摊耗时为常数。在列表中间插入或删除时，后续元素仍需移动。字典扩容时也会为后续条目预留空间。已有的 `.knc` 换用新版 VM 即可受益，独立 VM 可执行文件需重新打包才能使用新版运行时。

## dict：字符串键、any 值

| 方法 | 结果 | 行为 |
|------|------|------|
| `Set(string key, any value)` | `void` | 插入或替换 |
| `Fetch(string key)` | `any` | 读取值，键不存在时返回 null。 |
| `TryFetch(string key, any fallback)` | `any` | 读取或返回显式后备值 |
| `Contains(string key)` | `bool` | 查询键是否存在 |
| `Remove(string key)` | `bool` | 是否移除了键 |
| `Count()` | `int` | 条目数 |
| `IsEmpty()` | `bool` | 条目数是否为零 |
| `Keys()` | `list` | 创建包含键的列表 |
| `Values()` | `list` | 创建包含值的列表 |

键是 string，不是任意 any。TryFetch 必须有后备实参，例如 `settings.TryFetch("missing", "default")`。存储 null 与缺失键不同，需要区分时使用 Contains。键／值枚举顺序不构成公开排序保证。

## set：唯一字符串

| 方法 | 结果 | 行为 |
|------|------|------|
| `Add(string value)` | `void` | 添加，重复字符串不增加数量 |
| `Contains(string value)` | `bool` | 查询成员 |
| `Remove(string value)` | `bool` | 是否移除了元素 |
| `Count()` | `int` | 元素数 |
| `IsEmpty()` | `bool` | 是否为空 |
| `Clear()` | `void` | 清空 |
| `Values()` | `list` | 创建包含字符串的列表 |

Set 元素限定为字符串，不是覆盖所有 Kinal 类型的泛型数学集合。不要依赖 Values 顺序；排序和深层语义比较由应用负责。

在 Native 和 KinalVM 上，删除字典键或集合成员会保留现有哈希表及其容量。删除时只需检查相邻的连续条目，移动查找路径经过空位的条目。对固定长度的键，散列均匀时预期耗时为常数，大量键碰撞时最坏可达线性。覆盖已有字典键、向集合重复添加成员也会复用当前表。

## 完整示例

```kinal
Unit Docs.Collections;
Get IO.Console;

Static Function int Main()
{
    list names = list.Create();
    names.Add("Kinal");
    names.Insert(0, "Hello");
    IO.Console.PrintLine(names.Fetch(0));
    IO.Console.PrintLine(names.TryFetch(10, "fallback"));
    IO.Console.PrintLine(names.Pop());
    IO.Console.PrintLine(names.Count());

    dict settings = { "name": "Kinal" };
    settings.Set("mode", "native");
    IO.Console.PrintLine(settings.TryFetch("mode", "unknown"));
    IO.Console.PrintLine(settings.TryFetch("missing", "default"));
    IO.Console.PrintLine(settings.Remove("missing"));

    set tags = set.Create();
    tags.Add("language");
    tags.Add("language");
    IO.Console.PrintLine(tags.Count());
    IO.Console.PrintLine(tags.Contains("language"));
    Return 0;
}
```

```text
Hello
fallback
Kinal
1
native
default
false
1
true
```

此示例可以在 Native 和 KinalVM 上运行。`list.Fetch` 和 `dict.Fetch` 均返回 `any`。读取已存入的整数可以写作 `[int](items.Fetch(index))`。

Foreach 当前面向数组／字符串，不是通用集合迭代协议。List 使用 Count／Fetch 遍历；Dict／set 可先取 Keys／Values。遍历时不要随意修改同一集合，除非算法明确定义索引变化。

## 相关文档

- [类型系统](../language/types.md)
- [JSON](json.md)
- [内存与生命周期](../advanced/memory-management.md)
- [后端支持](../advanced/backend-support.md)
