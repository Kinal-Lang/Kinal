#include "kn/format.h"
#include "kn/lexer.h"
#include "kn/util.h"

#include <stdio.h>

typedef struct
{
    Token *items;
    int count;
    int cap;
} KnFmtTokenList;

typedef struct
{
    char *data;
    size_t len;
    size_t cap;
} KnFmtBuf;

typedef struct
{
    KnFmtBuf out;
    const char *src;
    size_t src_len;
    const char *newline;
    int indent;
    int paren_depth;
    int bracket_depth;
    int line_start;
    int pending_blank_line;
    int force_space_before_next;
    int has_prev;
    int prev_was_case_label;
    Token prev;
} KnFmtState;

static int fmt_str_eq(const char *a, const char *b)
{
    return kn_strcmp(a ? a : "", b ? b : "") == 0;
}

static int fmt_is_crlf(const char *src, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (src[i] == '\r')
        {
            if (i + 1 < len && src[i + 1] == '\n')
                return 1;
            return 0;
        }
        if (src[i] == '\n')
            return 0;
    }
    return 1;
}

static void fmt_buf_reserve(KnFmtBuf *b, size_t extra)
{
    size_t need = b->len + extra + 1;
    if (need <= b->cap) return;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < need)
        cap = cap < 4096 ? cap * 2 : cap + 4096;
    char *next = (char *)kn_realloc(b->data, cap);
    if (!next)
        kn_die("formatter: out of memory");
    b->data = next;
    b->cap = cap;
}

static void fmt_buf_append_char(KnFmtBuf *b, char c)
{
    fmt_buf_reserve(b, 1);
    b->data[b->len++] = c;
    b->data[b->len] = 0;
}

static void fmt_buf_append(KnFmtBuf *b, const char *s, size_t len)
{
    if (!s || !len) return;
    fmt_buf_reserve(b, len);
    kn_memcpy(b->data + b->len, s, len);
    b->len += len;
    b->data[b->len] = 0;
}

static void fmt_buf_append_str(KnFmtBuf *b, const char *s)
{
    fmt_buf_append(b, s, kn_strlen(s));
}

static void fmt_tokens_push(KnFmtTokenList *list, Token tok)
{
    if (list->count >= list->cap)
    {
        int cap = list->cap ? list->cap * 2 : 128;
        Token *next = (Token *)kn_realloc(list->items, sizeof(Token) * (size_t)cap);
        if (!next)
            kn_die("formatter: out of memory");
        list->items = next;
        list->cap = cap;
    }
    list->items[list->count++] = tok;
}

static int fmt_token_is_word(TokenType t)
{
    switch (t)
    {
    case TOK_ID:
    case TOK_NUMBER:
    case TOK_STRING:
    case TOK_BAD_STRING:
    case TOK_CHAR_LIT:
    case TOK_BAD_CHAR:
    case TOK_UNIT:
    case TOK_GET:
    case TOK_SET:
    case TOK_BY:
    case TOK_PROPERTY:
    case TOK_FUNCTION:
    case TOK_ASYNC:
    case TOK_AWAIT:
    case TOK_RETURN:
    case TOK_IF:
    case TOK_ELSE:
    case TOK_ELSEIF:
    case TOK_WHILE:
    case TOK_FOR:
    case TOK_SWITCH:
    case TOK_CASE:
    case TOK_BREAK:
    case TOK_CONTINUE:
    case TOK_TRY:
    case TOK_CATCH:
    case TOK_THROW:
    case TOK_CONST:
    case TOK_VAR:
    case TOK_SAFE:
    case TOK_TRUSTED:
    case TOK_UNSAFE:
    case TOK_EXTERN:
    case TOK_CLASS:
    case TOK_INTERFACE:
    case TOK_STRUCT:
    case TOK_ENUM:
    case TOK_PUBLIC:
    case TOK_PRIVATE:
    case TOK_PROTECTED:
    case TOK_INTERNAL:
    case TOK_STATIC:
    case TOK_VIRTUAL:
    case TOK_OVERRIDE:
    case TOK_ABSTRACT:
    case TOK_SEALED:
    case TOK_CONSTRUCTOR:
    case TOK_NEW:
    case TOK_THIS:
    case TOK_BASE:
    case TOK_BLOCK:
    case TOK_RECORD:
    case TOK_JUMP:
    case TOK_DELEGATE:
    case TOK_TRUE:
    case TOK_FALSE:
    case TOK_IS:
    case TOK_DEFAULT:
    case TOK_NULL:
    case TOK_TYPE_VOID:
    case TOK_TYPE_BOOL:
    case TOK_TYPE_BYTE:
    case TOK_TYPE_CHAR:
    case TOK_TYPE_INT:
    case TOK_TYPE_FLOAT:
    case TOK_TYPE_F16:
    case TOK_TYPE_F32:
    case TOK_TYPE_F64:
    case TOK_TYPE_F128:
    case TOK_TYPE_I8:
    case TOK_TYPE_I16:
    case TOK_TYPE_I32:
    case TOK_TYPE_I64:
    case TOK_TYPE_U8:
    case TOK_TYPE_U16:
    case TOK_TYPE_U32:
    case TOK_TYPE_U64:
    case TOK_TYPE_ISIZE:
    case TOK_TYPE_USIZE:
    case TOK_TYPE_STRING:
    case TOK_TYPE_ANY:
        return 1;
    default:
        return 0;
    }
}

