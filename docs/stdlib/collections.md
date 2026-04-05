# IO.Collection — Collection Types

Kinal has three built-in collection types: `list` (ordered list), `dict` (key-value dictionary), and `set` (unique-element set). They are all reference types and must be created before use.

At the language level, `list`, `dict`, and `set` are provided as built-in aliases for their corresponding modules and can be used directly as type names.

## Import

Collection types are injected as built-ins by the compiler and do not need to be manually imported. You can also explicitly bring in the namespace:

```kinal
Get IO.Collection;
```

---

## list — Ordered List

`list` is a dynamically sized ordered collection, equivalent to ArrayList / Vec in other languages.

### Creating

```kinal
list names = IO.Collection.list.Create();
```

Or using literal syntax (if supported by the language version):

```kinal
list nums = [1, 2, 3];
```

### Function Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `Create()` | `→ list` | Create an empty list |
| `Add(l, value)` | `list, any → void` | Append an element to the end |
| `Fetch(l, index)` | `list, int → any` | Get by index (throws exception on out-of-bounds) |
| `TryFetch(l, index)` | `list, int → any` | Get by index (returns `null` on out-of-bounds) |
| `Set(l, index, value)` | `list, int, any → void` | Set value by index |
| `Insert(l, index, value)` | `list, int, any → void` | Insert at the specified position |
| `RemoveAt(l, index)` | `list, int → void` | Remove element at the specified position |
| `Pop(l)` | `list → any` | Remove and return the last element |
| `Peek(l)` | `list → any` | Return the last element (without removing) |
| `Contains(l, value)` | `list, any → bool` | Whether the list contains the specified value |
| `IndexOf(l, value)` | `list, any → int` | Index of first occurrence (-1 if not found) |
| `Count(l)` | `list → int` | Number of elements |
| `IsEmpty(l)` | `list → bool` | Whether the list is empty |
| `Clear(l)` | `list → void` | Clear the list |

### Example

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

## dict — Key-Value Dictionary

`dict` is a hash map that stores key-value pairs; both keys and values are of type `any`.

### Creating

```kinal
dict config = IO.Collection.dict.Create();
```

### Function Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `Create()` | `→ dict` | Create an empty dictionary |
| `Set(d, key, value)` | `dict, any, any → void` | Set a key-value pair (adds if not present, updates if present) |
| `Fetch(d, key)` | `dict, any → any` | Get the value for a key (throws exception if key does not exist) |
| `TryFetch(d, key)` | `dict, any → any` | Get value (returns `null` if key does not exist) |
| `Contains(d, key)` | `dict, any → bool` | Whether the dictionary contains the specified key |
| `Remove(d, key)` | `dict, any → void` | Delete the specified key |
| `Count(d)` | `dict → int` | Number of entries |
| `IsEmpty(d)` | `dict → bool` | Whether the dictionary is empty |
| `Keys(d)` | `dict → list` | List of all keys |
| `Values(d)` | `dict → list` | List of all values |

### Example

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

## set — Unique-Element Set

`set` stores unique elements; adding the same value multiple times produces no duplicates.

### Creating

```kinal
set tags = IO.Collection.set.Create();
```

### Function Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `Create()` | `→ set` | Create an empty set |
| `Add(s, value)` | `set, any → void` | Add an element (ignored if already present) |
| `Contains(s, value)` | `set, any → bool` | Whether the set contains the specified value |
| `Remove(s, value)` | `set, any → void` | Remove the specified element |
| `Count(s)` | `set → int` | Number of elements |
| `IsEmpty(s)` | `set → bool` | Whether the set is empty |
| `Clear(s)` | `set → void` | Clear the set |
| `Values(s)` | `set → list` | Return a list of all elements |

### Example

```kinal
set seen = IO.Collection.set.Create();
IO.Collection.set.Add(seen, "apple");
IO.Collection.set.Add(seen, "banana");
IO.Collection.set.Add(seen, "apple");   // Duplicate, ignored

int count = IO.Collection.set.Count(seen);  // 2

bool hasApple = IO.Collection.set.Contains(seen, "apple");  // true

list all = IO.Collection.set.Values(seen);  // ["apple", "banana"]
```

---

## Combined Example

```kinal
Unit App.CollectionDemo;

Get IO.Console;
Get IO.Collection;

Static Function int Main()
{
    // list: count words
    list words = IO.Collection.list.Create();
    IO.Collection.list.Add(words, "hello");
    IO.Collection.list.Add(words, "world");
    IO.Collection.list.Add(words, "hello");

    IO.Console.PrintLine(IO.Collection.list.Count(words));  // 3

    // dict: word frequency count
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

    // set: deduplicate
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

## See Also

- [Type System](../language/types.md) — `list`/`dict`/`set` as built-in type aliases
- [Exception Handling](../language/exceptions.md) — Exceptions thrown when `Fetch` goes out-of-bounds or key does not exist
