# Kinal CLI 设计规范

本文档定义 `kinal` 命令行工具的设计约定。所有新增 CLI 功能必须遵循此规范。

---

## 1. 总体架构

```
kinal <子命令> [选项] [位置参数...]
```

`kinal` 采用**子命令**（subcommand）架构。每个顶级操作对应一个子命令，子命令下可以有二级子命令。

### 子命令树

```
kinal
├── build       编译为原生产物
├── run         编译并运行临时原生可执行文件
├── fmt         代码格式化
├── vm          字节码操作
│   ├── build   编译为 .knc
│   ├── run     通过 KinalVM 执行 .kn 或 .knc
│   └── pack    打包为 vm-exe
└── pkg         包管理
    ├── build   构建 .klib
    ├── info    查看 .klib 信息
    └── unpack  解包 .klib
```

### 独立工具

| 工具 | 用途 |
|------|------|
| `kinalvm` | 独立 KinalVM 可执行文件，直接运行 `.knc` |

独立工具不是子命令，而是单独的可执行文件。

---

## 2. 命名规则

### 2.1 子命令命名

- 使用**小写英文单词**，不含连字符或下划线
- 必须是**动词**或**名词**（表示操作对象）
- 长度 2–8 个字符
- 优先使用社区通用名称（`build`、`run`、`fmt`、`test`）

```
✅ build, run, fmt, pkg, vm, test, init, clean
❌ Build, compile-and-link, format_code, do-build
```

### 2.2 选项命名

- 使用 `--` 前缀 + **小写连字符分隔**（kebab-case）
- 短选项使用 `-` + **单个字母**
- 布尔选项通过 `--no-` 前缀取反

```
✅ --output, --emit, --target, --link-file, --no-superloop
❌ --Output, --link_file, --linkFile, --nosuperloop
```

### 2.3 选项值命名

- 枚举值使用**小写**，多词用连字符分隔
- 文件路径原样传入，不做大小写转换

```
✅ --emit obj, --kind static, --linker lld
❌ --emit OBJ, --kind Static
```

---

## 3. 选项分类与约定

### 3.1 通用选项

以下选项在**所有子命令**中可用（在子命令解析前处理）：

| 选项 | 说明 |
|------|------|
| `-h, --help` | 显示帮助信息 |
| `--color <auto\|always\|never>` | 颜色输出模式 |
| `--lang <en\|zh>` | 诊断语言 |
| `--locale-file <文件>` | 自定义本地化文件 |

### 3.2 输出选项

所有产生文件输出的子命令使用统一的输出选项：

| 选项 | 说明 |
|------|------|
| `-o, --output <文件>` | 指定输出路径 |

> **注意**：长选项统一为 `--output`，不使用 `--out`。

### 3.3 布尔选项

- 默认为 `true` 的选项通过 `--no-<名称>` 禁用
- 默认为 `false` 的选项通过 `--<名称>` 启用
- 不使用 `--<名称>=true/false` 形式

```
--superloop          ← 默认启用
--no-superloop       ← 显式禁用

--keep-temps         ← 默认禁用，显式启用
```

### 3.4 多值选项

可重复的选项通过多次传入累加，不使用逗号分隔：

```bash
# 正确
kinal build main.kn -L ./lib1 -L ./lib2 -l foo -l bar

# 错误
kinal build main.kn -L ./lib1,./lib2
```

---

## 4. 子命令设计原则

### 4.1 单一职责

每个子命令只做一件事。如果一个操作涉及多个步骤：
- 提供组合子命令（如 `kinal run` = 编译 + 执行）
- 不在单个子命令中塞入不相关的功能

### 4.2 显式优于隐式

- 不依赖位置参数的顺序来改变语义
- 不通过文件扩展名猜测用户意图（`.knc` 输入到 `build` 应报错，而非自动切换到 VM 模式）
- 每种操作有明确的子命令入口

### 4.3 子命令分组

当操作围绕同一个对象时，使用**二级子命令**：

```
kinal vm build    ← 围绕 VM 字节码的操作
kinal vm run
kinal vm pack

kinal pkg build   ← 围绕包的操作
kinal pkg info
kinal pkg unpack
```

