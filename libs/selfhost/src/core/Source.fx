Unit SelfHost.Source;

Get IO.File;
Get IO.Text;
Get IO.Type.list;

Class SourceFile {
    Public string Path;
    Public string Text;
    Public IO.Type.list Lines;

    Public Safe Constructor(string path, string text) {
        This.Path = path;
        This.Text = text;
        This.Lines = IO.Text.Lines(text);
    }

    Public Safe Function string GetLine(int line) {
        If (line < 1 || line > This.Lines.Count()) {
            Return "";
        }
        Return [string](This.Lines.Fetch(line - 1));
    }

    Public Safe Function int LineCount() {
        Return This.Lines.Count();
    }
}

Safe Function SourceFile Load(string path) {
    If (!IO.File.Exists(path)) {
        Return New SourceFile(path, "");
    }
    string text = IO.File.ReadAllText(path);
    char[] chars = text.ToChars();
    If (chars.Length() >= 3 &&
        [int](chars.Index(0)) == 239 &&
        [int](chars.Index(1)) == 187 &&
        [int](chars.Index(2)) == 191) {
        text = IO.Text.Slice(text, 3, IO.Text.Length(text) - 3);
    }
    Return New SourceFile(path, text);
}
