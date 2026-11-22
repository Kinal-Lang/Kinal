"""Verify token spelling and byte positions in stage0, the selfhost CLI and a KNC lexer consumer."""
import os
import re
import shutil
import subprocess


def make_case(tokens, *, bom=True):
    # Input tokens are specified independently. Columns count source bytes after the BOM.
    data = bytearray(b"\xef\xbb\xbf" if bom else b"")
    rows = []
    line = column = 1
    for trivia, kind, spelling in tokens:
        prefix = trivia.encode("utf-8")
        data.extend(prefix)
        line += prefix.count(b"\n")
        if b"\n" in prefix:
            column = 1 + len(prefix.rsplit(b"\n", 1)[1])
        else:
            column += len(prefix)
        raw = spelling.encode("utf-8")
        rows.append(f"{kind}\t{line}\t{column}\t{raw.hex().upper()}")
        data.extend(raw)
        column += len(raw)
    return data, rows


def retained_input(out, prefix):
    rows = []
    expected = []
    for i in range(33):
        # The three NUL-terminated spellings fill a 2 KiB block. Only its middle address survives.
        name = f"{prefix}{i}"
        middle = f"{name}界".ljust(108 + i, "x")
        left = name.ljust(1023, "L")
        right = name.ljust(1022 - len(middle.encode("utf8")), "R")
        rows.append(" ".join((left, middle, right)))
        expected.append(middle)
    for length in (1024, 1025, 2047, 4097):
        middle = prefix.ljust(length, "v")
        rows.append(f"Z {middle} K")
        expected.append(middle)
    path = out / (prefix + ".kn")
    path.write_bytes(("\r\n".join(rows) + "\n").encode("utf8"))
    return path, expected


def check_lexer_spans(compiler, stage0, vm, root, out, *, stage0_reference=True):
    out.mkdir(parents=True, exist_ok=True)
    src = out / "src"
    shutil.copytree(root / "apps/kinal-selfhost/src/IO", src / "IO", dirs_exist_ok=True)
    shutil.copyfile(root / "tests/selfhost/fixtures/lexer_guest_tokens.kn", src / "Main.kn")
    project = out / "kinal.knproj"
    source_set = 'Source { Entry="src/Main.kn"; Sets=["app"]; Mode=ReachableUnits; }'
    project.write_text('Project LexerConsumer { SourceSet "app" { Roots=["src"]; Include=["**/*.kn"]; } '
                       f'Profile "test" {{ {source_set} Build {{ Backend=VM; }} }} '
                       f'Profile "native" {{ {source_set} Build {{ Backend=Native; }} }} }}\n', encoding="utf-8")

    def run(command):
        return subprocess.run(command, cwd=out, capture_output=True, encoding="utf-8", timeout=180)

    def rows(text):
        return [line for line in text.splitlines() if re.fullmatch(r"\d+\t\d+\t\d+\t[0-9A-F]*", line)]

    bytecode = out / "lexer.knc"
    built = run([compiler, "vm", "build", "--project", project, "--profile", "test", "-o", bytecode])
    assert built.returncode == 0, built.stdout + built.stderr
    consumers = [("VM", [vm, bytecode])]
    native_tools = [("selfhost Native", compiler)]
    if stage0_reference:
        native_tools.append(("stage0 Native", stage0))
    for label, tool in native_tools:
        executable = out / (label.split()[0] + ("-lexer.exe" if os.name == "nt" else "-lexer"))
        build = run([tool, "build", "--project", project, "--profile", "native", "--link-file",
                     compiler.parent / "bridge/kn_selfhost_runtime.o", "-o", executable])
        assert build.returncode == 0, build.stdout + build.stderr
        consumers.append((label, [executable]))
    for target, write, other in [("win64", "_write", "write"), ("linux64", "write", "_write")]:
        ir = out / (target + ".ll")
        built = run([compiler, "build", "--project", project, "--profile", "native",
                     "--target", target, "--emit", "ir", "-o", ir])
        assert built.returncode == 0, built.stdout + built.stderr
        function = re.search(r"^define [^\n]*ConsoleIO_WriteCore_\d+\([^\n]*\).*?^}",
                             ir.read_text(encoding="utf-8"), flags=re.MULTILINE | re.DOTALL)
        assert function and f"@{write}(" in function[0] and f"@{other}(" not in function[0], target
    cases = {
        "spans": [("", 7, "Unit"), (" /*a\n雪\n*/\t", 1, "模块_2"), ("", 92, ";"),
                  ("\r\n//行\r\n/**/ ", 3, r'"a\\\";)}b"'), (" ", 2, "0x1F"),
                  (" ", 2, "12.5f"), (" ", 5, r"'\n'"), (" /* unterminated\n", 0, "")],
        "recovery": [("", 4, '"broken\\'), ("\n", 1, "Next"), ("\n", 6, "'\\"),
                     ("\r\n", 1, "After"), ("\r", 4, r'"a\;b'), ("", 92, ";"),
                     ("\n", 4, r'"c\\'), ("", 86, ")"), ("\n", 4, '"end\\'), ("", 0, "")],
        "boundaries": [(" \t" * 256 + "/*" + "*" * 257 + "/", 1, "界" * 128 + "_9"),
                       ("//" + "/" * 128 + "\n", 3, '""'), ("", 3, r'"\\"'), ("// eof", 0, "")],
        "empty": [("", 0, "")],
    }
    retained = []
    for i in range(128):
        retained.extend([("\n", 1, f"Identifier_{i}"), (" ", 3, f'"literal-{i}"')])
    cases["retained"] = [*retained, ("", 0, "")]
    count = 0
    for name, tokens in cases.items():
        source = out / (name + ".kn")
        data, expected = make_case(tokens, bom=name not in {"retained", "empty"})
        source.write_bytes(data)
        reference = out / (name + ".ktokens")
        # The frozen C driver's file reader rejects zero-byte files before lexing.
        # A UTF-8 BOM alone yields the same EOF at (1, 1) for the reference.
        seed_source = source
        if not data:
            seed_source = out / "empty-bom.kn"
            seed_source.write_bytes(b"\xef\xbb\xbf")
        seed = run([stage0, "build", seed_source, "--no-module-discovery", "--emit", "tokens", "-o", reference])
        assert seed.returncode == 0, seed.stdout + seed.stderr
        seed_rows = rows(reference.read_text(encoding="utf-8"))
        assert seed_rows == expected, (name, "stage0", seed_rows)
        host = run([compiler, "tokens", source])
        assert rows(host.stdout) == expected, (name, "selfhost", host.stdout)
        if name == "recovery":
            # A backslash at end of line must leave the next line available for diagnostics.
            assert host.returncode != 0 and "Unterminated string" in host.stdout + host.stderr
        else:
            assert host.returncode == 0, host.stdout + host.stderr
        for label, command in consumers:
            result = run([*command, source])
            assert result.returncode == 0 and result.stdout.splitlines() == expected, (name, label, result.stdout, result.stderr)
        count += len(expected)
    source, retained = retained_input(out, "Alpha")
    churn, _ = retained_input(out, "Fresh")
    for label, command in consumers:
        result = run([*command, source, churn])
        assert result.returncode == 0 and result.stdout.splitlines() == retained, (label, result.returncode, result.stderr)
    return {"name": "lexer_spans", "ok": True, "sources": len(cases), "tokens": count,
            "retained_spellings": len(retained)}
