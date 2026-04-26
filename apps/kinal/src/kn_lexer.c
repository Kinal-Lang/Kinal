#include "kn/lexer.h"

void lex_init(Lexer *l, const char *src)
{
    l->src = src ? src : "";
    l->len = kn_strlen(l->src);
    l->pos = 0;
    l->line = 1;
    l->col = 1;
    if (l->len >= 3 &&
        (unsigned char)l->src[0] == 0xEF &&
        (unsigned char)l->src[1] == 0xBB &&
        (unsigned char)l->src[2] == 0xBF)
    {
        l->pos = 3;
    }
    l->ch = l->pos < l->len ? l->src[l->pos] : 0;
}

static void lex_advance(Lexer *l)
{
    if (!l->ch) return;
    if (l->ch == '\n')
    {
        l->line++;
        l->col = 1;
    }
    else
    {
        l->col++;
    }
    l->pos++;
    l->ch = l->pos < l->len ? l->src[l->pos] : 0;
}

static char lex_peek(Lexer *l)
{
    if (l->pos + 1 < l->len) return l->src[l->pos + 1];
    return 0;
}

static bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_digit(char c)
{
    return (c >= '0' && c <= '9');
}

static bool is_hex_digit(char c)
{
    return is_digit(c) ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static bool has_closing_quote_before_eol(const Lexer *l)
{
    if (!l || l->ch != '"') return false;
    size_t i = l->pos + 1;
    int escaped = 0;
    while (i < l->len)
    {
        char c = l->src[i];
        if (c == '\n' || c == '\r')
            return false;
        if (!escaped && c == '"')
            return true;
        if (!escaped && c == '\\')
            escaped = 1;
        else
            escaped = 0;
        i++;
    }
    return false;
}

static bool is_char_escape(char c)
{
    switch (c)
    {
    case '0':
    case 'b':
    case 'n':
    case 'r':
    case 't':
    case '\\':
    case '\'':
    case '"':
        return true;
    default:
        return false;
    }
}

static void lex_skip_ws(Lexer *l)
{
    for (;;)
    {
        while (l->ch == ' ' || l->ch == '\t' || l->ch == '\r' || l->ch == '\n')
            lex_advance(l);
        if (l->ch == '/' && lex_peek(l) == '/')
        {
            lex_advance(l);
            lex_advance(l);
            while (l->ch && l->ch != '\n') lex_advance(l);
            continue;
        }
        if (l->ch == '/' && lex_peek(l) == '*')
        {
            lex_advance(l);
            lex_advance(l);
            while (l->ch)
            {
                if (l->ch == '*' && lex_peek(l) == '/')
                {
                    lex_advance(l);
                    lex_advance(l);
                    break;
                }
                lex_advance(l);
            }
            continue;
        }
        break;
    }
}

static Token make_token(Lexer *l, TokenType t, size_t start, size_t len, int line, int col)
{
    Token tok;
    tok.type = t;
    tok.start = l->src + start;
    tok.length = len;
    tok.line = line;
    tok.col = col;
    return tok;
}

typedef struct { const char *kw; TokenType type; } Kw;

static const Kw g_keywords[] = {
    {"Unit", TOK_UNIT},
    {"Get", TOK_GET},
    {"Set", TOK_SET},
    {"By", TOK_BY},
    {"Property", TOK_PROPERTY},
    {"Function", TOK_FUNCTION},
    {"Extern", TOK_EXTERN},
    {"Return", TOK_RETURN},
    {"If", TOK_IF},
    {"Else", TOK_ELSE},
    {"ElseIf", TOK_ELSEIF},
    {"While", TOK_WHILE},
    {"For", TOK_FOR},
    {"Switch", TOK_SWITCH},
    {"Case", TOK_CASE},
    {"Break", TOK_BREAK},
    {"Continue", TOK_CONTINUE},
    {"Try", TOK_TRY},
    {"Catch", TOK_CATCH},
    {"Throw", TOK_THROW},
    {"Class", TOK_CLASS},
    {"Interface", TOK_INTERFACE},
    {"Struct", TOK_STRUCT},
    {"Enum", TOK_ENUM},
    {"Public", TOK_PUBLIC},
    {"Private", TOK_PRIVATE},
    {"Protected", TOK_PROTECTED},
    {"Internal", TOK_INTERNAL},
    {"Static", TOK_STATIC},
    {"Virtual", TOK_VIRTUAL},
    {"Override", TOK_OVERRIDE},
    {"Abstract", TOK_ABSTRACT},
    {"Sealed", TOK_SEALED},
    {"Constructor", TOK_CONSTRUCTOR},
    {"New", TOK_NEW},
    {"This", TOK_THIS},
    {"Base", TOK_BASE},
    {"Block", TOK_BLOCK},
    {"Record", TOK_RECORD},
    {"Jump", TOK_JUMP},
    {"Delegate", TOK_DELEGATE},
    {"Const", TOK_CONST},
    {"Var", TOK_VAR},
    {"Safe", TOK_SAFE},
    {"Trusted", TOK_TRUSTED},
    {"Unsafe", TOK_UNSAFE},
    {"Async", TOK_ASYNC},
    {"Await", TOK_AWAIT},
    {"true", TOK_TRUE},
    {"false", TOK_FALSE},
    {"Is", TOK_IS},
    {"default", TOK_DEFAULT},
    {"null", TOK_NULL},

    // primitive type keywords (lowercase)
    {"void", TOK_TYPE_VOID},
    {"bool", TOK_TYPE_BOOL},
    {"byte", TOK_TYPE_BYTE},
    {"char", TOK_TYPE_CHAR},
    {"int", TOK_TYPE_INT},
    {"float", TOK_TYPE_FLOAT},
    {"f16", TOK_TYPE_F16},
    {"f32", TOK_TYPE_F32},
    {"f64", TOK_TYPE_F64},
    {"f128", TOK_TYPE_F128},
    {"i8", TOK_TYPE_I8},
    {"i16", TOK_TYPE_I16},
    {"i32", TOK_TYPE_I32},
    {"i64", TOK_TYPE_I64},
    {"u8", TOK_TYPE_U8},
    {"u16", TOK_TYPE_U16},
    {"u32", TOK_TYPE_U32},
    {"u64", TOK_TYPE_U64},
    {"isize", TOK_TYPE_ISIZE},
    {"usize", TOK_TYPE_USIZE},
    {"string", TOK_TYPE_STRING},
    {"any", TOK_TYPE_ANY},
};

static TokenType kw_lookup(const char *s, size_t len)
{
    for (size_t i = 0; i < sizeof(g_keywords)/sizeof(g_keywords[0]); i++)
    {
        const char *kw = g_keywords[i].kw;
        size_t klen = kn_strlen(kw);
        if (klen == len && kn_strncmp(s, kw, len) == 0)
            return g_keywords[i].type;
    }
    return TOK_ID;
}

Token lex_next(Lexer *l)
{
    lex_skip_ws(l);
    if (!l->ch) return make_token(l, TOK_EOF, l->pos, 0, l->line, l->col);

    if (is_alpha(l->ch))
    {
        size_t start = l->pos;
        int line = l->line, col = l->col;
        while (is_alpha(l->ch) || is_digit(l->ch)) lex_advance(l);
        size_t len = l->pos - start;
        TokenType t = kw_lookup(l->src + start, len);
        return make_token(l, t, start, len, line, col);
    }

    if (is_digit(l->ch))
    {
        size_t start = l->pos;
        int line = l->line, col = l->col;
        if (l->ch == '0' && (lex_peek(l) == 'x' || lex_peek(l) == 'X'))
        {
            lex_advance(l);
            lex_advance(l);
            while (is_hex_digit(l->ch)) lex_advance(l);
        }
        else
        {
            while (is_digit(l->ch)) lex_advance(l);
            if (l->ch == '.' && is_digit(lex_peek(l)))
            {
                lex_advance(l);
                while (is_digit(l->ch)) lex_advance(l);
            }
        }
        if (l->ch == 'f' || l->ch == 'F')
        {
            lex_advance(l);
        }
        return make_token(l, TOK_NUMBER, start, l->pos - start, line, col);
    }

    if (l->ch == '"')
    {
        size_t start = l->pos;
        int line = l->line, col = l->col;
        bool terminated = has_closing_quote_before_eol(l);
        lex_advance(l); // opening quote
        while (l->ch)
        {
            if (terminated && l->ch == '"')
            {
                lex_advance(l);
                break;
            }
            if (!terminated && (l->ch == '\n' || l->ch == '\r' || l->ch == ')' || l->ch == ';' || l->ch == '}'))
                break;
            if (l->ch == '\\')
            {
                lex_advance(l);
                if (l->ch && l->ch != '\n' && l->ch != '\r')
                    lex_advance(l);
                continue;
            }
            if (l->ch == '\n' || l->ch == '\r')
                break;
            lex_advance(l);
        }
        return make_token(l, terminated ? TOK_STRING : TOK_BAD_STRING, start, l->pos - start, line, col);
    }

    if (l->ch == '\'')
    {
        size_t start = l->pos;
        int line = l->line, col = l->col;
        int value_count = 0;
        bool valid = true;
        lex_advance(l); // opening quote
        while (l->ch)
        {
            if (l->ch == '\n' || l->ch == '\r')
                break;
            if (l->ch == '\'')
            {
                lex_advance(l);
                break;
            }

            value_count++;
            if (l->ch == '\\')
            {
                lex_advance(l);
                if (!l->ch || l->ch == '\n' || l->ch == '\r')
                    break;
                if (!is_char_escape(l->ch))
                    valid = false;
            }
            lex_advance(l);
        }

        if (l->pos <= start + 1 || l->src[l->pos - 1] != '\'')
            valid = false;
        if (value_count != 1)
            valid = false;
        return make_token(l, valid ? TOK_CHAR_LIT : TOK_BAD_CHAR, start, l->pos - start, line, col);
    }

    // operators / punctuation
    {
        size_t start = l->pos;
        int line = l->line, col = l->col;
        char c = l->ch;
        char n = lex_peek(l);
        if (c == '=' && n == '=') { lex_advance(l); lex_advance(l); return make_token(l, TOK_EQ, start, 2, line, col); }
        if (c == '!' && n == '=') { lex_advance(l); lex_advance(l); return make_token(l, TOK_NE, start, 2, line, col); }
        if (c == '<' && n == '=') { lex_advance(l); lex_advance(l); return make_token(l, TOK_LE, start, 2, line, col); }
        if (c == '>' && n == '=') { lex_advance(l); lex_advance(l); return make_token(l, TOK_GE, start, 2, line, col); }
        if (c == '<' && n == '<') { lex_advance(l); lex_advance(l); return make_token(l, TOK_SHL, start, 2, line, col); }
        if (c == '>' && n == '>') { lex_advance(l); lex_advance(l); return make_token(l, TOK_SHR, start, 2, line, col); }
        if (c == '&' && n == '&') { lex_advance(l); lex_advance(l); return make_token(l, TOK_ANDAND, start, 2, line, col); }
        if (c == '|' && n == '|') { lex_advance(l); lex_advance(l); return make_token(l, TOK_OROR, start, 2, line, col); }
        if (c == '+' && n == '+') { lex_advance(l); lex_advance(l); return make_token(l, TOK_PLUSPLUS, start, 2, line, col); }
        if (c == '-' && n == '-') { lex_advance(l); lex_advance(l); return make_token(l, TOK_MINUSMINUS, start, 2, line, col); }
        if (c == '+' && n == '=') { lex_advance(l); lex_advance(l); return make_token(l, TOK_PLUS_ASSIGN, start, 2, line, col); }
        if (c == '-' && n == '=') { lex_advance(l); lex_advance(l); return make_token(l, TOK_MINUS_ASSIGN, start, 2, line, col); }
        if (c == '*' && n == '=') { lex_advance(l); lex_advance(l); return make_token(l, TOK_STAR_ASSIGN, start, 2, line, col); }
        if (c == '/' && n == '=') { lex_advance(l); lex_advance(l); return make_token(l, TOK_SLASH_ASSIGN, start, 2, line, col); }
        if (c == '%' && n == '=') { lex_advance(l); lex_advance(l); return make_token(l, TOK_PERCENT_ASSIGN, start, 2, line, col); }

        switch (c)
        {
        case '(': lex_advance(l); return make_token(l, TOK_LPAREN, start, 1, line, col);
        case ')': lex_advance(l); return make_token(l, TOK_RPAREN, start, 1, line, col);
        case '{': lex_advance(l); return make_token(l, TOK_LBRACE, start, 1, line, col);
        case '}': lex_advance(l); return make_token(l, TOK_RBRACE, start, 1, line, col);
        case '[': lex_advance(l); return make_token(l, TOK_LBRACKET, start, 1, line, col);
        case ']': lex_advance(l); return make_token(l, TOK_RBRACKET, start, 1, line, col);
        case ',': lex_advance(l); return make_token(l, TOK_COMMA, start, 1, line, col);
        case ';': lex_advance(l); return make_token(l, TOK_SEMI, start, 1, line, col);
        case '.': lex_advance(l); return make_token(l, TOK_DOT, start, 1, line, col);
        case ':': lex_advance(l); return make_token(l, TOK_COLON, start, 1, line, col);
        case '?': lex_advance(l); return make_token(l, TOK_QUESTION, start, 1, line, col);
        case '@': lex_advance(l); return make_token(l, TOK_AT, start, 1, line, col);
        case '+': lex_advance(l); return make_token(l, TOK_PLUS, start, 1, line, col);
        case '-': lex_advance(l); return make_token(l, TOK_MINUS, start, 1, line, col);
        case '*': lex_advance(l); return make_token(l, TOK_STAR, start, 1, line, col);
        case '/': lex_advance(l); return make_token(l, TOK_SLASH, start, 1, line, col);
        case '%': lex_advance(l); return make_token(l, TOK_PERCENT, start, 1, line, col);
        case '=': lex_advance(l); return make_token(l, TOK_ASSIGN, start, 1, line, col);
        case '<': lex_advance(l); return make_token(l, TOK_LT, start, 1, line, col);
        case '>': lex_advance(l); return make_token(l, TOK_GT, start, 1, line, col);
        case '!': lex_advance(l); return make_token(l, TOK_NOT, start, 1, line, col);
        case '&': lex_advance(l); return make_token(l, TOK_AMP, start, 1, line, col);
        case '|': lex_advance(l); return make_token(l, TOK_BITOR, start, 1, line, col);
        case '^': lex_advance(l); return make_token(l, TOK_XOR, start, 1, line, col);
        case '~': lex_advance(l); return make_token(l, TOK_TILDE, start, 1, line, col);
        default: break;
        }

        lex_advance(l);
        return make_token(l, TOK_EOF, start, 1, line, col);
    }
}
