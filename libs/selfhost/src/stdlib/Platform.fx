Unit SelfHost.Std.Platform;

Safe Function bool IsWindows() {
    Return IO.Target.OS == IO.Target.OS.Windows;
}

Safe Function bool IsLinux() {
    Return IO.Target.OS == IO.Target.OS.Linux;
}

Safe Function string Name() {
    If (IO.Target.OS == IO.Target.OS.Windows) {
        Return "windows";
    } Else If (IO.Target.OS == IO.Target.OS.Linux) {
        Return "linux";
    }
    Return "unknown";
}

Safe Function string PathSeparator() {
    If (IO.Target.OS == IO.Target.OS.Windows) {
        Return "\\";
    }
    Return "/";
}

Safe Function string ExeSuffix() {
    If (IO.Target.OS == IO.Target.OS.Windows) {
        Return ".exe";
    }
    Return "";
}

Safe Function string ObjSuffix() {
    If (IO.Target.OS == IO.Target.OS.Windows) {
        Return ".obj";
    }
    Return ".o";
}

Safe Function string StaticLibSuffix() {
    If (IO.Target.OS == IO.Target.OS.Windows) {
        Return ".lib";
    }
    Return ".a";
}

Safe Function string SharedLibSuffix() {
    If (IO.Target.OS == IO.Target.OS.Windows) {
        Return ".dll";
    }
    Return ".so";
}
