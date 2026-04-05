Unit SelfHost.Lexer;

Get IO.Text;
Get IO.Type.list;
Get SelfHost.Source;
Get SelfHost.Diag;
Get SelfHost.Frontend.Token;
Get SelfHost.Frontend.TokenType;

Class LexerState {
    Public SourceFile Source;
    Public char[] Chars;
    Public int Length;
    Public int Pos;
    Public int Line;
    Public int Col;

    Public Safe Constructor(SourceFile source) {
        This.Source = source;
        This.Chars = source.Text.ToChars();
        This.Length = This.Chars.Length();
        This.Pos = 0;
        This.Line = 1;
        This.Col = 1;
    }
}

Safe Function char Current(LexerState st) {
    If (st.Pos < 0 || st.Pos >= st.Length) {
        Return [char](0);
    }
    Return st.Chars.Index(st.Pos);
}

Safe Function char Peek(LexerState st) {
    int at = st.Pos + 1;
    If (at < 0 || at >= st.Length) {
        Return [char](0);
    }
    Return st.Chars.Index(at);
}

Safe Function void Advance(LexerState st) {
    char c = Current(st);
    If ([int](c) == 0) {
        Return;
    }

    If ([int](c) == 10) {
        st.Line = st.Line + 1;
        st.Col = 1;
    } Else {
        st.Col = st.Col + 1;
    }
    st.Pos = st.Pos + 1;
}

Safe Function void AdvanceMany(LexerState st, int count) {
    int i = 0;
    While (i < count) {
        Advance(st);
        i = i + 1;
    }
}

Safe Function bool IsAlpha(char c) {
    int ci = [int](c);
    Return (ci >= 97 && ci <= 122) || (ci >= 65 && ci <= 90) || ci == 95;
}

Safe Function bool IsDigit(char c) {
    int ci = [int](c);
    Return ci >= 48 && ci <= 57;
}

Safe Function bool IsHexDigit(char c) {
    int ci = [int](c);
    Return IsDigit(c) || (ci >= 97 && ci <= 102) || (ci >= 65 && ci <= 70);
}

Safe Function bool IsWhitespace(char c) {
    int ci = [int](c);
    Return ci == 32 || ci == 9 || ci == 13 || ci == 10;
}

Safe Function bool IsStringEscapeChar(char c) {
    int ci = [int](c);
    Return ci == 48 || ci == 110 || ci == 114 || ci == 116 || ci == 92 || ci == 34 || ci == 39;
}

