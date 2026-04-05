Unit SelfHost.Diag;

Get Console By IO.Console;
Get IO.Text;
Get IO.Type.list;
Get SelfHost.Source;

Safe Function string Repeat(string text, int count) {
    If (count <= 0) {
        Return "";
    }

    string out = "";
    int i = 0;
    While (i < count) {
        out = out + text;
        i = i + 1;
    }
    Return out;
}

Safe Function string BuildMessage(SourceFile source, string stage, string title, string detail, string got, int line, int col, int length) {
    int safeLine = line;
    int safeCol = col;
    int caretLen = length;
    If (safeLine < 1) {
        safeLine = 1;
    }
    If (safeCol < 1) {
        safeCol = 1;
    }
    If (caretLen < 1) {
        caretLen = 1;
    }

    string n = [string]([char](10));
    string header = "[" + stage + "] " + title;
    If (!IO.Text.IsEmpty(detail)) {
        header = header + ": " + detail;
    }
    If (!IO.Text.IsEmpty(got)) {
        header = header + " (Got '" + got + "')";
    }

    string path = source.Path;
    string lineText = source.GetLine(safeLine);

    string lineNo = [string](safeLine);
    string codeLine = lineNo + " | " + lineText;
    string caretPrefix = Repeat(" ", IO.Text.Length(lineNo) + 3 + safeCol - 1);
    string caretLine = caretPrefix + Repeat("^", caretLen);

    string loc = "in <" + path + "> at line <" + [string](safeLine) + ">, column <" + [string](safeCol) + ">";
    Return header + n + loc + n + n + codeLine + n + caretLine;
}

Safe Function void Add(IO.Type.list diagnostics, SourceFile source, string stage, string title, string detail, string got, int line, int col, int length) {
    diagnostics.Add(BuildMessage(source, stage, title, detail, got, line, col, length));
}

Safe Function bool HasAny(IO.Type.list diagnostics) {
    Return diagnostics.Count() > 0;
}

Safe Function void PrintAll(IO.Type.list diagnostics) {
    int i = 0;
    While (i < diagnostics.Count()) {
        Console.PrintLine([string](diagnostics.Fetch(i)));
        i = i + 1;
    }
}
