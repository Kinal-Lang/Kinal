# IO.Version（编译器与 VM 版本常量）

`IO.Version.*` 与 `IO.Version.VM.*` 是一组编译期常量，用于在源码中读取当前产物编译时所使用的工具链版本。

与 `IO.Target.*`、`IO.Host.*`、`IO.Runtime.*` 一样，这些符号会在语义阶段直接折叠为字面值，不产生任何运行时开销。

> **注意：** `IO.Version` 表示编译器/工具链版本，不是你的应用自身版本号。应用版本请自行定义项目常量，或从包元数据中读取。

---

## 常量一览

### 编译器版本

| 符号 | 类型 | 说明 |
|------|------|------|
| `IO.Version` | `string` | 编译器版本字符串，与 `IO.Version.String` 等价 |
| `IO.Version.String` | `string` | 同上 |
| `IO.Version.Major` | `int` | 编译器主版本号 |
| `IO.Version.Minor` | `int` | 编译器次版本号 |
| `IO.Version.Patch` | `int` | 编译器补丁版本号 |

### VM 版本

| 符号 | 类型 | 说明 |
|------|------|------|
| `IO.Version.VM` | `string` | VM 版本字符串，与 `IO.Version.VM.String` 等价 |
| `IO.Version.VM.String` | `string` | 同上 |
| `IO.Version.VM.Major` | `int` | VM 主版本号 |
| `IO.Version.VM.Minor` | `int` | VM 次版本号 |
| `IO.Version.VM.Patch` | `int` | VM 补丁版本号 |

当产物面向原生后端时，VM 字符串常量为空串，VM 数字常量为 `0`。

---

## 用法示例

```kinal
Unit MyLib;

Get IO.Console;

Static Function void PrintVersions()
{
    IO.Console.PrintLine("Compiler: " + IO.Version);
    IO.Console.PrintLine("Compiler minor: " + [string](IO.Version.Minor));

    If (IO.Runtime.IsVM)
    {
        IO.Console.PrintLine("VM: " + IO.Version.VM);
    }
}
```

也可以直接使用数字成员做条件判断：

```kinal
If (IO.Version.Major == 0 && IO.Version.Minor >= 8)
{
    // ...
}
```

---

## 各命令对应的值

| 命令 | `IO.Version` | `IO.Version.VM` |
|------|--------------|-----------------|
| `kinal build` | 编译器版本 | `""` |
| `kinal run` | 编译器版本 | `""` |
| `kinal vm build` | 编译器版本 | VM 版本 |
| `kinal vm pack` | 编译器版本 | VM 版本 |
| `kinal vm run <file.kn>` | 编译器版本 | VM 版本 |
| `--emit obj` / `--emit asm` / `--emit llvm-ir` | 编译器版本 | `""` |
| `--emit knc` | 编译器版本 | VM 版本 |

`IO.Version.Major` / `Minor` / `Patch` 始终表示生成该产物时所使用的编译器版本。

`IO.Version.VM.*` 只对面向 VM 的产物表示 VM 版本；对原生产物，这些成员会折叠为空串或 `0`。

这些常量会在编译期写入产物，之后不会再变化。用 `kinal vm build` 生成的 `.knc` 会保留生成时的编译器与 VM 版本常量。

---

## 与 `kinal --version` 的关系

CLI 命令：

```bash
kinal --version
kinal --version --verbose
```

显示的正是源码中 `IO.Version` 与 `IO.Version.VM` 对应的版本族。

---

## 相关

- [IO.Runtime 常量](runtime-environment-constants.md) — 判断当前产物面向原生后端还是 KinalVM
- [编译器 CLI](../cli/compiler.md) — `kinal --version` 与其他全局选项
- [KinalVM](../cli/vm.md) — 面向 VM 的构建与运行命令
- [编译流程详解](compilation-pipeline.md) — 编译期常量折叠发生的阶段