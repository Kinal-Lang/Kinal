Unit SelfHost.Link;

Get IO.File;
Get IO.Path;
Get IO.System;
Get IO.Text;
Get IO.Target;
Get SelfHost.Std.Process;

Safe Function string ExePathFromCommandLine() {
    string cmd = IO.System.CommandLine();
    char[] cs = cmd.ToChars();
    int n = cs.Length();
    int i = 0;

    While (i < n) {
        int c = [int](cs.Index(i));
        If (c != 32 && c != 9) {
            Break;
        }
        i = i + 1;
    }

    If (i >= n) {
        Return "";
    }

    bool quoted = [int](cs.Index(i)) == 34;
    If (quoted) {
        i = i + 1;
    }

    string out = "";
    While (i < n) {
        int c2 = [int](cs.Index(i));
        If (quoted && c2 == 34) {
            Break;
        }
        If (!quoted && (c2 == 32 || c2 == 9)) {
            Break;
        }
        out = out + [string]([char](c2));
        i = i + 1;
    }
    Return IO.Text.Trim(out);
}

Safe Function int LastSepIndex(string path) {
    char[] cs = path.ToChars();
    int i = 0;
    int at = -1;
    While (i < cs.Length()) {
        int c = [int](cs.Index(i));
        If (c == 47 || c == 92) {
            at = i;
        }
        i = i + 1;
    }
    Return at;
}

Safe Function string ExeDir() {
    string exe = ExePathFromCommandLine();
    If (IO.Text.IsEmpty(exe)) {
        Return ".";
    }
    int at = LastSepIndex(exe);
    If (at < 0) {
        Return ".";
    }
    If (at == 0) {
        Return IO.Text.Slice(exe, 0, 1);
    }
    Return IO.Text.Slice(exe, 0, at);
}

Safe Function string FromExeDir(string rel) {
    Return IO.Path.Normalize(IO.Path.Combine(ExeDir(), rel));
}

Safe Function string LlcPath() {
    If (IO.Target.OS == IO.Target.OS.Windows) {
        string p00 = FromExeDir("linker/llc.exe");
        If (IO.File.Exists(p00)) {
            Return p00;
        }
        string p0 = FromExeDir("llvm/bin/llc.exe");
        If (IO.File.Exists(p0)) {
            Return p0;
        }
        string p2 = "llvm/bin/llc.exe";
        If (IO.File.Exists(p2)) {
            Return p2;
        }
        string p = "third_party/llvm/prebuilt/clang+llvm-21.1.8-x86_64-pc-windows-msvc/bin/llc.exe";
        If (IO.File.Exists(p)) {
            Return p;
        }
        Return "llc.exe";
    }
    Return "llc";
}

Safe Function string LldPath() {
    If (IO.Target.OS == IO.Target.OS.Windows) {
        string p00 = FromExeDir("linker/lld-link.exe");
        If (IO.File.Exists(p00)) {
            Return p00;
        }
        string p0 = FromExeDir("llvm/bin/lld-link.exe");
        If (IO.File.Exists(p0)) {
            Return p0;
        }
        string p2 = "llvm/bin/lld-link.exe";
        If (IO.File.Exists(p2)) {
            Return p2;
        }
        string p = "third_party/llvm/prebuilt/clang+llvm-21.1.8-x86_64-pc-windows-msvc/bin/lld-link.exe";
        If (IO.File.Exists(p)) {
            Return p;
        }
        Return "lld-link.exe";
    }
    Return "ld.lld";
}

Safe Function string ObjPathFromLl(string llPath) {
    Return llPath + ".obj";
}

Safe Function string ResolveKernel32Lib() {
    string p00 = FromExeDir("runtime/win-x64/kernel32.lib");
    If (IO.File.Exists(p00)) {
        Return p00;
    }

    string p0 = FromExeDir("runtime/kernel32.lib");
    If (IO.File.Exists(p0)) {
        Return p0;
    }

    string p3 = "runtime/win-x64/kernel32.lib";
    If (IO.File.Exists(p3)) {
        Return p3;
    }

    string p = "runtime/kernel32.lib";
    If (IO.File.Exists(p)) {
        Return p;
    }

    p = "build/kernel32.lib";
    If (IO.File.Exists(p)) {
        Return p;
    }

    p = "kernel32.lib";
    If (IO.File.Exists(p)) {
        Return p;
    }

    Return "";
}

