# 测试指南

本文覆盖 C 编译器（`apps/kinal`）、由它构建的 VM，以及共享运行时和标准库。命令均从仓库根目录执行。自举编译器的 bootstrap、对等性测试属于独立流程。

## 构建与运行

```bash
python x.py dev --release
python x.py test --release
python x.py test --release --full
```

`test` 默认复用已有暂存编译器，仅在缺失或传入 `--build` 时构建。修改编译器或运行时后应先重新构建，否则通过的可能是旧二进制。

| 选项 | 行为 |
|------|------|
| `--release` | 使用 `out/stage/host-release/`，而非 `host-debug/` |
| `--compiler <path>` | 指定编译器可执行文件 |
| `--build` | 先构建并暂存所选主机配置 |
| `--full` | 使用 `tests/manifest.json` 并运行驱动集成检查，替代默认的 smoke 与 freestanding 清单 |

两种模式还会运行 Python 单元测试发现、内建函数／操作码／标准库覆盖归属检查、包载荷与缓存检查、目标拼写、版本及 Parser／Sema 阶段检查。`--full` 指主回归清单，不代表包括所有独立的 LSP、压力或自举测试。

部分 Python VM fixture 独立默认使用 `out/stage/host-debug/kinalvm`，`--release` 不会重定向这一默认值。验证刚构建的 Release VM 时，应先设置 `KINAL_VM_PATH`：

```bash
# Linux/macOS
export KINAL_VM_PATH="$PWD/out/stage/host-release/kinalvm"
```

```powershell
# Windows PowerShell
$env:KINAL_VM_PATH = (Resolve-Path out/stage/host-release/kinalvm.exe).Path
```

## 定向检查

```bash
python tests/run_tests.py --compiler out/stage/host-release/kinal.exe --manifest tests/smoke.json --out-dir out/test/docs-smoke
python tests/run_stage_tests.py --compiler out/stage/host-release/kinal.exe
python -m unittest discover -s tests -p test_knc_numeric_format.py
python tests/check_builtin_registry.py
python tests/check_knc_opcode_registry.py
python tests/check_stdlib_coverage.py
```

以上显式编译器路径使用 Windows 拼写；Linux／macOS 去掉 `.exe`。`x.py test` 不接受位置参数形式的源文件名。检查单个程序可直接使用 `kinal build`／`run`，或向 `tests/run_tests.py` 提供小型临时清单。

## 清单约定

源文件仅放进 `tests/` 并不会自动成为回归测试；需要注册到对应 JSON 清单。

| 字段 | 含义 |
|------|------|
| `name` | 唯一测试名，也是产物名称 |
| `sources` | 相对仓库根目录的源文件列表 |
| `expected` | 正向运行测试的预期标准输出 |
| `expect_error` | 负向测试应出现的编译错误文本 |
| `compile_only` | 编译成功且产物存在即可通过 |
| `platforms` | 限定可执行该测试的主机平台 |
| `expected_exit_code` | 程序退出码，默认零 |
| `stdin` | 提供给程序标准输入的文本 |
| `runtime_files` | 复制到生成的可执行文件旁的附加文件 |

新增用例前先参照相邻条目：fixture、目标、后端和链接设置等可选项由测试驱动解释。`error_` 只是命名约定；真正决定负向测试的是 `expect_error`，源文件注释不能替代清单中的预期。

共享测试通常位于 `tests/common/`；主机相关测试位于 `tests/windows/`、`tests/linux/`、`tests/macos/`。一个清单条目可以组合多个源文件，验证跨 Unit 行为。

## 编写有效回归

1. 将问题缩减为仍会触发真实行为的完整程序或工程。
2. 运行时变更应断言输出或退出状态，不能只检查编译成功。
3. 错误用例应确认目标诊断出现，且编译器／后端不崩溃。
4. 注册主清单，并按需加入 smoke 或 freestanding 覆盖。
5. 新增内建函数或官方包时更新 `tests/stdlib_coverage.json`，运行覆盖归属检查。
6. 修改 KNC 操作码时同时核对发射器、声明、指令长度、加载器／VM 和 ABI 注册表检查。

程序创建临时文件时使用 `out/test/my_case_data/` 等独立路径。Unix 下 `out/test/my_case` 可能已是生成的可执行文件。网络测试使用本地 fixture；区分可选依赖缺失与真实失败，并保持输出确定。

## 压力、LSP 与跨平台证据

```bash
python x.py stress --release --profile quick
python x.py stress --release --profile max
python tests/lsp/run_lsp_tests.py --help
```

压力测试在 `out/stress/` 生成工作负载，补充行为清单。LSP 驱动使用自己的参数和生成工程，检查诊断及编辑器协议行为。

Hosted CI 包含 Windows、Linux、macOS 的 x64 与 ARM64。跨目标 IR／目标文件生成只能证明相应代码生成，不能代替目标主机执行。汇报结果应记录主机、编译器路径／版本、清单、跳过项和失败项。C 测试通过也不能证明自举 bootstrap 稳定。

## 相关文档

- [构建指南](build.md)
- [架构](architecture.md)
- [诊断参考](../../docs/zh-CN/advanced/error-codes/index.md)
- [KinalVM](../../docs/zh-CN/cli/vm.md)
