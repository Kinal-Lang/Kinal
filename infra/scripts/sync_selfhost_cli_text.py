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