static int fmt_token_is_control_with_paren(TokenType t)
{
    return t == TOK_IF || t == TOK_ELSEIF || t == TOK_FOR || t == TOK_WHILE || t == TOK_SWITCH || t == TOK_CATCH;
}

static int fmt_token_can_end_expr(TokenType t)
{
    switch (t)
    {
    case TOK_ID:
    case TOK_NUMBER:
    case TOK_STRING:
    case TOK_BAD_STRING:
    case TOK_CHAR_LIT:
    case TOK_BAD_CHAR:
    case TOK_TRUE:
    case TOK_FALSE:
    case TOK_NULL:
    case TOK_THIS:
    case TOK_BASE:
    case TOK_RPAREN:
    case TOK_RBRACKET:
    case TOK_PLUSPLUS:
    case TOK_MINUSMINUS:
        return 1;
    default:
        return 0;
    }
}

static int fmt_token_is_ambiguous_operator(TokenType t)
{
    return t == TOK_PLUS || t == TOK_MINUS || t == TOK_STAR || t == TOK_AMP || t == TOK_PLUSPLUS || t == TOK_MINUSMINUS;
}

static int fmt_token_is_initializer_context(TokenType t)
{
    switch (t)
    {
    case TOK_ASSIGN:
    case TOK_COMMA:
    case TOK_LPAREN:
    case TOK_LBRACKET:
    case TOK_COLON:
    case TOK_QUESTION:
    case TOK_RETURN:
    case TOK_THROW:
    case TOK_PLUS:
    case TOK_MINUS:
    case TOK_STAR:
    case TOK_SLASH:
    case TOK_PERCENT:
    case TOK_PLUS_ASSIGN:
    case TOK_MINUS_ASSIGN:
    case TOK_STAR_ASSIGN:
    case TOK_SLASH_ASSIGN:
    case TOK_PERCENT_ASSIGN:
    case TOK_EQ:
    case TOK_NE:
    case TOK_LT:
    case TOK_LE:
    case TOK_GT:
    case TOK_GE:
    case TOK_ANDAND:
    case TOK_OROR:
    case TOK_NOT:
    case TOK_AMP:
    case TOK_BITOR:
    case TOK_XOR:
    case TOK_SHL:
    case TOK_SHR:
    case TOK_TILDE:
    case TOK_BY:
        return 1;
    default:
        return 0;
    }
}

static int fmt_token_is_block_brace(const KnFmtState *st, Token current)
{
    if (current.type != TOK_LBRACE || !st->has_prev)
        return 0;
    return !fmt_token_is_initializer_context(st->prev.type);
}

