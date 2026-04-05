# Selfhost 状态

更新时间：2026-03-11

## 当前结论

- `python .\x.py selfhost` 可以稳定构建 stage0。
- `python .\x.py selfhost --test` 是新的标准回归入口。
- `python .\x.py selfhost-bootstrap` 是新的标准自举入口。
- `full --backend host` 已经可用于生成下一阶段编译器。
- `full --backend self` 已经可以独立编译并运行 `selfhost/examples/LexDemo.fx`。
- `full --backend self` 已经额外吃下 `tests/control.kn` 和 `tests/functions.kn`。
- `selfhost` 的输出和中间产物已经统一迁到顶层 [out/selfhost](K:/Project/Flexia-Meta/out/selfhost)。
- 当前 `self` 后端按 [tests/manifest.json](K:/Project/Flexia-Meta/tests/manifest.json) 计是 `9/120 = 7.50%`。

## 已完成

1. selfhost 源码已经迁到当前 Kinal 语法：
   - primitive cast 改成 `[type](expr)`
   - 数组声明改成 `Type[] name`
   - 严格 import 规则下的 `TokenType` 限定名已补齐
2. `HostApi.fx` 已对齐当前宿主导出符号：
   - `__kn_host_compile`
   - `__kn_host_compile_ex`
3. `Link.fx` 已对齐新 bundle 布局：
   - `linker/llc.exe`
   - `linker/lld-link.exe`
   - `runtime/win-x64/kernel32.lib`
   - `runtime/win-x64/kn_util_hostlib.obj`
4. stage0、自举脚本、C-suite 脚本都已切到新的 `out/selfhost` 布局。
5. `self` LLVM backend 现在新增了：
   - 整数 `+ - * / %`
   - `while`
   - `for (; cond; )`
   - 简单顶层函数定义与调用
   - `kn_chkstk.obj` 链接接入

## 下一步

1. 继续推进 `self` 后端覆盖范围，优先补 extern 调用、限定名调用、字符字面量和集合/数组子集。
2. 把 `selfhost-bootstrap` 的阶段结果继续纳入更细的完成度统计。
3. 清理 `selfhost/examples` 的旧警告样例。
