#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import queue
import random
import shutil
import statistics
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUT = ROOT / "out" / "lsp-stress"


@dataclass(frozen=True)
class StressProfile:
    name: str
    generated_units: int
    edit_iterations: int
    completion_requests: int
    hover_requests: int
    definition_requests: int
    reference_requests: int
    semantic_requests: int
    startup_cycles: int
    request_timeout_sec: float
    total_timeout_sec: float


PROFILES: dict[str, StressProfile] = {
    "smoke": StressProfile(
        name="smoke",
        generated_units=48,
        edit_iterations=40,
        completion_requests=16,
        hover_requests=12,
        definition_requests=10,
        reference_requests=8,
        semantic_requests=8,
        startup_cycles=2,
        request_timeout_sec=10.0,
        total_timeout_sec=120.0,
    ),
    "bounded": StressProfile(
        name="bounded",
        generated_units=650,
        edit_iterations=360,
        completion_requests=140,
        hover_requests=100,
        definition_requests=90,
        reference_requests=60,
        semantic_requests=50,
        startup_cycles=8,
        request_timeout_sec=25.0,
        total_timeout_sec=1800.0,
    ),
    "soak": StressProfile(
        name="soak",
        generated_units=1200,
        edit_iterations=1600,
        completion_requests=600,
        hover_requests=400,
        definition_requests=300,
        reference_requests=240,
        semantic_requests=200,
        startup_cycles=30,
        request_timeout_sec=35.0,
        total_timeout_sec=8 * 3600.0,
    ),
}


@dataclass
class CheckResult:
    name: str
    ok: bool
    detail: str = ""
    duration_ms: float = 0.0


@dataclass
class Metrics:
    latencies: dict[str, list[float]] = field(default_factory=dict)
    checks: list[CheckResult] = field(default_factory=list)
    notifications: int = 0
    diagnostics: int = 0
    stderr: str = ""

    def add_latency(self, kind: str, elapsed_ms: float) -> None:
        self.latencies.setdefault(kind, []).append(elapsed_ms)

    def check(self, name: str, ok: bool, detail: str = "", duration_ms: float = 0.0) -> None:
        self.checks.append(CheckResult(name, ok, detail, duration_ms))

    @property
    def ok(self) -> bool:
        return all(c.ok for c in self.checks)


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def path_to_uri(path: Path) -> str:
    p = path.resolve().as_posix()
    if os.name == "nt" and len(p) >= 2 and p[1] == ":":
        p = "/" + p[0].lower() + p[1:]
    out = []
    for ch in p:
        o = ord(ch)
        if (
            0x30 <= o <= 0x39
            or 0x41 <= o <= 0x5A
            or 0x61 <= o <= 0x7A
            or ch in "-._~/"
        ):
            out.append(ch)
        else:
            out.append(f"%{o:02X}")
    return "file://" + "".join(out)


def line_col(text: str, needle: str, offset: int = 0) -> tuple[int, int]:
    pos = text.find(needle)
    if pos < 0:
        raise AssertionError(f"needle not found: {needle!r}")
    pos += offset
    prefix = text[:pos]
    line = prefix.count("\n")
    last_nl = prefix.rfind("\n")
    line_prefix = prefix if last_nl < 0 else prefix[last_nl + 1 :]
    col = utf16_units(line_prefix)
    return line, col


def utf16_units(text: str) -> int:
    return len(text.encode("utf-16-le")) // 2


def codepoint_index_from_utf16_col(line_text: str, utf16_col: int) -> int:
    units = 0
    for index, ch in enumerate(line_text):
        char_units = 2 if ord(ch) > 0xFFFF else 1
        if units + char_units > utf16_col:
            return index
        units += char_units
        if units == utf16_col:
            return index + 1
    return len(line_text)


def text_offset_from_lsp_pos(text: str, line: int, character: int) -> int:
    if line < 0 or character < 0:
        raise AssertionError(f"invalid LSP position: {line}:{character}")
    starts = [0]
    for index, ch in enumerate(text):
        if ch == "\n":
            starts.append(index + 1)
    if line >= len(starts):
        raise AssertionError(f"LSP line out of range: {line}")
    start = starts[line]
    end = text.find("\n", start)
    if end < 0:
        end = len(text)
    line_text = text[start:end]
    if line_text.endswith("\r"):
        line_text = line_text[:-1]
    return start + codepoint_index_from_utf16_col(line_text, character)


def apply_lsp_text_edit(text: str, edit: dict[str, Any]) -> str:
    rng = edit.get("range")
    if not isinstance(rng, dict):
        raise AssertionError("text edit has no range")
    start = rng.get("start") or {}
    end = rng.get("end") or {}
    so = text_offset_from_lsp_pos(text, int(start.get("line", 0)), int(start.get("character", 0)))
    eo = text_offset_from_lsp_pos(text, int(end.get("line", 0)), int(end.get("character", 0)))
    if so > eo:
        raise AssertionError("text edit range is reversed")
    return text[:so] + str(edit.get("newText") or "") + text[eo:]


def range_for_literal(text: str, literal: str) -> dict[str, Any]:
    line, col = line_col(text, literal)
    return {
        "start": {"line": line, "character": col},
        "end": {"line": line, "character": col + utf16_units(literal)},
    }