static int fmt_token_is_binary_operator(TokenType t, int has_prev, TokenType prev)
{
    switch (t)
    {
    case TOK_ASSIGN:
    case TOK_PLUS_ASSIGN:
    case TOK_MINUS_ASSIGN:
    case TOK_STAR_ASSIGN:
    case TOK_SLASH_ASSIGN:
    case TOK_PERCENT_ASSIGN:
    case TOK_EQ:
    case TOK_NE:
    case TOK_LT:
    case TOK_LE:
    case TOK_GT:
    case TOK_GE:
    case TOK_ANDAND:
    case TOK_OROR:
    case TOK_BITOR:
    case TOK_XOR:
    case TOK_SHL:
    case TOK_SHR:
    case TOK_SLASH:
    case TOK_PERCENT:
    case TOK_IS:
        return 1;
    case TOK_PLUS:
    case TOK_MINUS:
    case TOK_STAR:
    case TOK_AMP:
        return has_prev && fmt_token_can_end_expr(prev);
    default:
        return 0;
    }
}

static int fmt_buf_last_is_space(const KnFmtBuf *b)
{
    if (!b || b->len == 0) return 0;
    return b->data[b->len - 1] == ' ';
}

static int fmt_buf_last_is_newline(const KnFmtBuf *b)
{
    if (!b || b->len == 0) return 0;
    return b->data[b->len - 1] == '\n';
}

static void fmt_trim_trailing_spaces(KnFmtBuf *b)
{
    while (b->len && (b->data[b->len - 1] == ' ' || b->data[b->len - 1] == '\t'))
        b->len--;
    if (b->data)
        b->data[b->len] = 0;
}

static void fmt_emit_indent(KnFmtState *st)
{
    if (!st->line_start) return;
    for (int i = 0; i < st->indent; i++)
        fmt_buf_append_str(&st->out, "    ");
    st->line_start = 0;
}

static void fmt_emit_newline(KnFmtState *st)
{
    fmt_trim_trailing_spaces(&st->out);
    fmt_buf_append_str(&st->out, st->newline);
    st->line_start = 1;
    st->force_space_before_next = 0;
}

static void fmt_emit_blank_line_if_needed(KnFmtState *st)
{
    if (!st->pending_blank_line) return;
    if (!st->line_start)
        fmt_emit_newline(st);
    if (st->out.len > 0)
        fmt_emit_newline(st);
    st->pending_blank_line = 0;
}

static void fmt_emit_space_if_needed(KnFmtState *st)
{
    if (st->line_start) return;
    if (fmt_buf_last_is_space(&st->out)) return;
    fmt_buf_append_char(&st->out, ' ');
}

static void fmt_process_gap(KnFmtState *st, size_t begin, size_t end)
{
    int newline_count = 0;
    size_t p = begin;
    while (p < end)
    {
        char c = st->src[p];
        if (c == '/' && p + 1 < end && st->src[p + 1] == '/')
        {
            size_t start = p;
            p += 2;
            while (p < end && st->src[p] != '\n' && st->src[p] != '\r')
                p++;
            if (newline_count >= 2)
                st->pending_blank_line = 1;
            fmt_emit_blank_line_if_needed(st);
            if (!st->line_start)
                fmt_emit_space_if_needed(st);
            fmt_emit_indent(st);
            fmt_buf_append(&st->out, st->src + start, p - start);
            fmt_emit_newline(st);
            if (p < end && st->src[p] == '\r' && p + 1 < end && st->src[p + 1] == '\n')
                p += 2;
            else if (p < end && (st->src[p] == '\r' || st->src[p] == '\n'))
                p++;
            newline_count = 0;
            continue;
        }
        if (c == '/' && p + 1 < end && st->src[p + 1] == '*')
        {
            size_t start = p;
            int has_newline = 0;
            p += 2;
            while (p < end)
            {
                if (st->src[p] == '\n' || st->src[p] == '\r')
                    has_newline = 1;
                if (st->src[p] == '*' && p + 1 < end && st->src[p + 1] == '/')
                {
                    p += 2;
                    break;
                }
                p++;
            }
            if (newline_count >= 2)
                st->pending_blank_line = 1;
            fmt_emit_blank_line_if_needed(st);
            if (!st->line_start)
                fmt_emit_space_if_needed(st);
            fmt_emit_indent(st);
            fmt_buf_append(&st->out, st->src + start, p - start);
            if (has_newline)
                fmt_emit_newline(st);
            newline_count = 0;
            continue;
        }
        if (c == '\r')
        {
            newline_count++;
            if (p + 1 < end && st->src[p + 1] == '\n')
                p++;
            p++;
            continue;
        }
        if (c == '\n')
        {
            newline_count++;
            p++;
            continue;
        }
        p++;
    }
    if (newline_count >= 2)
        st->pending_blank_line = 1;
}