不要将这些操作拍平为顶级命令：

```
❌ kinal vm-build
❌ kinal pkg-info
```

### 4.4 选项归属

选项属于它所在的子命令，不从父命令"继承"语义：

```
kinal build --target win64 ...    ← --target 属于 build
kinal vm pack --target win64 ...  ← --target 属于 vm pack（含义相同但独立声明）
```

---

## 5. 帮助系统

### 5.1 帮助层级

```bash
kinal --help              # 顶级帮助：列出子命令 + build/run/vm/pkg 速查
kinal build --help        # 子命令帮助：列出该子命令的所有选项
kinal vm --help           # 子命令组帮助：列出二级子命令
kinal vm build --help     # 二级子命令帮助
```

### 5.2 帮助格式

```
用法: kinal <子命令> [选项] [参数...]

子命令:
  build       编译源码为原生产物
  run         编译为临时原生可执行文件并立即运行
  fmt         格式化代码
  vm          字节码操作
  pkg         包管理

顶级速查:
  -h, --help                  显示此帮助
  --color <auto|always|never> 颜色输出
  --lang <en|zh>              诊断语言
  kinal build ...             原生构建入口
  kinal run ...               原生快速运行入口
  kinal vm ...                字节码构建/运行/打包入口
  kinal pkg ...               包构建/查看/解包入口

使用 "kinal <子命令> --help" 查看子命令详细帮助。
```

### 5.3 错误提示

未知子命令或选项时，提供建议：

```
kinal buid main.kn
error: unknown subcommand 'buid', did you mean 'build'?
```

---

## 6. 退出码

| 退出码 | 含义 |
|--------|------|
| `0` | 成功 |
| `1` | 编译/运行错误 |
| `2` | CLI 参数错误（未知选项、缺少参数等） |

---

## 7. 规范入口

CLI 只保留规范入口，不保留旧式别名或顶级隐式路由：

- 原生构建统一使用 `kinal build`
- 原生快速运行统一使用 `kinal run`
- 字节码构建、运行、打包统一使用 `kinal vm build|run|pack`
- 包相关操作统一使用 `kinal pkg build|info|unpack`
- 文档、测试、构建脚本不得使用 `kinal main.kn`、`kinal --vm`、`--emit-klib` 等旧式写法

---

## 8. 新增子命令检查清单

添加新子命令时，依次确认：

- [ ] 子命令名称符合 §2.1 命名规则
- [ ] 所有选项符合 §2.2 / §2.3 命名规则
- [ ] 输出选项使用 `-o, --output`（§3.2）
- [ ] 布尔选项使用 `--no-` 取反（§3.3）
- [ ] 实现 `-h, --help`（§5.1）
- [ ] 退出码符合 §6 约定
- [ ] 在 `print_help()` 顶级帮助中注册
- [ ] 在 `docs/cli/compiler.md` 子命令一览表中添加条目
- [ ] 编写对应的 `docs/cli/<子命令>.md` 文档
- [ ] 在 `kn_main()` 中添加子命令分发逻辑
- [ ] 不新增兼容别名或旧式路由（§7）
- [ ] 更新 `locales/zh-CN.json` 帮助文本

---

## 9. 示例：添加 `kinal test` 子命令

假设要添加一个运行项目测试的子命令：

### 1) 确定接口

```
kinal test [选项] [文件或目录...]
  --filter <模式>     只运行匹配的测试
  --timeout <秒>      超时时间
  --no-color          禁用颜色（用通用 --color never 替代，不需要）
```

检查：名称 `test` ✅，选项 kebab-case ✅，无 `--no-color`（通用选项覆盖） ✅

### 2) 实现分发

```c
// kn_driver_cli.inc — kn_main() 开头
if (argc >= 2 && kn_strcmp(argv[1], "test") == 0)
    return kn_test_main(argc - 1, argv + 1);
```

### 3) 更新帮助

在 `print_help()` 的子命令列表中添加 `test` 条目。

### 4) 编写文档

创建 `docs/cli/test.md`，在 `compiler.md` 子命令一览表中添加行。
