# 交叉编译

原生交叉编译为另一 OS／架构生成代码。Triple 只是完整目标工具链的一部分，不会自动提供 SDK、所有运行库或在主机执行产物的能力。

## 选择目标与输出阶段

```bash
kinal build main.kn --target linux-arm64 --emit ir -o out/main-aarch64.ll
kinal build main.kn --target linux-arm64 --emit obj -o out/main-aarch64.o
```

目标链接尚未配置时，可先检查 IR／对象发射。生成可运行程序还需要匹配链接器、目标启动／CRT／系统库及包原生资产。命令以 kinal build 开始，不能把构建选项直接放在 kinal 后代替子命令接口。

## 主要目标别名

| 别名 | Triple |
|------|--------|
| `win86` | `i686-pc-windows-msvc` |
| `win64` | `x86_64-pc-windows-msvc` |
| `win-arm64` | `aarch64-pc-windows-msvc` |
| `linux64` | `x86_64-pc-linux-gnu` |
| `linux-arm64` | `aarch64-unknown-linux-gnu` |
| `mac64` | `x86_64-apple-darwin` |
| `macos-arm64` | `aarch64-apple-darwin` |
| `bare64` | `x86_64-unknown-none-elf` |
| `bare-arm64` | `aarch64-unknown-none-elf` |

也可传入受支持完整 triple。别名只规范平台拼写，不增加目标库；产物后缀和 ABI 随目标变化，不随编译主机变化。

## 链接外来目标

```bash
kinal build main.kn --target linux-arm64 --linker zig -o out/app-linux-arm64
kinal build main.kn --target win64 --linker lld -o out/app-windows.exe
```

以上命令要求对应目标依赖已经就绪。Zig 可提供部分跨工具链，但不能保证所有厂商 SDK、Windows MSVC 库、Apple SDK 和第三方依赖。LLD／MSVC 也需要相应输入。Hosted Linux／macOS 默认优先可用 Zig，否则警告并回退 LLD；显式选择会覆盖该默认。

当前没有通用 sysroot CLI 选项。目标库目录使用 -L／项目 Link 字段，必要时通过 link-arg 传对应后端参数。单个 -L 不提供头文件、启动文件或完整 SDK。外部 FFI 库要匹配架构和 ABI，不只是扩展名。

## Target、Host 与 Runtime 常量

| 常量族 | 含义 |
|--------|------|
| `IO.Target.OS`、`.Arch`、`.Env` | 编译产物的 OS、架构、hosted／freestanding 环境 |
| `IO.Host.OS`、`.Arch`、`.Env` | 编译器主机相应属性 |
| `IO.Target.PointerBits`／`IO.Host.PointerBits` | 相应机器的指针宽度 |
| `.PathSeparator`、`.NewLine`、`.ExeSuffix`、`.ObjectSuffix`、`.DynamicLibrarySuffix`、`.StaticLibrarySuffix` | 对应族的平台拼写 |
| `IO.Runtime.*` | Native／VM 输出后端，独立于环境／内存配置 |

OS 常量包括 Windows／Linux／MacOS／Unknown，Arch 包括 X86／X64／Arm64／Unknown，Env 包括 Hosted／Freestanding／Unknown。比较常量，不要硬编码内部整数值。

```kinal
Unit Docs.Platform;
Get IO.Console;

Static Function int Main()
{
    If (IO.Target.OS == IO.Target.OS.Windows)
    {
        IO.Console.PrintLine("Windows target");
    }
    Else
    {
        IO.Console.PrintLine("non-Windows target");
    }
    IO.Console.PrintLine(IO.Target.PointerBits);
    IO.Console.PrintLine(IO.Target.DynamicLibrarySuffix);
    Return 0;
}
```

编译期非活动分支仍要语法有效，但不参与语义绑定。程序路径／ABI 策略使用目标属性，编译器主机不是部署平台。交叉编译不会修复 Windows 原生 argv／文件系统编码边界。

## Freestanding 目标

```bash
kinal build kernel.kn --target bare-arm64 --freestanding --runtime none \
    --panic trap --no-crt --link-script board.ld -o out/kernel.elf
```

目标需要真实启动／引导契约和链接脚本；Alloc／GC 还需要目标运行时 hook。生成 ELF 不能证明硬件可启动，参见[Freestanding](freestanding.md)。

## 静态链接与部署

选择 Zig 或启用 LTO 不会自动使 GNU Linux 目标完全静态化，也不会消除包／系统依赖。kind static 创建静态库，不是完全静态可执行文件开关。crt static 选择受支持 CRT 模式，不是任意 CRT 文件或无依赖分发策略。

应在目标平台检查最终二进制和实际链接库，按最终发行目录核对运行资产、搜索路径及系统兼容。参见[链接](linking.md)。

## 验证

建议依次验证语义、目标 IR／对象、目标链接和目标主机执行，分别记录结果。Hosted CI 与七目标阶段发射检查提供不同证据，均不能代替应用自身依赖和行为验证。

矩阵构建为各目标使用不同目录／名称，避免互相覆盖。项目 Profile 可保存目标／链接／源码选择，参见[项目文件](../cli/project-files.md)。
