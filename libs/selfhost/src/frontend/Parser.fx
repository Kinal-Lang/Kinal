Unit SelfHost.Parser;

Get IO.Text;
Get IO.Type.list;
Get SelfHost.Source;
Get SelfHost.Diag;
Get SelfHost.Frontend.Token;
Get SelfHost.Frontend.TokenType;

Class ParseCursor {
    Public IO.Type.list Tokens;
    Public int Pos;

    Public Safe Constructor(IO.Type.list tokens) {
        This.Tokens = tokens;
        This.Pos = 0;
    }
}

Class ParseState {
    Public Safe Constructor() {
        // syntax-only parser state reserved for future grammar switches
    }
}

Safe Function int TokenCount(ParseCursor cur) {
    Return cur.Tokens.Count();
}

Safe Function string RawTokenAt(ParseCursor cur, int index) {
    If (index < 0 || index >= TokenCount(cur)) {
        Return "";
    }
    Return [string](cur.Tokens.Fetch(index));
}

Safe Function int KindAt(ParseCursor cur, int index) {
    string raw = RawTokenAt(cur, index);
    If (IO.Text.IsEmpty(raw)) {
        Return [int](SelfHost.Frontend.TokenType.TokenType.Eof);
    }
    Return SelfHost.Frontend.Token.Kind(raw);
}

Safe Function string TextAt(ParseCursor cur, int index) {
    string raw = RawTokenAt(cur, index);
    If (IO.Text.IsEmpty(raw)) {
        Return "";
    }
    Return SelfHost.Frontend.Token.Text(raw);
}

Safe Function int LineAt(ParseCursor cur, int index) {
    string raw = RawTokenAt(cur, index);
    If (IO.Text.IsEmpty(raw)) {
        Return 1;
    }
    Return SelfHost.Frontend.Token.Line(raw);
}

Safe Function int ColAt(ParseCursor cur, int index) {
    string raw = RawTokenAt(cur, index);
    If (IO.Text.IsEmpty(raw)) {
        Return 1;
    }
    Return SelfHost.Frontend.Token.Col(raw);
}

Safe Function int CurrentKind(ParseCursor cur) {
    Return KindAt(cur, cur.Pos);
}

Safe Function string CurrentText(ParseCursor cur) {
    Return TextAt(cur, cur.Pos);
}

Safe Function int CurrentLine(ParseCursor cur) {
    Return LineAt(cur, cur.Pos);
}

Safe Function int CurrentCol(ParseCursor cur) {
    Return ColAt(cur, cur.Pos);
}

Safe Function bool IsEnd(ParseCursor cur) {
    Return CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.Eof);
}

Safe Function void Advance(ParseCursor cur) {
    If (cur.Pos < TokenCount(cur)) {
        cur.Pos = cur.Pos + 1;
    }
}

Safe Function int TokenSpan(ParseCursor cur) {
    string text = CurrentText(cur);
    int len = IO.Text.Length(text);
    If (len < 1) {
        Return 1;
    }
    Return len;
}

Safe Function void ReportToken(IO.Type.list diagnostics, SourceFile source, ParseCursor cur, string title, string detail) {
    SelfHost.Diag.Add(
        diagnostics,
        source,
        "Parser",
        title,
        detail,
        CurrentText(cur),
        CurrentLine(cur),
        CurrentCol(cur),
        TokenSpan(cur)
    );
}

Safe Function bool Match(ParseCursor cur, int kind) {
    If (CurrentKind(cur) == kind) {
        Advance(cur);
        Return true;
    }
    Return false;
}

Safe Function bool Expect(ParseCursor cur, IO.Type.list diagnostics, SourceFile source, int kind, string title, string detail) {
    If (CurrentKind(cur) == kind) {
        Advance(cur);
        Return true;
    }
    ReportToken(diagnostics, source, cur, title, detail);
    Return false;
}

Safe Function bool ParseQualifiedName(ParseCursor cur, IO.Type.list diagnostics, SourceFile source) {
    If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Qualified name must start with identifier")) {
        Return false;
    }
    While (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.Dot))) {
        If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Missing identifier after '.'")) {
            Return false;
        }
    }
    Return true;
}

