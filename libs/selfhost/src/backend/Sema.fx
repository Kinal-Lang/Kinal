Unit SelfHost.Sema;

Get IO.Text;
Get IO.Type.list;
Get SelfHost.Source;
Get SelfHost.Diag;
Get SelfHost.Frontend.Token;
Get SelfHost.Frontend.TokenType;

Class Cursor {
    Public IO.Type.list Tokens;
    Public int Pos;

    Public Safe Constructor(IO.Type.list tokens) {
        This.Tokens = tokens;
        This.Pos = 0;
    }
}

Safe Function int TokenCount(Cursor cur) {
    Return cur.Tokens.Count();
}

Safe Function string RawTokenAt(Cursor cur, int index) {
    If (index < 0 || index >= TokenCount(cur)) {
        Return "";
    }
    Return [string](cur.Tokens.Fetch(index));
}

Safe Function int KindAt(Cursor cur, int index) {
    string raw = RawTokenAt(cur, index);
    If (IO.Text.IsEmpty(raw)) {
        Return [int](SelfHost.Frontend.TokenType.TokenType.Eof);
    }
    Return SelfHost.Frontend.Token.Kind(raw);
}

Safe Function string TextAt(Cursor cur, int index) {
    string raw = RawTokenAt(cur, index);
    If (IO.Text.IsEmpty(raw)) {
        Return "";
    }
    Return SelfHost.Frontend.Token.Text(raw);
}

Safe Function int LineAt(Cursor cur, int index) {
    string raw = RawTokenAt(cur, index);
    If (IO.Text.IsEmpty(raw)) {
        Return 1;
    }
    Return SelfHost.Frontend.Token.Line(raw);
}

Safe Function int ColAt(Cursor cur, int index) {
    string raw = RawTokenAt(cur, index);
    If (IO.Text.IsEmpty(raw)) {
        Return 1;
    }
    Return SelfHost.Frontend.Token.Col(raw);
}

Safe Function int CurrentKind(Cursor cur) {
    Return KindAt(cur, cur.Pos);
}

Safe Function string CurrentText(Cursor cur) {
    Return TextAt(cur, cur.Pos);
}

Safe Function int CurrentLine(Cursor cur) {
    Return LineAt(cur, cur.Pos);
}

Safe Function int CurrentCol(Cursor cur) {
    Return ColAt(cur, cur.Pos);
}

Safe Function bool IsEnd(Cursor cur) {
    Return CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.Eof);
}

Safe Function void Advance(Cursor cur) {
    If (cur.Pos < TokenCount(cur)) {
        cur.Pos = cur.Pos + 1;
    }
}

Safe Function int TokenSpan(Cursor cur) {
    string text = CurrentText(cur);
    int len = IO.Text.Length(text);
    If (len < 1) {
        Return 1;
    }
    Return len;
}

Safe Function void ReportToken(IO.Type.list diagnostics, SourceFile source, Cursor cur, string title, string detail) {
    SelfHost.Diag.Add(
        diagnostics,
        source,
        "Sema",
        title,
        detail,
        CurrentText(cur),
        CurrentLine(cur),
        CurrentCol(cur),
        TokenSpan(cur)
    );
}

Safe Function bool Match(Cursor cur, int kind) {
    If (CurrentKind(cur) == kind) {
        Advance(cur);
        Return true;
    }
    Return false;
}

Safe Function bool Expect(Cursor cur, IO.Type.list diagnostics, SourceFile source, int kind, string title, string detail) {
    If (CurrentKind(cur) == kind) {
        Advance(cur);
        Return true;
    }
    ReportToken(diagnostics, source, cur, title, detail);
    Return false;
}

Safe Function bool IsFunctionModifier(int kind) {
    Return kind == [int](SelfHost.Frontend.TokenType.TokenType.KwStatic) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwSafe) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwTrusted) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwUnsafe) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwPublic) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwPrivate) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwProtected) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwInternal) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwExtern);
}

