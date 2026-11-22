"""Process timing and Windows working-set counters for benchmark workers."""
import ctypes
from ctypes import wintypes
import hashlib
import os
from pathlib import Path
import queue
import subprocess
import threading
import time

ENV = dict(os.environ, PYTHONUTF8="1", DOTNET_CLI_TELEMETRY_OPTOUT="1",
           DOTNET_SKIP_FIRST_TIME_EXPERIENCE="1", DOTNET_NOLOGO="1",
           GOTOOLCHAIN="local", GOPROXY="off", GOSUMDB="off")


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def capture(args, *, timeout, env, cwd):
    started = time.perf_counter()
    try:
        process = subprocess.run(args, cwd=cwd, env=env or ENV, capture_output=True,
                                 text=True, encoding="utf-8", errors="replace", timeout=timeout)
        result = dict(exit_code=process.returncode, stdout=process.stdout, stderr=process.stderr,
                      status="ok" if process.returncode == 0 else "error")
    except subprocess.TimeoutExpired as exc:
        result = dict(stdout=str(exc.stdout or ""), stderr=str(exc.stderr or ""), status="timeout")
    except OSError as exc:
        result = dict(stdout="", stderr=str(exc), status="error")
    return dict(command=list(map(str, args)), wall_ms=(time.perf_counter() - started) * 1000, **result)


class MemoryCounters(ctypes.Structure):
    _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD)] + [
        (name, ctypes.c_size_t) for name in (
            "PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage", "QuotaPagedPoolUsage",
            "QuotaPeakNonPagedPoolUsage", "QuotaNonPagedPoolUsage", "PagefileUsage", "PeakPagefileUsage")]


if os.name == "nt":
    get_memory = ctypes.WinDLL("psapi", use_last_error=True).GetProcessMemoryInfo
    get_memory.argtypes = [wintypes.HANDLE, ctypes.POINTER(MemoryCounters), wintypes.DWORD]
    get_times = ctypes.WinDLL("kernel32", use_last_error=True).GetProcessTimes
    get_times.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4


def usage(proc):
    if os.name != "nt":
        return dict(peak_working_set_bytes=None, peak_commit_bytes=None, private_bytes=None, cpu_seconds=None)
    handle = wintypes.HANDLE(int(proc._handle))
    memory = MemoryCounters()
    memory.cb = ctypes.sizeof(memory)
    memory_ok = get_memory(handle, ctypes.byref(memory), memory.cb)
    creation, exit_time, kernel, user = (wintypes.FILETIME() for _ in range(4))
    times_ok = get_times(handle, *(ctypes.byref(t) for t in (creation, exit_time, kernel, user)))
    ticks = sum((t.dwHighDateTime << 32) + t.dwLowDateTime for t in (kernel, user))
    return dict(peak_working_set_bytes=memory.PeakWorkingSetSize if memory_ok else None,
                peak_commit_bytes=memory.PeakPagefileUsage if memory_ok else None,
                private_bytes=memory.PagefileUsage if memory_ok else None,
                cpu_seconds=ticks / 1e7 if times_ok else None)


class Worker:
    def __init__(self, variant, case_id, size):
        env = dict(ENV, BENCH_CASE=str(case_id), BENCH_N=str(size))
        env.pop("BENCH_STARTUP", None)
        self.proc = subprocess.Popen(variant["run"] + [str(case_id), str(size)], cwd=variant["cwd"],
                                     env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace",
                                     bufsize=1, creationflags=variant.get("creationflags", 0))
        self.lines = queue.Queue()
        self.errors = []
        self.readers = [threading.Thread(target=self.read_stdout, daemon=True),
                        threading.Thread(target=self.read_stderr, daemon=True)]
        for reader in self.readers:
            reader.start()
        try:
            ready = self.read()
            if ready != "READY":
                raise RuntimeError(f"worker not ready: {ready!r}")
        except Exception:
            self.close()
            raise

    def read_stdout(self):
        for line in self.proc.stdout:
            self.lines.put(line.rstrip("\r\n"))
        self.lines.put(None)

    def read_stderr(self):
        self.errors.append(self.proc.stderr.read())

    def read(self):
        try:
            line = self.lines.get(timeout=45)
        except queue.Empty:
            self.proc.kill()
            raise TimeoutError("45 second worker response timeout") from None
        if line is None:
            raise RuntimeError(f"worker stopped responding (exit {self.proc.poll()}): {''.join(self.errors)}")
        return line

    def batch(self, reps, seed):
        before = usage(self.proc)
        started = time.perf_counter_ns()
        self.proc.stdin.write(f"{reps}\n{seed}\n")
        self.proc.stdin.flush()
        output = self.read()
        elapsed = (time.perf_counter_ns() - started) / 1e6
        after = usage(self.proc)
        cpu_ms = None
        if after["cpu_seconds"] is not None and before["cpu_seconds"] is not None:
            cpu_ms = (after["cpu_seconds"] - before["cpu_seconds"]) * 1000
        return dict(wall_ms=elapsed, value=float(output), output=output, cpu_ms=cpu_ms, **after)

    def close(self):
        if self.proc.poll() is None:
            try:
                self.proc.stdin.write("0\n0\n")
                self.proc.stdin.flush()
                self.proc.wait(timeout=3)
            except (OSError, subprocess.TimeoutExpired):
                self.proc.kill()
                self.proc.wait()
        for reader in self.readers:
            reader.join()
        for stream in (self.proc.stdin, self.proc.stdout, self.proc.stderr):
            try:
                stream.close()
            except BrokenPipeError:
                pass
        return self.proc.returncode
