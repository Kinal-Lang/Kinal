# `kinal fmt`：代码格式化

`fmt` 内置于 C 编译器工具。它处理源码文本，不能替代语法分析、语义检查或运行测试。

## 命令与退出状态

```bash
kinal fmt main.kn
kinal fmt src/
kinal fmt main.kn helpers.kn
kinal fmt --check src/
kinal fmt --stdout main.kn
```

普通文件模式原地替换有变化的文件。目录参数递归收集 `.kn` 文件，其他可支持的源文件拼写需要显式传入。`--check` 不修改文件，存在格式差异时返回非零；无效选项、读取失败等错误也可能返回非零。

| 选项 | 含义 |
|------|------|
| `--check` | 比较格式化结果，不写文件 |
| `--stdin` | 从标准输入读取源码 |
| `--stdout` | 将格式化文本写到标准输出 |
| `--stdin-filepath <path>` | 编辑器集成提供的虚拟文件名 |
| `-h`、`--help` | 显示格式化帮助 |

`--stdin` 默认输出格式化结果，除非被 `--check` 抑制；`--stdout` 显式要求输出。文件模式中的 `--stdout` 用于预览，不覆盖原文件。多个文件的结果会拼接，编辑器整文替换应一次处理一个文件。`--stdin-filepath` 是上下文元数据，不负责选择项目或指定输出。

## 标准输入

```bash
# POSIX shell
kinal fmt --stdin --stdout --stdin-filepath src/Main.kn < src/Main.kn
```

```powershell
# PowerShell
Get-Content -Raw -Encoding utf8 src/Main.kn | kinal fmt --stdin --stdout --stdin-filepath src/Main.kn
```

已有文本缓冲区的集成可直接将缓冲区写入子进程 stdin。VS Code 扩展使用这条路径，不需要为了请求格式化而先写磁盘源码。

## 格式风格

当前格式化器采用四空格缩进、普通二元操作符两侧空格、逗号后空格，块起始花括号独占一行：

```kinal
Function int Add(int a, int b)
{
    Return a + b;
}
```

它不重命名符号，也不会将本地化别名 token 改写成标准关键字。函数名 PascalCase 等属于项目建议，不是格式化器自动重写规则。注释和字符串的含义仍需保留，特殊语法变更应纳入正常代码审查。

## CI 与编辑器设置

```bash
kinal fmt --check src/
kinal build --project . --emit check -o out/project.kcheck
```

首条命令检查格式，第二条检查语言语义，两者都不能证明运行正确。先在本地格式化并审查 diff，再在 CI 重复只读检查。

VS Code 的 `.vscode/settings.json`：

```json
{
    "[kinal]": {
        "editor.formatOnSave": true,
        "editor.defaultFormatter": "kinal.kinal"
    }
}
```

编译器不在 PATH 时设置 `kinal.path`。此格式化器注册给 Kinal 源码，不负责 `.knproj` 配置文件。编译器、LSP 的诊断语言设置也独立于保存时格式化。

## 相关文档

- [编译器 CLI](compiler.md)
- [项目文件](project-files.md)
- [CLI 设计规范](cli-spec.md)
