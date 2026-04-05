Unit SelfHost.Frontend.TokenType;

Enum TokenType By int {
    Eof = 0,
    Identifier = 1,
    Number = 2,
    String = 3,

    KwUnit = 10,
    KwGet = 11,
    KwBy = 12,
    KwFunction = 13,
    KwReturn = 14,
    KwIf = 15,
    KwElse = 16,
    KwElseIf = 17,
    KwWhile = 18,
    KwFor = 19,
    KwBreak = 20,
    KwContinue = 21,
    KwTry = 22,
    KwCatch = 23,
    KwThrow = 24,
    KwConst = 25,
    KwVar = 26,
    KwSafe = 27,
    KwTrusted = 28,
    KwUnsafe = 29,
    KwExtern = 30,
    KwClass = 31,
    KwInterface = 32,
    KwStruct = 33,
    KwEnum = 34,
    KwPublic = 35,
    KwPrivate = 36,
    KwProtected = 37,
    KwInternal = 38,
    KwStatic = 39,
    KwVirtual = 40,
    KwOverride = 41,
    KwAbstract = 42,
    KwSealed = 43,
    KwConstructor = 44,
    KwNew = 45,
    KwThis = 46,
    KwBase = 47,
    KwTrue = 48,
    KwFalse = 49,

    LParen = 100,
    RParen = 101,
    LBrace = 102,
    RBrace = 103,
    LBracket = 104,
    RBracket = 105,
    Comma = 106,
    Semicolon = 107,
    Dot = 108,

    Plus = 120,
    Minus = 121,
    Star = 122,
    Slash = 123,
    Percent = 124,
    Assign = 125,

    Eq = 130,
    Ne = 131,
    Lt = 132,
    Le = 133,
    Gt = 134,
    Ge = 135,
    AndAnd = 136,
    OrOr = 137,
    Not = 138,
    Amp = 139,
    BitOr = 140,
    Xor = 141,
    Shl = 142,
    Shr = 143,
    Tilde = 144,

    Unknown = 999
}

Safe Function bool IsBuiltinTypeName(string id) {
    Return id == "void" || id == "bool" || id == "byte" || id == "char" || id == "int" || id == "float" ||
           id == "i8" || id == "i16" || id == "i32" || id == "i64" ||
           id == "u8" || id == "u16" || id == "u32" || id == "u64" ||
           id == "isize" || id == "usize" || id == "string" || id == "any";
}