Safe Function bool IsTypeDeclLead(int kind) {
    Return kind == [int](SelfHost.Frontend.TokenType.TokenType.KwClass) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwInterface) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwStruct) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwEnum) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwAbstract) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwSealed) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwPublic) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwPrivate) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwProtected) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwInternal);
}

Safe Function bool ListContains(IO.Type.list values, string value) {
    int i = 0;
    While (i < values.Count()) {
        If ([string](values.Fetch(i)) == value) {
            Return true;
        }
        i = i + 1;
    }
    Return false;
}

Safe Function void ListAddUnique(IO.Type.list values, string value) {
    If (IO.Text.IsEmpty(value)) {
        Return;
    }
    If (!ListContains(values, value)) {
        values.Add(value);
    }
}

Safe Function string LastSegment(string qname) {
    IO.Type.list parts = IO.Text.Split(qname, ".");
    int n = parts.Count();
    If (n <= 0) {
        Return qname;
    }
    Return [string](parts.Fetch(n - 1));
}

Safe Function bool ParseQualifiedNameText(Cursor cur, SourceFile source, IO.Type.list diagnostics, string[] outRef) {
    If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Qualified name must start with identifier")) {
        Return false;
    }

    string qname = TextAt(cur, cur.Pos - 1);
    While (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.Dot))) {
        If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Missing identifier after '.'")) {
            Return false;
        }
        qname = qname + "." + TextAt(cur, cur.Pos - 1);
    }

    outRef[0] = qname;
    Return true;
}

Safe Function bool ParseGetForSema(Cursor cur, SourceFile source, IO.Type.list diagnostics, IO.Type.list scopedSymbols) {
    If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.KwGet), "Expected Get", "Expected Get declaration")) {
        Return false;
    }

    string[] moduleRef = { "" };
    If (!ParseQualifiedNameText(cur, source, diagnostics, moduleRef)) {
        Return false;
    }
    string moduleOrSymbol = moduleRef.Index(0);

    bool hasSelectorList = false;
    bool hasBy = false;

    If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.LBrace))) {
        hasSelectorList = true;
        If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Get selector list cannot be empty")) {
            Return false;
        }

        string local = TextAt(cur, cur.Pos - 1);
        If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwBy))) {
            If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Alias target is missing")) {
                Return false;
            }
            local = TextAt(cur, cur.Pos - 1);
        }
        ListAddUnique(scopedSymbols, local);

        While (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.Comma))) {
            If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Missing selector after ','")) {
                Return false;
            }
            local = TextAt(cur, cur.Pos - 1);
            If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwBy))) {
                If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Alias target is missing")) {
                    Return false;
                }
                local = TextAt(cur, cur.Pos - 1);
            }
            ListAddUnique(scopedSymbols, local);
        }

        If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.RBrace), "Expected Right Brace", "Get selector list must end with '}'")) {
            Return false;
        }
    }

    If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwBy))) {
        hasBy = true;
        string[] targetRef = { "" };
        If (!ParseQualifiedNameText(cur, source, diagnostics, targetRef)) {
            Return false;
        }
    }

    If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon), "Expected Semicolon", "Missing ';' after Get declaration")) {
        Return false;
    }

    Return true;
}

Safe Function bool CheckScopedSymbolCalls(Cursor scan, SourceFile source, IO.Type.list diagnostics, IO.Type.list scopedSymbols) {
    If (scopedSymbols.Count() <= 0) {
        Return true;
    }

    int i = 0;
    While (i + 1 < TokenCount(scan)) {
        If (KindAt(scan, i) == [int](SelfHost.Frontend.TokenType.TokenType.Identifier) && KindAt(scan, i + 1) == [int](SelfHost.Frontend.TokenType.TokenType.LParen)) {
            int prev = [int](SelfHost.Frontend.TokenType.TokenType.Unknown);
            If (i > 0) {
                prev = KindAt(scan, i - 1);
            }

            If (prev != [int](SelfHost.Frontend.TokenType.TokenType.Dot)) {
                string name = TextAt(scan, i);
                If (ListContains(scopedSymbols, name)) {
                    SelfHost.Diag.Add(
                        diagnostics,
                        source,
                        "Sema",
                        "Scoped Symbol Required",
                        "Imported symbol must be called with module or alias qualifier",
                        name,
                        LineAt(scan, i),
                        ColAt(scan, i),
                        IO.Text.Length(name)
                    );
                    Return false;
                }
            }
        }
        i = i + 1;
    }

    Return true;
}

