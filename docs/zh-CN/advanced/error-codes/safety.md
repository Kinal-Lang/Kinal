# 安全级别警告

与 Safe / Trusted / Unsafe 传播相关的警告。

本页记录预注册模板。正确示例展示支持的写法；触发形态用于说明规则或解析状态，可能依赖外围声明，或在当前 C 编译器中先报告其他诊断。

---

## 错误码目录

| 错误码 | 标题 | 默认详情 |
|------|------|----------|
| `W-SAF-00001` | Entry Unsafe | Entry point is marked as Unsafe. This disables safety guarantees for the entire program. Consider using Safe Main and isolating Unsafe blocks locally. |

---

## W-SAF-00001 — Entry Unsafe

- 级别：警告
- 默认详情：Entry point is marked as Unsafe. This disables safety guarantees for the entire program. Consider using Safe Main and isolating Unsafe blocks locally.
- 说明：Native 可执行入口被标记为 Unsafe。其他函数中的调用仍遵循各自的安全级别。

错误示例：

```kinal
Static Unsafe Function int Main()
{
    Return 0;
}
```

正确示例：

```kinal
Static Safe Function int Main()
{
    Unsafe Block LowLevel</
        byte* ptr = null;
    />
    Return 0;
}
```

原因：Unsafe 入口允许整个 Main 内使用受保护的操作。请用 Native 可执行构建验证该警告；仅前端检查不会选择入口。

修复方式：优先保持 `Main` 为 `Safe`，只在确实需要的局部区域使用 `Trusted` 或 `Unsafe Block`。
