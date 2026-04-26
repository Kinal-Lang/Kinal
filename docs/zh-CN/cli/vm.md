# kinal vm — 字节码虚拟机

`kinal vm` 是管理 KinalVM 字节码的子命令组，提供构建、执行、打包三种操作。

---

## 子命令

| 子命令 | 用途 |
|--------|------|
| `kinal vm build` | 将 `.kn` 源文件编译为 `.knc` 字节码 |
| `kinal vm run` | 执行 `.kn` 源文件或 `.knc` 字节码文件 |
| `kinal vm pack` | 将源文件或 `.knc` 打包为独立可执行文件（vm-exe） |

---

## kinal vm build

编译源文件为 `.knc` KinalVM 字节码。

```bash
kinal vm build <文件.kn> [-o <输出.knc>]
```

| 选项 | 说明 |
|------|------|
| `-o, --output <文件>` | 输出文件路径（默认同名 `.knc`） |
| `--project <文件\|目录>` | 从 `kinal.knproj` 或旧项目清单构建 |
| `--profile <名称>` | 选择 `kinal.knproj` 中的 Profile |
| `--no-superloop` | 禁用 KNC 超级循环融合 |
| `--superloop` | 启用 KNC 超级循环融合（默认） |
| `--no-module-discovery` | 禁用自动模块发现 |
| `--pkg-root <目录>` | 添加本地包根目录 |
| `--stdpkg-root <目录>` | 添加官方 stdpkg 根目录覆盖 |

```bash
# 编译为字节码
kinal vm build main.kn -o main.knc

# 从项目文件构建
kinal vm build --project . --profile vm

# 禁用超级循环
kinal vm build --no-superloop main.kn
```

---

## kinal vm run

执行 `.kn` 源文件或 `.knc` 字节码文件。

```bash
kinal vm run <文件.kn|文件.knc> [-- 程序参数...]
```

| 选项 | 说明 |
|------|------|
| `--project <文件\|目录>` | 从 `kinal.knproj` 或旧项目清单运行 |
| `--profile <名称>` | 选择 `kinal.knproj` 中的 Profile |
| `--superloop` | 启用超级循环（默认） |
| `--no-superloop` | 禁用超级循环 |
| `--vm-path <路径>` | 指定 KinalVM 可执行文件路径 |
| `--no-module-discovery` | 编译源码输入时禁用自动模块发现 |
| `--keep-temps` | 保留自动生成的临时 `.knc` 文件 |

```bash
# 直接执行源码
kinal vm run main.kn

# 运行项目的默认 Profile
kinal vm run --project .

# 传递参数
kinal vm run main.knc -- arg1 arg2
```

---

## kinal vm pack

将字节码与 KinalVM 打包为独立可执行文件（vm-exe），实现零依赖部署。

```bash
kinal vm pack <文件.kn|文件.knc> [-o <输出>]
```

| 选项 | 说明 |
|------|------|
| `-o, --output <文件>` | 输出可执行文件路径 |
| `--project <文件\|目录>` | 从 `kinal.knproj` 或旧项目清单打包 |
| `--profile <名称>` | 选择 `kinal.knproj` 中的 Profile |
| `--vm-path <路径>` | 指定用于打包的 KinalVM 路径 |
| `--target <三元组>` | 目标平台（影响打包的 VM 二进制） |
| `--superloop` | 启用超级循环融合 |
| `--no-superloop` | 禁用超级循环融合 |
| `--no-module-discovery` | 对源码输入禁用自动发现 |
| `--pkg-root <目录>` | 添加本地包根目录 |
| `--stdpkg-root <目录>` | 添加官方 stdpkg 根目录覆盖 |

```bash
# 从源码打包
kinal vm pack main.kn -o myapp

# 从项目文件打包
kinal vm pack --project . --profile vm -o myapp

# 从已有字节码打包
kinal vm pack main.knc -o myapp

# 运行打包后的程序
./myapp arg1 arg2
```

---

## kinal run vs kinal vm

| | `kinal run` | `kinal vm build` + `kinal vm run` |
|---|---|---|
| 编译后端 | LLVM 原生代码 | KinalVM 字节码 |
| 步骤 | 一步到位 | 分步操作 |
| 中间文件 | 自动清理（临时 exe） | 保留 `.knc` |
| 用途 | 快速运行 `.kn` 源文件 | 生产字节码、分发、跨平台 |

`kinal run main.kn` 在后台执行：编译 → 原生 exe → 运行 → 删除临时 exe。  
`kinal vm run` 则走 VM 路线：如果输入是源码，会先编译成临时 `.knc`，再交给 KinalVM 执行。

```bash
# 等效操作（使用 VM 路径）：
kinal vm build main.kn -o main.knc
kinal vm run main.knc
```

---

## `.knc` 字节码格式

`.knc` 是 Kinal 的原生字节码格式：

- **平台无关** — 同一份 `.knc` 可在任何安装了 KinalVM 的平台上运行
- **紧凑** — 比 LLVM IR 体积更小
- **快速启动** — 跳过链接阶段

---

## 超级循环（Superloop）调度模型

KinalVM 的异步运行时基于事件循环：

- **启用**（默认）：支持 `IO.Async.Spawn`、`Await` 等协程调度
- **禁用**（`--no-superloop`）：单线程顺序执行，适合同步程序或嵌入式场景

---

## 独立 kinalvm 工具

`kinalvm` 是独立的 KinalVM 可执行文件，可直接运行 `.knc`：

```bash
kinalvm [--superloop|--no-superloop] <文件.knc> [-- 程序参数...]
```

---

## KinalVM 与原生编译对比

| 特性 | KinalVM (`.knc`) | 原生编译 |
|------|-----------------|---------|
| 执行速度 | 较慢（解释/JIT） | 快（机器码） |
| 启动时间 | 快（无链接） | 慢（需链接） |
| 平台无关性 | 是 | 否 |
| 部署要求 | 需要 KinalVM | 无额外依赖 |
| 适用场景 | 开发、脚本、嵌入 | 生产部署 |

---

## 相关

- [CLI 总览](compiler.md) — 所有子命令概览
- [异步编程](../language/async.md) — 超级循环与协程
- [裸机开发](../advanced/freestanding.md) — VM 在受限环境中的使用
- [IO.Runtime 常量](../advanced/runtime-environment-constants.md) — 在源码中判断当前执行后端
- [CLI 规范](cli-spec.md) — CLI 设计规范（开发者参考）
