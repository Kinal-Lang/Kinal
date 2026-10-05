"""Functional artifact/intermediate/legacy/link contracts against current C stage0.

Foreign bare binaries are inspected, never executed. Every host library is
consumed, so a successful link alone is not counted as runtime parity.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import sys


def check_driver_parity(compiler: Path, stage0: Path, root: Path, out: Path,
                        *, reference_only: bool = False) -> dict[str, object]:
    sys.path.insert(0, str(root))
    from infra.scripts.x.llvm import detect_llvm_dir, llvm_bin_dir
    llvm = llvm_bin_dir(detect_llvm_dir())
    suffix = '.exe' if os.name == 'nt' else ''
    obj_ext = '.obj' if os.name == 'nt' else '.o'
    lib_ext = '.lib' if os.name == 'nt' else '.a'
    shared_ext = '.dll' if os.name == 'nt' else '.dylib' if sys.platform == 'darwin' else '.so'
    out.mkdir(parents=True, exist_ok=True)
    logs = out / 'logs'; logs.mkdir(exist_ok=True)
    sequence = 0
    cases: list[dict[str, object]] = []

    def invoke(label: str, command: list[str | Path], *, code: int = 0,
               stdout: str | None = None, cwd: Path = out) -> subprocess.CompletedProcess[str]:
        nonlocal sequence
        sequence += 1
        proc = subprocess.run([str(arg) for arg in command], cwd=cwd,
                              text=True, capture_output=True, timeout=240)
        data = dict(command=proc.args, cwd=str(cwd), returncode=proc.returncode,
                    stdout=proc.stdout, stderr=proc.stderr)
        (logs / f'{sequence:03d}-{label}.json').write_text(json.dumps(data, indent=2)+'\n')
        assert proc.returncode == code, (label, data)
        if stdout is not None:
            assert proc.stdout.replace('\r\n', '\n') == stdout, (label, data)
        return proc

    def tool(name: str) -> Path:
        path = llvm / (name + suffix)
        assert path.is_file(), f'required selected LLVM tool is missing: {path}'
        return path

    roles = [('stage0', stage0)] if reference_only else [('stage0', stage0), ('selfhost', compiler)]
    main = out / 'Main.kn'
    main.write_text('Unit Tests.DriverParity; Static Function int Main() { Return 42; }\n')
    frontend = out / 'FrontendOnly.kn'
    frontend.write_text('Unit Tests.DriverFrontend; Function int Probe() { Return NotBound(); }\n')
    frontend_outputs: dict[str, dict[str, bytes]] = {}
    library = root / 'tests/common/dynlib_shared.kn'
    static_consumer = out / 'StaticConsumer.c'
    static_consumer.write_text('''#include <stdint.h>
extern int64_t probe(int64_t, int64_t) __asm__("Tests.Dynamic.Shared.Add");
int main(void) { return probe(20, 22) == 42 ? 0 : 1; }
''')
    startup_library = out / 'LibraryStartup.kn'
    startup_library.write_text('''Unit Tests.LibraryStartup;
int Calls = 0;
Function int Init() { Calls++; Return 7; }
int First = Init();
int Second = First;
int[] Values = {Second, 9};
Function int Probe() { Return Calls == 1 && Values[0] == 7 ? 42 : 0; }
''')
    startup_consumer = out / 'StartupConsumer.c'
    startup_consumer.write_text('''#include <stdint.h>
extern int64_t probe(void) __asm__("Tests.LibraryStartup.Probe");
int main(void) { return probe() == 42 && probe() == 42 ? 0 : 1; }
''')
    for role, compiler_path in roles:
        directory = out / role; directory.mkdir(exist_ok=True)
        frontend_outputs[role] = {}
        for mode, extension in [('tokens', '.ktokens'), ('ast', '.kast')]:
            output = directory / ('frontend' + extension)
            invoke(role+'-emit-'+mode, [compiler_path, 'build', '--no-module-discovery',
                   frontend, '--emit', mode, '-o', output])
            frontend_outputs[role][mode] = output.read_bytes().replace(b'\r\n', b'\n').strip()
            assert frontend_outputs[role][mode], (role, mode, 'empty syntax dump')
            cases.append(dict(name='frontend_emit_'+mode, role=role, passed=True))
        # A Clang-built C consumer avoids duplicate Kinal module metadata and
        # exercises the archive's exported Kinal ABI and packaged runtime leaves.
        archive = directory / ('probe' + lib_ext)
        invoke(role+'-static', [compiler_path, 'build', '--no-module-discovery', library,
                               '--kind', 'static', '-o', archive])
        assert archive.read_bytes().startswith(b'!<arch>\n'), archive
        consumer = directory / ('static-consumer'+suffix)
        consumer_flags = ['-no-pie', '-lm', '-ldl', '-lpthread'] if sys.platform == 'linux' else []
        invoke(role+'-static-consumer-build', [tool('clang'), static_consumer, archive,
                                              *consumer_flags, '-o', consumer])
        invoke(role+'-static-consumer-run', [consumer])
        cases.append(dict(name='static_artifact_consumer', role=role, passed=True))

        shared = directory / ('probe'+shared_ext)
        invoke(role+'-shared', [compiler_path, 'build', '--no-module-discovery', library,
                               '--kind', 'shared', '-o', shared])
        assert shared.is_file() and shared.stat().st_size > 0
        loader = directory / ('shared-consumer'+suffix)
        invoke(role+'-shared-consumer-build', [stage0, 'build', '--no-module-discovery',
               root/'tests/common/dynlib_loader.kn', '-o', loader])
        invoke(role+'-shared-consumer-run', [loader, shared], stdout='42\nshared-hi\n')
        if os.name == 'nt':
            assert shared.with_suffix('.lib').is_file(), 'missing shared import library'
        cases.append(dict(name='shared_artifact_consumer', role=role, passed=True))

        for kind, extension in [('static', lib_ext), ('shared', shared_ext)]:
            artifact = directory / ('startup-' + kind + extension)
            invoke(role+'-'+kind+'-startup-build', [compiler_path, 'build', '--no-module-discovery',
                   startup_library, '--kind', kind, '-o', artifact])
            linked_input = artifact.with_suffix('.lib') if os.name == 'nt' and kind == 'shared' else artifact
            executable = directory / (kind + '-startup-consumer' + suffix)
            invoke(role+'-'+kind+'-startup-consumer', [tool('clang'), startup_consumer, linked_input,
                   *consumer_flags, '-o', executable])
            invoke(role+'-'+kind+'-startup-run', [executable])
            cases.append(dict(name=kind+'_lazy_startup_once', role=role, passed=True))

        for mode, ext in [('ir', '.ll'), ('asm', '.s')]:
            intermediate = directory / ('intermediate'+ext)
            obj = directory / (mode+obj_ext)
            executable = directory / (mode+'-linked'+suffix)
            invoke(role+'-'+mode+'-emit', [compiler_path, 'build', '--no-module-discovery',
                   main, '--emit', mode, '-o', intermediate])
            invoke(role+'-'+mode+'-object', [compiler_path, 'build', intermediate,
                                           '--emit', 'obj', '-o', obj])
            invoke(role+'-'+mode+'-link', [compiler_path, 'build', obj, '-o', executable])
            invoke(role+'-'+mode+'-run', [executable], code=42)
            cases.append(dict(name=mode+'_object_executable', role=role, passed=True))

        helper = directory/'helper.c'; helper_obj=directory/('helper'+obj_ext)
        helper.write_text('int parity_helper(void) { return 42; }\n')
        invoke(role+'-mixed-helper', [tool('clang'), '-c', helper, '-o', helper_obj])
        mixed = directory/'Mixed.kn'
        mixed.write_text('''Unit Tests.DriverMixed;
[LinkName("parity_helper")] Extern Function i32 Probe() By C;
Trusted Static Function int Main() { Return Probe() == 42 ? 0 : 1; }
''')
        executable = directory/('mixed'+suffix)
        invoke(role+'-mixed-link', [compiler_path, 'build', '--no-module-discovery',
               mixed, helper_obj, '-o', executable])
        invoke(role+'-mixed-run', [executable])
        cases.append(dict(name='mixed_source_object', role=role, passed=True))

        legacy = directory/'legacy'; legacy.mkdir(exist_ok=True)
        (legacy/'Main.kn').write_text(main.read_text())
        manifest=legacy/'kinal.json'
        manifest.write_text(json.dumps(dict(kind='project', name='LegacyParity', entry='Main.kn'))+'\n')
        # C accepts arbitrary explicit JSON filenames, but directory discovery
        # uses the historical kinal.pkg.json convention.
        (legacy/'kinal.pkg.json').write_text(manifest.read_text())
        for form, target in [('file',manifest), ('directory',legacy)]:
            executable=directory/('legacy-'+form+suffix)
            invoke(role+'-legacy-'+form, [compiler_path, 'build', '--project', target, '-o', executable])
            invoke(role+'-legacy-'+form+'-run', [executable], code=42)
            cases.append(dict(name='legacy_project_'+form, role=role, passed=True))

    if not reference_only:
        assert frontend_outputs['selfhost'] == frontend_outputs['stage0'], frontend_outputs

    # Target-specific attributes must choose real assets, and ignore deliberately
    # missing assets for the other OS. Library roots include target subdirectories.
    arm = platform.machine().lower() in {'aarch64','arm64'}
    family = 'win' if os.name=='nt' else 'macos' if sys.platform=='darwin' else 'linux'
    host_tag = family+('-arm64' if arm else '-x64')
    triple = ('aarch64-pc-windows-msvc' if arm else 'x86_64-pc-windows-msvc') if os.name=='nt' else (
             ('aarch64-apple-darwin' if arm else 'x86_64-apple-darwin') if sys.platform=='darwin' else
             ('aarch64-unknown-linux-gnu' if arm else 'x86_64-pc-linux-gnu'))
    foreign = 'linux-x64' if os.name=='nt' else 'win-x64'
    assets=out/'target-assets'; assets.mkdir(exist_ok=True)
    libdir=assets/'root'/'lib'/triple; libdir.mkdir(parents=True,exist_ok=True)
    file_c=assets/'file.c'; file_o=assets/('file'+obj_ext)
    file_c.write_text('int parity_file(void) { return 19; }\n')
    lib_c=assets/'library.c'; lib_o=assets/('library'+obj_ext)
    lib_c.write_text('int parity_library(void) { return 23; }\n')
    for name,src,obj in [('file',file_c,file_o),('lib',lib_c,lib_o)]:
        invoke('target-assets-'+name,[tool('clang'),'-c',src,'-o',obj])
    archive=libdir/('parity.lib' if os.name=='nt' else 'libparity.a')
    invoke('target-archive',[tool('llvm-ar'),'rcs',archive,lib_o])
    source=assets/'Main.kn'
    source.write_text(f'''Unit Tests.TargetLink;
[LinkFileTarget("{host_tag}", "{file_o.name}")]
[LinkFileTarget("{foreign}", "missing-other-target.obj")]
[LinkName("parity_file")] Extern Function i32 FileProbe() By C;
[LinkLibTarget("{host_tag}", "parity")]
[LinkLibTarget("{foreign}", "missing_other_target_library")]
[LinkName("parity_library")] Extern Function i32 LibraryProbe() By C;
Trusted Static Function int Main() {{ Return FileProbe() + LibraryProbe() == 42 ? 0 : 1; }}
''')
    project=assets/'kinal.knproj'
    project.write_text('''Project TargetLinks {
 DefaultProfile = "native";
 SourceSet "main" { Files = ["Main.kn"]; RequireUnit = true; }
 Profile "native" {
  Source { Entry = "Main.kn"; Sets = ["main"]; Mode = FileOnly; }
  Link { LinkRoots = ["root"]; }
 }
}
''')
    for role,compiler_path in roles:
        executable=out/role/('target-links'+suffix)
        invoke(role+'-target-links',[compiler_path,'build','--project',project,'-o',executable])
        invoke(role+'-target-links-run',[executable])
        cases.append(dict(name='target_attributes_linkroots',role=role,passed=True))

    if sys.platform == 'linux':
        # Use explicit LLD: the available Zig rejects --whole-archive. C's LLD
        # path also needs an explicit no-PIE linker flag on PIE-default hosts.
        unused_c=assets/'unused.c'; unused_o=assets/'unused.o'; unused_a=assets/'unused.a'
        unused_c.write_text('int parity_unused_archive(void) { return 19; }\n')
        invoke('unused-object', [tool('clang'), '-c', unused_c, '-o', unused_o])
        invoke('unused-archive', [tool('llvm-ar'), 'rcs', unused_a, unused_o])
        for role, compiler_path in roles:
            executable=out/role/'ordered-link'
            invoke(role+'-ordered-link', [compiler_path, 'build', '--no-module-discovery', main,
                   '--linker', 'lld', '--link-arg', '-no-pie', '--link-arg', '--whole-archive',
                   '--link-file', unused_a, '--link-arg', '--no-whole-archive', '-o', executable])
            symbols=invoke(role+'-ordered-symbols', [tool('llvm-nm'), executable]).stdout
            assert 'parity_unused_archive' in symbols
            invoke(role+'-ordered-run', [executable], code=42)
            cases.append(dict(name='ordered_raw_link_arguments', role=role, passed=True))

    # Freestanding linker-script acceptance has a working C reference on Linux.
    # The ELF is inspected, never mistaken for a host user-space executable.
    if sys.platform=='linux':
        bare=out/'Bare.kn'
        bare.write_text('Unit Tests.DriverBare; Static Function void Launch() {}\n')
        script=out/'bare.ld'
        script.write_text('ENTRY(__kn_entry)\nSECTIONS { . = 0x400000; .text : { *(.text*) } .rodata : { *(.rodata*) } .data : { *(.data*) } .bss : { *(.bss*) } }\n')
        for role,compiler_path in roles:
            for target in ['bare64','bare-arm64']:
                binary=out/role/(target+'.elf')
                invoke(role+'-'+target+'-link',[compiler_path,'build','--no-module-discovery',bare,
                       '--freestanding','--runtime','none','--entry','Launch','--target',target,
                       '--link-script',script,'-o',binary])
                data=binary.read_bytes()
                assert data[:5]==b'\x7fELF\x02', (role,target)
                expected_machine=62 if target=='bare64' else 183
                assert int.from_bytes(data[18:20],'little')==expected_machine
                assert int.from_bytes(data[24:32],'little')>=0x400000
                undefined=invoke(role+'-'+target+'-undefined',[tool('llvm-nm'),'--undefined-only',binary])
                assert not undefined.stdout.strip(),undefined.stdout
                cases.append(dict(name='freestanding_link_script_'+target,role=role,passed=True,executed=False))
    result=dict(name='driver_parity',ok=True,cases=cases,commands=sequence,
                platform=platform.platform(),reference_only=reference_only,
                limits=['Foreign bare binaries are inspected, never executed.'])
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(f'[OK] driver parity: {len(cases)} functional contracts, {sequence} commands',flush=True)
    return result


def check_shared_metadata_cli(compiler: Path, stage0: Path, root: Path,
                              out: Path) -> dict[str, object]:
    """Retain the established dynamic-library metadata fixture in both directions."""
    out.mkdir(parents=True, exist_ok=True)
    suffix = '.exe' if os.name == 'nt' else ''
    extension = '.dll' if os.name == 'nt' else '.dylib' if sys.platform == 'darwin' else '.so'
    sequence = 0
    def invoke(label: str, command: list[str | Path], *, expected: str | None = None) -> None:
        nonlocal sequence
        sequence += 1
        result = subprocess.run([str(value) for value in command], cwd=root,
                                text=True, capture_output=True, timeout=240)
        record = dict(command=result.args, returncode=result.returncode,
                      stdout=result.stdout, stderr=result.stderr)
        (out/f'{sequence:02d}-{label}.json').write_text(json.dumps(record, indent=2)+'\n')
        assert result.returncode == 0, (label, record)
        if expected is not None:
            assert result.stdout.replace('\r\n', '\n') == expected, (label, record)
    libraries, consumers = {}, {}
    for role, tool in [('stage0', stage0), ('selfhost', compiler)]:
        library = out/(role+extension)
        consumer = out/(role+'-consumer'+suffix)
        invoke(role+'-library', [tool, 'build', '--no-module-discovery',
               root/'tests/common/dynlib_shared.kn', '--kind', 'shared', '-o', library])
        invoke(role+'-consumer', [tool, 'build', '--no-module-discovery',
               root/'tests/common/meta_dynlib_loader.kn', '-o', consumer])
        libraries[role], consumers[role] = library, consumer
    cases = []
    for producer, library in libraries.items():
        for consumer, executable in consumers.items():
            invoke(consumer+'-loads-'+producer, [executable, library],
                   expected='true\n1\nTests.Dynamic.Shared\nhello\nfalse\n')
            cases.append(dict(producer=producer, consumer=consumer, passed=True))
    result = dict(name='shared_metadata_cli', ok=True, cases=cases, commands=sequence)
    (out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--compiler',type=Path,required=True)
    parser.add_argument('--stage0',type=Path,required=True)
    parser.add_argument('--out-dir',type=Path,required=True)
    parser.add_argument('--reference-only',action='store_true')
    args=parser.parse_args()
    check_driver_parity(args.compiler.resolve(),args.stage0.resolve(),Path(__file__).resolve().parents[2],
                        args.out_dir.resolve(),reference_only=args.reference_only)