static int fmt_need_space_before(const KnFmtState *st, Token current)
{
    if (!st->has_prev || st->line_start) return 0;

    TokenType prev = st->prev.type;
    TokenType curr = current.type;

    if (curr == TOK_RPAREN || curr == TOK_RBRACKET || curr == TOK_COMMA || curr == TOK_SEMI || curr == TOK_DOT || curr == TOK_COLON)
        return 0;
    if (prev == TOK_LPAREN || prev == TOK_LBRACKET || prev == TOK_DOT || prev == TOK_AT)
        return 0;
    if (st->force_space_before_next)
        return 1;
    if (curr == TOK_LBRACKET)
        return (fmt_token_is_word(prev) && !fmt_token_can_end_expr(prev)) ? 1 : 0;
    if (curr == TOK_LPAREN)
        return fmt_token_is_control_with_paren(prev);
    if (prev == TOK_RBRACKET && (curr == TOK_LPAREN || curr == TOK_STRING || curr == TOK_BAD_STRING || curr == TOK_CHAR_LIT || curr == TOK_BAD_CHAR))
        return 0;
    if (fmt_token_is_word(prev) && fmt_token_is_word(curr))
        return 1;
    if (fmt_token_is_word(curr) && (prev == TOK_RPAREN || prev == TOK_RBRACKET))
        return 1;
    if (fmt_token_can_end_expr(prev) && fmt_token_is_binary_operator(curr, st->has_prev, prev))
        return 1;
    if (curr == TOK_LBRACE)
    {
        if (fmt_token_is_block_brace(st, current))
            return 0;
        return 1;
    }
    return 0;
}

static void fmt_emit_token(KnFmtState *st, Token current, Token next)
{
    TokenType t = current.type;
    int block_brace = fmt_token_is_block_brace(st, current);

    if (t == TOK_RBRACE)
    {
        if (st->indent > 0)
            st->indent--;
        if (!st->line_start)
            fmt_emit_newline(st);
    }

    if ((t == TOK_CASE || t == TOK_DEFAULT || t == TOK_CATCH) && !st->line_start)
        fmt_emit_newline(st);
    if (block_brace && !st->line_start)
        fmt_emit_newline(st);

    fmt_emit_blank_line_if_needed(st);
    if (fmt_need_space_before(st, current))
        fmt_emit_space_if_needed(st);
    fmt_emit_indent(st);
    fmt_buf_append(&st->out, current.start, current.length);
    st->line_start = 0;
    st->force_space_before_next = 0;

    if (t == TOK_LPAREN)
        st->paren_depth++;
    else if (t == TOK_RPAREN && st->paren_depth > 0)
        st->paren_depth--;
    else if (t == TOK_LBRACKET)
        st->bracket_depth++;
    else if (t == TOK_RBRACKET && st->bracket_depth > 0)
        st->bracket_depth--;

    if (t == TOK_LBRACE)
    {
        st->indent++;
        fmt_emit_newline(st);
    }
    else if (t == TOK_RBRACE)
    {
        if (next.type != TOK_SEMI && next.type != TOK_COMMA && next.type != TOK_EOF)
            fmt_emit_newline(st);
    }
    else if (t == TOK_SEMI)
    {
        if (st->paren_depth == 0 && st->bracket_depth == 0)
            fmt_emit_newline(st);
        else
            st->force_space_before_next = 1;
    }
    else if (t == TOK_COMMA)
    {
        st->force_space_before_next = 1;
    }
    else if (t == TOK_COLON)
    {
        if (st->prev_was_case_label)
            fmt_emit_newline(st);
        else
            st->force_space_before_next = 1;
    }
    else if (fmt_token_is_binary_operator(t, st->has_prev, st->prev.type))
    {
        st->force_space_before_next = 1;
    }
    else if (fmt_token_is_ambiguous_operator(t))
    {
        if (fmt_token_is_binary_operator(t, st->has_prev, st->prev.type))
            st->force_space_before_next = 1;
    }

    st->prev_was_case_label = (t == TOK_CASE || t == TOK_DEFAULT);
    st->prev = current;
    st->has_prev = 1;
}