Safe Function void SkipBalanced(Cursor cur, int openKind, int closeKind) {
    If (CurrentKind(cur) != openKind) {
        Return;
    }

    int depth = 1;
    Advance(cur);
    While (!IsEnd(cur) && depth > 0) {
        int kind = CurrentKind(cur);
        If (kind == openKind) {
            depth = depth + 1;
        } Else If (kind == closeKind) {
            depth = depth - 1;
        }
        Advance(cur);
    }
}

Safe Function void SkipGet(Cursor cur) {
    While (!IsEnd(cur) && CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.Semicolon)) {
        If (CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) {
            SkipBalanced(cur, [int](SelfHost.Frontend.TokenType.TokenType.LBrace), [int](SelfHost.Frontend.TokenType.TokenType.RBrace));
            Continue;
        }
        Advance(cur);
    }
    Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon));
}

Safe Function string BuildType(Cursor cur, int start, int endExclusive) {
    string out = "";
    int i = start;
    While (i < endExclusive) {
        out = out + TextAt(cur, i);
        i = i + 1;
    }
    Return out;
}

Safe Function string BuildTypeSkip(Cursor cur, int start, int endExclusive, int skipIndex) {
    string out = "";
    int i = start;
    While (i < endExclusive) {
        If (i != skipIndex) {
            out = out + TextAt(cur, i);
        }
        i = i + 1;
    }
    Return out;
}

Safe Function string NormalizeTypeName(string typeName) {
    If (IO.Text.StartsWith(typeName, "IO.Type.")) {
        int n = IO.Text.Length(typeName);
        Return IO.Text.Slice(typeName, 8, n - 8);
    }
    Return typeName;
}

Safe Function bool IsVoidType(string typeName) {
    Return NormalizeTypeName(typeName) == "void";
}

Safe Function bool IsIntType(string typeName) {
    Return NormalizeTypeName(typeName) == "int";
}

Safe Function bool IsStringArrayType(string typeName) {
    Return NormalizeTypeName(typeName) == "string[]";
}

Safe Function void SkipExpressionToSemicolon(Cursor cur) {
    int paren = 0;
    int bracket = 0;
    int brace = 0;

    While (!IsEnd(cur)) {
        int kind = CurrentKind(cur);
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.Semicolon) && paren == 0 && bracket == 0 && brace == 0) {
            Advance(cur);
            Break;
        }
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.LParen)) paren = paren + 1;
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.RParen) && paren > 0) paren = paren - 1;
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.LBracket)) bracket = bracket + 1;
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.RBracket) && bracket > 0) bracket = bracket - 1;
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) brace = brace + 1;
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.RBrace)) {
            If (brace == 0) {
                Break;
            }
            brace = brace - 1;
        }
        Advance(cur);
    }
}

Safe Function bool ParseBlockGuaranteedReturn(Cursor cur, SourceFile source, IO.Type.list diagnostics) {
    If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.LBrace), "Expected Left Brace", "Statement block must start with '{'")) {
        Return false;
    }

    bool guaranteed = false;
    While (!IsEnd(cur) && CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.RBrace)) {
        bool stmtGuaranteed = ParseStatementGuaranteedReturn(cur, source, diagnostics);
        If (stmtGuaranteed) {
            guaranteed = true;
        }
        If (SelfHost.Diag.HasAny(diagnostics)) {
            Return false;
        }
    }

    If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.RBrace), "Expected Right Brace", "Statement block is not closed")) {
        Return false;
    }
    Return guaranteed;
}