def make_project(root: Path, generated_units: int, *, lsp_profile: str = "reachable") -> dict[str, Path]:
    if root.exists():
        shutil.rmtree(root)
    root.mkdir(parents=True)

    write_text(
        root / "kinal.knproj",
        f"""Project LspStress
{{
    DefaultProfile = "reachable";

    Workspace
    {{
        Ignore = ["out/**", "scratch/**", "ignored/**"];
    }}

    SourceSet "app"
    {{
        Roots = ["src"];
        Include = ["**/*.kn"];
        Exclude = ["Ignored/**"];
        RequireUnit = true;
    }}

    SourceSet "support"
    {{
        Roots = ["support"];
        Include = ["**/*.kn"];
        RequireUnit = true;
    }}

    Profile "reachable"
    {{
        Source
        {{
            Entry = "src/Main.kn";
            Sets = ["app"];
            Mode = ReachableUnits;
        }}

        Build
        {{
            Backend = Native;
            Environment = Hosted;
        }}
    }}

    Profile "fileonly"
    {{
        Source
        {{
            Entry = "src/Main.kn";
            Sets = ["app"];
            Mode = FileOnly;
        }}

        Build
        {{
            Backend = Native;
            Environment = Hosted;
        }}
    }}

    Profile "entryunit"
    {{
        Source
        {{
            Entry = "src/Main.kn";
            Sets = ["app"];
            Mode = EntryUnit;
        }}

        Build
        {{
            Backend = Native;
            Environment = Hosted;
        }}
    }}

    Profile "allsources"
    {{
        Source
        {{
            Entry = "src/Main.kn";
            Sets = ["app"];
            Mode = AllSources;
        }}

        Build
        {{
            Backend = Native;
            Environment = Hosted;
        }}
    }}

    Lsp
    {{
        Profile = "{lsp_profile}";
        ExtraSets = ["support"];
        StrictProjectScope = true;
    }}
}}
""",
    )

    main_text = """Unit Stress.Main;

Get Console By IO.Console;
Get Root By Stress.Gen.M0;
Get Support By Stress.Support;

Function int Churn()
{
    int churn = 0;
    Return churn;
}

Static Function int Main()
{
    int value = Root.Value() + Support.Bonus() + Churn();
    Console.PrintLine(value);
    Return 0;
}
"""
    write_text(root / "src" / "Main.kn", main_text)
    write_text(
        root / "support" / "Support.kn",
        """Unit Stress.Support;

Function int Bonus()
{
    Return 7;
}
""",
    )
    write_text(
        root / "src" / "SameUnitA.kn",
        """Unit Stress.Main;

Function int SameUnitA()
{
    Return 1;
}
""",
    )
    write_text(
        root / "src" / "Ignored" / "Broken.kn",
        """Unit Stress.Ignored;

Function int Broken()
{
    Return ;
}
""",
    )
    for i in range(generated_units):
        next_get = ""
        body = f"    Return {i};"
        if i + 1 < generated_units:
            next_get = f"Get Next By Stress.Gen.M{i + 1};\n"
            body = f"    Return {i} + Next.Value();"
        write_text(
            root / "src" / "Gen" / f"M{i}.kn",
            f"""Unit Stress.Gen.M{i};

{next_get}Function int Value()
{{
{body}
}}
""",
        )
    write_text(
        root / "scratch" / "Outside.kn",
        """Unit Stress.Outside;

Get Root By Stress.Gen.M0;

Function int Outside()
{
    Return Root.Value();
}
""",
    )
    write_text(
        root / "src" / "LegacyArray.kn",
        """Unit Stress.LegacyArray;

Function int Legacy()
{
    int nums[] = {1, 2, 3};
    Return nums[0];
}
""",
    )
    write_text(
        root / "src" / "UnicodePositions.kn",
        """Unit Stress.UnicodePositions;

Function int 加一Local(int 输入)
{
    Return 输入 + 1;
}

Function int Probe()
{
    int 前缀 = 1; int after = 加一Local(前缀);
    int 计数 = 前缀; int nums[] = {1, 2, 3};
    Return after + nums[0] + 计数;
}
""",
    )
    write_text(
        root / "src" / "AliasPositions.kn",
        """Unit Stress.AliasPositions;

Get IO.Console;

Alias Print By IO.Console.PrintLine;
Alias Say By Stress.AliasPositions.Box.Say;
Alias Ready By Stress.AliasPositions.Mode.Ready;

Enum Mode
{
    Ready = 7
}

Class Box
{
    Public Static Function string Say()
    {
        Return "method";
    }
}

Function int AliasProbe()
{
    Print(Say());
    Print(Ready);
    Return 0;
}
""",
    )
    write_text(
        root / "src" / "AliasLate.kn",
        """Unit Stress.AliasLate;

Function int Probe()
{
    Return 1;
}

Alias Print By IO.Console.PrintLine;
""",
    )
    write_text(
        root / "src" / "AliasInvalid.kn",
        """Unit Stress.AliasInvalid;

Alias Ghost By Stress.Missing.Nope;

Function int Probe()
{
    Return 1;
}
""",
    )
    write_text(
        root / "scratch" / "UnsafeAliases.kn",
        """Unsafe Alias 获取 By Get;
Unsafe Alias 别名 By Alias;
Unsafe Alias 来自 By By;
Unsafe Alias 静态 By Static;
Unsafe Alias 函数 By Function;
Unsafe Alias 整数 By int;
Unsafe Alias 返回 By Return;

获取 Console 来自 IO.Console;
别名 Print 来自 IO.Console.PrintLine;

静态 函数 整数 Main()
{
    Print("localized");
    返回 0;
}
""",
    )
    write_text(
        root / "scratch" / "UnsafeTriple.kn",
        """Unsafe Unsafe Unsafe Alias !! By 1;
Unsafe Unsafe Unsafe Alias ========= By ==;
Unsafe Unsafe Unsafe Alias 1011 By ;;

Static Function int Main()
{
    If (!! ========= 1)
    {
        Return 0 1011
    }
    Return 1;
}
""",
    )
    write_text(
        root / "scratch" / "UnsafeNoSpaceAliases.kn",
        """Unsafe Unsafe Unsafe Alias!!By==;
Unsafe Unsafe Unsafe Alias 1011By;;
Unsafe Alias"yes"By true;
Unsafe Alias 0000By false;
Unsafe Alias fn By Function;Unsafe Alias ret By Return;Unsafe Alias int32 By int;

fn int32 Main()
{
    If (1 !! 1 && "yes" == "yes" && 0000 == false)
    {
        ret 0 1011
    }
    ret 1;
}
""",
    )
    write_text(
        root / "scratch" / "UnsafeCollapsedWords.kn",
        """UnsafeAlias fn By Function;
Unsafe Aliasfn By Function;
Unsafe UnsafeUnsafe Alias!!By==;
AliasPrint By IO.Console.PrintLine;
Unsafe Alias真 By true;
Unsafe Alias 返回ByReturn;

Function int Main()
{
    Return 0;
}
""",
    )
    return {
        "root": root,
        "main": root / "src" / "Main.kn",
        "support": root / "support" / "Support.kn",
        "outside": root / "scratch" / "Outside.kn",
        "legacy": root / "src" / "LegacyArray.kn",
        "unicode": root / "src" / "UnicodePositions.kn",
        "alias": root / "src" / "AliasPositions.kn",
        "alias_late": root / "src" / "AliasLate.kn",
        "alias_invalid": root / "src" / "AliasInvalid.kn",
        "unsafe_alias": root / "scratch" / "UnsafeAliases.kn",
        "unsafe_triple_alias": root / "scratch" / "UnsafeTriple.kn",
        "unsafe_nospace_alias": root / "scratch" / "UnsafeNoSpaceAliases.kn",
        "unsafe_collapsed_words": root / "scratch" / "UnsafeCollapsedWords.kn",
    }


