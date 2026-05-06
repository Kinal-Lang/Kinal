from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path


@dataclass
class BlockStats:
    suspicious_runs: int = 0
    normalized_runs: int = 0


def detect_eol(text: str) -> str:
    return "\r\n" if "\r\n" in text else "\n"


def split_lines(text: str) -> list[str]:
    return text.replace("\r\n", "\n").split("\n")


def join_lines(lines: list[str], eol: str) -> str:
    return eol.join(lines)


def is_kinal_fence(line: str) -> bool:
    stripped = line.strip()
    if not stripped.startswith("```"):
        return False
    return stripped[3:].strip().lower() == "kinal"


def is_fence_close(line: str) -> bool:
    return line.strip() == "```"


def analyze_block(lines: list[str]) -> int:
    suspicious_runs = 0
    index = 0
    while index < len(lines):
        if lines[index].strip() != "":
            index += 1
            continue
        end = index
        while end < len(lines) and lines[end].strip() == "":
            end += 1
        next_nonblank = lines[end].strip() if end < len(lines) else ""
        if next_nonblank == "{" or end - index > 1:
            suspicious_runs += 1
        index = end
    return suspicious_runs


def normalize_block(lines: list[str]) -> tuple[list[str], BlockStats]:
    stats = BlockStats(suspicious_runs=analyze_block(lines))
    normalized: list[str] = []
    index = 0
    while index < len(lines):
        if lines[index].strip() != "":
            normalized.append(lines[index])
            index += 1
            continue
        end = index
        while end < len(lines) and lines[end].strip() == "":
            end += 1
        next_nonblank = lines[end].strip() if end < len(lines) else ""
        if next_nonblank == "{":
            stats.normalized_runs += 1
            index = end
            continue
        normalized.append("")
        if end - index > 1:
            stats.normalized_runs += 1
        index = end
    return normalized, stats


def process_file(path: Path, check_only: bool) -> tuple[bool, BlockStats]:
    text = path.read_text(encoding="utf-8")
    eol = detect_eol(text)
    lines = split_lines(text)
    output: list[str] = []
    changed = False
    stats = BlockStats()
    index = 0
    while index < len(lines):
        line = lines[index]
        output.append(line)
        if not is_kinal_fence(line):
            index += 1
            continue
        index += 1
        block_start = len(output)
        block: list[str] = []
        while index < len(lines) and not is_fence_close(lines[index]):
            block.append(lines[index])
            index += 1
        normalized_block, block_stats = normalize_block(block)
        stats.suspicious_runs += block_stats.suspicious_runs
        stats.normalized_runs += block_stats.normalized_runs
        output.extend(normalized_block)
        if block != normalized_block:
            changed = True
        if index < len(lines):
            output.append(lines[index])
            index += 1
        else:
            del output[block_start:]
            output.extend(block)
    if changed and not check_only:
        new_text = join_lines(output, eol)
        if text.endswith(("\n", "\r\n")):
            new_text += eol
        path.write_text(new_text, encoding="utf-8")
    return changed, stats


def iter_markdown_files(roots: list[Path]) -> list[Path]:
    files: list[Path] = []
    for root in roots:
        if root.is_file() and root.suffix.lower() == ".md":
            files.append(root)
            continue
        if root.is_dir():
            files.extend(sorted(root.rglob("*.md")))
    return sorted({file.resolve() for file in files})


def main() -> int:
    parser = argparse.ArgumentParser(description="Normalize extra blank lines in fenced kinal Markdown blocks.")
    parser.add_argument("paths", nargs="+", help="Markdown files or directories to scan")
    parser.add_argument("--check", action="store_true", help="Report suspicious blocks without rewriting files")
    args = parser.parse_args()

    files = iter_markdown_files([Path(path) for path in args.paths])
    changed_files = 0
    suspicious_files = 0
    suspicious_runs = 0
    normalized_runs = 0

    for path in files:
        changed, stats = process_file(path, args.check)
        if changed:
            changed_files += 1
        if stats.suspicious_runs:
            suspicious_files += 1
            suspicious_runs += stats.suspicious_runs
            if args.check:
                print(f"SUSPICIOUS {path}: {stats.suspicious_runs}")
        normalized_runs += stats.normalized_runs

    if args.check:
        print(f"checked {len(files)} markdown files; suspicious files: {suspicious_files}; suspicious runs: {suspicious_runs}")
        return 1 if suspicious_runs else 0

    print(f"normalized {changed_files} files; normalized runs: {normalized_runs}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())