Safe Function bool ParseIfTailGuaranteedReturn(Cursor cur, SourceFile source, IO.Type.list diagnostics) {
    If (CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.LParen)) {
        ReportToken(diagnostics, source, cur, "Expected Left Parenthesis", "If condition must start with '('");
        Return false;
    }
    SkipBalanced(cur, [int](SelfHost.Frontend.TokenType.TokenType.LParen), [int](SelfHost.Frontend.TokenType.TokenType.RParen));
    bool thenGuaranteed = ParseStatementGuaranteedReturn(cur, source, diagnostics);
    If (SelfHost.Diag.HasAny(diagnostics)) {
        Return false;
    }

    bool hasElse = false;
    bool elseGuaranteed = false;
    If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwElse))) {
        hasElse = true;
        elseGuaranteed = ParseStatementGuaranteedReturn(cur, source, diagnostics);
    } Else If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwElseIf))) {
        hasElse = true;
        elseGuaranteed = ParseIfTailGuaranteedReturn(cur, source, diagnostics);
    }

    Return hasElse && thenGuaranteed && elseGuaranteed;
}

Safe Function bool ParseTryGuaranteedReturn(Cursor cur, SourceFile source, IO.Type.list diagnostics) {
    Advance(cur);

    bool tryGuaranteed = ParseStatementGuaranteedReturn(cur, source, diagnostics);
    If (SelfHost.Diag.HasAny(diagnostics)) {
        Return false;
    }

    int catchCount = 0;
    bool allCatchGuaranteed = true;
    While (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwCatch))) {
        catchCount = catchCount + 1;
        If (CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.LParen)) {
            SkipBalanced(cur, [int](SelfHost.Frontend.TokenType.TokenType.LParen), [int](SelfHost.Frontend.TokenType.TokenType.RParen));
        }
        bool catchGuaranteed = ParseStatementGuaranteedReturn(cur, source, diagnostics);
        If (!catchGuaranteed) {
            allCatchGuaranteed = false;
        }
        If (SelfHost.Diag.HasAny(diagnostics)) {
            Return false;
        }
    }

    Return tryGuaranteed && catchCount > 0 && allCatchGuaranteed;
}

Safe Function bool ParseStatementGuaranteedReturn(Cursor cur, SourceFile source, IO.Type.list diagnostics) {
    int kind = CurrentKind(cur);

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) {
        Return ParseBlockGuaranteedReturn(cur, source, diagnostics);
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwIf)) {
        Advance(cur);
        Return ParseIfTailGuaranteedReturn(cur, source, diagnostics);
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwElseIf)) {
        Advance(cur);
        Return ParseIfTailGuaranteedReturn(cur, source, diagnostics);
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwTry)) {
        Return ParseTryGuaranteedReturn(cur, source, diagnostics);
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwWhile) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwFor)) {
        Advance(cur);
        If (CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.LParen)) {
            SkipBalanced(cur, [int](SelfHost.Frontend.TokenType.TokenType.LParen), [int](SelfHost.Frontend.TokenType.TokenType.RParen));
        }
        ParseStatementGuaranteedReturn(cur, source, diagnostics);
        Return false;
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwReturn)) {
        Advance(cur);
        SkipExpressionToSemicolon(cur);
        Return true;
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwThrow)) {
        Advance(cur);
        SkipExpressionToSemicolon(cur);
        Return true;
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwBreak) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwContinue)) {
        Advance(cur);
        Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon));
        Return false;
    }

    SkipExpressionToSemicolon(cur);
    Return false;
}

