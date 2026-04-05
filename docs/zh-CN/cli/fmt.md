# kinal fmt — 代码格式化

`kinal fmt` 是 Kinal 语言的官方代码格式化子命令，内置于 `kinal` 工具链，无需单独安装。

## 用法

```bash
kinal fmt <文件.kn>
kinal fmt <目录/>
kinal fmt <文件1.kn> <文件2.kn> ...
```

格式化会**直接修改文件**（in-place），将文件内容替换为符合 Kinal 规范的格式。

---

## 选项

| 选项 | 说明 |
|------|------|
| `--check` | 仅检查是否已格式化，不修改文件 |
| `--stdin` | 从标准输入读取源码 |
| `--stdout` | 将格式化结果写入标准输出（不修改文件） |
| `--stdin-filepath <路径>` | 为 stdin 提供文件路径上下文 |
| `-h, --help` | 显示 fmt 帮助 |

---

## 示例

### 格式化单个文件

```bash
kinal fmt main.kn
```

### 格式化目录下所有 .kn 文件

```bash
kinal fmt src/
```

### 格式化多个文件

```bash
kinal fmt main.kn utils.kn helpers.kn
```

---

## Kinal 代码风格规范

`kinal fmt` 遵循以下格式规则：

### 缩进

- 使用 **4 个空格**缩进（不使用 Tab）
- 每级嵌套增加 4 个空格

### 括号与大括号

- 函数体、类体、控制流语句体的左大括号 `{` 与语句在同一行
- 右大括号 `}` 独占一行

```kinal
// 正确
Function int Add(int a, int b)
{
    Return a + b;
}

// 错误（格式化后会修正）
Function int Add(int a, int b)
{
    Return a + b;
}
```

### 关键字与标识符

- 关键字首字母大写（`If`、`While`、`Function` 等）由编译器强制要求，`fmt` 不负责修改关键字大小写
- 类名使用 PascalCase（`MyClass`）
- 函数名使用 PascalCase（`GetValue`）
- 变量名使用 camelCase（`myValue`）

### 空格

- 运算符前后各一个空格：`a + b`、`x == y`
- 逗号后一个空格：`Function int Add(int a, int b)`
- 不在括号内侧加空格：`Add(a, b)` 而非 `Add( a, b )`

### 空行

- 顶级声明之间保留一个空行
- 函数内逻辑块之间可以有空行，但不超过一个

---

## 在 CI 中检查格式

使用 `--check` 在 CI 中验证代码是否符合规范：

```bash
kinal fmt --check src/
```

`--check` 模式不会修改文件，只在存在格式差异时以非零状态码退出。

也可以用 `git diff` 方式：

```bash
kinal fmt src/
git diff --exit-code
```

---

## 编辑器集成

### VS Code

安装 Kinal 语言插件后，可以将 `kinal fmt` 配置为保存时自动格式化：

```json
// .vscode/settings.json
{
    "[kinal]":
    {
        "editor.formatOnSave": true,
        "editor.defaultFormatter": "kinal-lang.kinal"
    }
}
```

---

## 相关

- [CLI 总览](compiler.md) — 所有子命令概览
- [项目结构](../getting-started/project-structure.md) — 推荐目录布局
- [CLI 规范](cli-spec.md) — CLI 设计规范（开发者参考）
