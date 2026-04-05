Unit SelfHost.Frontend.Token;

Get IO.Text;
Get SelfHost.Frontend.TokenType;

Safe Function string Sep() {
    Return [string]([char](31));
}

Safe Function string Encode(int kind, string text, int line, int col) {
    string sep = Sep();
    string safeText = IO.Text.Replace(text, sep, " ");
    Return [string](kind) + sep + [string](line) + sep + [string](col) + sep + safeText;
}

Safe Function string Part(string token, int index) {
    IO.Type.list parts = IO.Text.Split(token, Sep());
    If (index < 0 || index >= parts.Count()) {
        Return "";
    }
    Return [string](parts.Fetch(index));
}

Safe Function int Kind(string token) {
    string text = Part(token, 0);
    If (IO.Text.IsEmpty(text)) {
        Return [int](SelfHost.Frontend.TokenType.TokenType.Unknown);
    }
    Return [int](text);
}

Safe Function int Line(string token) {
    string text = Part(token, 1);
    If (IO.Text.IsEmpty(text)) {
        Return 0;
    }
    Return [int](text);
}

Safe Function int Col(string token) {
    string text = Part(token, 2);
    If (IO.Text.IsEmpty(text)) {
        Return 0;
    }
    Return [int](text);
}

Safe Function string Text(string token) {
    Return Part(token, 3);
}

Safe Function string Dump(string token) {
    int kind = Kind(token);
    string text = Text(token);
    text = IO.Text.Replace(text, [string]([char](10)), "\\n");
    text = IO.Text.Replace(text, [string]([char](13)), "\\r");
    text = IO.Text.Replace(text, [string]([char](9)), "\\t");
    Return SelfHost.Frontend.TokenType.Name(kind) + " @" + [string](Line(token)) + ":" + [string](Col(token)) + " '" + text + "'";
}
