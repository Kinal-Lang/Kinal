"""Compare actual console color rendering without opening a visible window.

Windows uses a hidden console and records every character/attribute cell. POSIX
uses the same raw pseudo-terminal for stderr for both compilers. Pipe/ANSI byte
parity is tested separately by check_cli_presentation.py.
"""
from __future__ import annotations

import argparse
import base64
import ctypes
import json
import os
from pathlib import Path
import subprocess
import sys


def windows_capture(command: list[str], cwd: Path) -> dict:
    from ctypes import wintypes as w
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, w.LPVOID, w.DWORD, w.DWORD, w.HANDLE]
    kernel.CreateFileW.restype = w.HANDLE
    handle = kernel.CreateFileW('CONOUT$', 0xc0000000, 3, None, 3, 0, None)
    assert handle not in (None, ctypes.c_void_p(-1).value), ctypes.get_last_error()

    class Coord(ctypes.Structure):
        _fields_ = [('x', w.SHORT), ('y', w.SHORT)]
    class Rect(ctypes.Structure):
        _fields_ = [('left', w.SHORT), ('top', w.SHORT), ('right', w.SHORT), ('bottom', w.SHORT)]
    class Info(ctypes.Structure):
        _fields_ = [('size', Coord), ('cursor', Coord), ('attributes', w.WORD), ('window', Rect), ('maximum', Coord)]

    kernel.SetConsoleOutputCP.argtypes = [w.UINT]
    kernel.SetConsoleOutputCP(65001)
    kernel.GetConsoleScreenBufferInfo.argtypes = [w.HANDLE, ctypes.POINTER(Info)]
    kernel.ReadConsoleOutputCharacterW.argtypes = [w.HANDLE, w.LPWSTR, w.DWORD, Coord, ctypes.POINTER(w.DWORD)]
    kernel.ReadConsoleOutputAttribute.argtypes = [w.HANDLE, ctypes.POINTER(w.WORD), w.DWORD, Coord, ctypes.POINTER(w.DWORD)]
    kernel.GetConsoleMode.argtypes = [w.HANDLE, ctypes.POINTER(w.DWORD)]
    kernel.SetConsoleMode.argtypes = [w.HANDLE, w.DWORD]
    # Start with VT disabled so auto must enable it using stderr's handle.
    mode = w.DWORD()
    assert kernel.GetConsoleMode(handle, ctypes.byref(mode))
    assert kernel.SetConsoleMode(handle, mode.value & ~4)
    with open('CONOUT$', 'wb', buffering=0) as console:
        process = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, stderr=console, timeout=90)
    info = Info()
    assert kernel.GetConsoleScreenBufferInfo(handle, ctypes.byref(info))
    size = info.size.x * (info.cursor.y + 1)
    characters = ctypes.create_unicode_buffer(size + 1)
    attributes = (w.WORD * size)()
    read = w.DWORD()
    assert kernel.ReadConsoleOutputCharacterW(handle, characters, size, Coord(0, 0), ctypes.byref(read))
    assert kernel.ReadConsoleOutputAttribute(handle, attributes, size, Coord(0, 0), ctypes.byref(read))
    assert kernel.GetConsoleMode(handle, ctypes.byref(mode))
    kernel.CloseHandle.argtypes = [w.HANDLE]
    kernel.CloseHandle(handle)
    return dict(returncode=process.returncode, stdout=base64.b64encode(process.stdout).decode(),
                screen=characters[:size], attributes=list(attributes), vt=bool(mode.value & 4))


def posix_capture(command: list[str], cwd: Path) -> dict:
    import pty
    import termios
    import threading
    reader, writer = pty.openpty()
    config = termios.tcgetattr(writer)
    config[1] &= ~termios.OPOST
    termios.tcsetattr(writer, termios.TCSANOW, config)
    chunks = []
    def collect():
        try:
            while data := os.read(reader, 65536):
                chunks.append(data)
        except OSError:
            pass
    thread = threading.Thread(target=collect)
    thread.start()
    try:
        process = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, stderr=writer, timeout=90)
        os.close(writer)
        writer = -1
        thread.join(timeout=5)
        assert not thread.is_alive()
        return dict(returncode=process.returncode, stdout=base64.b64encode(process.stdout).decode(),
                    stderr=base64.b64encode(b''.join(chunks)).decode())
    finally:
        if writer >= 0:
            os.close(writer)
        os.close(reader)


def check_cli_console(compiler: Path, stage0: Path, root: Path, out: Path) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    source = out / 'Console.kn'
    source.write_text('Unit Tests.ConsoleColors;\nStatic Function int Main() { int values[1]; Return 0; }\n', encoding='utf-8')
    rows = []
    for language in ('en', 'zh'):
        for color in ('never', 'auto', 'always'):
            outputs = []
            for tool in (stage0, compiler):
                command = [str(tool), 'build', '--no-module-discovery', str(source), '--emit', 'check',
                           '-o', str(out/'check.txt'), '--color', color, '--lang', language]
                if os.name == 'nt':
                    helper = subprocess.run([sys.executable, '-X', 'utf8', __file__, '--capture', str(root), *command],
                        capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW, timeout=100)
                    assert helper.returncode == 0, helper.stderr
                    outputs.append(json.loads(helper.stdout))
                else:
                    outputs.append(posix_capture(command, root))
            row = dict(language=language, color=color, passed=outputs[0] == outputs[1],
                       reference=outputs[0], selfhost=outputs[1])
            rows.append(row)
    report = dict(cases=len(rows), passed=sum(row['passed'] for row in rows), rows=rows)
    (out/'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    assert all(row['passed'] for row in rows), f'console presentation differs; see {out / "report.json"}'
    return dict(cases=len(rows), passed=len(rows), platform=sys.platform)


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--capture':
        print(json.dumps(windows_capture(sys.argv[3:], Path(sys.argv[2]))))
    else:
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument('--compiler', required=True, type=Path)
        parser.add_argument('--stage0', required=True, type=Path)
        parser.add_argument('--output', required=True, type=Path)
        args = parser.parse_args()
        print(json.dumps(check_cli_console(args.compiler.resolve(), args.stage0.resolve(),
              Path(__file__).resolve().parents[2], args.output.resolve()), indent=2))
