"""Source-level metrics contracts. Independent expected values, no app execution."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import subprocess
from pathlib import Path


def check_metrics(compiler: Path, root: Path, out: Path) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    fixture = root / 'tests/selfhost/fixtures/metrics/Main.kn'
    checks = 0

    def invoke(name: str, args: list[str | Path], code: int = 0, cwd: Path = root):
        nonlocal checks
        result = subprocess.run([str(compiler), 'metrics', *map(str, args)], cwd=cwd,
                                capture_output=True, timeout=180)
        (out / (name + '.stdout')).write_bytes(result.stdout)
        (out / (name + '.stderr')).write_bytes(result.stderr)
        assert result.returncode == code, (name, result.returncode, result.stdout[-2000:], result.stderr[-5000:])
        checks += 1
        return result

    args = ['--no-module-discovery', fixture, '--format', 'json']
    first = invoke('fixture', args)
    report = json.loads(first.stdout)
    assert report['schema'] == 'kinal.metrics.v1'
    assert first.stderr == b'', first.stderr
    assert invoke('deterministic', args).stdout == first.stdout
    functions = report['functions']
    named = {f['name']: f for f in functions}

    def function(name):
        return named['Tests.Metrics.' + name]

    for name, cc, cognitive, nesting in [('Straight', 1, 0, 0), ('Text', 1, 0, 0),
                                         ('Nested', 5, 6, 2), ('Logical', 4, 2, 0),
                                         ('Choose', 3, 1, 1), ('Expressions', 4, 3, 1),
                                         ('Recover', 2, 1, 1), ('A', 2, 2, 1), ('B', 2, 2, 1),
                                         ('Loops',4,4,2), ('JumpFlow',1,1,0), ('MakeBlock',1,0,0)]:
        f = function(name)
        assert (f['cyclomatic'], f['counts']['cognitive'], f['counts']['nesting']) == (cc, cognitive, nesting), (name, f)
    assert function('Nested')['counts']['parameters'] == 3
    assert function('Nested')['counts']['statements'] == 7
    assert function('Logical')['counts']['logical_operators'] == 3
    assert function('Choose')['counts']['cases'] == 3
    assert function('Recover')['counts']['catches'] == 1
    assert function('Identity')['generic'] and function('Identity')['bound']
    assert function('UnusedGeneric')['generic'] and not function('UnusedGeneric')['bound']
    assert len([f for f in functions if f['name'] == 'Tests.Metrics.Identity']) == 1
    lambdas = [f for f in functions if f['kind'] == 'lambda' and f['parent'] == function('Factory')['id']]
    assert len(lambdas) == 1 and lambdas[0]['parent'] == function('Factory')['id']
    assert lambdas[0]['cyclomatic'] == 2 and function('Factory')['cyclomatic'] == 1
    assert 'captured' in lambdas[0]['captures'], lambdas
    generic_lambdas = [f for f in functions if f['parent'] == function('GenericFactory')['id']]
    assert len(generic_lambdas) == 1 and generic_lambdas[0]['bound'], generic_lambdas
    initializer = function('GlobalValue.<initializer>')
    assert initializer['cyclomatic'] == 2 and initializer['halstead'] is None and initializer['code_lines'] is None
    block = next(f for f in functions if f['kind'] == 'block')
    assert block['cyclomatic'] == 2 and block['counts']['jumps'] == 1 and block['counts']['records'] == 1
    assert function('JumpFlow')['counts']['unsafe_blocks'] == 1
    assert function('Loops')['counts']['locals'] == 4
    assert {function('A')['id'], function('B')['id']} in [set(c) for c in report['recursive_components']]
    assert function('Main')['dynamic_calls'] == 1
    assert function('Main')['virtual_calls'] == 1
    assert function('Main')['bound_source_calls'] == function('Main')['counts']['calls']
    types = {t['name']: t for t in report['types']}
    assert types['Tests.Metrics.Counter']['inheritance_depth'] == 1
    assert types['Tests.Metrics.BaseCounter']['direct_derived_types'] == 1
    assert 'Tests.Metrics.BaseCounter' in types['Tests.Metrics.Counter']['coupled_types']
    for f in functions:
        h = f['halstead']
        if h is None:
            assert f['kind'] == 'initializer'
            continue
        n = h['distinct_operators'] + h['distinct_operands']
        v = (h['operators'] + h['operands']) * math.log2(n) if n else 0
        assert math.isclose(h['volume'], v, abs_tol=.01), f
        if f['maintainability_index'] is not None:
            mi = max(0, min(100, (171-5.2*math.log(max(v,1))-.23*f['cyclomatic']-16.2*math.log(max(f['code_lines'],1)))*100/171))
            assert f['maintainability_index'] == int(mi + .5), f
    file = report['files'][0]
    assert file['lines'] == len(fixture.read_text(encoding='utf8').splitlines())
    assert file['lines'] == file['code_lines'] + file['comment_only_lines'] + file['blank_lines']
    assert file['comment_lines'] == file['comment_only_lines'] + file['mixed_lines']
    assert report['summary']['counts']['decisions'] == sum(f['counts']['decisions'] for f in functions)
    assert report['summary']['bound_bodies'] <= report['summary']['bodies'] + report['summary']['initializers']

    text = invoke('text', ['--no-module-discovery', fixture, '--all']).stdout.decode('utf8')
    assert 'Tests.Metrics.Nested' in text and 'CC=5  Cognitive=6  Nesting=2' in text
    output = out / 'nested/export/metrics.json'
    invoke('export-json', ['--no-module-discovery', fixture, '-o', output])
    assert output.read_bytes() == first.stdout
    html = out / 'metrics.html'
    invoke('export-html', ['--no-module-discovery', fixture, '-o', html])
    html_text = html.read_text(encoding='utf8')
    embedded = re.search(r'<script type="application/json" id="data">(.*?)</script>', html_text, re.S)
    assert embedded and json.loads(embedded[1]) == report
    assert '<script src=' not in html_text and 'http-equiv="refresh"' not in html_text
    gated = invoke('threshold', [*args, '--max-cyclomatic', '4', '-o', out/'gated.json'], 3)
    assert gated.stdout == b''
    gate = json.loads((out/'gated.json').read_bytes())['thresholds']
    assert not gate['passed'] and function('Nested')['id'] in gate['violations']
    assert json.loads(invoke('threshold-pass', [*args, '--max-cyclomatic', '100']).stdout)['thresholds']['passed']
    for option in ['--format', '--top', '--max-cognitive']:
        invoke('missing-' + option[2:], [option], 2)
    for option, value in [('--format','xml'), ('--top','-1'), ('--top','2147483648'), ('--max-cognitive','bad'), ('--emit','ir'), ('--lto','full')]:
        invoke('invalid-' + option[2:] + '-' + value, [option, value, fixture], 2)
    assert b'kinal metrics' in invoke('help', ['--help']).stderr

    # Never clobber sources or an existing report on a frontend error.
    before = fixture.read_bytes()
    invoke('source-output', ['--no-module-discovery', fixture, '-o', fixture], 1)
    assert fixture.read_bytes() == before
    broken = out / 'Broken.kn'; broken.write_text('Static Function int Main() { Return missing; }\n', encoding='utf8')
    output.write_text('existing report', encoding='utf8')
    invoke('analysis-error', ['--no-module-discovery', broken, '-o', output], 1)
    assert output.read_text(encoding='utf8') == 'existing report'
    invoke('io-error', ['--no-module-discovery', fixture, '-o', out], 1)

    # Exact line partition, BOM, CRLF, multi-line comments, comment markers inside strings.
    lexical = out/'Lexical.kn'
    lexical.write_bytes(b'\xef\xbb\xbfUnit Tests.Lines;\r\n// comment\r\n/* block\r\n\r\nend */\r\n\r\nStatic Function int Main() { string x = "// /*"; Return 0; } // mixed\r\n')
    lex = json.loads(invoke('lines', ['--no-module-discovery', lexical, '--format', 'json']).stdout)['files'][0]
    assert (lex['lines'],lex['code_lines'],lex['comment_only_lines'],lex['mixed_lines'],lex['blank_lines']) == (7,2,4,1,1), lex
    lexical.write_text('Static Function int Main() { Return 0; }',encoding='utf8')
    assert json.loads(invoke('no-final-newline', ['--no-module-discovery', lexical, '--format','json']).stdout)['files'][0]['lines'] == 1

    # Same-Unit multi-file sources, scope/profile selection and package exclusion.
    project = out/'project'; project.mkdir(exist_ok=True)
    (project/'Main.kn').write_text('Unit Tests.Scope; Get IO.Console; Static Function int Main() { IO.Console.PrintLine("analyze only"); Return Value(); }\n',encoding='utf8')
    (project/'Value.kn').write_text('Unit Tests.Scope; Static Function int Value() { Return 1; }\n',encoding='utf-8-sig')
    (project/'Unused.kn').write_text('Unit Tests.Unused; Static Function int Unused() { Return 2; }\n',encoding='utf8')
    manifest=project/'kinal.knproj'
    manifest.write_text('Project Scope { DefaultProfile = "reachable"; SourceSet "src" { Roots = ["."]; Include = ["*.kn"]; } Profile "reachable" { Source { Entry = "Main.kn"; Sets = ["src"]; Mode = ReachableUnits; } } Profile "all" { Source { Entry = "Main.kn"; Sets = ["src"]; Mode = AllSources; } } }\n',encoding='utf8')
    scoped=json.loads(invoke('project', ['--project',manifest,'--format','json']).stdout)
    assert {f['path'] for f in scoped['files']} == {'Main.kn','Value.kn'},scoped['files']
    assert scoped['excluded_dependency_files'] > 0
    all_sources=json.loads(invoke('all-sources',['--project',manifest,'--profile','all','--format','json']).stdout)
    assert all_sources['summary']['files'] == 3
    dependencies=json.loads(invoke('dependencies',['--project',manifest,'--include-dependencies','--format','json']).stdout)
    assert dependencies['summary']['files'] > 2 and any(f['dependency'] for f in dependencies['files'])
    assert b'Kinal project metrics: Scope' in invoke('implicit-project',[],cwd=project).stdout
    invoke('manifest-output',['--project',manifest,'-o',manifest],1)
    assert not list(project.glob('*.exe')) and not (project/'out').exists()
    # Metrics respects a VM profile without attempting native or VM codegen.
    vm_project=project/'vm.knproj'
    vm_project.write_text('Project VmScope { DefaultProfile = "vm"; Profile "vm" { Source { Entry = "Value.kn"; Mode = FileOnly; } Build { Backend = VM; } } }\n',encoding='utf8')
    vm=json.loads(invoke('vm-profile',['--project',vm_project,'--format','json']).stdout)
    assert vm['summary']['files'] == 1 and vm['source_mode'] == 'FileOnly'
    assert not list(project.glob('*.knc'))
    return {'name':'metrics','ok':True,'checks':checks,'fixture_sha256':hashlib.sha256(before).hexdigest(),
            'files':report['summary']['files'],'callables':report['summary']['callables']}


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    print(json.dumps(check_metrics(args.compiler.resolve(),Path(__file__).resolve().parents[2],args.output.resolve()),indent=2))
