# 发布流程

本文说明 C 编译器发行版。发布前应阅读实际 workflow 配置；创建标签或 Release 可能触发构建和生产同步。

## 组件版本

根目录 `VERSION` 是唯一版本来源：

```text
kinal=0.9.0
kinalvm=0.8.0
```

这是独立组件版本，不要求二者主版本／次版本一致。根据实际契约变更更新对应组件。字节码格式版本是另一维度：KNC 格式 3 不能由任一组件的语义版本号推导。

CMake 在构建目录生成 `kn/version.h`，不要手工修改生成头文件。KinalVM 已提交的 `BuildInfo.kn` 由 `VERSION:kinalvm` 生成：

```bash
python -m infra.scripts.x.vm_metadata
python -m infra.scripts.x.vm_metadata --check
```

提交生成的 Unit 是为了支持 bootstrap 编译器直接构建 VM 工程。修改 VM 版本时必须保持同步。

## 本地验证与打包

```bash
python x.py doctor
python x.py dev --release
python x.py test --release --full
python x.py dist
```

`dist` 构建并打包当前主机的 Release 工具链。不存在 `x.py release` 子命令。除主测试集外，还应运行与变更相关的平台测试和 LSP 检查。

打包前核对暂存工具：

```bash
kinal --version --verbose
kinalvm --version
```

如果 `PATH` 中还有其他安装，请显式运行 `out/stage/host-release/` 中的工具。编译器版本、VM 版本、包内 `VERSION`、VM 生成元数据和 `README.txt` 应一致。编译器版本正确不代表旁边的旧 VM 能读取新字节码。

发行目录包含编译器／LSP／VM、LLVM 运行及开发文件、链接工具、语言包、运行时对象／源码、标准包与文档。移动安装时应保留完整结构。Release 标准包以 `.klib` 分发，包内源代码目录会从发行目录移除。最终打包产物位于 `artifacts/release/`。

## GitHub Release 工作流

`Release` 工作流由 `v*` 标签触发，也支持手动运行。标签触发时，`v{VERSION:kinal}` 必须与编译器版本完全相符。例如 `kinal=0.9.0` 对应 `v0.9.0`。

当前工作流构建 Windows／Linux／macOS 的 x64、ARM64 包。手动输入控制 draft、prerelease 和自动生成发布说明；手动运行的 draft 默认值为 true。应核对 workflow 矩阵及上传产物，不能用本地主机打包结果代替所有平台。

仅 VM 变更时可以更新 `kinalvm` 组件版本，但当前 Release 工作流没有独立的 `vm/v*` 发布触发器，不应把这种标签写成现成自动发布入口。

## 文档、发行索引与官网

- 主仓库的 `Docs and Scripts Sync` 工作流将 `docs/` 和安装脚本复制到配置的官网目录。
- `Published Release Sync` 生成发行索引，并选择官网生产环境使用的编译器产物；prerelease 事件不部署生产编译器。
- 官网仓库另有网站代码／资源部署工作流。
- 修改本地 Markdown 本身不会发布文档；发布 Release 与部署网站代码也是独立动作。

发布说明应包含语言／API 变化、运行依赖、迁移步骤、已知限制，以及编译器／VM 兼容要求。面向用户的行为变化应同步中英文文档。不要将跨目标编译描述为目标主机运行验证。

## 发布前核对

1. 确认源码提交、组件版本及生成元数据。
2. 重新构建并运行相关 C 编译器、运行时、VM、包和编辑器检查。
3. 解压检查最终发行包，从该位置编译并运行小程序。
4. 核对 `.knc` 和打包可执行文件使用兼容的 VM。
5. 检查最终文档、发布说明、产物和标签，再启动发布。

## 相关文档

- [构建指南](build.md)
- [测试指南](testing.md)
- [KinalVM 兼容性](../../docs/zh-CN/cli/vm.md)