static char *fmt_read_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return 0;
    }
    long size = ftell(f);
    if (size < 0)
    {
        fclose(f);
        return 0;
    }
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return 0;
    }
    char *buf = (char *)kn_malloc((size_t)size + 1);
    if (!buf)
    {
        fclose(f);
        return 0;
    }
    size_t read = 0;
    if (size > 0)
        read = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (read != (size_t)size)
    {
        kn_free(buf);
        return 0;
    }
    buf[read] = 0;
    if (out_size) *out_size = read;
    return buf;
}

static int fmt_write_file(const char *path, const char *text, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t wrote = len ? fwrite(text, 1, len, f) : 0;
    fclose(f);
    return wrote == len;
}

static char *fmt_format_source(const char *src, size_t src_len, size_t *out_len)
{
    Lexer lex;
    KnFmtTokenList tokens = {0};
    KnFmtState st;
    kn_memset(&st, 0, sizeof(st));
    st.src = src ? src : "";
    st.src_len = src_len;
    st.newline = fmt_is_crlf(st.src, st.src_len) ? "\r\n" : "\n";
    st.line_start = 1;

    size_t cursor = 0;
    if (src_len >= 3 &&
        (unsigned char)st.src[0] == 0xEF &&
        (unsigned char)st.src[1] == 0xBB &&
        (unsigned char)st.src[2] == 0xBF)
    {
        fmt_buf_append(&st.out, st.src, 3);
        cursor = 3;
    }

    lex_init(&lex, st.src);
    for (;;)
    {
        Token tok = lex_next(&lex);
        fmt_tokens_push(&tokens, tok);
        if (tok.type == TOK_EOF)
            break;
    }

    for (int i = 0; i < tokens.count; i++)
    {
        Token tok = tokens.items[i];
        Token next = i + 1 < tokens.count ? tokens.items[i + 1] : tok;
        size_t start = (size_t)(tok.start - st.src);
        if (start > cursor)
            fmt_process_gap(&st, cursor, start);
        if (tok.type == TOK_EOF)
        {
            cursor = start;
            break;
        }
        fmt_emit_token(&st, tok, next);
        cursor = start + tok.length;
    }

    if (cursor < st.src_len)
        fmt_process_gap(&st, cursor, st.src_len);
    fmt_trim_trailing_spaces(&st.out);
    if (!fmt_buf_last_is_newline(&st.out))
        fmt_emit_newline(&st);
    fmt_trim_trailing_spaces(&st.out);
    if (!fmt_buf_last_is_newline(&st.out))
        fmt_buf_append_str(&st.out, st.newline);

    if (out_len) *out_len = st.out.len;
    kn_free(tokens.items);
    return st.out.data;
}

static int fmt_path_is_dir(const char *path)
{
    char pattern[KN_MAX_PATH];
    size_t len = kn_strlen(path);
    if (len + 3 >= KN_MAX_PATH) return 0;
    kn_memcpy(pattern, path, len);
    pattern[len]     = '\\';
    pattern[len + 1] = '*';
    pattern[len + 2] = '\0';
    KN_WIN32_FIND_DATAA data;
    KN_HANDLE h = FindFirstFileA(pattern, &data);
    if (h == KN_INVALID_HANDLE_VALUE) return 0;
    FindClose(h);
    return 1;
}