Safe Function bool ParseFunctionParamTypes(Cursor cur, SourceFile source, IO.Type.list diagnostics, IO.Type.list outParamTypes) {
    If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.LParen), "Expected Left Parenthesis", "Function parameter list must start with '('")) {
        Return false;
    }

    If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.RParen))) {
        Return true;
    }

    While (!IsEnd(cur)) {
        int segStart = cur.Pos;
        int namePos = -1;

        While (!IsEnd(cur) && CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.Comma) && CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.RParen)) {
            If (CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.Identifier)) {
                namePos = cur.Pos;
            }
            Advance(cur);
        }

        If (namePos < segStart) {
            ReportToken(diagnostics, source, cur, "Invalid Parameter", "Cannot parse parameter name");
            Return false;
        }

        string typeName = BuildTypeSkip(cur, segStart, cur.Pos, namePos);
        typeName = NormalizeTypeName(typeName);
        outParamTypes.Add(typeName);

        If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.Comma))) {
            Continue;
        }
        If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.RParen))) {
            Return true;
        }

        ReportToken(diagnostics, source, cur, "Expected Delimiter", "Parameter list requires ',' or ')' ");
        Return false;
    }

    ReportToken(diagnostics, source, cur, "Expected Right Parenthesis", "Function parameter list is not closed");
    Return false;
}

Safe Function bool ParseFunctionDecl(Cursor cur, SourceFile source, IO.Type.list diagnostics, bool isStatic, bool isExtern, IO.Type.list functionNames, int[] mainCountRef) {
    int functionTokLine = CurrentLine(cur);
    int functionTokCol = CurrentCol(cur);
    Advance(cur);

    int namePos = -1;
    int scan = cur.Pos;
    While (scan + 1 < TokenCount(cur)) {
        If (KindAt(cur, scan) == [int](SelfHost.Frontend.TokenType.TokenType.Identifier) && KindAt(cur, scan + 1) == [int](SelfHost.Frontend.TokenType.TokenType.LParen)) {
            namePos = scan;
            Break;
        }
        int k = KindAt(cur, scan);
        If (k == [int](SelfHost.Frontend.TokenType.TokenType.Semicolon) || k == [int](SelfHost.Frontend.TokenType.TokenType.LBrace) || k == [int](SelfHost.Frontend.TokenType.TokenType.Eof)) {
            Break;
        }
        scan = scan + 1;
    }

    If (namePos < 0) {
        SelfHost.Diag.Add(diagnostics, source, "Sema", "Expected Function Name", "Cannot find function identifier before parameter list", "", functionTokLine, functionTokCol, 8);
        Return false;
    }

    string retType = BuildType(cur, cur.Pos, namePos);
    retType = NormalizeTypeName(retType);
    string fnName = TextAt(cur, namePos);
    int fnLine = LineAt(cur, namePos);
    int fnCol = ColAt(cur, namePos);

    int dupIndex = 0;
    While (dupIndex < functionNames.Count()) {
        string seen = [string](functionNames.Fetch(dupIndex));
        If (seen == fnName) {
            SelfHost.Diag.Add(diagnostics, source, "Sema", "Duplicate Function", "Function already defined", fnName, fnLine, fnCol, IO.Text.Length(fnName));
            Return false;
        }
        dupIndex = dupIndex + 1;
    }
    functionNames.Add(fnName);

    cur.Pos = namePos;
    If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Function Name", "Function name is missing")) {
        Return false;
    }

    IO.Type.list paramTypes = IO.Type.list.Create();
    If (!ParseFunctionParamTypes(cur, source, diagnostics, paramTypes)) {
        Return false;
    }

    If (isExtern) {
        If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwBy))) {
            string[] targetRef = { "" };
            If (!ParseQualifiedNameText(cur, source, diagnostics, targetRef)) {
                Return false;
            }
        }
        If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon), "Expected Semicolon", "Extern function declaration must end with ';'")) {
            Return false;
        }
        Return true;
    }

    If (fnName == "Main") {
        int oldCount = mainCountRef.Index(0);
        mainCountRef[0] = oldCount + 1;

        If (!isStatic) {
            SelfHost.Diag.Add(diagnostics, source, "Sema", "Entry Signature", "Main must be Static", fnName, fnLine, fnCol, IO.Text.Length(fnName));
        }
        If (!IsIntType(retType)) {
            SelfHost.Diag.Add(diagnostics, source, "Sema", "Entry Signature", "Main must return int", fnName, fnLine, fnCol, IO.Text.Length(fnName));
        }

        int pc = paramTypes.Count();
        If (pc != 0 && pc != 1) {
            SelfHost.Diag.Add(diagnostics, source, "Sema", "Entry Signature", "Main must take zero or one parameter", fnName, fnLine, fnCol, IO.Text.Length(fnName));
        } Else If (pc == 1) {
            string p0 = [string](paramTypes.Fetch(0));
            If (!IsStringArrayType(p0)) {
                SelfHost.Diag.Add(diagnostics, source, "Sema", "Entry Signature", "Main parameter must be[] string", p0, fnLine, fnCol, IO.Text.Length(fnName));
            }
        }
    }

    If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon))) {
        Return true;
    }

    bool guaranteed = ParseBlockGuaranteedReturn(cur, source, diagnostics);
    If (SelfHost.Diag.HasAny(diagnostics)) {
        Return false;
    }

    If (!IsVoidType(retType) && !guaranteed) {
        SelfHost.Diag.Add(
            diagnostics,
            source,
            "Sema",
            "Missing Return",
            "Non-void function requires a guaranteed return path",
            fnName,
            fnLine,
            fnCol,
            IO.Text.Length(fnName)
        );
        Return false;
    }

    Return true;
}

