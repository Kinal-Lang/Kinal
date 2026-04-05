# 项目结构

```
Kinal/
├── VERSION                  # Kinal 版本号（单一来源）
├── CMakeLists.txt           # 根 CMake 配置
├── x.py                     # 构建入口脚本
├── apps/
│   ├── kinal/               # Kinal 编译器（C）
│   │   ├── include/kn/      # 公共头文件（含 version.h）
│   │   └── src/             # 编译器源码
│   ├── kinal-lsp/           # LSP 语言服务器（C++）
│   │   └── server/src/
│   └── kinalvm/             # KinalVM 虚拟机（用 Kinal 语言编写）
│       ├── VERSION           # VM 版本号
│       └── src/
├── libs/
│   ├── runtime/             # 运行时库（C）
│   ├── selfhost/            # 自举支持
│   └── std/                 # 标准库包（stdpkg）
├── infra/
│   ├── assets/              # 资源文件（locales 等）
│   ├── cmake/               # CMake 模块
│   ├── scripts/             # Python 构建脚本
│   └── toolchains/          # CMake 工具链文件
├── tests/                   # 编译器测试用例
├── docs/                    # 用户文档
└── dev/                     # 开发者规范文档（本目录）
```

## 核心组件

- **Kinal 编译器** (`apps/kinal/`)：C 语言实现，使用 LLVM 后端生成本机代码或 VM 字节码
- **KinalVM** (`apps/kinalvm/`)：用 Kinal 语言自身编写的字节码虚拟机
- **LSP 服务器** (`apps/kinal-lsp/`)：C++ 实现，提供 IDE 语言支持
- **运行时** (`libs/runtime/`)：目标平台运行时库（C）
- **标准库** (`libs/std/`)：以 stdpkg 形式分发的标准库包