Safe Function bool ParseGetDecl(ParseCursor cur, IO.Type.list diagnostics, SourceFile source) {
    Advance(cur);

    If (!ParseQualifiedName(cur, diagnostics, source)) {
        Return false;
    }

    If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.LBrace))) {
        If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Get selector list cannot be empty")) {
            Return false;
        }
        If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwBy))) {
            If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Alias target is missing")) {
                Return false;
            }
        }
        While (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.Comma))) {
            If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Missing selector after ','")) {
                Return false;
            }
            If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwBy))) {
                If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Identifier), "Expected Identifier", "Alias target is missing")) {
                    Return false;
                }
            }
        }
        If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.RBrace), "Expected Right Brace", "Get selector list must end with '}'")) {
            Return false;
        }
    }

    If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwBy))) {
        If (!ParseQualifiedName(cur, diagnostics, source)) {
            Return false;
        }
    }

    If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon), "Expected Semicolon", "Missing ';' after Get declaration")) {
        Return false;
    }
    Return true;
}

Safe Function bool SkipBalancedParens(ParseCursor cur, IO.Type.list diagnostics, SourceFile source) {
    If (CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.LParen)) {
        ReportToken(diagnostics, source, cur, "Expected Left Parenthesis", "Expected '('");
        Return false;
    }

    int depth = 1;
    Advance(cur);
    While (!IsEnd(cur) && depth > 0) {
        If (CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.LParen)) {
            depth = depth + 1;
        } Else If (CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.RParen)) {
            depth = depth - 1;
        }
        Advance(cur);
    }
    If (depth != 0) {
        SelfHost.Diag.Add(diagnostics, source, "Parser", "Expected Right Parenthesis", "Function parameter list is not closed", "", CurrentLine(cur), CurrentCol(cur), 1);
        Return false;
    }
    Return true;
}

Safe Function bool SkipBalancedBraces(ParseCursor cur, IO.Type.list diagnostics, SourceFile source, bool checkGetInBody) {
    If (CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) {
        ReportToken(diagnostics, source, cur, "Expected Left Brace", "Expected '{'");
        Return false;
    }

    int depth = 1;
    Advance(cur);
    While (!IsEnd(cur) && depth > 0) {
        int kind = CurrentKind(cur);
        If (checkGetInBody && kind == [int](SelfHost.Frontend.TokenType.TokenType.KwGet)) {
            SelfHost.Diag.Add(
                diagnostics,
                source,
                "Parser",
                "Invalid Get Position",
                "Get can only appear at top-level after Unit declaration",
                CurrentText(cur),
                CurrentLine(cur),
                CurrentCol(cur),
                TokenSpan(cur)
            );
        }
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) {
            depth = depth + 1;
        } Else If (kind == [int](SelfHost.Frontend.TokenType.TokenType.RBrace)) {
            depth = depth - 1;
        }
        Advance(cur);
    }
    If (depth != 0) {
        SelfHost.Diag.Add(diagnostics, source, "Parser", "Expected Right Brace", "Declaration body is not closed", "", CurrentLine(cur), CurrentCol(cur), 1);
        Return false;
    }
    Return true;
}

Safe Function bool IsFunctionModifier(int kind) {
    Return kind == [int](SelfHost.Frontend.TokenType.TokenType.KwStatic) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwSafe) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwTrusted) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwUnsafe) ||
           kind == [int](SelfHost.Frontend.TokenType.TokenType.KwExtern);
}

Safe Function bool SkipExpressionToSemicolon(ParseCursor cur, IO.Type.list diagnostics, SourceFile source) {
    int paren = 0;
    int bracket = 0;
    int brace = 0;

    While (!IsEnd(cur)) {
        int kind = CurrentKind(cur);
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.Semicolon) && paren == 0 && bracket == 0 && brace == 0) {
            Advance(cur);
            Return true;
        }
        If (paren == 0 && bracket == 0 && brace == 0 &&
            (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwReturn) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwIf) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwElse) ||
             kind == [int](SelfHost.Frontend.TokenType.TokenType.KwElseIf) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwWhile) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwFor) ||
             kind == [int](SelfHost.Frontend.TokenType.TokenType.KwTry) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwCatch) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwThrow) ||
             kind == [int](SelfHost.Frontend.TokenType.TokenType.KwBreak) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwContinue) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwGet))) {
            ReportToken(diagnostics, source, cur, "Expected Semicolon", "Missing ';' after statement");
            Return false;
        }
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.RBrace) && paren == 0 && bracket == 0 && brace == 0) {
            Break;
        }
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.LParen)) paren = paren + 1;
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.RParen) && paren > 0) paren = paren - 1;
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.LBracket)) bracket = bracket + 1;
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.RBracket) && bracket > 0) bracket = bracket - 1;
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) brace = brace + 1;
        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.RBrace) && brace > 0) brace = brace - 1;
        Advance(cur);
    }

    ReportToken(diagnostics, source, cur, "Expected Semicolon", "Missing ';' after statement");
    Return false;
}

