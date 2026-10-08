"""Byte-exact public CLI differential: channels, newlines, ANSI, status and PE icons.

Never strip diagnostics, color, paths or whitespace to make the comparison pass.
The original executable is the oracle; --inventory records all failures as well.
The actual native link command is deliberately retained verbatim: installations,
deterministic PE timestamps and the selfhost's required stack reserve differ.
That one trace line has a separately reported contextual comparison; it is never
presented as byte-exact. All surrounding output must still match exactly.
Invalid `pkg info` keeps selfhost's nonzero failure status; the original prints
an error but incorrectly exits successfully. This is an explicit bug-fix
exception, separate from both byte equality and contextual linker output.
"""
from __future__ import annotations

import argparse
import base64
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
from pathlib import Path


def pe_icons(path: Path) -> dict[str, str]:
    """Read every RT_ICON/RT_GROUP_ICON leaf, including name and language IDs."""
    data = path.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[:2] == b'MZ' and data[pe:pe+4] == b'PE\0\0', path
    sections, optional_size = struct.unpack_from('<H12xH', data, pe + 6)
    optional = pe + 24
    directory = optional + (112 if struct.unpack_from('<H', data, optional)[0] == 0x20b else 96)
    resource_rva, resource_size = struct.unpack_from('<II', data, directory + 16)
    if not resource_rva or not resource_size:
        return {}
    headers = optional + optional_size

    def offset(rva: int) -> int:
        for index in range(sections):
            virtual_size, address, raw_size, raw = struct.unpack_from('<IIII', data, headers + 40 * index + 8)
            if address <= rva < address + max(virtual_size, raw_size):
                return raw + rva - address
        raise AssertionError(f'unmapped resource RVA {rva:x}')

    base = offset(resource_rva)
    leaves: dict[str, str] = {}

    def walk(relative: int, keys: tuple[str, ...] = ()) -> None:
        named, numbered = struct.unpack_from('<HH', data, base + relative + 12)
        for index in range(named + numbered):
            name, target = struct.unpack_from('<II', data, base + relative + 16 + 8 * index)
            if name & 0x80000000:
                start = base + (name & 0x7fffffff)
                length = struct.unpack_from('<H', data, start)[0]
                key = data[start+2:start+2+2*length].decode('utf-16-le')
            else:
                key = str(name)
            child = (*keys, key)
            if target & 0x80000000:
                walk(target & 0x7fffffff, child)
            elif child[0] in ('3', '14'):
                rva, size = struct.unpack_from('<II', data, base + target)
                leaves['/'.join(child)] = hashlib.sha256(data[offset(rva):offset(rva)+size]).hexdigest()
    walk(0)
    return leaves


