# 程序入口诊断

与 Main / KMain 入口点签名和唯一性有关的诊断。

本页记录预注册模板。正确示例展示支持的写法；触发形态用于说明规则或解析状态，可能依赖外围声明，或在当前 C 编译器中先报告其他诊断。


验证入口使用 kinal build main.kn，不要用不要求入口的 --emit check。Native 驱动选择入口后才检查 Static、返回类型和参数。E-ENT-00001／00002 为预注册模板，当前实际诊断细节不同的路径可能分配其他编号。

---

## 错误码目录

| 错误码 | 标题 | 默认详情 |
|------|------|----------|
| `E-ENT-00001` | Entry Ambiguous | Multiple Main functions found |
| `E-ENT-00002` | Entry Signature | Main must be Static |
| `E-ENT-00003` | Entry Signature | Main must return int |
| `E-ENT-00004` | Entry Signature | Main must take zero or one parameter |
| `E-ENT-00005` | Entry Signature | Main parameter must be string[] |

---

## E-ENT-00001 — Entry Ambiguous

- 级别：错误
- 默认详情：Multiple Main functions found
- 说明：程序入口只能有一个 `Main`。

将两个入口放在不同 Unit，避免同一 Unit 的重复函数诊断；运行 `kinal build first.kn second.kn`。当前 C 版报告 Entry Ambiguous / Multiple entry functions found，自动编号可不同。

**first.kn**

```kinal
Unit Entry.A;
Static Function int Main() { Return 0; }
```

**second.kn**

```kinal
Unit Entry.B;
Static Function int Main() { Return 0; }
```

正确示例：

```kinal
Static Function int Main(string[] args)
{
    Return 0;
}
```

原因：编译器无法在多个同名入口之间自动挑选最终启动点。

修复方式：只保留一个符合规范的 `Main`，其余入口改名为普通辅助函数。

---

## E-ENT-00002 — Entry Signature

- 级别：错误
- 默认详情：Main must be Static
- 说明：程序入口必须声明为 `Static`。

当前 C 原生构建对此报告 Entry Signature / Entry point must be Static；旧模板编号不一定出现。

错误示例：

```kinal
Function int Main()
{
    Return 0;
}
```

正确示例：

```kinal
Static Function int Main()
{
    Return 0;
}
```

原因：启动器不会先构造某个类实例再去调用入口；入口必须能直接静态调用。

修复方式：在 `Main` 前添加 `Static`。

---

## E-ENT-00003 — Entry Signature

- 级别：错误
- 默认详情：Main must return int
- 说明：程序入口返回类型必须是 `int`。

错误示例：

```kinal
Static Function void Main()
{
}
```

正确示例：

```kinal
Static Function int Main()
{
    Return 0;
}
```

原因：入口返回码会映射到进程退出码，因此签名必须稳定为 `int`。

修复方式：把返回类型改成 `int`，并显式返回退出码。

---

## E-ENT-00004 — Entry Signature

- 级别：错误
- 默认详情：Main must take zero or one parameter
- 说明：`Main` 只能不带参数，或只带一个参数。

错误示例：

```kinal
Static Function int Main(string[] args, int mode)
{
    Return 0;
}
```

正确示例：

```kinal
Static Function int Main(string[] args)
{
    Return 0;
}
```

原因：入口参数布局是固定约定，不能随意扩展成多个独立参数。

修复方式：把入口参数收敛到零个参数，或单个 `string[] args`。

---

## E-ENT-00005 — Entry Signature

- 级别：错误
- 默认详情：Main parameter must be string[]
- 说明：入口参数只能是命令行参数数组 `string[]`。

错误示例：

```kinal
Static Function int Main(int[] args)
{
    Return 0;
}
```

正确示例：

```kinal
Static Function int Main(string[] args)
{
    Return 0;
}
```

原因：宿主环境传入的是命令行字符串数组，而不是任意类型。

修复方式：把入口参数类型改成 `string[]`。