class LspClient:
    def __init__(self, server: Path, cwd: Path, transcript: Path, timeout_sec: float):
        self.server = server
        self.cwd = cwd
        self.timeout_sec = timeout_sec
        self.transcript = transcript
        self.proc: subprocess.Popen[bytes] | None = None
        self.messages: "queue.Queue[dict[str, Any] | None]" = queue.Queue()
        self.stderr_chunks: list[str] = []
        self.diags_by_uri: dict[str, list[dict[str, Any]]] = {}
        self.diag_seq: dict[str, int] = {}
        self.next_id = 1
        self.reader_thread: threading.Thread | None = None
        self.stderr_thread: threading.Thread | None = None
        self._transcript_fh = None

    def __enter__(self) -> "LspClient":
        self.start()
        return self

    def __exit__(self, exc_type: object, exc: object, tb: object) -> None:
        self.close()

    def start(self) -> None:
        self.transcript.parent.mkdir(parents=True, exist_ok=True)
        self._transcript_fh = self.transcript.open("w", encoding="utf-8", newline="\n")
        self.proc = subprocess.Popen(
            [str(self.server)],
            cwd=str(self.cwd),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            shell=False,
        )
        assert self.proc.stdout is not None
        assert self.proc.stderr is not None
        self.reader_thread = threading.Thread(target=self._reader, args=(self.proc.stdout,), daemon=True)
        self.stderr_thread = threading.Thread(target=self._stderr_reader, args=(self.proc.stderr,), daemon=True)
        self.reader_thread.start()
        self.stderr_thread.start()

    def close(self) -> None:
        if self.proc and self.proc.poll() is None:
            try:
                self.notify("exit", None)
            except Exception:
                pass
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=3)
        if self._transcript_fh:
            self._transcript_fh.close()
            self._transcript_fh = None

    def _stderr_reader(self, pipe: Any) -> None:
        while True:
            data = pipe.readline()
            if not data:
                break
            self.stderr_chunks.append(data.decode("utf-8", errors="replace"))

    def _reader(self, pipe: Any) -> None:
        try:
            while True:
                length: int | None = None
                while True:
                    line = pipe.readline()
                    if not line:
                        self.messages.put(None)
                        return
                    line = line.rstrip(b"\r\n")
                    if not line:
                        break
                    if line.lower().startswith(b"content-length:"):
                        length = int(line.split(b":", 1)[1].strip())
                if length is None:
                    continue
                payload = pipe.read(length)
                if len(payload) != length:
                    self.messages.put(None)
                    return
                obj = json.loads(payload.decode("utf-8", errors="replace"))
                self._log("<--", obj)
                self.messages.put(obj)
        except Exception as exc:
            self.stderr_chunks.append(f"reader failed: {exc}\n")
            self.messages.put(None)

    def _log(self, prefix: str, obj: dict[str, Any]) -> None:
        if not self._transcript_fh:
            return
        self._transcript_fh.write(prefix + " " + json.dumps(obj, ensure_ascii=False, separators=(",", ":")) + "\n")
        self._transcript_fh.flush()

    def _send(self, obj: dict[str, Any]) -> None:
        if not self.proc or not self.proc.stdin:
            raise RuntimeError("server is not running")
        payload = json.dumps(obj, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        header = f"Content-Length: {len(payload)}\r\n\r\n".encode("ascii")
        self._log("-->", obj)
        self.proc.stdin.write(header + payload)
        self.proc.stdin.flush()

    def notify(self, method: str, params: Any) -> None:
        obj: dict[str, Any] = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            obj["params"] = params
        self._send(obj)

    def request(self, method: str, params: Any, *, timeout_sec: float | None = None) -> tuple[Any, float]:
        req_id = self.next_id
        self.next_id += 1
        obj: dict[str, Any] = {"jsonrpc": "2.0", "id": req_id, "method": method}
        if params is not None:
            obj["params"] = params
        start = time.perf_counter()
        self._send(obj)
        deadline = start + (timeout_sec or self.timeout_sec)
        while True:
            remaining = deadline - time.perf_counter()
            if remaining <= 0:
                raise TimeoutError(f"request timed out: {method}")
            msg = self.messages.get(timeout=remaining)
            if msg is None:
                raise RuntimeError(f"server exited while waiting for {method}")
            self._handle_notification(msg)
            if msg.get("id") == req_id:
                elapsed = (time.perf_counter() - start) * 1000.0
                if "error" in msg:
                    raise RuntimeError(f"{method} returned error: {msg['error']}")
                return msg.get("result"), elapsed

    def pump(self, duration_sec: float = 0.05) -> None:
        deadline = time.perf_counter() + duration_sec
        while time.perf_counter() < deadline:
            try:
                msg = self.messages.get(timeout=max(0.0, deadline - time.perf_counter()))
            except queue.Empty:
                return
            if msg is None:
                raise RuntimeError("server exited")
            self._handle_notification(msg)

    def _handle_notification(self, msg: dict[str, Any]) -> None:
        if msg.get("method") != "textDocument/publishDiagnostics":
            return
        params = msg.get("params") or {}
        uri = str(params.get("uri") or "")
        diags = params.get("diagnostics")
        if not isinstance(diags, list):
            return
        self.diags_by_uri[uri] = diags
        self.diag_seq[uri] = self.diag_seq.get(uri, 0) + 1

    def wait_for_diagnostics(self, uri: str, previous_seq: int = 0, timeout_sec: float | None = None) -> list[dict[str, Any]]:
        deadline = time.perf_counter() + (timeout_sec or self.timeout_sec)
        while time.perf_counter() < deadline:
            if self.diag_seq.get(uri, 0) > previous_seq:
                return self.diags_by_uri.get(uri, [])
            remaining = max(0.0, deadline - time.perf_counter())
            try:
                msg = self.messages.get(timeout=min(0.25, remaining))
            except queue.Empty:
                continue
            if msg is None:
                raise RuntimeError("server exited while waiting for diagnostics")
            self._handle_notification(msg)
        raise TimeoutError(f"diagnostics timed out for {uri}")

    def stderr_text(self) -> str:
        return "".join(self.stderr_chunks)

    def exit_code(self) -> int | None:
        return self.proc.poll() if self.proc else None


def text_document(path: Path, text: str | None = None, version: int = 1, language_id: str = "kinal") -> dict[str, Any]:
    if text is None:
        text = path.read_text(encoding="utf-8")
    return {
        "uri": path_to_uri(path),
        "languageId": language_id,
        "version": version,
        "text": text,
    }


def did_change_full(client: LspClient, path: Path, text: str, version: int) -> None:
    client.notify(
        "textDocument/didChange",
        {
            "textDocument": {"uri": path_to_uri(path), "version": version},
            "contentChanges": [{"text": text}],
        },
    )


def did_change_range(client: LspClient, path: Path, text: str, literal: str, replacement: str, version: int) -> str:
    rng = range_for_literal(text, literal)
    new_text = text.replace(literal, replacement, 1)
    client.notify(
        "textDocument/didChange",
        {
            "textDocument": {"uri": path_to_uri(path), "version": version},
            "contentChanges": [{"range": rng, "text": replacement}],
        },
    )
    return new_text


def initialize(client: LspClient, workspace: Path) -> float:
    result, elapsed = client.request(
        "initialize",
        {
            "processId": os.getpid(),
            "rootUri": path_to_uri(workspace),
            "rootPath": str(workspace),
            "workspaceFolders": [{"uri": path_to_uri(workspace), "name": workspace.name}],
            "capabilities": {},
            "initializationOptions": {"diagnosticsLanguage": "en", "localeFile": ""},
        },
    )
    if not isinstance(result, dict) or "capabilities" not in result:
        raise AssertionError("initialize response has no capabilities")
    client.notify("initialized", {})
    return elapsed


def assert_no_error_diags(diags: list[dict[str, Any]]) -> tuple[bool, str]:
    errors = [d for d in diags if int(d.get("severity") or 0) == 1]
    if errors:
        return False, "; ".join(str(d.get("message") or d.get("code") or d) for d in errors[:5])
    return True, ""


def assert_nonempty_list(value: Any, label: str) -> tuple[bool, str]:
    if isinstance(value, dict) and isinstance(value.get("items"), list):
        return (len(value["items"]) > 0, f"{label} item count={len(value['items'])}")
    if isinstance(value, list):
        return (len(value) > 0, f"{label} item count={len(value)}")
    return False, f"{label} unexpected result shape: {type(value).__name__}"


def run_startup_cycles(server: Path, repo_root: Path, out_dir: Path, profile: StressProfile, metrics: Metrics) -> None:
    workspace = out_dir / "startup_workspace"
    paths = make_project(workspace, min(profile.generated_units, 128))
    for i in range(profile.startup_cycles):
        transcript = out_dir / "transcripts" / f"startup_{i:03d}.jsonl"
        start = time.perf_counter()
        try:
            with LspClient(server, repo_root, transcript, profile.request_timeout_sec) as client:
                elapsed = initialize(client, paths["root"])
                metrics.add_latency("initialize", elapsed)
                _, shutdown_elapsed = client.request("shutdown", None)
                metrics.add_latency("shutdown", shutdown_elapsed)
                client.notify("exit", None)
                client.close()
            total = (time.perf_counter() - start) * 1000.0
            metrics.check(f"startup_cycle_{i}", True, duration_ms=total)
        except Exception as exc:
            metrics.check(f"startup_cycle_{i}", False, str(exc))
            return


def run_interactive_scenario(server: Path, repo_root: Path, out_dir: Path, profile: StressProfile, seed: int, metrics: Metrics) -> None:
    rng = random.Random(seed)
    paths = make_project(out_dir / "workspace", profile.generated_units)
    main_path = paths["main"]
    main_uri = path_to_uri(main_path)
    main_text = main_path.read_text(encoding="utf-8")
    transcript = out_dir / "transcripts" / "interactive.jsonl"

    with LspClient(server, repo_root, transcript, profile.request_timeout_sec) as client:
        try:
            elapsed = initialize(client, paths["root"])
            metrics.add_latency("initialize", elapsed)
            metrics.check("initialize_capabilities", True, duration_ms=elapsed)
        except Exception as exc:
            metrics.check("initialize_capabilities", False, str(exc))
            return

        seq = client.diag_seq.get(main_uri, 0)
        client.notify("textDocument/didOpen", {"textDocument": text_document(main_path, main_text, 1)})
        try:
            diags = client.wait_for_diagnostics(main_uri, seq)
            ok, detail = assert_no_error_diags(diags)
            metrics.check("reachable_project_has_no_main_errors", ok, detail)
        except Exception as exc:
            metrics.check("reachable_project_has_no_main_errors", False, str(exc))

        pos_root_dot = line_col(main_text, "Root.", len("Root."))
        pos_support = line_col(main_text, "Support.Bonus", len("Support."))
        pos_console = line_col(main_text, "Console.", len("Console."))
        pos_root_name = line_col(main_text, "Root.Value", 1)
        pos_churn = line_col(main_text, "Churn", 1)

        request_specs: list[tuple[str, str, dict[str, Any], Any]] = []
        request_specs.append(("completion", "textDocument/completion", {"textDocument": {"uri": main_uri}, "position": {"line": pos_root_dot[0], "character": pos_root_dot[1]}}, assert_nonempty_list))
        request_specs.append(("hover", "textDocument/hover", {"textDocument": {"uri": main_uri}, "position": {"line": pos_support[0], "character": pos_support[1]}}, lambda v, l: (v is not None, f"{l} null")))
        request_specs.append(("definition", "textDocument/definition", {"textDocument": {"uri": main_uri}, "position": {"line": pos_root_name[0], "character": pos_root_name[1]}}, assert_nonempty_list))
        request_specs.append(("references", "textDocument/references", {"textDocument": {"uri": main_uri}, "position": {"line": pos_churn[0], "character": pos_churn[1]}, "context": {"includeDeclaration": True}}, assert_nonempty_list))
        request_specs.append(("documentSymbol", "textDocument/documentSymbol", {"textDocument": {"uri": main_uri}}, assert_nonempty_list))
        request_specs.append(("semanticTokens", "textDocument/semanticTokens/full", {"textDocument": {"uri": main_uri}}, lambda v, l: (isinstance(v, dict) and isinstance(v.get("data"), list) and len(v["data"]) > 0, f"{l} empty")))
        request_specs.append(("stdlibText", "kinal/stdlibText", {"module": "IO.Console"}, lambda v, l: (isinstance(v, str) and len(v) > 0, f"{l} empty")))

        for label, method, params, validator in request_specs:
            try:
                result, elapsed = client.request(method, params)
                metrics.add_latency(label, elapsed)
                ok, detail = validator(result, label)
                metrics.check(f"request_{label}", ok, detail, elapsed)
            except Exception as exc:
                metrics.check(f"request_{label}", False, str(exc))

        unicode_path = paths["unicode"]
        unicode_text = unicode_path.read_text(encoding="utf-8")
        unicode_uri = path_to_uri(unicode_path)
        unicode_version = 1
        unicode_warnings: list[dict[str, Any]] = []
        seq = client.diag_seq.get(unicode_uri, 0)
        client.notify("textDocument/didOpen", {"textDocument": text_document(unicode_path, unicode_text, unicode_version)})
        try:
            unicode_diags = client.wait_for_diagnostics(unicode_uri, seq)
            ok, detail = assert_no_error_diags(unicode_diags)
            metrics.check("unicode_positions_has_no_errors", ok, detail)
            unicode_warnings = [d for d in unicode_diags if d.get("code") == "W-SYN-00001"]
            metrics.check("unicode_legacy_array_warning_present", len(unicode_warnings) > 0, f"diag count={len(unicode_diags)}")
        except Exception as exc:
            metrics.check("unicode_positions_has_no_errors", False, str(exc))

        pos_unicode_after = line_col(unicode_text, "after = 加一Local", 1)
        try:
            result, elapsed = client.request(
                "textDocument/hover",
                {"textDocument": {"uri": unicode_uri}, "position": {"line": pos_unicode_after[0], "character": pos_unicode_after[1]}},
            )
            metrics.add_latency("unicodeHover", elapsed)
            metrics.check("unicode_hover_after_prefix", result is not None, "non-null" if result is not None else "null", elapsed)
        except Exception as exc:
            metrics.check("unicode_hover_after_prefix", False, str(exc))

        pos_unicode_call = line_col(unicode_text, "加一Local(前缀)", 1)
        decl_line = line_col(unicode_text, "加一Local(int", 1)[0]
        try:
            result, elapsed = client.request(
                "textDocument/definition",
                {"textDocument": {"uri": unicode_uri}, "position": {"line": pos_unicode_call[0], "character": pos_unicode_call[1]}},
            )
            metrics.add_latency("unicodeDefinition", elapsed)
            locs = result if isinstance(result, list) else []
            lines = [
                (((loc.get("range") or {}).get("start") or {}).get("line"))
                for loc in locs
                if isinstance(loc, dict)
            ]
            ok = any(line == decl_line for line in lines) and pos_unicode_call[0] not in lines[:1]
            metrics.check("unicode_definition_uses_utf16_position", ok, f"lines={lines}, decl_line={decl_line}", elapsed)
        except Exception as exc:
            metrics.check("unicode_definition_uses_utf16_position", False, str(exc))

        pos_unicode_prefix = line_col(unicode_text, "加一Local(前缀)", len("加一Local(") + 1)
        try:
            result, elapsed = client.request(
                "textDocument/references",
                {
                    "textDocument": {"uri": unicode_uri},
                    "position": {"line": pos_unicode_prefix[0], "character": pos_unicode_prefix[1]},
                    "context": {"includeDeclaration": True},
                },
            )
            metrics.add_latency("unicodeReferences", elapsed)
            count = len(result) if isinstance(result, list) else 0
            metrics.check("unicode_references_after_prefix", count >= 2, f"ref count={count}", elapsed)
        except Exception as exc:
            metrics.check("unicode_references_after_prefix", False, str(exc))

        if unicode_warnings:
            try:
                result, elapsed = client.request(
                    "textDocument/codeAction",
                    {
                        "textDocument": {"uri": unicode_uri},
                        "range": unicode_warnings[0]["range"],
                        "context": {"diagnostics": unicode_warnings},
                    },
                )
                metrics.add_latency("unicodeCodeAction", elapsed)
                actions = result if isinstance(result, list) else []
                edits: list[dict[str, Any]] = []
                for action in actions:
                    if not isinstance(action, dict):
                        continue
                    changes = ((action.get("edit") or {}).get("changes") or {})
                    if isinstance(changes, dict):
                        edits.extend(edit for edit in changes.get(unicode_uri, []) if isinstance(edit, dict))
                edited = apply_lsp_text_edit(unicode_text, edits[0]) if edits else unicode_text
                ok = "int 计数 = 前缀; int[] nums = {1, 2, 3};" in edited
                metrics.check("unicode_code_action_edit_range", ok, f"actions={len(actions)} edits={len(edits)}", elapsed)
            except Exception as exc:
                metrics.check("unicode_code_action_edit_range", False, str(exc))

        seq = client.diag_seq.get(unicode_uri, 0)
        unicode_version += 1
        unicode_text = did_change_range(
            client,
            unicode_path,
            unicode_text,
            "after = 加一Local(前缀)",
            "after = 加一Local(前缀) + 1",
            unicode_version,
        )
        try:
            unicode_diags = client.wait_for_diagnostics(unicode_uri, seq)
            ok, detail = assert_no_error_diags(unicode_diags)
            metrics.check("unicode_range_edit_after_prefix", ok, detail)
        except Exception as exc:
            metrics.check("unicode_range_edit_after_prefix", False, str(exc))

        alias_path = paths["alias"]
        alias_text = alias_path.read_text(encoding="utf-8")
        alias_uri = path_to_uri(alias_path)
        seq = client.diag_seq.get(alias_uri, 0)
        client.notify("textDocument/didOpen", {"textDocument": text_document(alias_path, alias_text, 1)})
        try:
            alias_diags = client.wait_for_diagnostics(alias_uri, seq)
            ok, detail = assert_no_error_diags(alias_diags)
            metrics.check("alias_positions_has_no_errors", ok, detail)
        except Exception as exc:
            metrics.check("alias_positions_has_no_errors", False, str(exc))

        alias_requests = [
            ("aliasPrint", "Print(Say())", 1),
            ("aliasSay", "Print(Say())", len("Print(") + 1),
            ("aliasReady", "Print(Ready)", len("Print(") + 1),
        ]
        for label, needle, offset in alias_requests:
            try:
                pos = line_col(alias_text, needle, offset)
                result, elapsed = client.request(
                    "textDocument/definition",
                    {"textDocument": {"uri": alias_uri}, "position": {"line": pos[0], "character": pos[1]}},
                )
                metrics.add_latency(label + "Definition", elapsed)
                ok, detail = assert_nonempty_list(result, label)
                metrics.check(f"{label}_definition", ok, detail, elapsed)
                result, elapsed = client.request(
                    "textDocument/hover",
                    {"textDocument": {"uri": alias_uri}, "position": {"line": pos[0], "character": pos[1]}},
                )
                metrics.add_latency(label + "Hover", elapsed)
                metrics.check(f"{label}_hover", result is not None, "non-null" if result is not None else "null", elapsed)
            except Exception as exc:
                metrics.check(f"{label}_lsp_request", False, str(exc))

        for key in (
            "alias_late",
            "alias_invalid",
            "unsafe_alias",
            "unsafe_triple_alias",
            "unsafe_nospace_alias",
            "unsafe_collapsed_words",
        ):
            path = paths[key]
            text = path.read_text(encoding="utf-8")
            uri = path_to_uri(path)
            seq = client.diag_seq.get(uri, 0)
            client.notify("textDocument/didOpen", {"textDocument": text_document(path, text, 1)})
            try:
                diags = client.wait_for_diagnostics(uri, seq)
                metrics.check(f"{key}_diagnostics_return", isinstance(diags, list), f"diag count={len(diags)}")
            except Exception as exc:
                metrics.check(f"{key}_diagnostics_return", False, str(exc))

            for label, method, params in [
                (f"{key}_semanticTokens", "textDocument/semanticTokens/full", {"textDocument": {"uri": uri}}),
                (f"{key}_documentSymbol", "textDocument/documentSymbol", {"textDocument": {"uri": uri}}),
            ]:
                try:
                    result, elapsed = client.request(method, params)
                    metrics.add_latency(label, elapsed)
                    ok = (isinstance(result, dict) and isinstance(result.get("data"), list)) or isinstance(result, list)
                    metrics.check(label, ok, type(result).__name__, elapsed)
                except Exception as exc:
                    metrics.check(label, False, str(exc))

            for needle in ("Alias", "Print", "获取", "!!", "1011", "\"yes\"", "fn", "ret", "真", "返回"):
                if needle not in text:
                    continue
                try:
                    pos = line_col(text, needle, min(1, len(needle) - 1))
                    result, elapsed = client.request(
                        "textDocument/hover",
                        {"textDocument": {"uri": uri}, "position": {"line": pos[0], "character": pos[1]}},
                    )
                    metrics.add_latency(f"{key}_hover", elapsed)
                    metrics.check(f"{key}_hover_{needle}", True, "returned", elapsed)
                    result, elapsed = client.request(
                        "textDocument/definition",
                        {"textDocument": {"uri": uri}, "position": {"line": pos[0], "character": pos[1]}},
                    )
                    metrics.add_latency(f"{key}_definition", elapsed)
                    ok = isinstance(result, list)
                    metrics.check(f"{key}_definition_{needle}", ok, type(result).__name__, elapsed)
                except Exception as exc:
                    metrics.check(f"{key}_position_{needle}", False, str(exc))

        version = 2
        text = main_text
        malformed_seq = client.diag_seq.get(main_uri, 0)
        malformed = text.replace("Return churn;", "Return ;", 1)
        did_change_full(client, main_path, malformed, version)
        version += 1
        try:
            diags = client.wait_for_diagnostics(main_uri, malformed_seq)
            metrics.check("malformed_edit_produces_diagnostic", len(diags) > 0, f"diag count={len(diags)}")
        except Exception as exc:
            metrics.check("malformed_edit_produces_diagnostic", False, str(exc))
        restore_seq = client.diag_seq.get(main_uri, 0)
        did_change_full(client, main_path, text, version)
        version += 1
        try:
            diags = client.wait_for_diagnostics(main_uri, restore_seq)
            ok, detail = assert_no_error_diags(diags)
            metrics.check("restore_clears_main_errors", ok, detail)
        except Exception as exc:
            metrics.check("restore_clears_main_errors", False, str(exc))

        churn_value = "0"
        for i in range(profile.edit_iterations):
            new_value = str(rng.randint(0, 9999))
            seq = client.diag_seq.get(main_uri, 0)
            before = time.perf_counter()
            text = did_change_range(client, main_path, text, f"churn = {churn_value}", f"churn = {new_value}", version)
            churn_value = new_value
            version += 1
            try:
                diags = client.wait_for_diagnostics(main_uri, seq, timeout_sec=profile.request_timeout_sec)
                elapsed = (time.perf_counter() - before) * 1000.0
                metrics.add_latency("didChange+diagnostics", elapsed)
                if i % max(1, profile.edit_iterations // 12) == 0:
                    ok, detail = assert_no_error_diags(diags)
                    metrics.check(f"edit_round_{i:04d}", ok, detail, elapsed)
            except Exception as exc:
                metrics.check(f"edit_round_{i:04d}", False, str(exc))
                break

        def request_many(kind: str, method: str, params: dict[str, Any], count: int) -> None:
            for i in range(count):
                try:
                    result, elapsed = client.request(method, params)
                    metrics.add_latency(kind, elapsed)
                    if i in {0, count - 1}:
                        if kind == "completion":
                            ok, detail = assert_nonempty_list(result, kind)
                        elif kind == "semanticTokens":
                            ok = isinstance(result, dict) and isinstance(result.get("data"), list)
                            detail = f"semantic token ints={len(result.get('data') or [])}" if isinstance(result, dict) else "bad shape"
                        elif kind in {"definition", "references"}:
                            ok, detail = assert_nonempty_list(result, kind)
                        else:
                            ok = result is not None
                            detail = "non-null" if ok else "null"
                        metrics.check(f"{kind}_round_{i:04d}", ok, detail, elapsed)
                except Exception as exc:
                    metrics.check(f"{kind}_round_{i:04d}", False, str(exc))
                    return

        request_many("completion", "textDocument/completion", {"textDocument": {"uri": main_uri}, "position": {"line": pos_console[0], "character": pos_console[1]}}, profile.completion_requests)
        request_many("hover", "textDocument/hover", {"textDocument": {"uri": main_uri}, "position": {"line": pos_support[0], "character": pos_support[1]}}, profile.hover_requests)
        request_many("definition", "textDocument/definition", {"textDocument": {"uri": main_uri}, "position": {"line": pos_root_name[0], "character": pos_root_name[1]}}, profile.definition_requests)
        request_many("references", "textDocument/references", {"textDocument": {"uri": main_uri}, "position": {"line": pos_churn[0], "character": pos_churn[1]}, "context": {"includeDeclaration": True}}, profile.reference_requests)
        request_many("semanticTokens", "textDocument/semanticTokens/full", {"textDocument": {"uri": main_uri}}, profile.semantic_requests)

        legacy_path = paths["legacy"]
        legacy_text = legacy_path.read_text(encoding="utf-8")
        legacy_uri = path_to_uri(legacy_path)
        seq = client.diag_seq.get(legacy_uri, 0)
        client.notify("textDocument/didOpen", {"textDocument": text_document(legacy_path, legacy_text, 1)})
        try:
            legacy_diags = client.wait_for_diagnostics(legacy_uri, seq)
            warn = [d for d in legacy_diags if d.get("code") == "W-SYN-00001"]
            metrics.check("legacy_array_warning_present", len(warn) > 0, f"diag count={len(legacy_diags)}")
            result, elapsed = client.request(
                "textDocument/codeAction",
                {
                    "textDocument": {"uri": legacy_uri},
                    "range": warn[0]["range"] if warn else {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 0}},
                    "context": {"diagnostics": warn},
                },
            )
            metrics.add_latency("codeAction", elapsed)
            ok = isinstance(result, list) and any("Convert to Type[] name" in str(item.get("title", "")) for item in result if isinstance(item, dict))
            metrics.check("legacy_array_code_action", ok, f"actions={len(result) if isinstance(result, list) else 'bad'}", elapsed)
        except Exception as exc:
            metrics.check("legacy_array_code_action", False, str(exc))

        dynamic_path = paths["root"] / "src" / "Dynamic.kn"
        write_text(
            dynamic_path,
            """Unit Stress.Dynamic;

Function int DynamicValue()
{
    Return 99;
}
""",
        )
        client.notify(
            "workspace/didChangeWatchedFiles",
            {"changes": [{"uri": path_to_uri(dynamic_path), "type": 1}]},
        )
        dynamic_user = paths["root"] / "src" / "DynamicUser.kn"
        dynamic_text = """Unit Stress.DynamicUser;

Get Dyn By Stress.Dynamic;

Function int UseDyn()
{
    Return Dyn.DynamicValue();
}
"""
        write_text(dynamic_user, dynamic_text)
        dynamic_uri = path_to_uri(dynamic_user)
        seq = client.diag_seq.get(dynamic_uri, 0)
        client.notify("textDocument/didOpen", {"textDocument": text_document(dynamic_user, dynamic_text, 1)})
        try:
            diags = client.wait_for_diagnostics(dynamic_uri, seq)
            ok, detail = assert_no_error_diags(diags)
            metrics.check("watched_create_resolves_new_unit", ok, detail)
        except Exception as exc:
            metrics.check("watched_create_resolves_new_unit", False, str(exc))

        outside_path = paths["outside"]
        outside_text = outside_path.read_text(encoding="utf-8")
        outside_uri = path_to_uri(outside_path)
        seq = client.diag_seq.get(outside_uri, 0)
        client.notify("textDocument/didOpen", {"textDocument": text_document(outside_path, outside_text, 1)})
        try:
            diags = client.wait_for_diagnostics(outside_uri, seq)
            metrics.check("strict_scope_outside_file_gets_diagnostic", len(diags) > 0, f"diag count={len(diags)}")
        except Exception as exc:
            metrics.check("strict_scope_outside_file_gets_diagnostic", False, str(exc))

        try:
            _, elapsed = client.request("shutdown", None)
            metrics.add_latency("shutdown", elapsed)
            metrics.check("shutdown", True, duration_ms=elapsed)
            client.notify("exit", None)
        except Exception as exc:
            metrics.check("shutdown", False, str(exc))

        metrics.stderr += client.stderr_text()


def percentile(values: list[float], pct: float) -> float:
    if not values:
        return 0.0
    if len(values) == 1:
        return values[0]
    ordered = sorted(values)
    idx = int(round((pct / 100.0) * (len(ordered) - 1)))
    return ordered[max(0, min(idx, len(ordered) - 1))]


def write_results(out_dir: Path, profile: StressProfile, metrics: Metrics, elapsed_sec: float) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    latency_summary: dict[str, dict[str, float | int]] = {}
    for kind, values in sorted(metrics.latencies.items()):
        latency_summary[kind] = {
            "count": len(values),
            "min_ms": min(values) if values else 0.0,
            "p50_ms": percentile(values, 50.0),
            "p95_ms": percentile(values, 95.0),
            "max_ms": max(values) if values else 0.0,
            "mean_ms": statistics.fmean(values) if values else 0.0,
        }
    data = {
        "profile": profile.name,
        "elapsed_sec": elapsed_sec,
        "ok": metrics.ok,
        "checks": [c.__dict__ for c in metrics.checks],
        "latencies": latency_summary,
        "stderr_tail": metrics.stderr[-8000:],
    }
    write_text(out_dir / "results.json", json.dumps(data, indent=2, ensure_ascii=False))

    lines = [
        f"# Kinal LSP stress results ({profile.name})",
        "",
        f"- ok: {metrics.ok}",
        f"- elapsed_sec: {elapsed_sec:.2f}",
        f"- checks: {sum(1 for c in metrics.checks if c.ok)}/{len(metrics.checks)}",
        "",
        "## Latency",
        "",
        "| kind | count | p50 ms | p95 ms | max ms |",
        "| --- | ---: | ---: | ---: | ---: |",
    ]
    for kind, item in latency_summary.items():
        lines.append(
            f"| {kind} | {item['count']} | {item['p50_ms']:.2f} | {item['p95_ms']:.2f} | {item['max_ms']:.2f} |"
        )
    lines.extend(["", "## Failed Checks", ""])
    failed = [c for c in metrics.checks if not c.ok]
    if not failed:
        lines.append("- none")
    else:
        for c in failed:
            lines.append(f"- {c.name}: {c.detail}")
    if metrics.stderr.strip():
        lines.extend(["", "## Stderr Tail", "", "```", metrics.stderr[-4000:], "```"])
    write_text(out_dir / "summary.md", "\n".join(lines) + "\n")


def find_default_server() -> Path:
    exe = "kinal-lsp-server.exe" if os.name == "nt" else "kinal-lsp-server"
    candidates = [
        ROOT / "out" / "stage" / "host-release" / exe,
        ROOT / "out" / "stage" / "host-debug" / exe,
        ROOT / "out" / "build" / "host-release" / "apps" / "kinal-lsp" / "server" / exe,
        ROOT / "out" / "build" / "host-debug" / "apps" / "kinal-lsp" / "server" / exe,
    ]
    for path in candidates:
        if path.exists():
            return path
    return Path(exe)


def main() -> int:
    ap = argparse.ArgumentParser(description="Kinal LSP JSON-RPC stress harness")
    ap.add_argument("--server", type=Path, default=find_default_server(), help="path to kinal-lsp-server")
    ap.add_argument("--profile", choices=sorted(PROFILES), default="bounded")
    ap.add_argument("--out-dir", type=Path, default=DEFAULT_OUT / "bounded")
    ap.add_argument("--seed", type=int, default=20260620)
    ap.add_argument("--repo-root", type=Path, default=ROOT, help="cwd for the LSP process")
    args = ap.parse_args()

    profile = PROFILES[args.profile]
    server = args.server.resolve()
    repo_root = args.repo_root.resolve()
    out_dir = args.out_dir.resolve()
    if not server.exists():
        raise SystemExit(f"LSP server not found: {server}")

    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    metrics = Metrics()
    start = time.perf_counter()
    try:
        run_startup_cycles(server, repo_root, out_dir, profile, metrics)
        if metrics.ok:
            run_interactive_scenario(server, repo_root, out_dir, profile, args.seed, metrics)
    except Exception as exc:
        metrics.check("harness_uncaught_exception", False, repr(exc))
    elapsed = time.perf_counter() - start
    if elapsed > profile.total_timeout_sec:
        metrics.check("total_timeout_budget", False, f"{elapsed:.2f}s > {profile.total_timeout_sec:.2f}s")
    else:
        metrics.check("total_timeout_budget", True, f"{elapsed:.2f}s")

    write_results(out_dir, profile, metrics, elapsed)
    print(f"[LSP] profile={profile.name} ok={metrics.ok} checks={sum(1 for c in metrics.checks if c.ok)}/{len(metrics.checks)} elapsed={elapsed:.2f}s")
    print(f"[LSP] summary={out_dir / 'summary.md'}")
    return 0 if metrics.ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
