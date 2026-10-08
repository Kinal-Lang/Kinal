"""Pure-Kinal formatter and diagnostic policy contract.

Formatting is compared byte-for-byte with stage0 on valid lexical inputs, and
re-lexed independently of formatter parity on boundary-sensitive inputs.
Malformed text is intentionally rejected rather than silently truncated. The
original compiler is the diagnostic oracle: channels, raw bytes, colors and
exit status must match, including fallback behavior for malformed options.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
from pathlib import Path


def check_formatter(compiler: Path, stage0: Path, root: Path, out: Path) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    checks = 0

    def invoke(tool: Path, *args: str | Path, data: bytes | None = None):
        nonlocal checks
        checks += 1
        return subprocess.run([str(tool), "fmt", *map(str, args)], input=data,
                              cwd=root, capture_output=True, timeout=60)

    fixtures = [
        b"Unit A;Function int Main(){Return 0;}",
        b"Unit A;\n// comment\nFunction int Main(){int x=2+3;If(x>2){Return x;}Else{Return 0;}}\n",
        'Unit Café;\r\n// café 雪\r\nFunction string Text(){Return "雪\\n café";}\r\n'.encode(),
        b"\xef\xbb\xbfUnit A;\nFunction void Main(){}\n",
        b"/*hi*/Unit A;\nFunction int Main(){int[] a={1,2,3};For(int i=0;i<3;i++){a[i]+=i;}Return a[0];}\n",
        b"", b"//comment", b"\n\n\n", b"/*comment*/", b"\xef\xbb\xbf",
        b"Function void Main(){/*multi\nline*/Return;}\n",
        b"Function int Main(){int a=-1;int b=+a;int c=a*-b;Return a++ + --b;}\n",
        b"Unsafe Static Function int Main(){int x=7;int a=- - x;int c=- - - x;int* p=&x;int** q=&p;int d=* * q;Return a==7 && c==-7 && d==7 && x==7 ? 0 : 1;}\n",
        b"Function void Main(){Switch(1){Case(1){Break;}Case(default){Return;}}}\n",
        b"Function void Main(){Try{Throw \"message\";}Catch(string text){Return;}}\n",
        b"Function string Main(){Return \"quote: \\\" slash: \\\\ line: \\n\";}\n",
        b"Unit A;\rFunction int Main(){Return 0;}\r",
        b"Unit A;\r\n\n\nFunction int Main(){Return 0;}\r\n",
        b"Unit A;\n\n\nFunction int Main(){\n\nReturn 0;\n\n}\n\n",
        b"Get IO.Console;Alias Print By IO.Console.PrintLine;\nFunction int Main(){int[] values={1,2};Foreach(int value In values){Print(value);}Return 0;}\n",
    ]
    # Real compiler sources exercise the same formatter on its own input syntax.
    corpus = sorted((root / "apps/kinal-selfhost/src").rglob("*.kn"))
    fixtures += [path.read_bytes() for path in corpus]
    for number, original in enumerate(fixtures):
        actual = invoke(compiler, "--stdin", data=original)
        reference = invoke(stage0, "--stdin", data=original)
        assert actual.returncode == reference.returncode == 0, (number, actual, reference)
        assert actual.stderr == b"", (number, actual.stderr)
        assert actual.stdout == reference.stdout, (number, actual.stdout, reference.stdout)
        again = invoke(compiler, "--stdin", data=actual.stdout)
        assert again.returncode == 0 and again.stdout == actual.stdout, (number, "not idempotent")
        checked = invoke(compiler, "--stdin", "--check", data=actual.stdout)
        assert checked.returncode == 0 and checked.stdout == b"" and checked.stderr == b""

    # Use the compiler's lexer (kind + exact spelling), ignoring only locations.
    # Stage0 parity and formatter idempotence can share the same semantic bug.
    boundaries = [b"- - x", b"+ + x", b"- - - x", b"/ /", b"/ *", b"1 . 2",
                  b"< <", b"> >", b"& &", b"| |", b"! =", b"+ =", b"- ="]
    for number, original in enumerate(boundaries):
        formatted = invoke(compiler, "--stdin", data=original)
        assert formatted.returncode == 0
        streams = []
        for label, content in (("before", original), ("after", formatted.stdout)):
            path = out / f"boundary-{number}-{label}.kn"
            path.write_bytes(content)
            token_path = path.with_suffix(".ktokens")
            result = subprocess.run([str(compiler), "build", "--emit", "tokens", "--no-module-discovery",
                                     str(path), "-o", str(token_path)], cwd=root, capture_output=True, timeout=60)
            assert result.returncode == 0, (path, result.stdout, result.stderr)
            fields = [line.split("\t") for line in token_path.read_text(encoding="utf-8").splitlines()]
            streams.append([(parts[0], parts[3]) for parts in fields if len(parts) == 4])
        assert streams[0] and streams[0] == streams[1], (original, formatted.stdout, streams)

    # Executable behavior is an additional oracle for the original corruption.
    executable_source = next(source for source in fixtures if b"int a=- - x" in source)
    for role, tool in (("stage0", stage0), ("selfhost", compiler)):
        formatted = invoke(tool, "--stdin", data=executable_source)
        for label, content in (("before", executable_source), ("after", formatted.stdout)):
            path = out / f"operators-{role}-{label}.kn"
            path.write_bytes(content)
            executable = path.with_suffix(".exe" if os.name == "nt" else "")
            result = subprocess.run([str(tool), "build", "--no-module-discovery", str(path), "-o", str(executable)],
                                    cwd=root, capture_output=True, timeout=180)
            assert result.returncode == 0, (path, result.stdout, result.stderr)
            result = subprocess.run([str(executable)], cwd=root, capture_output=True, timeout=30)
            assert result.returncode == 0, (path, result.stdout, result.stderr)

    original = fixtures[0]
    expected = invoke(compiler, "--stdin", data=original).stdout
    assert invoke(compiler, "--stdin", "--check", data=original).returncode == 1
    both = invoke(compiler, "--stdin", "--check", "--stdout", data=original)
    assert both.returncode == 1 and both.stdout == expected

    files = out / "directory with spaces 雪"
    nested = files / "nested"
    nested.mkdir(parents=True, exist_ok=True)
    first = files / "first.kn"
    second = nested / "second.kn"
    ignored = files / "leave.txt"
    for path in (first, second, ignored):
        path.write_bytes(original)
    preview = invoke(compiler, "--stdout", first)
    assert preview.returncode == 0 and preview.stdout == expected and first.read_bytes() == original
    checked = invoke(compiler, "--check", "--stdout", first)
    assert checked.returncode == 1 and checked.stdout == b"" and first.read_bytes() == original
    formatted = invoke(compiler, files)
    assert formatted.returncode == 0 and formatted.stdout == formatted.stderr == b""
    assert first.read_bytes() == second.read_bytes() == expected
    assert ignored.read_bytes() == original
    assert invoke(compiler, "--check", files).returncode == 0
    alternate = out / "explicit.fx"
    alternate.write_bytes(original)
    assert invoke(compiler, alternate).returncode == 0 and alternate.read_bytes() == expected

    # Lexically malformed input must never produce partial stdout or rewrite a
    # file. Syntax errors with a valid token stream remain format-able, as in C.
    invalid = [b'Unit A; "unterminated', b"Unit A; '\\x'", b"Unit A; `bad`",
               b"Unit A; /* unclosed", b"Unit A;\0Function void Main(){}"]
    for number, source in enumerate(invalid):
        result = invoke(compiler, "--stdin", "--stdin-filepath", "virtual-雪.kn", data=source)
        assert result.returncode == 1 and result.stdout == b"" and result.stderr, (number, result)
        path = out / f"invalid-{number}.kn"
        path.write_bytes(source)
        result = invoke(compiler, path)
        assert result.returncode == 1 and result.stdout == b"" and path.read_bytes() == source
    for arguments in [("--unknown",), ("--stdin-filepath",), (out / "missing.kn",)]:
        result = invoke(compiler, *arguments)
        assert result.returncode == 1 and result.stdout == b"" and result.stderr
    assert invoke(compiler, "--help").returncode == 0
    assert invoke(compiler).returncode == 1
    return {"fixtures": len(fixtures), "compiler_source_files": len(corpus), "checks": checks,
            "boundary_token_streams": len(boundaries), "executable_semantics": True}


def check_diagnostics(compiler: Path, stage0: Path, root: Path, out: Path) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    source = out / "warning-{1}-café-雪.kn"
    source.write_text("Unit Tests.WarningPolicy;\nFunction int Main() { int values[1]; Return 0; }\n", encoding="utf-8")
    checks = 0

    def invoke(*options: str | Path):
        nonlocal checks
        results = []
        for tool in (stage0, compiler):
            result = subprocess.run([str(tool), "build", str(source), "--emit", "check",
                                     "-o", str(out / "summary.kcheck"), *map(str, options)],
                                    cwd=root, capture_output=True, timeout=60)
            results.append((result.returncode, result.stdout, result.stderr))
        checks += 1
        assert results[0] == results[1], (options, results)
        return results[1][2]

    normal = invoke("--color", "never")
    for options in [("--warn-level", "0"), ("--Werror",), ("--Werror", "--warn-level", "0"),
                    ("--color", "always"), ("--color", "auto"), ("--lang", "zh"),
                    ("--lang", "zh-CN"), ("--color", "invalid"), ("--lang", "invalid"),
                    ("--warn-level", "-1"), ("--warn-level", "no"),
                    ("--warn-level", "9999999999999999999999999"),
                    ("--locale-file", out / "missing.json"), ("--locale-file",)]:
        invoke(*options)
    locale = out / "custom-locale.json"
    locale.write_text(json.dumps({
        "ui.stage.parser": {"text": "解析 café"},
        "ui.severity.warning": {"text": "注意"},
        "ui.diag.location": {"text": "{0}：{1}：{2}"},
        "W-SYN-00001": {"title": 'Custom "array" 雪', "detail": "Use canonical arrays"},
    }, ensure_ascii=True), encoding="utf-8")
    invoke("--locale-file", locale)
    locale.write_text(json.dumps({"selfhost.Parser.Legacy Array Syntax": {"title": "Direct entry"}}), encoding="utf-8")
    invoke("--locale-file", locale)
    for contents in ["{", '{"x": {"text": "bad\\uD800"}}', '{"x": 2}',
                     '{"x": {"title": "ok",}}', '{"x": {}} trailing', "[]"]:
        locale.write_text(contents, encoding="utf-8")
        invoke("--locale-file", locale)
    assert invoke("--color", "never") == normal

    exported = out / "english.json"
    exports = []
    for tool in (stage0, compiler):
        if exported.exists():
            exported.unlink()
        result = subprocess.run([str(tool), "build", "--dump-locale-en", str(exported)],
                                cwd=root, capture_output=True, timeout=60)
        assert result.returncode == 0, result
        exports.append((result.returncode, result.stdout, result.stderr, exported.read_bytes()))
    assert exports[0] == exports[1], ("English locale export differs", exports)
    template = json.loads(exported.read_text(encoding="utf-8"))
    assert invoke("--locale-file", exported, "--color", "never") == normal
    return {"checks": checks, "exported_entries": len(template), "byte_exact": True}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--formatter-only", action="store_true")
    args = parser.parse_args()
    root, output = args.root.resolve(), args.output.resolve()
    result = {"formatter": check_formatter(args.compiler.resolve(), args.stage0.resolve(), root, output / "formatter")}
    if not args.formatter_only:
        result["diagnostics"] = check_diagnostics(args.compiler.resolve(), args.stage0.resolve(), root, output / "diagnostics")
    print(json.dumps(result, indent=2))
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
