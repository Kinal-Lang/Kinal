# kinal pkg — 包管理

`kinal pkg` 是管理 `.klib` 包文件的子命令组。Kinal 使用基于 `.knpkg.json` 的包系统和 `.klib` 二进制库格式。

---

## 子命令

| 子命令 | 用途 |
|--------|------|
| `kinal pkg build` | 从包清单构建 `.klib` 或本地包布局 |
| `kinal pkg info` | 查看 `.klib` 元数据 |
| `kinal pkg unpack` | 解包 `.klib` 到目录 |

---

## kinal pkg build

从包清单（`.knpkg.json`）构建 `.klib` 库文件或本地包布局。

```bash
kinal pkg build --manifest <文件|目录> [-o <输出>]
kinal pkg build --manifest <文件|目录> --layout <目录>
```

| 选项 | 说明 |
|------|------|
| `--manifest <文件\|目录>` | 包清单路径（必需） |
| `-o, --output <文件>` | 输出 `.klib` 文件路径 |
| `--layout <目录>` | 输出本地包目录结构（与 `-o` 互斥） |

```bash
# 构建 klib
kinal pkg build --manifest ./mylib/ -o mylib.klib

# 输出本地包布局
kinal pkg build --manifest ./mylib/ --layout ./packages/
```

---

## kinal pkg info

显示 `.klib` 文件的元数据。

```bash
kinal pkg info <文件.klib>
```

输出示例：

```
包名:    mylib
版本:    1.0.0
模块:    Mylib.Core
        Mylib.Utils
导出:    42 个符号
架构:    x86_64
平台:    linux
```

---

## kinal pkg unpack

将 `.klib` 解包到目录，恢复源码和元信息。

```bash
kinal pkg unpack <文件.klib> [-o <目录>]
```

| 选项 | 说明 |
|------|------|
| `-o, --output <目录>` | 输出目录（默认为与 `.klib` 同名的目录） |

```bash
kinal pkg unpack mylib.klib -o ./recovered/
```

---

## 包清单：`.knpkg.json`

每个包的根目录包含一个 `.knpkg.json` 文件：

```json
{
    "name": "mypackage",
    "version": "1.0.0",
    "description": "我的 Kinal 包",
    "author": "开发者名称",
    "entry": "src/main.kn",
    "dependencies":
    {
        "somelib": "1.0.0"
    }
}
```

### 字段说明

| 字段 | 必填 | 说明 |
|------|------|------|
| `name` | 是 | 包名（小写字母、数字、连字符） |
| `version` | 是 | 语义化版本号（`major.minor.patch`） |
| `description` | 否 | 包的简短描述 |
| `author` | 否 | 作者信息 |
| `entry` | 否 | 主入口文件（默认 `src/main.kn`） |
| `dependencies` | 否 | 依赖包的名称和版本映射 |

---

## `.klib` 库文件

`.klib` 是 Kinal 的预编译库格式，包含：

- 已编译的目标代码
- 类型元数据（供编译器类型检查）
- 导出的模块接口

---

## 在 build 中使用 `.klib`

`.klib` 通过 `kinal build` 的链接选项引入：

```bash
# 链接 klib
kinal build main.kn --link-file mylib.klib -o app

# 通过包根目录自动发现
kinal build main.kn --pkg-root ./packages -o app
```

---

## 构建完整项目

```bash
# 使用 --project 自动读取 .knpkg.json
kinal build --project . -o app

# 手动指定入口和包根目录
kinal build src/main.kn --pkg-root ./packages -o app
```

---

## 推荐项目结构

```
myproject/
├── .knpkg.json        ← 包清单
├── src/
│   ├── main.kn        ← 主入口
│   └── lib/
│       └── utils.kn   ← 内部模块
├── tests/
│   └── test_utils.kn
└── packages/          ← 第三方 .klib（依赖）
    └── somelib.klib
```

---

## 版本规范

包版本遵循语义化版本（SemVer）：

- `1.0.0` — 正式发布
- `1.0.1` — Bug 修复
- `1.1.0` — 向后兼容的新功能
- `2.0.0` — 破坏性变更

---

## 相关

- [CLI 总览](compiler.md) — 所有子命令概览
- [项目结构](../getting-started/project-structure.md) — 推荐目录布局
- [模块系统](../language/modules.md) — `Unit` 和 `Get` 声明
- [CLI 规范](cli-spec.md) — CLI 设计规范（开发者参考）
