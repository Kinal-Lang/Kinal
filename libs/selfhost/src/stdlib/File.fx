Unit SelfHost.Std.File;

Get IO.File;
Get IO.Type.list;
Get SelfHost.Std.Path;

Safe Function bool Exists(string path) {
    Return IO.File.Exists(SelfHost.Std.Path.Normalize(path));
}

Safe Function string ReadAllText(string path) {
    Return IO.File.ReadAllText(SelfHost.Std.Path.Normalize(path));
}

Safe Function bool WriteAllText(string path, string text) {
    Return IO.File.WriteAllText(SelfHost.Std.Path.Normalize(path), text);
}

Safe Function bool WriteLines(string path, IO.Type.list lines) {
    Return IO.File.WriteLines(SelfHost.Std.Path.Normalize(path), lines);
}

Safe Function IO.Type.list ReadLines(string path) {
    Return IO.File.ReadLines(SelfHost.Std.Path.Normalize(path));
}

Safe Function bool Delete(string path) {
    Return IO.File.Delete(SelfHost.Std.Path.Normalize(path));
}
