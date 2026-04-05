Unit SelfHost.Std.Path;

Get IO.Path;
Get IO.Text;
Get SelfHost.Std.Platform;

Safe Function string Normalize(string path) {
    Return IO.Path.Normalize(path);
}

Safe Function string Join(string a, string b) {
    Return Normalize(IO.Path.Combine(a, b));
}

Safe Function string EnsureSuffix(string value, string suffix) {
    If (IO.Text.IsEmpty(suffix) || IO.Text.EndsWith(value, suffix)) {
        Return value;
    }
    Return value + suffix;
}

Safe Function string EnsureExe(string baseName) {
    Return EnsureSuffix(baseName, SelfHost.Std.Platform.ExeSuffix());
}

Safe Function string WithObjSuffix(string baseName) {
    Return EnsureSuffix(baseName, SelfHost.Std.Platform.ObjSuffix());
}