Safe Function string ResolveUtilObj() {
    string p00 = FromExeDir("runtime/win-x64/kn_util_hostlib.obj");
    If (IO.File.Exists(p00)) {
        Return p00;
    }

    string p0 = FromExeDir("runtime/kn_util_hostlib.obj");
    If (IO.File.Exists(p0)) {
        Return p0;
    }

    string p3 = "runtime/win-x64/kn_util_hostlib.obj";
    If (IO.File.Exists(p3)) {
        Return p3;
    }

    string p = "runtime/kn_util_hostlib.obj";
    If (IO.File.Exists(p)) {
        Return p;
    }

    p = "build/kn_util_hostlib.obj";
    If (IO.File.Exists(p)) {
        Return p;
    }

    p = "kn_util_hostlib.obj";
    If (IO.File.Exists(p)) {
        Return p;
    }

    Return "";
}

Safe Function string ResolveChkstkObj() {
    string p00 = FromExeDir("runtime/win-x64/kn_chkstk.obj");
    If (IO.File.Exists(p00)) {
        Return p00;
    }

    string p0 = FromExeDir("runtime/kn_chkstk.obj");
    If (IO.File.Exists(p0)) {
        Return p0;
    }

    string p3 = "runtime/win-x64/kn_chkstk.obj";
    If (IO.File.Exists(p3)) {
        Return p3;
    }

    string p = "runtime/kn_chkstk.obj";
    If (IO.File.Exists(p)) {
        Return p;
    }

    p = "build/kn_chkstk.obj";
    If (IO.File.Exists(p)) {
        Return p;
    }

    p = "kn_chkstk.obj";
    If (IO.File.Exists(p)) {
        Return p;
    }

    Return "";
}

Safe Function bool NormalizeEscapedIr(string llPath, IO.Type.list diagnostics) {
    string raw = IO.File.ReadAllText(llPath);
    If (IO.Text.IsEmpty(raw)) {
        diagnostics.Add("[Link] failed to read llvm ir file: " + llPath);
        Return false;
    }

    string bs = [string]([char](92));
    string dq = [string]([char](34));

    raw = IO.Text.Replace(raw, bs + bs, bs);
    raw = IO.Text.Replace(raw, bs + dq, dq);

    If (!IO.File.WriteAllText(llPath, raw)) {
        diagnostics.Add("[Link] failed to normalize llvm ir file: " + llPath);
        Return false;
    }
    Return true;
}

Safe Function bool Run(string llPath, string exePath, IO.Type.list diagnostics) {
    If (IO.Target.OS != IO.Target.OS.Windows) {
        diagnostics.Add("[Link] self backend currently supports Windows only");
        Return false;
    }

    string llc = LlcPath();
    string lld = LldPath();
    string obj = ObjPathFromLl(llPath);
    string kernel32Lib = ResolveKernel32Lib();
    string utilObj = ResolveUtilObj();
    string chkstkObj = ResolveChkstkObj();

    If (IO.Text.IsEmpty(kernel32Lib)) {
        diagnostics.Add("[Link] missing kernel32 import library (runtime/win-x64/kernel32.lib or legacy runtime/kernel32.lib)");
        Return false;
    }
    If (IO.Text.IsEmpty(utilObj)) {
        diagnostics.Add("[Link] missing runtime support object (runtime/win-x64/kn_util_hostlib.obj or legacy runtime/kn_util_hostlib.obj)");
        Return false;
    }
    If (IO.Text.IsEmpty(chkstkObj)) {
        diagnostics.Add("[Link] missing runtime stack helper object (runtime/win-x64/kn_chkstk.obj or legacy runtime/kn_chkstk.obj)");
        Return false;
    }

    string qllc = SelfHost.Std.Process.Quote(llc);
    string qlld = SelfHost.Std.Process.Quote(lld);
    string qll = SelfHost.Std.Process.Quote(llPath);
    string qo = SelfHost.Std.Process.Quote(obj);
    string qexe = SelfHost.Std.Process.Quote(exePath);
    string qk32 = SelfHost.Std.Process.Quote(kernel32Lib);
    string qutil = SelfHost.Std.Process.Quote(utilObj);
    string qchk = SelfHost.Std.Process.Quote(chkstkObj);

    If (!NormalizeEscapedIr(llPath, diagnostics)) {
        Return false;
    }

    string objCmd = qllc + " -filetype=obj -o " + qo + " " + qll;
    int rc1 = SelfHost.Std.Process.Exec(objCmd);
    If (rc1 != 0) {
        diagnostics.Add("[Link] llc failed rc=" + [string](rc1));
        Return false;
    }

    string ldCmd = qlld + " /nologo /subsystem:console /entry:mainCRTStartup /out:" + qexe + " " + qo + " " + qutil + " " + qchk + " " + qk32;
    int rc2 = SelfHost.Std.Process.Exec(ldCmd);
    If (rc2 != 0) {
        diagnostics.Add("[Link] lld-link failed rc=" + [string](rc2));
        Return false;
    }

    Return true;
}