def check_cli_presentation(compiler: Path, stage0: Path, root: Path, out: Path,
                           *, manifest: bool = False) -> dict:
    for script in ['sync_selfhost_cli_text.py', 'sync_selfhost_diagnostic_catalog.py']:
        subprocess.run([sys.executable, '-X', 'utf8', str(root/'infra/scripts'/script), '--check'],
                       cwd=root, check=True, capture_output=True, timeout=30)
    out.mkdir(parents=True, exist_ok=True)
    logs = out / 'logs'
    logs.mkdir(exist_ok=True)
    source = out / 'Main.kn'
    source.write_text('Unit Tests.CliPresentation;\nStatic Function int Main() { Return 0; }\n', encoding='utf-8')
    warning = out / 'Warning.kn'
    warning.write_text('Unit Tests.CliWarning;\nStatic Function int Main()\n{\n\tint values[1];\n\tReturn 0;\n}\n', encoding='utf-8')
    error = out / 'Error.kn'
    error.write_text('Unit Tests.CliError;\nStatic Function int Main() { Return Missing(); }\n', encoding='utf-8')
    syntax = out / 'Syntax.kn'
    syntax.write_text('Unit Tests.CliSyntax;\nStatic Function int Main() { Return 0 }\n', encoding='utf-8')
    locale = out / 'locale.json'
    locale.write_text(json.dumps({'ui.severity.warning': {'text': 'notice'},
        'ui.diag.location': {'text': '{0} ({1},{2})'},
        'ui.diag.summary': {'text': 'counts: {0}/{1}'},
        'W-SYN-00001': {'title': 'Array notation', 'detail': 'Use prefix brackets'}}, ensure_ascii=False), encoding='utf-8')
    cases: list[tuple[str, list[str | Path]]] = []
    def add(name: str, *args: str | Path) -> None:
        cases.append((name, list(args)))
    add('no-arguments')
    for args in [('--help',), ('-h',), ('--version',), ('-V',), ('--version', '--verbose'),
                 ('help',), ('version',), ('unknown',), ('--unknown',), ('--lang',),
                 ('--lang', 'zh', '--help'), ('--lang', 'zh-CN', '--help'),
                 ('--lang', 'invalid', '--help'), ('--lang', 'zh'),
                 ('--locale-file', str(out/'missing.json'), '--help')]:
        add('top-'+ '-'.join(args), *args)
    commands = ['build', 'run', 'fmt', 'vm', 'pkg', 'vm build', 'vm run', 'vm disasm', 'vm pack',
                'pkg build', 'pkg info', 'pkg unpack']
    for command in commands:
        for option in [[], ['-h'], ['--help'], ['--unknown']]:
            add(command + ' ' + ' '.join(option), *command.split(), *option)
    for command in ['build', 'run', 'vm build', 'vm run', 'vm disasm', 'vm pack']:
        for option in ['--project', '--profile', '-o', '--emit', '--target', '--color', '--lang',
                       '--locale-file', '--warn-level', '--pkg-root', '--stdpkg-root', '--vm-path',
                       '--listing', '--link-file', '--linker', '--linker-path', '-L', '-l']:
            add(command+' missing '+option, *command.split(), option)
        for option in ['--color', '--emit', '--kind', '--env', '--runtime', '--panic', '--crt', '--linker']:
            add(command+' invalid '+option, *command.split(), option, 'invalid')
    for option in ['--color', '--emit', '--kind', '--env', '--runtime', '--panic', '--crt', '--linker']:
        add('build invalid value '+option, 'build', source, option, 'invalid')
    for command in ['pkg build', 'pkg info', 'pkg unpack']:
        for option in ['--manifest', '--layout', '-o', '--output']:
            add(command+' missing '+option, *command.split(), option)
    for name, path in [('warning', warning), ('error', error), ('syntax', syntax)]:
        for language in ['en', 'zh', 'zh-CN']:
            for color in ['auto', 'always', 'never']:
                add(f'{name} {language} {color}', 'build', '--no-module-discovery', path,
                    '--emit', 'check', '-o', out/'summary.kcheck', '--lang', language, '--color', color)
    for options in [[], ['--Werror'], ['--warn-level', '0'], ['--Werror', '--warn-level', '0'],
                    ['--warn-level', '-1'], ['--warn-level', 'no'], ['--warn-level', '2x'],
                    ['--locale-file', locale], ['--locale-file', out/'missing.json']]:
        add('warning policy '+str(options), 'build', '--no-module-discovery', warning,
            '--emit', 'check', '-o', out/'summary.kcheck', *options)
    for mode in ['check', 'tokens', 'ast', 'ir', 'obj', 'asm', 'bin']:
        add('build success '+mode, 'build', '--no-module-discovery', source, '--emit', mode,
            '-o', out/('program.exe' if mode == 'bin' else 'program.'+mode))
    add('run success', 'run', '--no-module-discovery', source)
    for mode in ['check', 'tokens', 'ast', 'ir', 'obj', 'asm', 'bin']:
        add('trace success '+mode, 'build', '--no-module-discovery', source, '--emit', mode,
            '-o', out/('trace.exe' if mode == 'bin' else 'trace.'+mode), '--trace')
    for path in [warning, error, syntax]:
        add('trace diagnostic '+path.stem, 'build', '--no-module-discovery', path,
            '--emit', 'check', '-o', out/'trace-check.txt', '--trace')
    for command in ['build', 'run', 'vm build', 'vm run', 'vm pack', 'vm disasm']:
        add(command+' missing source', *command.split(), out/'missing.kn')
    for command in ['build', 'run', 'vm build', 'vm run', 'vm pack']:
        add(command+' missing project', *command.split(), '--project', out/'missing.knproj')
    for command in ['build', 'run']:
        add(command+' unsupported extension', *command.split(), locale)
        add(command+' duplicate input', *command.split(), source, source,
            '--emit', 'check', '-o', out/'duplicates.kcheck')
    add('profile without project', 'build', '--no-module-discovery', source,
        '--profile', 'ignored', '--emit', 'check', '-o', out/'profile.kcheck')
    add('unknown project profile', 'build', '--project', root/'tests/selfhost/fixtures/collection_runtime/kinal.knproj',
        '--profile', 'missing')
    add('fmt missing source', 'fmt', out/'missing.kn')
    for command in ['pkg info', 'pkg unpack']:
        add(command+' missing archive', *command.split(), out/'missing.klib')
        add(command+' invalid archive', *command.split(), locale)
    error_limit = out / 'ErrorLimit.kn'
    error_limit.write_text('Unit Tests.ErrorLimit;\nStatic Function int Main() {\n' +
        ''.join(f' Missing{number}();\n' for number in range(140)) + ' Return 0;\n}\n', encoding='utf-8')
    for language in ['en', 'zh']:
        add('diagnostic limit '+language, 'build', '--no-module-discovery', error_limit,
            '--emit', 'check', '-o', out/'limit.kcheck', '--lang', language, '--color', 'never')
    if manifest:
        sys.path.insert(0, str(Path(__file__).parent))
        from audit_manifest_diagnostics import collect_cases
        for name, sources, _, auto in collect_cases(root, 'windows' if os.name == 'nt' else 'macos' if sys.platform == 'darwin' else 'linux'):
            add('manifest '+name, 'build', *([] if auto else ['--no-module-discovery']), *sources,
                '--color', 'never', '--emit', 'check', '-o', out/'summary.kcheck')
    def compare(case: tuple[int, tuple[str, list[str | Path]]]) -> dict:
        index, (name, original_args) = case
        args = list(original_args)
        isolated = out / 'case-output' / str(index)
        isolated.mkdir(parents=True, exist_ok=True)
        # Output paths and temporary directories belong to one comparison pair.
        # The reference and selfhost receive exactly the same arguments.
        for position, argument in enumerate(args[:-1]):
            if argument == '-o':
                args[position+1] = isolated / Path(args[position+1]).name
        environment = dict(os.environ, TMP=str(isolated), TEMP=str(isolated), TMPDIR=str(isolated))
        results = []
        for role, executable in [('reference', stage0), ('selfhost', compiler)]:
            process = subprocess.run([str(executable), *map(str, args)], cwd=root,
                capture_output=True, timeout=180, env=dict(environment, PATH=str(executable.parent)+os.pathsep+os.environ.get('PATH','')))
            result = dict(returncode=process.returncode,
                stdout=base64.b64encode(process.stdout).decode(), stderr=base64.b64encode(process.stderr).decode())
            results.append(result)
        exact = results[0] == results[1]
        contextual = None
        correction = None
        if not exact and name == 'trace success bin':
            raw = [base64.b64decode(result['stderr']).splitlines(keepends=True) for result in results]
            commands = [[line for line in lines if line.startswith(b'[Trace] link cmd: ')] for lines in raw]
            phases = [[line for line in lines if not line.startswith(b'[Trace] link cmd: ')] for lines in raw]
            if (all(len(lines) == 1 for lines in commands) and phases[0] == phases[1]
                    and results[0]['returncode'] == results[1]['returncode'] == 0
                    and results[0]['stdout'] == results[1]['stdout']):
                contextual = 'Actual linker invocation: installed tool/runtime paths, deterministic timestamp and 16 MiB stack reserve.'
        if name in ('pkg info missing archive', 'pkg info invalid archive'):
            if (results[0]['returncode'] == 0 and results[1]['returncode'] == 1
                    and results[0]['stdout'] == results[1]['stdout']
                    and results[0]['stderr'] == results[1]['stderr']):
                correction = 'Invalid archive must return failure, despite the original pkg info exit status of 0.'
        row = dict(name=name, args=list(map(str, args)),
                   passed=exact or contextual is not None or correction is not None,
                   byte_exact=exact, contextual_difference=contextual,
                   corrected_original_bug=correction,
                   reference=results[0], selfhost=results[1])
        (logs/f'{index:04d}.json').write_text(json.dumps(row, indent=2)+'\n', encoding='utf-8')
        return row
    with ThreadPoolExecutor(max_workers=4) as workers:
        rows = list(workers.map(compare, enumerate(cases)))
    timings = []
    for mode in ['tokens', 'ast', 'check', 'ir', 'bin']:
        measured = []
        for executable in (stage0, compiler):
            process = subprocess.run([str(executable), 'build', '--no-module-discovery', str(source),
                '--emit', mode, '-o', str(out/('timing.exe' if mode == 'bin' else 'timing.'+mode)), '--time'],
                capture_output=True, cwd=root, timeout=180)
            lines = process.stderr.splitlines(keepends=True)
            phases = [re.fullmatch(rb'\[Profile\] ([a-z]+): ([0-9]+) ms\n', line) for line in lines]
            assert process.returncode == 0 and process.stdout == b'' and phases and all(phases), (mode, process)
            measured.append([(match[1].decode(), int(match[2])) for match in phases])
        # Durations are measurements, not reference constants. Compare the
        # complete phase schema and retain both measured values in the report.
        assert [phase for phase, _ in measured[0]] == [phase for phase, _ in measured[1]], measured
        timings.append(dict(mode=mode, reference=measured[0], selfhost=measured[1]))
    icons = None
    if os.name == 'nt':
        expected, actual = pe_icons(stage0), pe_icons(compiler)
        icons = dict(passed=bool(expected) and expected == actual, reference=expected, selfhost=actual)
    report = dict(format='kinal-cli-presentation-v1',
        executables={role: dict(path=str(exe), sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
                     for role, exe in [('reference', stage0), ('selfhost', compiler)]},
        cases=len(rows), passed=sum(row['passed'] for row in rows),
        byte_exact=sum(row['byte_exact'] for row in rows),
        contextual_differences=[dict(name=row['name'], reason=row['contextual_difference'])
                                for row in rows if row['contextual_difference']],
        corrected_original_bugs=[dict(name=row['name'], reason=row['corrected_original_bug'])
                                 for row in rows if row['corrected_original_bug']],
        timing_schemas=timings,
        failures=[row['name'] for row in rows if not row['passed']], icons=icons)
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--stage0', type=Path, required=True)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', action='store_true')
    parser.add_argument('--inventory', action='store_true')
    args = parser.parse_args()
    report = check_cli_presentation(args.compiler.resolve(), args.stage0.resolve(), args.root.resolve(), args.output.resolve(), manifest=args.manifest)
    print(json.dumps(report, indent=2))
    return 0 if args.inventory or (not report['failures'] and (report['icons'] is None or report['icons']['passed'])) else 1


if __name__ == '__main__':
    raise SystemExit(main())
