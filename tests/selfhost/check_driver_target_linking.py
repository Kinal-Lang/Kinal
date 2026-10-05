"""Reference-gated LTO and hosted Linux ARM64 linking for the Kinal driver.

Host LTO artifacts are executed. Foreign binaries are checked as ELF only.
Capabilities are required by default. Explicit optional-toolchain checks may
exclude only recognized unavailable executables, never arbitrary compiler failures.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys


def check_driver_target_linking(compiler: Path, stage0: Path, root: Path,
                                out: Path, *, allow_missing_toolchain: bool = False) -> dict[str, object]:
    out.mkdir(parents=True, exist_ok=True)
    logs = out / 'logs'
    logs.mkdir(exist_ok=True)
    source = out / 'Main.kn'
    source.write_text('Unit Tests.DriverTargetLinking; Static Function int Main() { Return 42; }\n')
    cases: list[dict[str, object]] = []
    sequence = 0

    def run(label: str, command: list[str | Path]) -> subprocess.CompletedProcess[str]:
        nonlocal sequence
        sequence += 1
        result = subprocess.run([str(part) for part in command], cwd=out,
                                capture_output=True, text=True, timeout=300)
        (logs / f'{sequence:03d}-{label}.json').write_text(json.dumps(dict(
            command=result.args, returncode=result.returncode,
            stdout=result.stdout, stderr=result.stderr), indent=2) + '\n')
        return result

    def unavailable(result: subprocess.CompletedProcess[str]) -> bool:
        text = (result.stdout + result.stderr).lower()
        messages = ('zig toolchain is not available', 'clang toolchain is not available',
                    'lld toolchain is not available', 'configured linker not found')
        return allow_missing_toolchain and any(message in text for message in messages)

    versions = dict(line.split('=', 1) for line in (root / 'VERSION').read_text().splitlines() if '=' in line)
    for option in ('version', '--version', '-V'):
        result = run('selfhost-version-' + option.lstrip('-'), [compiler, option, '--verbose'])
        assert result.returncode == 0, (option, result.stdout, result.stderr)
        assert result.stdout.splitlines() == ['Kinal selfhost ' + versions['kinal'], 'kinalvm ' + versions['kinalvm']]
    cases.append(dict(name='verbose_component_versions', status='passed', executed=True))

    suffix = '.exe' if os.name == 'nt' else ''
    selectors = out / 'UnknownSelectors.kn'
    selectors.write_text('''Unit Tests.UnknownLinkSelectors;
[LinkLibTarget("host", "missing_unsupported_host_selector")]
[LinkLibTarget("x64", "missing_cli_only_selector")]
[LinkFileTarget("not-a-target", "missing_unknown_selector.o")]
[LinkName("unused_selector_probe")] Extern Function i32 Probe() By C;
Static Function int Main() { Return 42; }
''')
    for name, executable in [('stage0', stage0), ('selfhost', compiler)]:
        binary = out / (name + '-unknown-selectors' + suffix)
        result = run(name + '-unknown-selectors', [executable, 'build', '--no-module-discovery',
                     selectors, '-o', binary])
        assert result.returncode == 0, (name, result.stdout, result.stderr)
        result = run(name + '-unknown-selectors-execute', [binary])
        assert result.returncode == 42, (name, result.stdout, result.stderr)
    cases.append(dict(name='unknown_link_selectors_inactive', status='passed', executed=True))

    for mode in ('full', 'thin'):
        reference = out / ('stage0-lto-' + mode + suffix)
        expected = run('stage0-lto-' + mode, [stage0, 'build', source,
                       '--no-module-discovery', '--lto=' + mode, '-o', reference])
        if expected.returncode and unavailable(expected):
            cases.append(dict(name='lto_' + mode, status='skipped',
                              reason='reference compiler/toolchain cannot link this LTO mode',
                              reference_returncode=expected.returncode))
            continue
        assert expected.returncode == 0, (mode, 'reference compile', expected.stdout, expected.stderr)
        result = run('stage0-lto-' + mode + '-execute', [reference])
        assert result.returncode == 42, (mode, 'reference executable', result)
        actual = out / ('selfhost-lto-' + mode + suffix)
        result = run('selfhost-lto-' + mode, [compiler, 'build', source,
                     '--no-module-discovery', '--lto=' + mode, '-o', actual])
        assert result.returncode == 0, (mode, result.stdout, result.stderr)
        result = run('selfhost-lto-' + mode + '-execute', [actual])
        assert result.returncode == 42, (mode, 'selfhost executable', result)
        cases.append(dict(name='lto_' + mode, status='passed', executed=True))

    # Cross-target attributes and link roots must select ARM64 assets even
    # while the compiler process is running on an x64 host.
    sys.path.insert(0, str(root))
    from infra.scripts.x.llvm import detect_llvm_dir, llvm_bin_dir
    llvm = llvm_bin_dir(detect_llvm_dir())
    tool_suffix = '.exe' if os.name == 'nt' else ''
    helper = out / 'target-helper.c'
    helper.write_text('int target_file(void) { return 19; }\n')
    helper_object = out / 'target-helper.o'
    result = run('arm64-helper', [llvm / ('clang' + tool_suffix), '--target=aarch64-linux-gnu',
                 '-c', helper, '-o', helper_object])
    assert result.returncode == 0, (result.stdout, result.stderr)
    library_root = out / 'target-root'
    library_dir = library_root / 'lib/linux-arm64'
    library_dir.mkdir(parents=True, exist_ok=True)
    library_helper = out / 'target-library.c'
    library_helper.write_text('int target_library(void) { return 23; }\n')
    library_object = out / 'target-library.o'
    result = run('arm64-library-helper', [llvm / ('clang' + tool_suffix), '--target=aarch64-linux-gnu',
                 '-c', library_helper, '-o', library_object])
    assert result.returncode == 0, (result.stdout, result.stderr)
    archive = library_dir / 'libtargetparity.a'
    # Avoid retaining an earlier archive member when rerunning in the same dir.
    archive.unlink(missing_ok=True)
    result = run('arm64-archive', [llvm / ('llvm-ar' + tool_suffix), 'rcs', archive, library_object])
    assert result.returncode == 0, (result.stdout, result.stderr)
    foreign_source = out / 'Foreign.kn'
    foreign_source.write_text('''Unit Tests.ForeignDriverLink;
[LinkLibTarget("linux-arm64", "targetparity")]
[LinkLibTarget("linux-x64", "missing_wrong_arch_library")]
[LinkName("target_library")] Extern Function i32 TargetLibrary() By C;
[LinkFileTarget("linux-arm64", "target-helper.o")]
[LinkFileTarget("linux-x64", "missing_wrong_arch_object.o")]
[LinkName("target_file")] Extern Function i32 TargetFile() By C;
Trusted Static Function int Main() { Return TargetLibrary() + TargetFile(); }
''')
    reference = out / 'stage0-linux-arm64'
    expected = run('stage0-linux-arm64', [stage0, 'build', foreign_source,
                   '--no-module-discovery', '--target', 'linux-arm64',
                   '--link-root', library_root, '-o', reference])
    if expected.returncode and unavailable(expected):
        cases.append(dict(name='hosted_linux_arm64', status='skipped',
                          reason='reference target SDK/runtime/linker unavailable',
                          reference_returncode=expected.returncode))
    else:
        assert expected.returncode == 0, ('reference cross compile', expected.stdout, expected.stderr)
        actual = out / 'selfhost-linux-arm64'
        result = run('selfhost-linux-arm64', [compiler, 'build', foreign_source,
                     '--no-module-discovery', '--target', 'linux-arm64',
                     '--link-root', library_root, '--show-link', '-o', actual])
        assert result.returncode == 0, (result.stdout, result.stderr)
        for binary in (reference, actual):
            data = binary.read_bytes()
            assert data[:6] == b'\x7fELF\x02\x01', (binary, 'expected little-endian ELF64')
            assert int.from_bytes(data[18:20], 'little') == 183, (binary, 'expected AArch64')
        cases.append(dict(name='hosted_linux_arm64', status='passed', executed=False))

    joined_dir = out / 'joined libraries'
    joined_dir.mkdir(exist_ok=True)
    joined_c = joined_dir / 'helper.c'
    joined_c.write_text('int joined_helper(void) { return 42; }\n')
    joined_object = joined_dir / ('helper.obj' if os.name == 'nt' else 'helper.o')
    result = run('joined-helper', [llvm / ('clang' + tool_suffix), '-c', joined_c, '-o', joined_object])
    assert result.returncode == 0, (result.stdout, result.stderr)
    joined_archive = joined_dir / ('joined.lib' if os.name == 'nt' else 'libjoined.a')
    joined_archive.unlink(missing_ok=True)
    result = run('joined-archive', [llvm / ('llvm-ar' + tool_suffix), 'rcs', joined_archive, joined_object])
    assert result.returncode == 0, (result.stdout, result.stderr)
    joined_source = out / 'Joined.kn'
    joined_source.write_text('''Unit Tests.JoinedRunLink;
[LinkName("joined_helper")] Extern Function i32 Probe() By C;
Trusted Static Function int Main(string[] args)
{
    If (args.Length() == 0) Return Probe();
    If (args.Length() == 2 && args[0] == "-Lprogram" && args[1] == "-lprogram") Return Probe() + 1;
    Return 99;
}
''')
    for name, executable in [('stage0', stage0), ('selfhost', compiler)]:
        result = run(name + '-joined-run-link', [executable, 'run', '--no-module-discovery',
                     '-L' + str(joined_dir), '-ljoined', joined_source])
        assert result.returncode == 42, (name, result.stdout, result.stderr)
        result = run(name + '-joined-program-arguments', [executable, 'run', '--no-module-discovery',
                     '-L' + str(joined_dir), '-ljoined', joined_source, '--', '-Lprogram', '-lprogram'])
        assert result.returncode == 43, (name, result.stdout, result.stderr)
    cases.append(dict(name='joined_run_link_options', status='passed', executed=True))

    # An explicit project input takes precedence before validating the profile's
    # default entry, even when that default is intentionally absent.
    project = out / 'kinal.knproj'
    project.write_text('''Project EntryOverride {
 SourceSet "main" { Files = ["Main.kn"]; RequireUnit = true; }
 Profile "native" { Source { Entry = "Missing.kn"; Sets = ["main"]; Mode = FileOnly; } }
}
''')
    for name, executable in [('stage0', stage0), ('selfhost', compiler)]:
        result = run(name + '-project-entry-override', [executable, 'build',
                     '--project', project, source, '--emit', 'check', '-o', out / (name + '.kcheck')])
        assert result.returncode == 0, (name, result.stdout, result.stderr)
    cases.append(dict(name='explicit_project_entry_override', status='passed', executed=False))
    legacy = out / 'legacy-explicit.json'
    legacy.write_text('{"kind":"project","name":"ExplicitLegacy"}\n')
    for name, executable in [('stage0', stage0), ('selfhost', compiler)]:
        result = run(name + '-legacy-explicit-entry', [executable, 'build', '--project', legacy,
                     source, '--emit', 'check', '-o', out / (name + '-legacy.kcheck')])
        assert result.returncode == 0, (name, result.stdout, result.stderr)
    cases.append(dict(name='legacy_explicit_entry', status='passed', executed=False))
    report = dict(name='driver_target_linking', ok=True, cases=cases, commands=sequence)
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print('[OK] driver target linking: ' + str(len(cases)) + ' contracts', flush=True)
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--stage0', type=Path, required=True)
    parser.add_argument('--out-dir', type=Path, required=True)
    parser.add_argument('--allow-missing-toolchain', action='store_true')
    args = parser.parse_args()
    check_driver_target_linking(args.compiler.resolve(), args.stage0.resolve(),
                                Path(__file__).resolve().parents[2], args.out_dir.resolve(),
                                allow_missing_toolchain=args.allow_missing_toolchain)