Safe Function int LookupKeyword(string id) {
    If (id == "Unit") Return [int](SelfHost.Frontend.TokenType.TokenType.KwUnit);
    If (id == "Get") Return [int](SelfHost.Frontend.TokenType.TokenType.KwGet);
    If (id == "By") Return [int](SelfHost.Frontend.TokenType.TokenType.KwBy);
    If (id == "Function") Return [int](SelfHost.Frontend.TokenType.TokenType.KwFunction);
    If (id == "Return") Return [int](SelfHost.Frontend.TokenType.TokenType.KwReturn);
    If (id == "If") Return [int](SelfHost.Frontend.TokenType.TokenType.KwIf);
    If (id == "Else") Return [int](SelfHost.Frontend.TokenType.TokenType.KwElse);
    If (id == "ElseIf") Return [int](SelfHost.Frontend.TokenType.TokenType.KwElseIf);
    If (id == "While") Return [int](SelfHost.Frontend.TokenType.TokenType.KwWhile);
    If (id == "For") Return [int](SelfHost.Frontend.TokenType.TokenType.KwFor);
    If (id == "Break") Return [int](SelfHost.Frontend.TokenType.TokenType.KwBreak);
    If (id == "Continue") Return [int](SelfHost.Frontend.TokenType.TokenType.KwContinue);
    If (id == "Try") Return [int](SelfHost.Frontend.TokenType.TokenType.KwTry);
    If (id == "Catch") Return [int](SelfHost.Frontend.TokenType.TokenType.KwCatch);
    If (id == "Throw") Return [int](SelfHost.Frontend.TokenType.TokenType.KwThrow);
    If (id == "Const") Return [int](SelfHost.Frontend.TokenType.TokenType.KwConst);
    If (id == "Var") Return [int](SelfHost.Frontend.TokenType.TokenType.KwVar);
    If (id == "Safe") Return [int](SelfHost.Frontend.TokenType.TokenType.KwSafe);
    If (id == "Trusted") Return [int](SelfHost.Frontend.TokenType.TokenType.KwTrusted);
    If (id == "Unsafe") Return [int](SelfHost.Frontend.TokenType.TokenType.KwUnsafe);
    If (id == "Extern") Return [int](SelfHost.Frontend.TokenType.TokenType.KwExtern);
    If (id == "Class") Return [int](SelfHost.Frontend.TokenType.TokenType.KwClass);
    If (id == "Interface") Return [int](SelfHost.Frontend.TokenType.TokenType.KwInterface);
    If (id == "Struct") Return [int](SelfHost.Frontend.TokenType.TokenType.KwStruct);
    If (id == "Enum") Return [int](SelfHost.Frontend.TokenType.TokenType.KwEnum);
    If (id == "Public") Return [int](SelfHost.Frontend.TokenType.TokenType.KwPublic);
    If (id == "Private") Return [int](SelfHost.Frontend.TokenType.TokenType.KwPrivate);
    If (id == "Protected") Return [int](SelfHost.Frontend.TokenType.TokenType.KwProtected);
    If (id == "Internal") Return [int](SelfHost.Frontend.TokenType.TokenType.KwInternal);
    If (id == "Static") Return [int](SelfHost.Frontend.TokenType.TokenType.KwStatic);
    If (id == "Virtual") Return [int](SelfHost.Frontend.TokenType.TokenType.KwVirtual);
    If (id == "Override") Return [int](SelfHost.Frontend.TokenType.TokenType.KwOverride);
    If (id == "Abstract") Return [int](SelfHost.Frontend.TokenType.TokenType.KwAbstract);
    If (id == "Sealed") Return [int](SelfHost.Frontend.TokenType.TokenType.KwSealed);
    If (id == "Constructor") Return [int](SelfHost.Frontend.TokenType.TokenType.KwConstructor);
    If (id == "New") Return [int](SelfHost.Frontend.TokenType.TokenType.KwNew);
    If (id == "This") Return [int](SelfHost.Frontend.TokenType.TokenType.KwThis);
    If (id == "Base") Return [int](SelfHost.Frontend.TokenType.TokenType.KwBase);
    If (id == "true") Return [int](SelfHost.Frontend.TokenType.TokenType.KwTrue);
    If (id == "false") Return [int](SelfHost.Frontend.TokenType.TokenType.KwFalse);

    // Builtin type names are identifiers, not dedicated token kinds.
    If (SelfHost.Frontend.TokenType.IsBuiltinTypeName(id)) {
        Return [int](SelfHost.Frontend.TokenType.TokenType.Identifier);
    }

    Return [int](SelfHost.Frontend.TokenType.TokenType.Identifier);
}

Safe Function void SkipWhitespaceAndComments(LexerState st) {
    While (true) {
        While (IsWhitespace(Current(st))) {
            Advance(st);
        }

        char c = Current(st);
        char n = Peek(st);
        int ci = [int](c);
        int ni = [int](n);

        If (ci == 47 && ni == 47) {
            AdvanceMany(st, 2);
            While ([int](Current(st)) != 0 && [int](Current(st)) != 10) {
                Advance(st);
            }
            Continue;
        }

        If (ci == 47 && ni == 42) {
            AdvanceMany(st, 2);
            While ([int](Current(st)) != 0) {
                If ([int](Current(st)) == 42 && [int](Peek(st)) == 47) {
                    AdvanceMany(st, 2);
                    Break;
                }
                Advance(st);
            }
            Continue;
        }

        Break;
    }
}

Safe Function string ReadIdentifier(LexerState st, int line, int col) {
    int start = st.Pos;
    While (IsAlpha(Current(st)) || IsDigit(Current(st))) {
        Advance(st);
    }
    string text = IO.Text.Slice(st.Source.Text, start, st.Pos - start);
    int kind = LookupKeyword(text);
    Return SelfHost.Frontend.Token.Encode(kind, text, line, col);
}

Safe Function string ReadNumber(LexerState st, int line, int col) {
    int start = st.Pos;

    If ([int](Current(st)) == 48 && ([int](Peek(st)) == 120 || [int](Peek(st)) == 88)) {
        AdvanceMany(st, 2);
        While (IsHexDigit(Current(st))) {
            Advance(st);
        }
    } Else {
        While (IsDigit(Current(st))) {
            Advance(st);
        }
        If ([int](Current(st)) == 46 && IsDigit(Peek(st))) {
            Advance(st);
            While (IsDigit(Current(st))) {
                Advance(st);
            }
        }
    }

    If ([int](Current(st)) == 102 || [int](Current(st)) == 70) {
        Advance(st);
    }

    string text = IO.Text.Slice(st.Source.Text, start, st.Pos - start);
    Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Number), text, line, col);
}

