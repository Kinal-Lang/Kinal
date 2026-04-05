# 构建指南

## 前置依赖

- Python 3.10+
- CMake 3.20+
- Ninja
- LLVM/Clang 工具链（通过 `python x.py fetch llvm-prebuilt` 获取）
- Zig（可选，用于交叉编译；通过 `python x.py fetch zig-prebuilt` 获取）

## 常用命令

```bash
# 检查环境
python x.py doctor

# 开发构建（Debug）
python x.py dev

# 发布构建
python x.py release

# 运行测试
python x.py test

# 仅 CMake 配置
python x.py configure
```

## 构建产物

| 路径 | 说明 |
|------|------|
| `out/build/host-debug/` | CMake 构建目录 |
| `out/stage/host-debug/` | 分阶段产物（编译器 + VM + 运行时） |
| `artifacts/release/` | 发布包 |

## 版本号

版本号由 `VERSION` 和 `apps/kinalvm/VERSION` 文件控制。CMake 在配置阶段读取它们并生成 `apps/kinal/include/kn/version.h`。

详见 [releasing.md](releasing.md)。
