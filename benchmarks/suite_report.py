"""Render the reports for one benchmark run using its recorded summary."""

import argparse
import csv
import json
from pathlib import Path
from statistics import median


STATUS = {"passed": "通过", "failed": "失败", "skipped": "跳过"}
CSV_COLUMNS = (
    "configuration", "language", "version", "status", "reason", "phase", "case",
    "n", "samples", "median_ms", "min_ms", "max_ms", "batch_repetitions",
    "seven_seed_peak_working_set_mib",
)


def cell(value):
    text = " ".join(str(value).splitlines())
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("|", "&#124;")


def table(headers, rows):
    def line(values):
        return "| " + " | ".join(cell(value) for value in values) + " |"

    return [line(headers), line(["---"] * len(headers)), *(line(row) for row in rows), ""]


def duration(samples):
    return f"{median(samples):.4g}" if samples else "—"


def write_csv(path, summary):
    sizes = summary.get("case_sizes", {})
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=CSV_COLUMNS)
        writer.writeheader()
        for variant in summary["variants"]:
            base = {
                "configuration": variant["id"], "language": variant["language"],
                "version": variant.get("version", ""), "status": variant["status"],
                "reason": variant.get("reason", ""),
            }
            if variant["status"] != "passed":
                writer.writerow(base)
                continue
            for phase in ("build", "startup", "runtime"):
                cases = summary["cases"] if phase == "runtime" else [""]
                for case in cases:
                    point = variant["runtime"][case] if case else {}
                    samples = point["ms"] if case else variant.get(phase + "_ms", [])
                    row = dict(base, phase=phase, case=case, n=sizes.get(case), samples=len(samples))
                    if samples:
                        row.update(median_ms=median(samples), min_ms=min(samples), max_ms=max(samples))
                    if point:
                        row.update(batch_repetitions=point["reps"],
                                   seven_seed_peak_working_set_mib=point.get("peak_mib"))
                    writer.writerow(row)


def render(run_dir):
    run_dir = Path(run_dir)
    summary = json.loads((run_dir / "summary.json").read_text(encoding="utf-8"))
    cases = summary["cases"]
    variants = summary["variants"]
    passed = [v for v in variants if v["status"] == "passed"]
    settings = summary["settings"]
    descriptions = summary.get("descriptions", {})
    sizes = summary.get("case_sizes", {})
    generated = settings.get("generated_cases")
    program = f"含全部 {generated} 个用例的程序" if generated else "完整基准程序"

    has_other_languages = any(v["language"] != "Kinal" for v in variants)
    lines = [
        "# Kinal 跨语言性能基准" if has_other_languages else "# Kinal 性能基准", "",
        f'{summary["started"]}　{summary["host"]}', "",
        f"共选取 {len(cases)} 个运行用例和 {len(variants)} 种配置，{len(passed)} 种配置完成验证和测量。",
        "",
        "`kinal-self` 使用自举 stage3 编译器，`kinal-c` 使用选定的 stage0 编译器。"
        "无后缀时两者各用默认设置，有 `-lto` 后缀时两者都传入 `--lto=full`。"
        "每轮先测选中的 Kinal 配置，各配置依次完成，只有同一配置内的用例顺序打乱。"
        "机器温度和后台负载随时间的变化可能影响较小的耗时差异。",
        "", "## 构建与启动", "",
        f"构建列衡量{program}的构建，启动列衡量新进程启动、输出校验值并退出。"
        + ("Java 和 .NET 的构建产物为字节码和 IL。解释执行的配置以横线表示构建耗时。" if has_other_languages else ""), "",
    ]
    if "build_samples" in settings and "startup_samples" in settings:
        lines.extend([f'构建和启动各预热一次，再分别取 {settings["build_samples"]} 次构建、'
                      f'{settings["startup_samples"]} 次启动的中位耗时。', ""])
    rows = []
    for v in variants:
        good = v["status"] == "passed"
        reason = " ".join(str(v.get("reason", "")).split())
        if len(reason) > 220:
            reason = reason[:217] + "…"
        rows.append((v["id"], v["language"], v.get("version", "") or "—",
                     STATUS.get(v["status"], v["status"]),
                     duration(v.get("build_ms", [])) if good else "—",
                     duration(v.get("startup_ms", [])) if good else "—", reason or ""))
    lines.extend(table(["配置", "语言", "版本", "状态", "构建 (ms)", "启动 (ms)", "原因"], rows))

    if passed and cases:
        lines.extend(["## 运行时执行", "",
                      "下表为每次算法调用的中位耗时，单位为毫秒。父进程从发出批次到收到结果计时，"
                      "耗时包含一次管道往返，按实际调用次数均摊。表中各列分别比较同一输入规模。", ""])
        if "processes" in settings and "samples_per_process" in settings:
            lines.extend([f'每种配置、每个用例启动 {settings["processes"]} 个工作进程，'
                          f'每个进程预热后采集 {settings["samples_per_process"]} 批正式样本。', ""])
        lines.extend([
            "新增四项的参考值来自独立实现，原有 18 项的参考值来自规范 Python 算法。"
            "每项均在一半和完整输入规模下逐一核对种子 7–13，另校验每个计时批次的结果总和。"
            "整数校验值须精确相等。浮点矩阵乘法和浮点递推按输出值核对，"
            "相对容差为 `2e-6`，绝对容差为 `1e-6`。", ""])
        if descriptions or sizes:
            case_rows = []
            for case in cases:
                title, detail = descriptions.get(case, [case, ""])
                case_rows.append((case, title, sizes.get(case, "—"), detail))
            lines.extend(table(["用例", "名称", "输入 n", "工作量"], case_rows))
        for start in range(0, len(cases), 5):
            group = cases[start:start + 5]
            times = [(v["id"], *(duration(v["runtime"][key]["ms"]) for key in group)) for v in passed]
            lines.extend(table(["配置", *group], times))

    lines.extend(["CSV 包含每个用例的样本数、时间范围和进程峰值工作集。"
                  "内存数据在独立进程中测量，每个输入规模使用七个种子各执行一次，单位为 MiB。"
                  " [对比 CSV](comparison.csv) · [逐批原始采样](samples.jsonl) · [运行配置与结果](summary.json)", ""])
    write_csv(run_dir / "comparison.csv", summary)
    (run_dir / "REPORT.md").write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_dir", type=Path, help="directory containing summary.json")
    render(parser.parse_args().run_dir)
