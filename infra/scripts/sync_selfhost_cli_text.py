"""Generate selfhost help text from the original CLI's literal strings."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DESTINATION = ROOT / "apps/kinal-selfhost/src/IO/Kinal/Compiler/Driver/CliText.kn"
STRING = r'"(?:[^"\\]|\\.)*"'


def strings(text: str) -> str:
    return "".join(json.loads(part) for part in re.findall(STRING, text))


def generate() -> str:
    cli = (ROOT / "apps/kinal/src/driver/kn_driver_cli.inc").read_text(encoding="utf-8")
    formatter = (ROOT / "apps/kinal/src/kn_format.c").read_text(encoding="utf-8")
    entries = {}
    for command, function in [("", "print_help"), ("build", "print_build_help"),
                              ("run", "print_run_help"), ("vm", "print_vm_help"),
                              ("pkg", "print_pkg_help")]:
        match = re.search(r"static void " + function + r"\(void\).*?const char \*\w+\s*=\s*(.*?);", cli, re.S)
        assert match, function
        entries[command] = strings(match[1])
    for command in ["vm build", "vm run", "vm disasm", "vm pack", "pkg build", "pkg info", "pkg unpack"]:
        match = re.search(r'kn_write_str\(("Usage: kinal ' + command + r' .*?)\);', cli, re.S)
        assert match, command
        entries[command] = strings(match[1])
    match = re.search(r'("Usage: kinal fmt .*?);', formatter, re.S)
    assert match
    entries["fmt"] = strings(match[1])
    # Selfhost-only commands extend the public CLI without changing stage0.
    entries["metrics"] = (
        "Usage: kinal metrics [options] [file.kn ...]\n"
        "Analyze project complexity without generating or running a program.\n\n"
        "  --project <path>           Project directory or manifest\n"
        "  --profile <name>           Select project profile/source scope\n"
        "  --format <text|json|html>   Report format (inferred from -o, else text)\n"
        "  -o, --output <path>        Export report; omit or use - for stdout\n"
        "  --top <N>                  Show N hotspots in text (default 10; 0 = all)\n"
        "  --all                      Show every body in text\n"
        "  --include-dependencies     Include participating package/runtime files\n"
        "  --max-cyclomatic <N>        Fail if any body's structured CC exceeds N\n"
        "  --max-cognitive <N>         Fail if any cognitive complexity exceeds N\n"
        "  --max-nesting <N>           Fail if any control nesting exceeds N\n"
        "  --target <target>          Target used for semantic analysis\n"
        "  --pkg-root <path>          Additional package root\n"
        "  --stdpkg-root <path>       Official package root\n"
        "  --no-module-discovery     Analyze only explicitly selected sources\n"
        "  --time                    Show phase timings on stderr\n"
        "  --color / --lang / --locale-file / --warn-level / --Werror\n"
        "                            Standard diagnostic options\n"
        "  -h, --help                Show this help\n\n"
        "With no arguments, use ./kinal.knproj when present.\n"
        "JSON and HTML always include all records. Analysis errors produce no report.\n"
        "Exit: 0 success; 1 analysis/I/O error; 2 invalid option; 3 threshold exceeded.\n"
    )
    lines = ["Unit IO.Kinal.Compiler.Driver.CliText;", "",
             "// Generated from the original CLI by infra/scripts/sync_selfhost_cli_text.py.",
             "// Keep original wording, whitespace and final newlines; --check detects drift.",
             "Safe Function string Help(string command)", "{"]
    for key, value in entries.items():
        lines.append(f"    If (command == {json.dumps(key)}) Return {json.dumps(value, ensure_ascii=False)};")
    lines += ['    Return "";', "}", ""]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected = generate()
    if args.check:
        if DESTINATION.read_text(encoding="utf-8") != expected:
            raise SystemExit("selfhost CLI text is stale; run infra/scripts/sync_selfhost_cli_text.py")
    else:
        DESTINATION.write_text(expected, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
