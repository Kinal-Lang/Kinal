# 包与 .klib 归档

Kinal 包使用 `package.knpkg.json` 描述（兼容旧文件名 `package.knpkg`）。
工程使用独立的 `kinal.knproj`，两者不能混用。

## 包管理命令

C stage0 CLI 提供：

```sh
kinal pkg build --manifest ./mylib -o ./mylib.klib
kinal pkg build --manifest ./mylib --layout ./kpkg
kinal pkg info ./mylib.klib
kinal pkg unpack ./mylib.klib -o ./recovered
```

`--manifest` 可以是文件或包目录。`--layout` 生成
`<name>/<version>/package.knpkg.json` 包装清单和 `lib/<name>.klib`，
与 `-o` 二选一。`pkg info` 显示归档文件、生成它的编译器、
条目数及条目总字节数，不提供“已编译导出符号表”。

自举编译器已能在工程编译时使用这些包；自举版 CLI 尚未实现
`pkg build/info/unpack` 命令。

## 包清单

```json
{
  "kind": "library",
  "name": "Acme.Greeter",
  "version": "1.0.0",
  "summary": "Greeting helpers",
  "source_root": "src",
  "modules": ["Acme.Greeter"],
  "dependencies": []
}
```

| 字段 | 含义 |
|------|------|
| `name` | 必填包标识；`IO.` 开头的名称保留给官方包根目录。 |
| `version` | 版本选择用的文本；建议每个发布包都填写。 |
| `source_root` | 递归搜索 Kinal 源码的目录。 |
| `source_files` | 显式源码路径字符串数组，优先于 `source_root`；各文件必须声明 Unit。 |
| `klib` | 可选归档路径，存在时优先使用归档。 |
| `summary`、`url` | 可选文本元数据。 |
| `modules`、`dependencies` | 可选字符串数组元数据，不会自动下载包或求解版本约束。 |

库清单必须提供名称，并至少提供 `source_root`、非空 `source_files`
或 `klib` 之一。路径相对于清单所在目录解析。指定的归档不存在时回退到源码字段；
归档存在但损坏时必须报错，不会静默回退。

清单使用 JSON 字符串、数组、对象、数字、布尔值和 null。
Unicode 转义及代理项对解码为 UTF-8。损坏的 JSON、尾随数据、
无效转义和超过 64 层的嵌套会被拒绝。
路径和名称接口以 NUL 结尾，因此清单字符串不允许包含 NUL。

## 工程依赖

```text
Project Example
{
    DefaultProfile = "native";

    Packages
    {
        Roots = ["packages"];
        OfficialRoots = ["official-packages"];
    }

    SourceSet "main"
    {
        Roots = ["src"];
        Include = ["**/*.kn"];
    }

    Profile "native"
    {
        Source
        {
            Entry = "src/Main.kn";
            Sets = ["main"];
            Mode = ReachableUnits;
        }
        Build { Backend = Native; Environment = Hosted; }
        Packages { Roots = ["profile-packages"]; }
    }
}
```

运行 `kinal build --project .`。普通包根目录顺序为：
工程级 Roots、当前 Profile 的 Roots、工程内自动识别的 `kpkg`。
官方包根目录顺序为：工程级 OfficialRoots、Profile 的 OfficialRoots、
已安装标准库。根目录应包含包目录及清单，不能只是随意放置的 `.klib` 文件。
扫描跳过 `.git`、`.kinal-cache`、`build` 和 `out` 子树。

普通包与官方包分别按包名选择最高版本。点分的数字段按数值比较，
其他段按文本比较；缺失的数字段视作零（`1.0` 等于 `1.0.0`）。
相同版本保留先出现根目录中的包。这不是完整的 SemVer 依赖求解器。

导入按 **Unit** 解析，而不是按包名覆盖：工程 Unit 优先于普通包，
普通包优先于官方包。同一个包内未被覆盖的其他 Unit 仍然可用。
`AutoDiscovery = false` 限制工程本地源码发现及普通依赖；
显式导入的官方标准库仍可解析。

C CLI 的单文件构建还支持 `--pkg-root <dir>`。
自举版目前通过 `kinal.knproj` 配置额外包根目录。

## 归档内容与原生库

当前 `KNKLIB1` 保存嵌入式包清单及源码、原生资源载荷，
**不是**预编译的 Kinal 目标码、typed HIR 或稳定的序列化类型接口。
使用者导入 Unit 时，编译器仍会编译其中的 Kinal 源码；
原生资源则保留各自的平台与 ABI 要求。

不要用 `--link-file` 传递 `.klib`。它用于目标文件、静态库等原生链接输入。
`.klib` 应通过包根目录发现；其原生载荷通过 Kinal FFI 元数据或工程 Link 配置链接。

自举版把已安装标准包提取到编译器目录下的 `stdlib-cache` 代目录，
把工程归档提取到按内容指纹隔离的 `package-cache`。
归档内容改变后不会复用旧源码文件，包括文件总长度不变的替换。

## 相关文档

- [CLI 概览](compiler.md)
- [项目结构](../getting-started/project-structure.md)
- [模块系统](../language/modules.md)
