# Kinal 发行流程

公开 SDK 使用 stage3 编译器作为 `kinal` 命令。v0.10.0 是 C 实现的最后一个功能版本。维护分支 `maintenance/c-0.10` 保留 C 编译器及手动触发的 CI 和发行工作流。

## 准备版本

在 `VERSION` 中分别设置编译器和 VM 版本，更新两个已签入的元数据模块，并在 `dev/releases/<编译器版本>.md` 编写发行说明。

```sh
python -m infra.scripts.x.compiler_metadata
python -m infra.scripts.x.vm_metadata
```

KNC 格式版本独立编号。格式 4 记录生成字节码的编译器版本。

## 验证 SDK

macOS 上的独立 ELF 构建需要安装 Homebrew 的 `lld@21`。

将发行提交推入默认分支。**CI** 在 Windows、Linux、macOS 的 x64 和 Arm64 主机运行。各主机核验固定 C 种子的校验和，完成 stage3 自举和回归测试。工作流解压公开 ZIP，核对编译器与收敛产物一致，再用解压后的 SDK 构建并运行 Native 和 KNC 程序。拉取请求执行相同检查。默认分支还上传发行包和各平台校验和。

本地可以使用解压的 v0.10.0 SDK 运行 `python x.py selfhost-bootstrap --stage0-bundle <sdk>`，再将 stage3 编译器和冻结后的 stage0 传给 `tests/selfhost/run_selfhost_tests.py`。Linux 和 macOS 的原生链接使用 LLVM 21，也可通过 `KINAL_CLANG` 指定 clang 路径。

## 创建 GitHub Release

六个平台全部通过后，在同一提交上触发 **Release**，可指定 CI 运行编号。推送匹配 `v<编译器版本>` 的标签也会准备草稿。发行工作流核对默认分支、提交、工作流、六项成功结果和所有归档校验和，直接使用已验证的 SDK。手动触发默认生成草稿。明确选择 `publish` 时，资产上传完成后才会公开发布。

GitHub 发行和网站部署使用独立工作流。新版仍为草稿时，安装器和网站下载源继续使用最新公开版。