Safe Function string ReadString(LexerState st, IO.Type.list diagnostics, int line, int col) {
    int start = st.Pos;
    Advance(st);

    bool closed = false;
    While ([int](Current(st)) != 0) {
        int ci = [int](Current(st));
        If (ci == 34) {
            Advance(st);
            closed = true;
            Break;
        }
        If (ci == 92) {
            int escLine = st.Line;
            int escCol = st.Col;
            Advance(st);
            If ([int](Current(st)) != 0) {
                If (!IsStringEscapeChar(Current(st))) {
                    string bad = IO.Text.Slice(st.Source.Text, st.Pos - 1, 2);
                    SelfHost.Diag.Add(diagnostics, st.Source, "Lexer", "Invalid String Escape", "Unsupported escape sequence in string literal", bad, escLine, escCol, 2);
                }
                Advance(st);
            }
            Continue;
        }
        If (ci == 10) {
            Break;
        }
        Advance(st);
    }

    string text = IO.Text.Slice(st.Source.Text, start, st.Pos - start);
    If (!closed) {
        SelfHost.Diag.Add(diagnostics, st.Source, "Lexer", "Unterminated String", "String literal is not closed", text, line, col, 1);
    }
    Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.String), text, line, col);
}

Safe Function string ReadSymbol(LexerState st, IO.Type.list diagnostics, int line, int col) {
    char c = Current(st);
    char n = Peek(st);
    int ci = [int](c);
    int ni = [int](n);

    If (ci == 61 && ni == 61) { AdvanceMany(st, 2); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Eq), "==", line, col); }
    If (ci == 33 && ni == 61) { AdvanceMany(st, 2); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Ne), "!=", line, col); }
    If (ci == 60 && ni == 61) { AdvanceMany(st, 2); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Le), "<=", line, col); }
    If (ci == 62 && ni == 61) { AdvanceMany(st, 2); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Ge), ">=", line, col); }
    If (ci == 60 && ni == 60) { AdvanceMany(st, 2); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Shl), "<<", line, col); }
    If (ci == 62 && ni == 62) { AdvanceMany(st, 2); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Shr), ">>", line, col); }
    If (ci == 38 && ni == 38) { AdvanceMany(st, 2); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.AndAnd), "&&", line, col); }
    If (ci == 124 && ni == 124) { AdvanceMany(st, 2); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.OrOr), "||", line, col); }

    If (ci == 40) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.LParen), "(", line, col); }
    If (ci == 41) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.RParen), ")", line, col); }
    If (ci == 123) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.LBrace), "{", line, col); }
    If (ci == 125) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.RBrace), "}", line, col); }
    If (ci == 91) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.LBracket), "[", line, col); }
    If (ci == 93) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.RBracket), "]", line, col); }
    If (ci == 44) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Comma), ",", line, col); }
    If (ci == 59) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Semicolon), ";", line, col); }
    If (ci == 46) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Dot), ".", line, col); }
    If (ci == 43) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Plus), "+", line, col); }
    If (ci == 45) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Minus), "-", line, col); }
    If (ci == 42) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Star), "*", line, col); }
    If (ci == 47) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Slash), "/", line, col); }
    If (ci == 37) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Percent), "%", line, col); }
    If (ci == 61) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Assign), "=", line, col); }
    If (ci == 60) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Lt), "<", line, col); }
    If (ci == 62) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Gt), ">", line, col); }
    If (ci == 33) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Not), "!", line, col); }
    If (ci == 38) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Amp), "&", line, col); }
    If (ci == 124) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.BitOr), "|", line, col); }
    If (ci == 94) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Xor), "^", line, col); }
    If (ci == 126) { Advance(st); Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Tilde), "~", line, col); }

    string got = IO.Text.Slice(st.Source.Text, st.Pos, 1);
    SelfHost.Diag.Add(diagnostics, st.Source, "Lexer", "Unexpected Character", "Unsupported token in current selfhost stage", got, line, col, 1);
    Advance(st);
    Return SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Unknown), got, line, col);
}

Safe Function string TokenDumpLine(string token) {
    Return SelfHost.Frontend.Token.Dump(token);
}

Safe Function IO.Type.list Tokenize(SourceFile source, IO.Type.list diagnostics) {
    IO.Type.list tokens = IO.Type.list.Create();
    LexerState st = New LexerState(source);

    While (true) {
        SkipWhitespaceAndComments(st);

        int line = st.Line;
        int col = st.Col;
        char c = Current(st);

        If ([int](c) == 0) {
            tokens.Add(SelfHost.Frontend.Token.Encode([int](SelfHost.Frontend.TokenType.TokenType.Eof), "", line, col));
            Break;
        }

        If (IsAlpha(c)) {
            tokens.Add(ReadIdentifier(st, line, col));
            Continue;
        }
        If (IsDigit(c)) {
            tokens.Add(ReadNumber(st, line, col));
            Continue;
        }
        If ([int](c) == 34) {
            tokens.Add(ReadString(st, diagnostics, line, col));
            Continue;
        }

        tokens.Add(ReadSymbol(st, diagnostics, line, col));
    }

    Return tokens;
}