Safe Function bool SkipTopLevelTypeDecl(Cursor cur, IO.Type.list diagnostics, SourceFile source) {
    While (!IsEnd(cur) && CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.LBrace) && CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.Semicolon)) {
        Advance(cur);
    }

    If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon))) {
        Return true;
    }

    If (CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) {
        ReportToken(diagnostics, source, cur, "Expected Left Brace", "Type declaration body must start with '{'");
        Return false;
    }
    SkipBalanced(cur, [int](SelfHost.Frontend.TokenType.TokenType.LBrace), [int](SelfHost.Frontend.TokenType.TokenType.RBrace));
    Return true;
}

Safe Function bool Run(SourceFile source, IO.Type.list tokens, IO.Type.list diagnostics) {
    Cursor cur = New Cursor(tokens);
    IO.Type.list functionNames = IO.Type.list.Create();
    int[] mainCountRef = { 0 };

    If (CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.KwUnit)) {
        Advance(cur);
        While (!IsEnd(cur) && CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.Semicolon)) {
            Advance(cur);
        }
        If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon), "Expected Semicolon", "Missing ';' after Unit declaration")) {
            Return false;
        }
    }

    IO.Type.list scopedSymbols = IO.Type.list.Create();
    While (CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.KwGet)) {
        If (!ParseGetForSema(cur, source, diagnostics, scopedSymbols)) {
            Return false;
        }
    }

    Cursor scan = New Cursor(tokens);
    If (!CheckScopedSymbolCalls(scan, source, diagnostics, scopedSymbols)) {
        Return false;
    }

    While (!IsEnd(cur)) {
        bool isStatic = false;
        bool isExtern = false;
        int kind = CurrentKind(cur);

        While (IsFunctionModifier(kind)) {
            If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwStatic)) {
                isStatic = true;
            } Else If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwExtern)) {
                isExtern = true;
            }
            Advance(cur);
            kind = CurrentKind(cur);
        }

        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwFunction)) {
            If (!ParseFunctionDecl(cur, source, diagnostics, isStatic, isExtern, functionNames, mainCountRef)) {
                Return false;
            }
            Continue;
        }

        If (IsTypeDeclLead(kind)) {
            If (!SkipTopLevelTypeDecl(cur, diagnostics, source)) {
                Return false;
            }
            Continue;
        }

        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.Eof)) {
            Break;
        }

        // Conservative forward progress on unknown top-level tokens.
        Advance(cur);
    }

    int mainCount = mainCountRef.Index(0);
    If (mainCount == 0) {
        SelfHost.Diag.Add(diagnostics, source, "Sema", "Missing Entry", "Main function not found", "", 1, 1, 1);
        Return false;
    }
    If (mainCount > 1) {
        SelfHost.Diag.Add(diagnostics, source, "Sema", "Entry Ambiguous", "Multiple Main functions found", "", 1, 1, 1);
        Return false;
    }

    Return true;
}
