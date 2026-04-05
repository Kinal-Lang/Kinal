# Kinal Selfhost

`selfhost/` 是正在推进中的 Kinal 自举编译器。当前稳定路径是：

- `lex/parse/sema`
- `full --backend host`
- `full --backend self` 的最小闭环

`selfhost` 现在统一通过根目录的 [x.py](K:/Project/Flexia-Meta/x.py) 驱动，不再使用旧的 `build.py` 或 `build/` 目录。

## 目录

- `src/`: selfhost 编译器源码
- `examples/`: 自举和回归样例
- `tests/`: stage0、自举、C-suite 脚本
- `support/`: selfhost 后端辅助宿主对象

## 常用命令

```powershell
python .\x.py selfhost
python .\x.py selfhost --test
python .\x.py selfhost-bootstrap --metric-backend self
```

## 输出目录

- stage0 bundle: [out/selfhost/stage0-host](K:/Project/Flexia-Meta/out/selfhost/stage0-host)
- selfhost 回归输出: [out/selfhost/tests](K:/Project/Flexia-Meta/out/selfhost/tests)
- 自举阶段输出: [out/selfhost/bootstrap](K:/Project/Flexia-Meta/out/selfhost/bootstrap)

## 当前能力

- `host` 后端已经可用于构建下一阶段编译器
- `self` 后端已经能独立编译并运行 `LexDemo`
- `self` 后端现在还能吃下基础整数算术、`while/for`、简单顶层函数定义和调用
- bundle 内已经包含 `kinal-host.lib`、`LLVM-C.lib`、`llc`、`lld-link`、runtime 对象和 import libs

## 已知限制

- `self` 后端仍是子集实现，距离和 C 版 1:1 对齐还有差距
- `selfhost/tests/bootstrap_until_target.py` 的完成度取决于 `tests/manifest.json` 当前覆盖范围
- `selfhost/examples` 里仍有少量旧语法样例，只是不再阻塞 stage0
