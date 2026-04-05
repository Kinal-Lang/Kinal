Unit SelfHost.Backend.Native.HostApi;

Get IO.Text;
Get IO.Type.list;

Extern Function int __kn_host_compile(string inputPath, string outputPath) By C;
Extern Function int __kn_host_compile_ex(string inputPaths, string outputPath, int autoLink, string extraLinkPaths) By C;

Safe Function string JoinLines(IO.Type.list items) {
    string out = "";
    int i = 0;
    While (i < items.Count()) {
        string s = [string](items.Fetch(i));
        If (!IO.Text.IsEmpty(s)) {
            If (!IO.Text.IsEmpty(out)) {
                out = out + [string]([char](10));
            }
            out = out + s;
        }
        i = i + 1;
    }
    Return out;
}

Trusted Function bool CompileMany(IO.Type.list inputPaths, string outputPath, bool autoLink, IO.Type.list extraLinks) {
    string inText = JoinLines(inputPaths);
    string linkText = JoinLines(extraLinks);
    int rc = __kn_host_compile_ex(inText, outputPath, [int](autoLink), linkText);
    Return rc == 0;
}

Trusted Function bool Compile(string inputPath, string outputPath) {
    IO.Type.list inputs = IO.Type.list.Create();
    inputs.Add(inputPath);
    IO.Type.list links = IO.Type.list.Create();
    Return CompileMany(inputs, outputPath, true, links);
}