Safe Function string Name(int kind) {
    If (kind == [int](TokenType.Eof)) Return "TOK_EOF";
    If (kind == [int](TokenType.Identifier)) Return "TOK_ID";
    If (kind == [int](TokenType.Number)) Return "TOK_NUMBER";
    If (kind == [int](TokenType.String)) Return "TOK_STRING";

    If (kind == [int](TokenType.KwUnit)) Return "TOK_UNIT";
    If (kind == [int](TokenType.KwGet)) Return "TOK_GET";
    If (kind == [int](TokenType.KwBy)) Return "TOK_BY";
    If (kind == [int](TokenType.KwFunction)) Return "TOK_FUNCTION";
    If (kind == [int](TokenType.KwReturn)) Return "TOK_RETURN";
    If (kind == [int](TokenType.KwIf)) Return "TOK_IF";
    If (kind == [int](TokenType.KwElse)) Return "TOK_ELSE";
    If (kind == [int](TokenType.KwElseIf)) Return "TOK_ELSEIF";
    If (kind == [int](TokenType.KwWhile)) Return "TOK_WHILE";
    If (kind == [int](TokenType.KwFor)) Return "TOK_FOR";
    If (kind == [int](TokenType.KwBreak)) Return "TOK_BREAK";
    If (kind == [int](TokenType.KwContinue)) Return "TOK_CONTINUE";
    If (kind == [int](TokenType.KwTry)) Return "TOK_TRY";
    If (kind == [int](TokenType.KwCatch)) Return "TOK_CATCH";
    If (kind == [int](TokenType.KwThrow)) Return "TOK_THROW";
    If (kind == [int](TokenType.KwConst)) Return "TOK_CONST";
    If (kind == [int](TokenType.KwVar)) Return "TOK_VAR";
    If (kind == [int](TokenType.KwSafe)) Return "TOK_SAFE";
    If (kind == [int](TokenType.KwTrusted)) Return "TOK_TRUSTED";
    If (kind == [int](TokenType.KwUnsafe)) Return "TOK_UNSAFE";
    If (kind == [int](TokenType.KwExtern)) Return "TOK_EXTERN";
    If (kind == [int](TokenType.KwClass)) Return "TOK_CLASS";
    If (kind == [int](TokenType.KwInterface)) Return "TOK_INTERFACE";
    If (kind == [int](TokenType.KwStruct)) Return "TOK_STRUCT";
    If (kind == [int](TokenType.KwEnum)) Return "TOK_ENUM";
    If (kind == [int](TokenType.KwPublic)) Return "TOK_PUBLIC";
    If (kind == [int](TokenType.KwPrivate)) Return "TOK_PRIVATE";
    If (kind == [int](TokenType.KwProtected)) Return "TOK_PROTECTED";
    If (kind == [int](TokenType.KwInternal)) Return "TOK_INTERNAL";
    If (kind == [int](TokenType.KwStatic)) Return "TOK_STATIC";
    If (kind == [int](TokenType.KwVirtual)) Return "TOK_VIRTUAL";
    If (kind == [int](TokenType.KwOverride)) Return "TOK_OVERRIDE";
    If (kind == [int](TokenType.KwAbstract)) Return "TOK_ABSTRACT";
    If (kind == [int](TokenType.KwSealed)) Return "TOK_SEALED";
    If (kind == [int](TokenType.KwConstructor)) Return "TOK_CONSTRUCTOR";
    If (kind == [int](TokenType.KwNew)) Return "TOK_NEW";
    If (kind == [int](TokenType.KwThis)) Return "TOK_THIS";
    If (kind == [int](TokenType.KwBase)) Return "TOK_BASE";
    If (kind == [int](TokenType.KwTrue)) Return "TOK_TRUE";
    If (kind == [int](TokenType.KwFalse)) Return "TOK_FALSE";

    If (kind == [int](TokenType.LParen)) Return "TOK_LPAREN";
    If (kind == [int](TokenType.RParen)) Return "TOK_RPAREN";
    If (kind == [int](TokenType.LBrace)) Return "TOK_LBRACE";
    If (kind == [int](TokenType.RBrace)) Return "TOK_RBRACE";
    If (kind == [int](TokenType.LBracket)) Return "TOK_LBRACKET";
    If (kind == [int](TokenType.RBracket)) Return "TOK_RBRACKET";
    If (kind == [int](TokenType.Comma)) Return "TOK_COMMA";
    If (kind == [int](TokenType.Semicolon)) Return "TOK_SEMI";
    If (kind == [int](TokenType.Dot)) Return "TOK_DOT";

    If (kind == [int](TokenType.Plus)) Return "TOK_PLUS";
    If (kind == [int](TokenType.Minus)) Return "TOK_MINUS";
    If (kind == [int](TokenType.Star)) Return "TOK_STAR";
    If (kind == [int](TokenType.Slash)) Return "TOK_SLASH";
    If (kind == [int](TokenType.Percent)) Return "TOK_PERCENT";
    If (kind == [int](TokenType.Assign)) Return "TOK_ASSIGN";

    If (kind == [int](TokenType.Eq)) Return "TOK_EQ";
    If (kind == [int](TokenType.Ne)) Return "TOK_NE";
    If (kind == [int](TokenType.Lt)) Return "TOK_LT";
    If (kind == [int](TokenType.Le)) Return "TOK_LE";
    If (kind == [int](TokenType.Gt)) Return "TOK_GT";
    If (kind == [int](TokenType.Ge)) Return "TOK_GE";
    If (kind == [int](TokenType.AndAnd)) Return "TOK_ANDAND";
    If (kind == [int](TokenType.OrOr)) Return "TOK_OROR";
    If (kind == [int](TokenType.Not)) Return "TOK_NOT";
    If (kind == [int](TokenType.Amp)) Return "TOK_AMP";
    If (kind == [int](TokenType.BitOr)) Return "TOK_BITOR";
    If (kind == [int](TokenType.Xor)) Return "TOK_XOR";
    If (kind == [int](TokenType.Shl)) Return "TOK_SHL";
    If (kind == [int](TokenType.Shr)) Return "TOK_SHR";
    If (kind == [int](TokenType.Tilde)) Return "TOK_TILDE";

    Return "TOK_UNKNOWN";
}