static int fmt_path_join(char *out, size_t cap, const char *dir, const char *name)
{
    size_t dlen = kn_strlen(dir);
    size_t nlen = kn_strlen(name);
    if (dlen + 1 + nlen + 1 > cap) return 0;
    kn_memcpy(out, dir, dlen);
    out[dlen] = '\\';
    kn_memcpy(out + dlen + 1, name, nlen);
    out[dlen + 1 + nlen] = '\0';
    return 1;
}

static int fmt_str_ends_with(const char *s, const char *suffix)
{
    size_t slen = kn_strlen(s);
    size_t suflen = kn_strlen(suffix);
    if (suflen > slen) return 0;
    return kn_strcmp(s + slen - suflen, suffix) == 0;
}

/* Recursively collect all .kn files under dir into the files array. */
static int fmt_collect_kn_files(const char *dir,
                                const char ***files, int *count, int *cap)
{
    char pattern[KN_MAX_PATH];
    size_t dlen = kn_strlen(dir);
    if (dlen + 3 >= KN_MAX_PATH) return 1;
    kn_memcpy(pattern, dir, dlen);
    pattern[dlen]     = '\\';
    pattern[dlen + 1] = '*';
    pattern[dlen + 2] = '\0';

    KN_WIN32_FIND_DATAA data;
    KN_HANDLE h = FindFirstFileA(pattern, &data);
    if (h == KN_INVALID_HANDLE_VALUE) return 1;

    do
    {
        const char *name = data.cFileName;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
            continue;
        char full[KN_MAX_PATH];
        if (!fmt_path_join(full, sizeof(full), dir, name))
            continue;
        if (data.dwFileAttributes & KN_FILE_ATTRIBUTE_DIRECTORY)
        {
            fmt_collect_kn_files(full, files, count, cap);
            continue;
        }
        if (!fmt_str_ends_with(name, ".kn"))
            continue;
        if (*count >= *cap)
        {
            int newcap = *cap ? *cap * 2 : 8;
            const char **next = (const char **)kn_realloc((void *)*files,
                                                          sizeof(char *) * (size_t)newcap);
            if (!next) { FindClose(h); return 0; }
            *files = next;
            *cap = newcap;
        }
        size_t flen = kn_strlen(full);
        char *copy = (char *)kn_malloc(flen + 1);
        if (!copy) { FindClose(h); return 0; }
        kn_memcpy(copy, full, flen + 1);
        (*files)[(*count)++] = copy;
    } while (FindNextFileA(h, &data));

    FindClose(h);
    return 1;
}

static void fmt_print_usage(void)
{
    kn_write_out_str(
        "Usage: kinal fmt [options] <files|dirs...>\n"
        "  --check             Return non-zero if formatting would change a file\n"
        "  --stdin             Read source from stdin\n"
        "  --stdout            Write formatted output to stdout\n"
        "  --stdin-filepath <path>  Virtual path for stdin input (editor integration)\n"
        "\n"
        "  When a directory is given, all .kn files in it are formatted recursively.\n");
}