Safe Function bool ParseBlock(ParseCursor cur, IO.Type.list diagnostics, SourceFile source) {
    If (CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) {
        ReportToken(diagnostics, source, cur, "Expected Left Brace", "Block statement must start with '{'");
        Return false;
    }

    Advance(cur);
    While (!IsEnd(cur) && CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.RBrace)) {
        If (!ParseStatement(cur, diagnostics, source)) {
            Return false;
        }
    }

    If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.RBrace), "Expected Right Brace", "Block is not closed")) {
        Return false;
    }
    Return true;
}

Safe Function bool ParseIfTail(ParseCursor cur, IO.Type.list diagnostics, SourceFile source) {
    If (!SkipBalancedParens(cur, diagnostics, source)) {
        Return false;
    }

    If (!ParseStatement(cur, diagnostics, source)) {
        Return false;
    }

    If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwElse))) {
        Return ParseStatement(cur, diagnostics, source);
    }

    If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwElseIf))) {
        Return ParseIfTail(cur, diagnostics, source);
    }

    Return true;
}

Safe Function bool ParseStatement(ParseCursor cur, IO.Type.list diagnostics, SourceFile source) {
    int kind = CurrentKind(cur);

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwGet)) {
        SelfHost.Diag.Add(
            diagnostics,
            source,
            "Parser",
            "Invalid Get Position",
            "Get can only appear at top-level after Unit declaration",
            CurrentText(cur),
            CurrentLine(cur),
            CurrentCol(cur),
            TokenSpan(cur)
        );
        Return false;
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) {
        Return ParseBlock(cur, diagnostics, source);
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwIf)) {
        Advance(cur);
        Return ParseIfTail(cur, diagnostics, source);
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwElseIf)) {
        Advance(cur);
        Return ParseIfTail(cur, diagnostics, source);
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwElse)) {
        Advance(cur);
        If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwIf))) {
            Return ParseIfTail(cur, diagnostics, source);
        }
        Return ParseStatement(cur, diagnostics, source);
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwWhile) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwFor)) {
        Advance(cur);
        If (!SkipBalancedParens(cur, diagnostics, source)) {
            Return false;
        }
        Return ParseStatement(cur, diagnostics, source);
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwTry)) {
        Advance(cur);
        If (!ParseStatement(cur, diagnostics, source)) {
            Return false;
        }
        If (!Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwCatch))) {
            ReportToken(diagnostics, source, cur, "Expected Catch", "Try statement requires at least one Catch");
            Return false;
        }
        While (true) {
            If (CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.LParen)) {
                If (!SkipBalancedParens(cur, diagnostics, source)) {
                    Return false;
                }
            }
            If (!ParseStatement(cur, diagnostics, source)) {
                Return false;
            }
            If (!Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwCatch))) {
                Break;
            }
        }
        Return true;
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwThrow)) {
        Advance(cur);
        If (CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.Semicolon)) {
            Advance(cur);
            Return true;
        }
        Return SkipExpressionToSemicolon(cur, diagnostics, source);
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwReturn)) {
        Advance(cur);
        If (CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.Semicolon)) {
            Advance(cur);
            Return true;
        }
        Return SkipExpressionToSemicolon(cur, diagnostics, source);
    }

    If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwBreak) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwContinue)) {
        Advance(cur);
        Return Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon), "Expected Semicolon", "Missing ';' after control statement");
    }

    Return SkipExpressionToSemicolon(cur, diagnostics, source);
}

