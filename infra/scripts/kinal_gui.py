from __future__ import annotations

import json
import os
import queue
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import font as tkfont
from pathlib import Path
from tkinter import messagebox, ttk

try:
    import ttkbootstrap as tb  # type: ignore
except Exception:
    tb = None


ROOT = Path(__file__).resolve().parents[2]
PYTHON = sys.executable
X_PY = ROOT / "x.py"
GUI_LOCALES_DIR = ROOT / "infra" / "assets" / "locales.build-gui"
DEFAULT_GUI_LANGUAGE = "en-US"
DEFAULT_GUI_LOCALE: dict[str, str] = {
    "window.title": "Kinal Build Dashboard",
    "toolbar.build_mode": "Build mode",
    "toolbar.test_mode": "Test mode",
    "toolbar.language": "UI language",
    "toolbar.clean_build_dir": "Clean build dir",
    "toolbar.clear_output": "Clear Output",
    "toolbar.cancel_task": "Cancel Task",
    "status.idle": "Idle",
    "status.running": "Running",
    "frame.status": "Status",
    "frame.actions": "Actions",
    "frame.output": "Output",
    "status.refresh": "Refresh Status",
    "status.tree.key": "Key",
    "status.tree.value": "Value",
    "actions.group.status_fetch": "Status & Fetch",
    "actions.group.build": "Build",
    "actions.group.validation": "Validation",
    "actions.update_third_party": "Update third-party on fetch",
    "actions.doctor": "Doctor",
    "actions.fetch_third_party": "Fetch Third-Party",
    "actions.fetch_llvm": "Fetch LLVM",
    "actions.fetch_zig": "Fetch Zig",
    "actions.dev": "Dev",
    "actions.dist": "Dist",
    "actions.stdpkg": "StdPkg",
    "actions.tests": "Tests",
    "actions.selfhost": "Selfhost",
    "actions.selfhost_bootstrap": "Selfhost Bootstrap",
    "actions.open_artifacts": "Open Artifacts",
    "dialog.title": "Kinal Build Dashboard",
    "dialog.running_command": "A command is already running.",
    "dialog.refresh_failed": "Failed to refresh status:\n{error}",
    "log.kinal_root": "Kinal root: {path}\n",
    "log.python": "Python: {path}\n",
    "log.command.done": "exit code {code}\n",
    "log.cancel_requested": "cancel requested for running task\n",
    "log.cancel_failed": "failed to cancel task: {error}\n",
    "command.doctor": "Doctor",
    "command.fetch_third_party": "Fetch Third-Party",
    "command.fetch_llvm": "Fetch LLVM",
    "command.fetch_zig": "Fetch Zig",
    "command.dev": "Dev Build",
    "command.dist": "Release Build",
    "command.stdpkg": "StdPkg Build",
    "command.tests": "Tests",
    "command.selfhost": "Selfhost",
    "command.selfhost_bootstrap": "Selfhost Bootstrap",
    "command.open_artifacts": "Open Artifacts",
    "meta.language_name": "English",
}


def available_gui_languages() -> list[str]:
    languages = {DEFAULT_GUI_LANGUAGE}
    if GUI_LOCALES_DIR.exists():
        for path in GUI_LOCALES_DIR.glob("*.json"):
            if path.is_file():
                languages.add(path.stem)
    return sorted(languages)


def gui_language_display_name(language: str) -> str:
    locale = load_gui_locale(language)
    return locale.get("meta.language_name", language)


def load_gui_locale(language: str) -> dict[str, str]:
    locale = dict(DEFAULT_GUI_LOCALE)
    path = GUI_LOCALES_DIR / f"{language}.json"
    if path.exists():
        data = json.loads(path.read_text(encoding="utf-8"))
        if isinstance(data, dict):
            for key, value in data.items():
                if isinstance(key, str) and isinstance(value, str):
                    locale[key] = value
    return locale


