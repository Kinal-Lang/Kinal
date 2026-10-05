# 构建指南

## 前置依赖

- Python 3.10+
- CMake 3.20+
- Ninja
- LLVM/Clang 工具链（通过 `python x.py fetch llvm-prebuilt` 获取）
- Zig（可选，用于交叉编译；通过 `python x.py fetch zig-prebuilt` 获取）
- `PATH` 中的 OpenSSL 命令行工具，用于 HTTPS 测试夹具（兼容 OpenSSL 1.1.1/3.x 命令语法）

HTTPS 测试在运行时生成新的 localhost 证书和私钥，证书有效期为一天。
生成的文件仅保存在私有临时目录中，并在夹具开始提供请求前删除。
OpenSSL 缺失或执行失败会导致测试失败，不会跳过测试；不会修改系统信任库。
运行 `python -m unittest discover -s tests -p test_request_https_fixture.py`
可执行不依赖编译器的夹具检查。Python TLS 客户端验证证书信任以及
localhost/127.0.0.1 身份；这不会改变
[IO.Request](../../docs/zh-CN/stdlib/request.md) 已说明的对端证书验证限制。

Linux 上，LLVM 引导脚本会在需要时，用官方归档中的静态组件生成
`libLLVM.so`。这需要主机的 C++ 开发工具链及 LLVM 的系统依赖（例如 zlib、
zstd、libxml2）。共享库构建采用原子替换；链接失败会直接报错，不会将不完整
的工具链标记为可用。也可以通过 `LLVM_DIR` 选择完整的系统 LLVM 安装。

## 常用命令

```bash
# 检查环境
python x.py doctor

# 开发构建（Debug）
python x.py dev

# 发布构建
python x.py dev --release

# 运行测试
python x.py test

# 构建发布包
python x.py dist
```

## 构建产物

| 路径 | 说明 |
|------|------|
| `out/build/host-debug/` | CMake 构建目录 |
| `out/stage/host-debug/` | 分阶段产物（编译器 + VM + 运行时） |
| `artifacts/release/` | 发布包 |

## 版本号

版本号统一由根目录 `VERSION` 文件控制。CMake 在配置阶段读取它，并在构建目录中生成 `generated/kn/version.h`。

KinalVM 通过生成的 `apps/kinalvm/src/IO/Kinal/VM/BuildInfo.kn` Unit 嵌入
`kinalvm` 组件版本。该文件纳入版本控制，保证全新检出可直接使用引导编译器
构建源码或项目。修改 `VERSION` 后，运行
`python -m infra.scripts.x.vm_metadata` 重新生成；使用
`python -m infra.scripts.x.vm_metadata --check` 检查同步状态。C 和自举 VM
构建流程也会自动刷新此文件。发行包包含对应的根目录 `VERSION` 文件。

详见 [releasing.md](releasing.md)。