Safe Function bool ParseFunctionDecl(ParseCursor cur, IO.Type.list diagnostics, SourceFile source, ParseState state) {
    bool isExtern = false;
    While (IsFunctionModifier(CurrentKind(cur))) {
        If (CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.KwExtern)) {
            isExtern = true;
        }
        Advance(cur);
    }

    If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.KwFunction), "Expected Function", "Function declaration is missing 'Function' keyword")) {
        Return false;
    }

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
        ReportToken(diagnostics, source, cur, "Expected Function Name", "Cannot find function identifier before parameter list");
        Return false;
    }

    cur.Pos = namePos + 1;
    If (!SkipBalancedParens(cur, diagnostics, source)) {
        Return false;
    }

    If (isExtern) {
        If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.KwBy))) {
            If (!ParseQualifiedName(cur, diagnostics, source)) {
                Return false;
            }
        }
        Return Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon), "Expected Semicolon", "Extern function declaration must end with ';'");
    }

    If (Match(cur, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon))) {
        Return true;
    }

    If (CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) {
        ReportToken(diagnostics, source, cur, "Expected Left Brace", "Function body must start with '{'");
        Return false;
    }

    If (!ParseBlock(cur, diagnostics, source)) {
        Return false;
    }

    Return !SelfHost.Diag.HasAny(diagnostics);
}

Safe Function bool ParseTypeDecl(ParseCursor cur, IO.Type.list diagnostics, SourceFile source) {
    Advance(cur);
    While (!IsEnd(cur) && CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) {
        Advance(cur);
    }
    If (CurrentKind(cur) != [int](SelfHost.Frontend.TokenType.TokenType.LBrace)) {
        ReportToken(diagnostics, source, cur, "Expected Left Brace", "Type declaration body must start with '{'");
        Return false;
    }
    Return SkipBalancedBraces(cur, diagnostics, source, true);
}

Safe Function bool SkipAttributes(ParseCursor cur, IO.Type.list diagnostics, SourceFile source) {
    While (CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.LBracket)) {
        int depth = 1;
        Advance(cur);
        While (!IsEnd(cur) && depth > 0) {
            int kind = CurrentKind(cur);
            If (kind == [int](SelfHost.Frontend.TokenType.TokenType.LBracket)) {
                depth = depth + 1;
            } Else If (kind == [int](SelfHost.Frontend.TokenType.TokenType.RBracket)) {
                depth = depth - 1;
            }
            Advance(cur);
        }
        If (depth != 0) {
            SelfHost.Diag.Add(diagnostics, source, "Parser", "Expected Right Bracket", "Attribute list is not closed", "", CurrentLine(cur), CurrentCol(cur), 1);
            Return false;
        }
    }
    Return true;
}

Safe Function bool Run(SourceFile source, IO.Type.list tokens, IO.Type.list diagnostics) {
    ParseCursor cur = New ParseCursor(tokens);
    ParseState state = New ParseState();

    If (CurrentKind(cur) == [int](SelfHost.Frontend.TokenType.TokenType.KwUnit)) {
        Advance(cur);
        If (!ParseQualifiedName(cur, diagnostics, source)) {
            Return false;
        }
        If (!Expect(cur, diagnostics, source, [int](SelfHost.Frontend.TokenType.TokenType.Semicolon), "Expected Semicolon", "Missing ';' after Unit declaration")) {
            Return false;
        }
    }

    While (!IsEnd(cur)) {
        If (!SkipAttributes(cur, diagnostics, source)) {
            Return false;
        }

        int kind = CurrentKind(cur);

        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwGet)) {
            If (!ParseGetDecl(cur, diagnostics, source)) {
                Return false;
            }
            Continue;
        }

        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwFunction) || IsFunctionModifier(kind)) {
            If (!ParseFunctionDecl(cur, diagnostics, source, state)) {
                Return false;
            }
            Continue;
        }

        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.KwClass) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwInterface) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwStruct) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwEnum) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwAbstract) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwSealed) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwPublic) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwPrivate) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwProtected) || kind == [int](SelfHost.Frontend.TokenType.TokenType.KwInternal)) {
            If (!ParseTypeDecl(cur, diagnostics, source)) {
                Return false;
            }
            Continue;
        }

        If (kind == [int](SelfHost.Frontend.TokenType.TokenType.Eof)) {
            Break;
        }

        ReportToken(diagnostics, source, cur, "Unexpected Top-Level Token", "Token is not a valid top-level declaration");
        Return false;
    }

    Return true;
}
