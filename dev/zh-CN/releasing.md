# 发布流程

本文档说明如何管理版本号、准备发布以及打 Git 标签。

---

## 一、版本号规范

Kinal 使用 [SemVer](https://semver.org/) 三段式版本号：`MAJOR.MINOR.PATCH`

| 段 | 何时递增 | 示例场景 |
|---|---------|---------|
| MAJOR | 语言或公共 API 存在不兼容变更 | 语法重构、删除已有功能 |
| MINOR | 向后兼容地新增功能 | 新语法特性、新标准库模块 |
| PATCH | 向后兼容的问题修复 | Bug 修复、崩溃修复 |

> 开发阶段（MAJOR = 0）：MINOR 变更也可能包含不兼容内容。

---

## 二、双版本策略

项目维护两个独立的版本号，统一记录在根目录的 `VERSION` 文件中：

```
kinal=0.5.0
kinalvm=0.5.0
```

| 键 | 组件 | 说明 |
|---|------|------|
| `kinal` | 编译器 + 标准库 | 主要版本，大多数发布只需改这一个 |
| `kinalvm` | 虚拟机 | VM 专属版本，通常随 kinal 同步 |

**同步规则：**
- KinalVM 的 MAJOR 和 MINOR 始终与 Kinal 保持一致
- KinalVM 的 PATCH 可以独立递增，仅在 VM 内部有修复而编译器没有变化时使用

**示例：**
```
# Kinal 0.6.0 发布时，两者同步
kinal=0.6.0
kinalvm=0.6.0

# VM 发现一个独立 bug，只改 kinalvm 行
kinal=0.6.0
kinalvm=0.6.1
```

---

## 三、修改版本号

版本号的**唯一来源**是根目录 `VERSION` 文件，不要直接编辑 `version.h`（它由 CMake 自动生成）。

**步骤：**

1. 编辑根目录 `VERSION`，修改对应行的版本号（通常两行一起改）：
   ```
   kinal=0.6.0
   kinalvm=0.6.0
   ```
   如果仅 VM 有独立补丁，只改 `kinalvm` 那行。
   ```bash
   python x.py dev
   ```

4. 验证版本号是否正确写入：
   ```bash
   kinal --version           # 输出: Kinal 0.6.0
   kinal --version --verbose # 额外输出: kinalvm 0.6.0
   kinalvm --version         # 输出: KinalVM 0.6.0
   ```

---

## 四、Git 标签格式

| 场景 | 标签格式 | 示例 |
|------|---------|------|
| 常规 Kinal 发布 | `v{VERSION}` | `v0.6.0` |
| KinalVM 独立补丁 | `vm/v{VERSION}` | `vm/v0.6.1` |

```bash
# 打标签
git tag v0.6.0
git push origin v0.6.0
```

---

## 五、发布清单

在打标签并发布前，按顺序确认以下各项：

- [ ] 所有测试通过：`python x.py test`
- [ ] `VERSION` 和 `apps/kinalvm/VERSION` 已更新
- [ ] 重新构建成功：`python x.py dev`
- [ ] `kinal --version --verbose` 输出的版本号正确
- [ ] 提交版本号变更并推送
- [ ] 打 Git 标签并推送（见第四节）
- [ ] 构建发布包：`python x.py release`
- [ ] 检查发布包内 `README.txt` 首行包含正确版本号