int kn_fmt_main(int argc, char **argv)
{
#define FMT_CLEANUP() \
    do { \
        if (files_owned) { for (int _fi = 0; _fi < file_count; _fi++) if (files_owned[_fi]) kn_free((void *)files[_fi]); } \
        kn_free(files_owned); \
        kn_free((void *)files); \
    } while (0)

    int check_only = 0;
    int use_stdin = 0;
    int use_stdout = 0;
    const char *stdin_filepath = 0;
    const char **files = 0;
    int *files_owned = 0;
    int file_count = 0;
    int file_cap = 0;
    int changed = 0;

    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        if (fmt_str_eq(arg, "-h") || fmt_str_eq(arg, "--help"))
        {
            fmt_print_usage();
            return 0;
        }
        if (fmt_str_eq(arg, "--check"))
        {
            check_only = 1;
            continue;
        }
        if (fmt_str_eq(arg, "--stdin"))
        {
            use_stdin = 1;
            continue;
        }
        if (fmt_str_eq(arg, "--stdout"))
        {
            use_stdout = 1;
            continue;
        }
        if (fmt_str_eq(arg, "--stdin-filepath"))
        {
            if (i + 1 >= argc)
            {
                kn_write_str("fmt: missing value after --stdin-filepath\n");
                return 1;
            }
            stdin_filepath = argv[++i];
            continue;
        }
        if (arg[0] == '-')
        {
            kn_write_str("fmt: unknown option: ");
            kn_write_str(arg);
            kn_write_str("\n");
            return 1;
        }
        if (fmt_path_is_dir(arg))
        {
            int prev_count = file_count;
            if (!fmt_collect_kn_files(arg, &files, &file_count, &file_cap))
                kn_die("formatter: out of memory");
            int *next_owned = (int *)kn_realloc(files_owned, sizeof(int) * (size_t)file_cap);
            if (!next_owned)
                kn_die("formatter: out of memory");
            files_owned = next_owned;
            for (int j = prev_count; j < file_count; j++)
                files_owned[j] = 1;
            continue;
        }
        if (file_count >= file_cap)
        {
            int cap = file_cap ? file_cap * 2 : 8;
            const char **next = (const char **)kn_realloc((void *)files, sizeof(char *) * (size_t)cap);
            if (!next)
                kn_die("formatter: out of memory");
            int *next_owned = (int *)kn_realloc(files_owned, sizeof(int) * (size_t)cap);
            if (!next_owned)
                kn_die("formatter: out of memory");
            files = next;
            files_owned = next_owned;
            file_cap = cap;
        }
        files_owned[file_count] = 0;
        files[file_count++] = arg;
    }

    if (!use_stdin && file_count == 0)
    {
        fmt_print_usage();
        FMT_CLEANUP();
        return 1;
    }

    if (use_stdin)
    {
        size_t src_len = 0;
        char *src = kn_read_stdin(&src_len);
        if (!src)
        {
            kn_write_str("fmt: failed to read stdin\n");
            FMT_CLEANUP();
            return 1;
        }
        size_t out_len = 0;
        char *formatted = fmt_format_source(src, src_len, &out_len);
        if (!formatted)
        {
            kn_write_str("fmt: failed to format stdin\n");
            kn_free(src);
            FMT_CLEANUP();
            return 1;
        }
        int differs = (src_len != out_len) || kn_strncmp(src, formatted, src_len > out_len ? src_len : out_len) != 0;
        if (!check_only || use_stdout)
            kn_write_out_buf(formatted, out_len);
        if (check_only && differs)
            changed = 1;
        (void)stdin_filepath;
        kn_free(formatted);
        kn_free(src);
    }

    for (int i = 0; i < file_count; i++)
    {
        size_t src_len = 0;
        char *src = fmt_read_file(files[i], &src_len);
        if (!src)
        {
            kn_write_str("fmt: failed to read file: ");
            kn_write_str(files[i]);
            kn_write_str("\n");
            FMT_CLEANUP();
            return 1;
        }
        size_t out_len = 0;
        char *formatted = fmt_format_source(src, src_len, &out_len);
        if (!formatted)
        {
            kn_write_str("fmt: failed to format file: ");
            kn_write_str(files[i]);
            kn_write_str("\n");
            kn_free(src);
            FMT_CLEANUP();
            return 1;
        }

        int differs = (src_len != out_len) || kn_strncmp(src, formatted, src_len > out_len ? src_len : out_len) != 0;
        if (differs)
            changed = 1;

        if (!check_only)
        {
            if (use_stdout)
            {
                kn_write_out_buf(formatted, out_len);
            }
            else if (differs && !fmt_write_file(files[i], formatted, out_len))
            {
                kn_write_str("fmt: failed to write file: ");
                kn_write_str(files[i]);
                kn_write_str("\n");
                kn_free(formatted);
                kn_free(src);
                FMT_CLEANUP();
                return 1;
            }
        }

        kn_free(formatted);
        kn_free(src);
    }

    FMT_CLEANUP();
#undef FMT_CLEANUP
    return (check_only && changed) ? 1 : 0;
}