class BuildDashboard:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.geometry("1180x780")
        self.root.minsize(1024, 680)

        self.queue: queue.Queue[tuple[str, str]] = queue.Queue()
        self.proc: subprocess.Popen[str] | None = None
        self.worker: threading.Thread | None = None

        self.build_mode = tk.StringVar(value="debug")
        self.test_mode = tk.StringVar(value="smoke")
        self.ui_language = tk.StringVar(value=DEFAULT_GUI_LANGUAGE)
        self.locale = load_gui_locale(self.ui_language.get())
        self.clean_var = tk.BooleanVar(value=False)
        self.update_tp_var = tk.BooleanVar(value=False)
        self.run_state = tk.StringVar(value=self._tr("status.idle"))
        self.available_languages = available_gui_languages()
        self.language_display_names = {
            language: gui_language_display_name(language) for language in self.available_languages
        }
        self.language_selector_var = tk.StringVar(
            value=self.language_display_names.get(self.ui_language.get(), self.ui_language.get())
        )

        self.command_buttons: list[ttk.Button] = []
        self.status_keys: dict[str, str] = {}
        self.toolbar_labels: dict[str, ttk.Label] = {}
        self.toolbar_buttons: dict[str, ttk.Button] = {}
        self.toolbar_checks: dict[str, ttk.Checkbutton] = {}
        self.section_headers: dict[str, ttk.Label] = {}
        self.section_frames: dict[str, ttk.LabelFrame] = {}
        self.action_buttons: dict[str, ttk.Button] = {}
        self.language_selector: ttk.Combobox | None = None
        self.status_refresh_button: ttk.Button | None = None
        self.status_label: ttk.Label | None = None
        self.update_fetch_checkbutton: ttk.Checkbutton | None = None

        self._build_ui()
        self.apply_locale()
        self.refresh_status()
        self.root.after(100, self._drain_queue)

    def _tr(self, key: str, **kwargs: object) -> str:
        text = self.locale.get(key, DEFAULT_GUI_LOCALE.get(key, key))
        if kwargs:
            return text.format(**kwargs)
        return text

    def apply_locale(self) -> None:
        self.root.title(self._tr("window.title"))
        if "build_mode" in self.toolbar_labels:
            self.toolbar_labels["build_mode"].configure(text=self._tr("toolbar.build_mode"))
        if "test_mode" in self.toolbar_labels:
            self.toolbar_labels["test_mode"].configure(text=self._tr("toolbar.test_mode"))
        if "language" in self.toolbar_labels:
            self.toolbar_labels["language"].configure(text=self._tr("toolbar.language"))
        if "clear_output" in self.toolbar_buttons:
            self.toolbar_buttons["clear_output"].configure(text=self._tr("toolbar.clear_output"))
        if "cancel_task" in self.toolbar_buttons:
            self.toolbar_buttons["cancel_task"].configure(text=self._tr("toolbar.cancel_task"))
        if "clean_build_dir" in self.toolbar_checks:
            self.toolbar_checks["clean_build_dir"].configure(text=self._tr("toolbar.clean_build_dir"))
        if self.update_fetch_checkbutton is not None:
            self.update_fetch_checkbutton.configure(text=self._tr("actions.update_third_party"))
        if "status" in self.section_frames:
            self.section_frames["status"].configure(text=self._tr("frame.status"))
        if "actions" in self.section_frames:
            self.section_frames["actions"].configure(text=self._tr("frame.actions"))
        if "output" in self.section_frames:
            self.section_frames["output"].configure(text=self._tr("frame.output"))
        if self.status_refresh_button is not None:
            self.status_refresh_button.configure(text=self._tr("status.refresh"))
        if "status_fetch" in self.section_headers:
            self.section_headers["status_fetch"].configure(text=self._tr("actions.group.status_fetch"))
        if "build" in self.section_headers:
            self.section_headers["build"].configure(text=self._tr("actions.group.build"))
        if "validation" in self.section_headers:
            self.section_headers["validation"].configure(text=self._tr("actions.group.validation"))
        for key, button in self.action_buttons.items():
            button.configure(text=self._tr(f"actions.{key}"))
        self.status_tree.heading("#0", text=self._tr("status.tree.key"))
        self.status_tree.heading("value", text=self._tr("status.tree.value"))
        self._set_running(self.proc is not None)

    def change_language(self, *_: object) -> None:
        selected_name = self.language_selector_var.get()
        selected_language = next(
            (
                language
                for language, display_name in self.language_display_names.items()
                if display_name == selected_name
            ),
            DEFAULT_GUI_LANGUAGE,
        )
        self.ui_language.set(selected_language)
        self.locale = load_gui_locale(self.ui_language.get())
        self.language_selector_var.set(
            self.language_display_names.get(self.ui_language.get(), self.ui_language.get())
        )
        self.apply_locale()

    def _build_ui(self) -> None:
        style = ttk.Style(self.root)
        style.configure("Status.Treeview", rowheight=24)
        style.configure("Primary.TButton", padding=(12, 7))

        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(1, weight=1)
        self.root.rowconfigure(2, weight=1)

        toolbar = ttk.Frame(self.root, padding=12)
        toolbar.grid(row=0, column=0, sticky="ew")
        toolbar.columnconfigure(8, weight=1)

        self.toolbar_labels["build_mode"] = ttk.Label(toolbar)
        self.toolbar_labels["build_mode"].grid(row=0, column=0, sticky="w")
        ttk.Combobox(toolbar, textvariable=self.build_mode, values=["debug", "release"], state="readonly", width=12).grid(row=0, column=1, sticky="w", padx=(6, 16))
        self.toolbar_labels["test_mode"] = ttk.Label(toolbar)
        self.toolbar_labels["test_mode"].grid(row=0, column=2, sticky="w")
        ttk.Combobox(toolbar, textvariable=self.test_mode, values=["smoke", "full"], state="readonly", width=12).grid(row=0, column=3, sticky="w", padx=(6, 16))
        self.toolbar_labels["language"] = ttk.Label(toolbar)
        self.toolbar_labels["language"].grid(row=0, column=4, sticky="w")
        self.language_selector = ttk.Combobox(
            toolbar,
            textvariable=self.language_selector_var,
            values=[self.language_display_names[language] for language in self.available_languages],
            state="readonly",
            width=12,
        )
        self.language_selector.grid(row=0, column=5, sticky="w", padx=(6, 16))
        self.language_selector.bind("<<ComboboxSelected>>", self.change_language)
        self.toolbar_checks["clean_build_dir"] = ttk.Checkbutton(toolbar, variable=self.clean_var)
        self.toolbar_checks["clean_build_dir"].grid(row=0, column=6, sticky="w")
        self.toolbar_buttons["clear_output"] = ttk.Button(toolbar, command=self.clear_output, style="Primary.TButton")
        self.toolbar_buttons["clear_output"].grid(row=0, column=8, sticky="e", padx=(0, 8))
        self.cancel_button = ttk.Button(toolbar, command=self.cancel_process, state=tk.DISABLED, style="Primary.TButton")
        self.cancel_button.grid(row=0, column=9, sticky="e", padx=(0, 8))
        self.toolbar_buttons["cancel_task"] = self.cancel_button
        self.status_label = ttk.Label(toolbar, textvariable=self.run_state)
        self.status_label.grid(row=0, column=10, sticky="e")

        main = ttk.PanedWindow(self.root, orient=tk.HORIZONTAL)
        main.grid(row=1, column=0, sticky="nsew", padx=12, pady=(0, 12))

        left = ttk.Frame(main, padding=0)
        right = ttk.Frame(main, padding=0)
        main.add(left, weight=3)
        main.add(right, weight=2)

        left.columnconfigure(0, weight=1)
        left.rowconfigure(0, weight=1)
        right.columnconfigure(0, weight=1)
        right.rowconfigure(0, weight=1)

        status_frame = ttk.LabelFrame(left, padding=12)
        status_frame.grid(row=0, column=0, sticky="nsew")
        status_frame.columnconfigure(0, weight=1)
        self.section_frames["status"] = status_frame
        actions_frame = ttk.LabelFrame(right, padding=12)
        actions_frame.grid(row=0, column=0, sticky="nsew")
        actions_frame.columnconfigure(0, weight=1)
        actions_frame.rowconfigure(0, weight=1)
        self.section_frames["actions"] = actions_frame
        log_frame = ttk.LabelFrame(self.root, padding=12)
        log_frame.grid(row=2, column=0, sticky="nsew", padx=12, pady=(0, 12))
        log_frame.rowconfigure(1, weight=1)
        log_frame.columnconfigure(0, weight=1)
        self.section_frames["output"] = log_frame

        status_bar = ttk.Frame(status_frame)
        status_bar.grid(row=0, column=0, sticky="ew", pady=(0, 10))
        status_bar.columnconfigure(0, weight=1)
        self.status_refresh_button = ttk.Button(status_bar, command=self.refresh_status)
        self.status_refresh_button.grid(row=0, column=0, sticky="ew")

        self.status_tree = ttk.Treeview(status_frame, columns=("value",), show="tree headings", height=18)
        self.status_tree.configure(style="Status.Treeview")
        self.status_tree.heading("#0", text="Key")
        self.status_tree.heading("value", text="Value")
        self.status_tree.column("#0", width=170, stretch=False)
        self.status_tree.column("value", width=360, stretch=True)
        self.status_tree.grid(row=1, column=0, sticky="nsew")
        status_scroll = ttk.Scrollbar(status_frame, orient="vertical", command=self.status_tree.yview)
        status_scroll.grid(row=1, column=1, sticky="ns")
        self.status_tree.configure(yscrollcommand=status_scroll.set)
        status_frame.rowconfigure(1, weight=1)

        actions_canvas = tk.Canvas(actions_frame, highlightthickness=0, borderwidth=0)
        actions_canvas.grid(row=0, column=0, sticky="nsew")
        actions_scroll = ttk.Scrollbar(actions_frame, orient="vertical", command=actions_canvas.yview)
        actions_scroll.grid(row=0, column=1, sticky="ns")
        actions_canvas.configure(yscrollcommand=actions_scroll.set)

        actions_inner = ttk.Frame(actions_canvas)
        actions_window = actions_canvas.create_window((0, 0), window=actions_inner, anchor="nw")

        def _sync_actions_scrollregion(_: object = None) -> None:
            actions_canvas.configure(scrollregion=actions_canvas.bbox("all"))

        def _sync_actions_width(event: tk.Event) -> None:
            actions_canvas.itemconfigure(actions_window, width=event.width)

        actions_inner.bind("<Configure>", _sync_actions_scrollregion)
        actions_canvas.bind("<Configure>", _sync_actions_width)

        groups: list[tuple[str, str, list[tuple[str, callable]]]] = [
            (
                "status_fetch",
                "actions.group.status_fetch",
                [
                    ("doctor", self.run_doctor),
                    ("fetch_third_party", self.run_fetch_third_party),
                    ("fetch_llvm", self.run_fetch_llvm),
                    ("fetch_zig", self.run_fetch_zig),
                ],
            ),
            (
                "build",
                "actions.group.build",
                [
                    ("dev", self.run_dev),
                    ("dist", self.run_dist),
                    ("stdpkg", self.run_stdpkg),
                ],
            ),
            (
                "validation",
                "actions.group.validation",
                [
                    ("tests", self.run_test),
                    ("selfhost", self.run_selfhost),
                    ("selfhost_bootstrap", self.run_selfhost_bootstrap),
                    ("open_artifacts", self.open_artifacts),
                ],
            ),
        ]
        self.command_buttons = []
        actions_inner.columnconfigure(0, weight=1)
        for group_index, (group_key, title_key, items) in enumerate(groups):
            section = ttk.Frame(actions_inner, padding=(4, 4, 4, 10))
            section.grid(row=group_index, column=0, sticky="ew", pady=(0, 6))
            for col in range(2):
                section.columnconfigure(col, weight=1)
            header = ttk.Label(section)
            header.grid(row=0, column=0, columnspan=2, sticky="w", pady=(0, 6))
            self.section_headers[group_key] = header
            ttk.Separator(section, orient=tk.HORIZONTAL).grid(row=1, column=0, columnspan=2, sticky="ew", pady=(0, 10))
            if group_key == "status_fetch":
                self.update_fetch_checkbutton = ttk.Checkbutton(section, variable=self.update_tp_var)
                self.update_fetch_checkbutton.grid(row=2, column=0, columnspan=2, sticky="w", pady=(0, 6))
                start_row = 3
            else:
                start_row = 2
            for idx, (label_key, handler) in enumerate(items):
                btn = ttk.Button(section, command=handler, style="Primary.TButton")
                btn.grid(row=start_row + idx // 2, column=idx % 2, sticky="ew", padx=4, pady=4, ipady=4)
                self.command_buttons.append(btn)
                self.action_buttons[label_key] = btn

        log_toolbar = ttk.Frame(log_frame)
        log_toolbar.grid(row=0, column=0, columnspan=2, sticky="ew", pady=(0, 10))
        log_toolbar.columnconfigure(0, weight=1)

        code_font = tkfont.nametofont("TkFixedFont").copy()
        code_font.configure(size=10)
        self.log = tk.Text(
            log_frame,
            wrap="word",
            font=code_font,
            padx=10,
            pady=10,
            height=14,
        )
        self.log.grid(row=1, column=0, sticky="nsew")
        log_scroll = ttk.Scrollbar(log_frame, orient="vertical", command=self.log.yview)
        log_scroll.grid(row=1, column=1, sticky="ns")
        self.log.configure(yscrollcommand=log_scroll.set)
        self._configure_log_tags()
        self._append_meta_header()

    def _append_meta_header(self) -> None:
        self._append_log(self._tr("log.kinal_root", path=ROOT), "meta")
        self._append_log(self._tr("log.python", path=PYTHON), "meta")

    def _configure_log_tags(self) -> None:
        self.log.tag_configure("meta", foreground="#8ea0b8")
        self.log.tag_configure("section", foreground="#ffd479", font=("Consolas", 11, "bold"))
        self.log.tag_configure("command", foreground="#7cc9ff")
        self.log.tag_configure("command_body", foreground="#cfe8ff")
        self.log.tag_configure("ok", foreground="#6ad39f")
        self.log.tag_configure("ok_body", foreground="#c7f2da")
        self.log.tag_configure("error", foreground="#ff7b88")
        self.log.tag_configure("error_body", foreground="#ffd6db")
        self.log.tag_configure("warn", foreground="#f4c66a")
        self.log.tag_configure("warn_body", foreground="#ffebba")
        self.log.tag_configure("info", foreground="#82bfff")
        self.log.tag_configure("info_body", foreground="#d7eaff")
        self.log.tag_configure("progress", foreground="#c799ff")
        self.log.tag_configure("path", foreground="#8df0cc")
        self.log.tag_configure("status", foreground="#c9d6e4")
        self.log.tag_configure("dim", foreground="#7d8a9d")

    def _append_log(self, text: str, tag: str | None = None) -> None:
        self.log.configure(state=tk.NORMAL)
        if tag:
            self.log.insert("end", text, tag)
        else:
            self.log.insert("end", text)
        self.log.see("end")
        self.log.configure(state=tk.DISABLED)

    def clear_output(self) -> None:
        self.log.configure(state=tk.NORMAL)
        self.log.delete("1.0", "end")
        self.log.configure(state=tk.DISABLED)
        self._append_meta_header()

    def _set_running(self, running: bool) -> None:
        self.run_state.set(self._tr("status.running") if running else self._tr("status.idle"))
        for button in self.command_buttons:
            button.configure(state=tk.DISABLED if running else tk.NORMAL)
        self.cancel_button.configure(state=tk.NORMAL if running else tk.DISABLED)

    def _start_command(self, title: str, args: list[str]) -> None:
        if self.proc is not None:
            messagebox.showwarning(self._tr("dialog.title"), self._tr("dialog.running_command"))
            return
        self._append_log(f"\n=== {title} ===\n", "section")
        self._append_log_structured("+ ", "command", " ".join(args) + "\n", "command_body")
        self._set_running(True)

        def worker() -> None:
            try:
                self.proc = subprocess.Popen(
                    args,
                    cwd=str(ROOT),
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    bufsize=1,
                )
                assert self.proc.stdout is not None
                for line in self.proc.stdout:
                    self.queue.put(("log", line))
                rc = self.proc.wait()
                self.queue.put(("done", str(rc)))
            except Exception as exc:
                self.queue.put(("log", f"[ERROR] {exc}\n"))
                self.queue.put(("done", "1"))

        self.worker = threading.Thread(target=worker, daemon=True)
        self.worker.start()

    def _drain_queue(self) -> None:
        try:
            while True:
                kind, payload = self.queue.get_nowait()
                if kind == "log":
                    self._append_structured_line(payload)
                elif kind == "done":
                    rc = int(payload)
                    if rc == 0:
                        self._append_log_structured("[DONE] ", "ok", self._tr("log.command.done", code=rc), "ok_body")
                    else:
                        self._append_log_structured("[DONE] ", "error", self._tr("log.command.done", code=rc), "error_body")
                    self.proc = None
                    self.worker = None
                    self._set_running(False)
                    self.refresh_status(silent=True)
        except queue.Empty:
            pass
        self.root.after(100, self._drain_queue)

    def cancel_process(self) -> None:
        if self.proc is not None:
            self._append_log_structured("[WARN] ", "warn", self._tr("log.cancel_requested"), "warn_body")
            try:
                if sys.platform == "win32":
                    subprocess.run(
                        ["taskkill", "/PID", str(self.proc.pid), "/T", "/F"],
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,
                        check=False,
                    )
                else:
                    self.proc.terminate()
                    try:
                        self.proc.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        self.proc.kill()
            except Exception as exc:
                self._append_log_structured("[ERROR] ", "error", self._tr("log.cancel_failed", error=exc), "error_body")

    def _cmd(self, *parts: str) -> list[str]:
        return [PYTHON, "-u", str(X_PY), *parts]

    def _append_log_structured(self, prefix: str, prefix_tag: str, body: str, body_tag: str | None = None) -> None:
        self.log.configure(state=tk.NORMAL)
        self.log.insert("end", prefix, prefix_tag)
        self.log.insert("end", body, body_tag or "status")
        self.log.see("end")
        self.log.configure(state=tk.DISABLED)

    def _append_structured_line(self, line: str) -> None:
        stripped = line.lstrip()
        if stripped.startswith("[ERROR] "):
            self._append_log_structured("[ERROR] ", "error", stripped[len("[ERROR] "):], "error_body")
            return
        if stripped.startswith("[WARN] "):
            self._append_log_structured("[WARN] ", "warn", stripped[len("[WARN] "):], "warn_body")
            return
        if stripped.startswith("[OK] "):
            self._append_log_structured("[OK] ", "ok", stripped[len("[OK] "):], "ok_body")
            return
        if stripped.startswith("[INFO] "):
            body = stripped[len("[INFO] "):]
            body_tag = "progress" if "progress:" in body.lower() else "info_body"
            self._append_log_structured("[INFO] ", "info", body, body_tag)
            return
        if stripped.startswith("+ "):
            self._append_log_structured("+ ", "command", stripped[2:], "command_body")
            return
        if stripped.startswith("Traceback"):
            self._append_log(line, "error")
            return
        if "Kinal-ThirdParty" in line or "\\llvm\\" in line or "/llvm/" in line or ".cache\\llvm" in line:
            self._append_log(line, "path")
            return
        if "%" in line and "progress" in line.lower():
            self._append_log(line, "progress")
            return
        if stripped.startswith("==="):
            self._append_log(line, "section")
            return
        if stripped.startswith("host=") or stripped.startswith("python=") or stripped.startswith("llvm_") or stripped.startswith("zig="):
            self._append_log(line, "dim")
            return
        self._append_log(line, "status")

    def _refresh_status_view(self, data: dict[str, object]) -> None:
        self.status_tree.delete(*self.status_tree.get_children())
        ordered_keys = [
            "host",
            "python",
            "third_party_root",
            "third_party_repo",
            "third_party_present",
            "llvm_ready",
            "llvm_dir",
            "llvm_bin",
            "doctor_hint",
            "next_step",
            "zig_ready",
            "zig",
            "build_debug",
            "stage_debug",
            "release",
        ]
        for key in ordered_keys:
            if key in data:
                value = data.get(key, "")
                self.status_tree.insert("", "end", text=key, values=(str(value),))

    def refresh_status(self, silent: bool = False) -> None:
        try:
            result = subprocess.run(
                self._cmd("status", "--json"),
                cwd=str(ROOT),
                capture_output=True,
                text=True,
                check=True,
            )
            data = json.loads(result.stdout)
            self._refresh_status_view(data)
        except Exception as exc:
            if not silent:
                messagebox.showerror(self._tr("dialog.title"), self._tr("dialog.refresh_failed", error=exc))

    def run_doctor(self) -> None:
        self._start_command(self._tr("command.doctor"), self._cmd("doctor"))

    def run_fetch_third_party(self) -> None:
        cmd = self._cmd("fetch", "third-party")
        if self.update_tp_var.get():
            cmd.append("--update")
        self._start_command(self._tr("command.fetch_third_party"), cmd)

    def run_fetch_llvm(self) -> None:
        self._start_command(self._tr("command.fetch_llvm"), self._cmd("fetch", "llvm-prebuilt"))

    def run_fetch_zig(self) -> None:
        self._start_command(self._tr("command.fetch_zig"), self._cmd("fetch", "zig-prebuilt"))

    def run_dev(self) -> None:
        cmd = self._cmd("dev")
        if self.build_mode.get() == "release":
            cmd.append("--release")
        if self.clean_var.get():
            cmd.append("--clean")
        self._start_command(self._tr("command.dev"), cmd)

    def run_dist(self) -> None:
        cmd = self._cmd("dist")
        if self.clean_var.get():
            cmd.append("--clean")
        self._start_command(self._tr("command.dist"), cmd)

    def run_stdpkg(self) -> None:
        cmd = self._cmd("stdpkg")
        if self.build_mode.get() == "release":
            cmd.append("--release")
        self._start_command(self._tr("command.stdpkg"), cmd)

    def run_test(self) -> None:
        cmd = self._cmd("test")
        if self.build_mode.get() == "release":
            cmd.append("--release")
        if self.test_mode.get() == "full":
            cmd.append("--full")
        self._start_command(self._tr("command.tests"), cmd)

    def run_selfhost(self) -> None:
        cmd = self._cmd("selfhost")
        if self.clean_var.get():
            cmd.append("--clean")
        self._start_command(self._tr("command.selfhost"), cmd)

    def run_selfhost_bootstrap(self) -> None:
        cmd = self._cmd("selfhost-bootstrap")
        if self.clean_var.get():
            cmd.append("--clean")
        self._start_command(self._tr("command.selfhost_bootstrap"), cmd)

    def open_artifacts(self) -> None:
        self._start_command(self._tr("command.open_artifacts"), self._cmd("open-artifacts"))


def launch_gui() -> None:
    if tb is not None:
        root = tb.Window(themename="darkly")
    else:
        root = tk.Tk()
    BuildDashboard(root)
    root.mainloop()


if __name__ == "__main__":
    launch_gui()
