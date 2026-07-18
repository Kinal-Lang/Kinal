#include "kn/parser.h"
#include "kn/diag.h"
#include "kn/source.h"
#include "kn/std.h"
#include <stdio.h>

// ----------------------
// Parser
// ----------------------

typedef struct
{
    Token *toks;
    int count;
    int pos;
    const KnSource *src;
    const char *unit_name;
    const char *decl_prefix;
    MetaList *metas_out;
    ClassList *classes_out;
    InterfaceList *interfaces_out;
    StructList *structs_out;
    EnumList *enums_out;
    int in_block_literal;
    int stmt_depth;
    int saw_stmt_depth_limit;
    int next_anon_func_id;
    int next_block_id;
    int package_literal_depth;
} Parser;

typedef struct
{
    TokenType type;
    const char *text;
    size_t length;
} UnsafeAliasToken;

typedef struct
{
    UnsafeAliasToken *items;
    int count;
    int cap;
} UnsafeAliasTokenList;

typedef struct
{
    UnsafeAliasTokenList source;
    UnsafeAliasTokenList target;
} UnsafeAliasEntry;

typedef struct
{
    UnsafeAliasEntry *items;
    int count;
    int cap;
} UnsafeAliasList;

#define KN_PARSER_STMT_DEPTH_LIMIT 192

static void tok_push(Token **arr, int *count, int *cap, Token t)
{
    if (*count + 1 > *cap)
    {
        int new_cap = *cap ? *cap * 2 : 128;
        Token *n = (Token *)kn_malloc(sizeof(Token) * (size_t)new_cap);
        if (*arr)
        {
            kn_memcpy(n, *arr, sizeof(Token) * (size_t)(*count));
            kn_free(*arr);
        }
        *arr = n;
        *cap = new_cap;
    }
    (*arr)[(*count)++] = t;
}

static const char *dup_token_text(const Token *t);

static void unsafe_alias_push(UnsafeAliasList *list, UnsafeAliasEntry entry)
{
    if (!list) return;
    if (list->count + 1 > list->cap)
    {
        int new_cap = list->cap ? list->cap * 2 : 8;
        UnsafeAliasEntry *n = (UnsafeAliasEntry *)kn_malloc(sizeof(UnsafeAliasEntry) * (size_t)new_cap);
        if (!n) kn_die("out of memory");
        if (list->items)
        {
            kn_memcpy(n, list->items, sizeof(UnsafeAliasEntry) * (size_t)list->count);
            kn_free(list->items);
        }
        list->items = n;
        list->cap = new_cap;
    }
    list->items[list->count++] = entry;
}

static void unsafe_alias_token_push_dup(UnsafeAliasTokenList *list, const Token *tok)
{
    if (!list || !tok) return;
    if (list->count + 1 > list->cap)
    {
        int new_cap = list->cap ? list->cap * 2 : 4;
        UnsafeAliasToken *n = (UnsafeAliasToken *)kn_malloc(sizeof(UnsafeAliasToken) * (size_t)new_cap);
        if (!n) kn_die("out of memory");
        if (list->items)
        {
            kn_memcpy(n, list->items, sizeof(UnsafeAliasToken) * (size_t)list->count);
            kn_free(list->items);
        }
        list->items = n;
        list->cap = new_cap;
    }
    list->items[list->count].type = tok->type;
    list->items[list->count].text = dup_token_text(tok);
    list->items[list->count].length = tok->length;
    list->count++;
}

static Token *peek(Parser *p, int off)
{
    int idx = p->pos + off;
    if (idx < 0) idx = 0;
    if (idx >= p->count) idx = p->count - 1;
    return &p->toks[idx];
}

static Token *cur(Parser *p) { return peek(p, 0); }

static bool match(Parser *p, TokenType t)
{
    if (cur(p)->type == t)
    {
        p->pos++;
        return true;
    }
    return false;
}

static const char *tok_text(const Token *t)
{
    char *s = (char *)kn_malloc(t->length + 1);
    if (!s) kn_die("out of memory");
    kn_memcpy(s, t->start, t->length);
    s[t->length] = 0;
    return s;
}

static const char *tok_got(Parser *p)
{
    Token *t = cur(p);
    if (t->type == TOK_EOF || t->length == 0) return "<end-of-file>";
    if (t->type == TOK_BAD_STRING) return "<unterminated-string>";
    if (t->type == TOK_BAD_CHAR) return "<invalid-char>";
    return tok_text(t);
}

static int64_t parse_int_lit(const Token *t);
static double parse_float_lit(const Token *t);
static int decode_literal_escape(char esc, int64_t *out);
static int64_t parse_char_lit(const Token *t);
static void parse_string_lit_range(Parser *p, const Token *t, int start, int len, int raw, int fold_braces,
                                   const char **out_ptr, int *out_len);
static void parse_string_lit(Parser *p, const Token *t, const char **out_ptr, int *out_len);
static bool is_type_tok(TokenType t);
static bool looks_like_type_decl(Parser *p);
static Expr *parse_array_len_suffix(Parser *p, int64_t *out_len);
static void apply_array_type(Type *ty, int64_t len);
static int parse_legacy_array_suffix(Parser *p, Type *ty, const Token *name_tok);
static bool expect(Parser *p, TokenType t, const char *title, const char *detail);
static const char *parse_qualified_name(Parser *p);
static const char *qualify_name(const char *unit, const char *name);
static const char *qualify_decl_name(Parser *p, const char *name);
static AttrList parse_attributes(Parser *p);
static Expr *parse_expr(Parser *p);
static Stmt *parse_stmt(Parser *p);
static Stmt *parse_block(Parser *p);
static MetaDecl parse_meta_decl(Parser *p);
static ClassDecl parse_class(Parser *p);
static InterfaceDecl parse_interface(Parser *p);
static StructDecl parse_struct(Parser *p);
static EnumDecl parse_enum(Parser *p);

static bool token_text_eq(const Token *t, const char *text)
{
    if (!t || !text) return false;
    size_t n = kn_strlen(text);
    return t->length == n && kn_strncmp(t->start, text, n) == 0;
}

static bool token_is_literal_type(TokenType t)
{
    return t == TOK_NUMBER || t == TOK_STRING || t == TOK_CHAR_LIT || t == TOK_TRUE || t == TOK_FALSE ||
           t == TOK_NULL;
}

static bool token_is_keyword_type(TokenType t)
{
    return t >= TOK_UNIT && t <= TOK_TYPE_ANY;
}

static bool token_is_operator_or_punct(TokenType t)
{
    return t >= TOK_LPAREN;
}

static bool token_can_start_unsafe_alias_lhs(TokenType t, int allow_tokens)
{
    if (t == TOK_ID || token_is_literal_type(t)) return true;
    if (token_is_keyword_type(t)) return true;
    if (allow_tokens && token_is_operator_or_punct(t)) return true;
    return false;
}

static bool token_can_start_unsafe_alias_rhs(TokenType t, int allow_tokens)
{
    if (token_is_literal_type(t)) return true;
    if (token_is_keyword_type(t)) return true;
    if (allow_tokens && token_is_operator_or_punct(t)) return true;
    return false;
}

static bool unsafe_alias_is_directive_terminator(Token *toks, int count, int pos)
{
    if (!toks || pos < 0 || pos >= count) return false;
    if (toks[pos].type != TOK_SEMI) return false;
    if (pos + 1 >= count) return true;
    if (toks[pos + 1].type == TOK_EOF) return true;
    return toks[pos + 1].line != toks[pos].line;
}

static const char *dup_token_text(const Token *t)
{
    char *s;
    if (!t) return 0;
    s = (char *)kn_malloc(t->length + 1);
    if (!s) kn_die("out of memory");
    kn_memcpy(s, t->start, t->length);
    s[t->length] = 0;
    return s;
}

static bool unsafe_alias_token_matches(const UnsafeAliasToken *spec, const Token *tok)
{
    return spec && tok &&
           tok->type == spec->type &&
           tok->length == spec->length &&
           kn_strncmp(tok->start, spec->text, tok->length) == 0;
}

static bool unsafe_alias_sequence_matches(const UnsafeAliasTokenList *spec, Token *toks, int count, int pos)
{
    if (!spec || !spec->items || spec->count <= 0) return false;
    if (!toks || pos < 0 || pos + spec->count > count) return false;
    for (int i = 0; i < spec->count; i++)
        if (!unsafe_alias_token_matches(&spec->items[i], &toks[pos + i]))
            return false;
    return true;
}

static void emit_unsafe_alias_target(Token **out_toks, int *out_count, int *out_cap,
                                     const UnsafeAliasTokenList *target, const Token *site)
{
    if (!out_toks || !out_count || !out_cap || !target || !site) return;
    for (int i = 0; i < target->count; i++)
    {
        Token t = *site;
        t.type = target->items[i].type;
        t.start = target->items[i].text;
        t.length = target->items[i].length;
        tok_push(out_toks, out_count, out_cap, t);
    }
}

static void preprocess_unsafe_aliases(const KnSource *src, Token **toks_io, int *count_io, int *cap_io, int *has_unsafe_out)
{
    Token *toks = toks_io ? *toks_io : 0;
    int count = count_io ? *count_io : 0;
    int pos = 0;
    int had_unsafe = 0;
    UnsafeAliasList aliases;
    kn_memset(&aliases, 0, sizeof(aliases));
    if (!toks || count <= 0)
    {
        if (has_unsafe_out) *has_unsafe_out = 0;
        return;
    }

    while (pos < count)
    {
        int unsafe_count = 0;
        int allow_tokens = 0;
        UnsafeAliasEntry entry;
        kn_memset(&entry, 0, sizeof(entry));

        while (pos + unsafe_count < count && toks[pos + unsafe_count].type == TOK_UNSAFE)
            unsafe_count++;
        if (unsafe_count == 0)
            break;
        if (!((unsafe_count == 1 || unsafe_count == 3) &&
              pos + unsafe_count < count &&
              toks[pos + unsafe_count].type == TOK_ALIAS))
            break;

        allow_tokens = unsafe_count == 3;
        had_unsafe = 1;
        pos += unsafe_count + 1;
        if (allow_tokens)
        {
            Token *first_source = pos < count ? &toks[pos] : &toks[count - 1];
            while (pos < count && toks[pos].type != TOK_BY)
            {
                Token *part = &toks[pos];
                if (!token_can_start_unsafe_alias_lhs(part->type, 1))
                {
                    kn_diag_report(src, KN_STAGE_PARSER, part->line, part->col, (int)(part->length ? part->length : 1),
                                   "Invalid Unsafe Alias",
                                   "Unsafe Unsafe Unsafe Alias requires token/literal/keyword source pieces",
                                   0);
                    goto preprocess_done;
                }
                unsafe_alias_token_push_dup(&entry.source, part);
                pos++;
            }
            if (entry.source.count <= 0)
            {
                kn_diag_report(src, KN_STAGE_PARSER, first_source->line, first_source->col,
                               (int)(first_source->length ? first_source->length : 1),
                               "Invalid Unsafe Alias",
                               "Unsafe Unsafe Unsafe Alias requires a token/literal/keyword source",
                               0);
                goto preprocess_done;
            }
            if (pos >= count || toks[pos].type != TOK_BY)
            {
                Token *site = pos < count ? &toks[pos] : &toks[count - 1];
                kn_diag_report(src, KN_STAGE_PARSER, site->line, site->col, (int)(site->length ? site->length : 1),
                               "Expected By",
                               "Unsafe Alias requires 'By'",
                               0);
                goto preprocess_done;
            }
            pos++;
            while (pos < count && !unsafe_alias_is_directive_terminator(toks, count, pos))
            {
                Token *part = &toks[pos];
                if (!token_can_start_unsafe_alias_rhs(part->type, 1))
                {
                    kn_diag_report(src, KN_STAGE_PARSER, part->line, part->col, (int)(part->length ? part->length : 1),
                                   "Invalid Unsafe Alias",
                                   "Unsafe Unsafe Unsafe Alias requires token/literal/keyword target pieces",
                                   0);
                    goto preprocess_done;
                }
                unsafe_alias_token_push_dup(&entry.target, part);
                pos++;
            }
            if (entry.target.count <= 0)
            {
                Token *site = pos < count ? &toks[pos] : &toks[count - 1];
                kn_diag_report(src, KN_STAGE_PARSER, site->line, site->col, (int)(site->length ? site->length : 1),
                               "Invalid Unsafe Alias",
                               "Unsafe Unsafe Unsafe Alias requires a token/literal/keyword target",
                               0);
                goto preprocess_done;
            }
        }
        else
        {
            Token *lhs;
            Token *rhs;
            if (pos >= count)
                goto preprocess_done;
            lhs = &toks[pos++];
            if (!token_can_start_unsafe_alias_lhs(lhs->type, 0))
            {
                kn_diag_report(src, KN_STAGE_PARSER, lhs->line, lhs->col, (int)(lhs->length ? lhs->length : 1),
                               "Invalid Unsafe Alias",
                               "Unsafe Alias only supports keyword or literal aliases",
                               0);
                goto preprocess_done;
            }
            unsafe_alias_token_push_dup(&entry.source, lhs);
            if (pos >= count || toks[pos].type != TOK_BY)
            {
                Token *site = pos < count ? &toks[pos] : lhs;
                kn_diag_report(src, KN_STAGE_PARSER, site->line, site->col, (int)(site->length ? site->length : 1),
                               "Expected By",
                               "Unsafe Alias requires 'By'",
                               0);
                goto preprocess_done;
            }
            pos++;
            if (pos >= count)
                goto preprocess_done;
            rhs = &toks[pos++];
            if (!token_can_start_unsafe_alias_rhs(rhs->type, 0))
            {
                kn_diag_report(src, KN_STAGE_PARSER, rhs->line, rhs->col, (int)(rhs->length ? rhs->length : 1),
                               "Invalid Unsafe Alias",
                               "Unsafe Alias only supports keyword or literal targets",
                               0);
                goto preprocess_done;
            }
            unsafe_alias_token_push_dup(&entry.target, rhs);
        }
        if (pos >= count || toks[pos].type != TOK_SEMI)
        {
            Token *site = pos < count ? &toks[pos] : &toks[count - 1];
            kn_diag_report(src, KN_STAGE_PARSER, site->line, site->col, (int)(site->length ? site->length : 1),
                           "Expected Semicolon",
                           "Missing ';' after Unsafe Alias",
                           0);
            goto preprocess_done;
        }
        pos++;
        unsafe_alias_push(&aliases, entry);
    }

preprocess_done:
    {
        Token *out_toks = 0;
        int out_count = 0;
        int out_cap = 0;
        for (int i = pos; i < count;)
        {
            int matched = 0;
            for (int j = aliases.count - 1; j >= 0; j--)
            {
                UnsafeAliasEntry *entry = &aliases.items[j];
                if (!unsafe_alias_sequence_matches(&entry->source, toks, count, i))
                    continue;
                emit_unsafe_alias_target(&out_toks, &out_count, &out_cap, &entry->target, &toks[i]);
                i += entry->source.count;
                matched = 1;
                break;
            }
            if (matched)
                continue;
            tok_push(&out_toks, &out_count, &out_cap, toks[i]);
            i++;
        }
        if (out_count == 0 || out_toks[out_count - 1].type != TOK_EOF)
            tok_push(&out_toks, &out_count, &out_cap, toks[count - 1]);
        if (toks_io) *toks_io = out_toks;
        if (count_io) *count_io = out_count;
        if (cap_io) *cap_io = out_cap;
        if (toks) kn_free(toks);
    }
    if (has_unsafe_out) *has_unsafe_out = had_unsafe;
}

static int unsafe_alias_prefix_length(Parser *p)
{
    int unsafe_count = 0;
    if (!p) return 0;
    while (peek(p, unsafe_count)->type == TOK_UNSAFE)
        unsafe_count++;
    if ((unsafe_count == 1 || unsafe_count == 3) &&
        peek(p, unsafe_count)->type == TOK_ALIAS)
        return unsafe_count + 1;
    return 0;
}

static bool looks_like_unsafe_alias_directive(Parser *p)
{
    return unsafe_alias_prefix_length(p) > 0;
}

static void skip_top_level_directive(Parser *p)
{
    if (!p) return;
    while (cur(p)->type != TOK_EOF && cur(p)->type != TOK_SEMI)
        p->pos++;
    if (cur(p)->type == TOK_SEMI)
        p->pos++;
}

static TypeKind builtin_type_kind(const char *name)
{
    if (!name) return TY_UNKNOWN;
    if (kn_strcmp(name, "void") == 0) return TY_VOID;
    if (kn_strcmp(name, "bool") == 0) return TY_BOOL;
    if (kn_strcmp(name, "byte") == 0) return TY_BYTE;
    if (kn_strcmp(name, "char") == 0) return TY_CHAR;
    if (kn_strcmp(name, "int") == 0) return TY_INT;
    if (kn_strcmp(name, "float") == 0) return TY_FLOAT;
    if (kn_strcmp(name, "f16") == 0) return TY_F16;
    if (kn_strcmp(name, "f32") == 0) return TY_F32;
    if (kn_strcmp(name, "f64") == 0) return TY_F64;
    if (kn_strcmp(name, "f128") == 0) return TY_F128;
    if (kn_strcmp(name, "i8") == 0) return TY_I8;
    if (kn_strcmp(name, "i16") == 0) return TY_I16;
    if (kn_strcmp(name, "i32") == 0) return TY_I32;
    if (kn_strcmp(name, "i64") == 0) return TY_I64;
    if (kn_strcmp(name, "u8") == 0) return TY_U8;
    if (kn_strcmp(name, "u16") == 0) return TY_U16;
    if (kn_strcmp(name, "u32") == 0) return TY_U32;
    if (kn_strcmp(name, "u64") == 0) return TY_U64;
    if (kn_strcmp(name, "isize") == 0) return TY_ISIZE;
    if (kn_strcmp(name, "usize") == 0) return TY_USIZE;
    if (kn_strcmp(name, "string") == 0) return TY_STRING;
    if (kn_strcmp(name, "any") == 0) return TY_ANY;
    return TY_UNKNOWN;
}

static bool token_is_builtin_type_name(const Token *t)
{
    if (!t || t->type != TOK_ID) return false;
    if (token_text_eq(t, "void")) return true;
    if (token_text_eq(t, "bool")) return true;
    if (token_text_eq(t, "byte")) return true;
    if (token_text_eq(t, "char")) return true;
    if (token_text_eq(t, "int")) return true;
    if (token_text_eq(t, "float")) return true;
    if (token_text_eq(t, "f16")) return true;
    if (token_text_eq(t, "f32")) return true;
    if (token_text_eq(t, "f64")) return true;
    if (token_text_eq(t, "f128")) return true;
    if (token_text_eq(t, "i8")) return true;
    if (token_text_eq(t, "i16")) return true;
    if (token_text_eq(t, "i32")) return true;
    if (token_text_eq(t, "i64")) return true;
    if (token_text_eq(t, "u8")) return true;
    if (token_text_eq(t, "u16")) return true;
    if (token_text_eq(t, "u32")) return true;
    if (token_text_eq(t, "u64")) return true;
    if (token_text_eq(t, "isize")) return true;
    if (token_text_eq(t, "usize")) return true;
    if (token_text_eq(t, "string")) return true;
    if (token_text_eq(t, "any")) return true;
    if (token_text_eq(t, "list")) return true;
    if (token_text_eq(t, "dict")) return true;
    if (token_text_eq(t, "set")) return true;
    return false;
}

static bool decl_name_follow_tok(TokenType t)
{
    return t == TOK_SEMI || t == TOK_ASSIGN || t == TOK_LBRACKET || t == TOK_COMMA || t == TOK_EOF;
}

static const char *type_token_name(TokenType t)
{
    switch (t)
    {
    case TOK_TYPE_VOID: return "void";
    case TOK_TYPE_BOOL: return "bool";
    case TOK_TYPE_BYTE: return "byte";
    case TOK_TYPE_CHAR: return "char";
    case TOK_TYPE_INT: return "int";
    case TOK_TYPE_FLOAT: return "float";
    case TOK_TYPE_F16: return "f16";
    case TOK_TYPE_F32: return "f32";
    case TOK_TYPE_F64: return "f64";
    case TOK_TYPE_F128: return "f128";
    case TOK_TYPE_I8: return "i8";
    case TOK_TYPE_I16: return "i16";
    case TOK_TYPE_I32: return "i32";
    case TOK_TYPE_I64: return "i64";
    case TOK_TYPE_U8: return "u8";
    case TOK_TYPE_U16: return "u16";
    case TOK_TYPE_U32: return "u32";
    case TOK_TYPE_U64: return "u64";
    case TOK_TYPE_ISIZE: return "isize";
    case TOK_TYPE_USIZE: return "usize";
    case TOK_TYPE_STRING: return "string";
    case TOK_TYPE_ANY: return "any";
    default: return 0;
    }
}

static const char *parse_name_token(Parser *p, const char *title, const char *detail)
{
    Token *t = cur(p);
    // A keyword is unambiguous after '.' or where a declaration grammar already
    // requires a member name. This keeps keywords reserved in ordinary identifier
    // positions while allowing names such as StatementKind.Return and IO.Type.Class.
    if (t->type == TOK_ID || token_is_keyword_type(t->type))
    {
        p->pos++;
        return tok_text(t);
    }
    expect(p, TOK_ID, title, detail);
    return "";
}

static void recover_after_expect(Parser *p, TokenType expected)
{
    if (!p || cur(p)->type == TOK_EOF)
        return;

    if (expected == TOK_SEMI)
    {
        // Panic-mode sync for statement termination:
        // keep current token if it already looks like next statement start.
        while (cur(p)->type != TOK_EOF &&
               cur(p)->type != TOK_SEMI &&
               cur(p)->type != TOK_RBRACE &&
               cur(p)->type != TOK_RETURN &&
               cur(p)->type != TOK_IF &&
               cur(p)->type != TOK_SWITCH &&
               cur(p)->type != TOK_FOR &&
               cur(p)->type != TOK_WHILE &&
               cur(p)->type != TOK_BREAK &&
               cur(p)->type != TOK_CONTINUE &&
               cur(p)->type != TOK_TRY &&
               cur(p)->type != TOK_THROW &&
               cur(p)->type != TOK_CONST &&
               cur(p)->type != TOK_VAR &&
               cur(p)->type != TOK_BLOCK &&
               cur(p)->type != TOK_CLASS &&
               cur(p)->type != TOK_STRUCT &&
               cur(p)->type != TOK_ENUM &&
               cur(p)->type != TOK_INTERFACE &&
               cur(p)->type != TOK_FUNCTION &&
               cur(p)->type != TOK_EXTERN &&
               cur(p)->type != TOK_GET &&
               !is_type_tok(cur(p)->type) &&
               cur(p)->type != TOK_ID)
            p->pos++;
        if (cur(p)->type == TOK_SEMI)
            p->pos++;
        return;
    }

    if (expected == TOK_RPAREN)
    {
        while (cur(p)->type != TOK_EOF &&
               cur(p)->type != TOK_RPAREN &&
               cur(p)->type != TOK_SEMI &&
               cur(p)->type != TOK_LBRACE &&
               cur(p)->type != TOK_RBRACE)
            p->pos++;
        if (cur(p)->type == TOK_RPAREN)
            p->pos++;
        return;
    }

    if (expected == TOK_RBRACKET)
    {
        while (cur(p)->type != TOK_EOF &&
               cur(p)->type != TOK_RBRACKET &&
               cur(p)->type != TOK_SEMI &&
               cur(p)->type != TOK_RBRACE)
            p->pos++;
        if (cur(p)->type == TOK_RBRACKET)
            p->pos++;
        return;
    }

    if (expected == TOK_RBRACE)
    {
        while (cur(p)->type != TOK_EOF &&
               cur(p)->type != TOK_RBRACE)
            p->pos++;
        if (cur(p)->type == TOK_RBRACE)
            p->pos++;
        return;
    }

    // Generic single-token recovery.
    p->pos++;
}

static bool expect(Parser *p, TokenType t, const char *title, const char *detail)
{
    if (!match(p, t))
    {
        Token *got = cur(p);
        kn_diag_report(p->src, KN_STAGE_PARSER, got->line, got->col, (int)got->length, title, detail, tok_got(p));
        recover_after_expect(p, t);
        return false;
    }
    return true;
}

static void expect_semi_after_expr(Parser *p, Expr *e)
{
    if (match(p, TOK_SEMI))
        return;

    Token *got = cur(p);
    if (e && got->line > e->line)
    {
        // Common typo: newline starts a new statement but previous expression
        // forgot ';'. Point to previous expression for clearer action.
        int col = e->col > 0 ? e->col : 1;
        size_t line_len = 0;
        const char *line_text = kn_source_get_line(p->src, e->line, &line_len);
        if (line_text && line_len > 0)
        {
            size_t i = line_len;
            while (i > 0)
            {
                char ch = line_text[i - 1];
                if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
                {
                    i--;
                    continue;
                }
                break;
            }
            if (i > 0)
                col = (int)i + 1; // caret after the last non-space char
        }
        kn_diag_report(p->src, KN_STAGE_PARSER, e->line, col, 1,
                       "Expected Semicolon", "Missing ';' after expression", 0);
    }
    else
    {
        kn_diag_report(p->src, KN_STAGE_PARSER, got->line, got->col, (int)got->length,
                       "Expected Semicolon", "Missing ';' after expression", tok_got(p));
    }
    recover_after_expect(p, TOK_SEMI);
}

static const char *parse_qualified_type_name(Parser *p)
{
    const char *parts[16];
    int part_count = 0;

    const char *first = parse_name_token(p, "Expected Identifier", "Expected name");
    if (!first) return 0;
    parts[part_count++] = first;

    while (match(p, TOK_DOT))
    {
        const char *next = parse_name_token(p, "Expected Identifier", "Expected name");
        if (part_count < (int)(sizeof(parts) / sizeof(parts[0])))
            parts[part_count++] = next;
    }

    size_t total = 0;
    for (int i = 0; i < part_count; i++)
        total += kn_strlen(parts[i]) + (i > 0 ? 1 : 0);
    char *out = (char *)kn_malloc(total + 1);
    if (!out) kn_die("out of memory");
    size_t off = 0;
    for (int i = 0; i < part_count; i++)
    {
        if (i > 0) out[off++] = '.';
        size_t len = kn_strlen(parts[i]);
        kn_memcpy(out + off, parts[i], len);
        off += len;
    }
    out[off] = 0;
    return out;
}

static TypeKind qualified_builtin_kind(const char *qname)
{
    if (!qname) return TY_UNKNOWN;
    const char *prefix = "IO.Type.";
    size_t plen = kn_strlen(prefix);
    if (kn_strncmp(qname, prefix, plen) == 0)
        return builtin_type_kind(qname + plen);
    return builtin_type_kind(qname);
}

static const char *normalize_type_alias(const char *qname)
{
    if (!qname) return qname;
    if (kn_strcmp(qname, "list") == 0)
        return "IO.Collection.list";
    if (kn_strcmp(qname, "dict") == 0)
        return "IO.Collection.dict";
    if (kn_strcmp(qname, "set") == 0)
        return "IO.Collection.set";
    const char *obj_prefix = "Object.";
    size_t obj_len = kn_strlen(obj_prefix);
    if (kn_strncmp(qname, obj_prefix, obj_len) == 0)
    {
        const char *full_prefix = "IO.Type.Object.";
        size_t full_len = kn_strlen(full_prefix);
        size_t tail_len = kn_strlen(qname + obj_len);
        char *out = (char *)kn_malloc(full_len + tail_len + 1);
        if (!out) kn_die("out of memory");
        kn_memcpy(out, full_prefix, full_len);
        kn_memcpy(out + full_len, qname + obj_len, tail_len);
        out[full_len + tail_len] = 0;
        return out;
    }
    return qname;
}

static Expr *parse_array_len_suffix(Parser *p, int64_t *out_len)
{
    Expr *expr = 0;
    if (out_len) *out_len = -1;
    if (cur(p)->type == TOK_NUMBER)
    {
        Token *num = cur(p);
        p->pos++;
        if (out_len) *out_len = parse_int_lit(num);
    }
    else if (cur(p)->type != TOK_RBRACKET)
    {
        expr = parse_expr(p);
    }
    expect(p, TOK_RBRACKET, "Expected ']'", "Missing ']'");
    return expr;
}

static void apply_array_type(Type *ty, int64_t len)
{
    if (!ty) return;
    if (ty->kind == TY_UNKNOWN)
    {
        *ty = type_array(TY_UNKNOWN, len);
        return;
    }
    if (ty->kind == TY_VOID)
        return;
    Type base = *ty;
    *ty = type_array_of(base, len);
}

static int parse_legacy_array_suffix(Parser *p, Type *ty, const Token *name_tok)
{
    if (!p || !ty || !match(p, TOK_LBRACKET))
        return 0;
    int parsed = 0;
    do
    {
        Token *lb = peek(p, -1);
        int64_t len = -1;
        Expr *len_expr = parse_array_len_suffix(p, &len);
        apply_array_type(ty, len);
        ty->array_len_expr = len_expr;
        if (name_tok)
        {
            kn_diag_warn(p->src, KN_STAGE_PARSER, 1, lb->line, lb->col, (int)(lb->length ? lb->length : 1),
                         "Legacy Array Syntax", "Use 'Type[] name' instead of 'Type name[]'");
        }
        parsed = 1;
    } while (match(p, TOK_LBRACKET));
    return parsed;
}

static const char *qualify_decl_name(Parser *p, const char *name)
{
    if (!name) return 0;
    if (p && p->decl_prefix && p->decl_prefix[0])
        return qualify_name(p->decl_prefix, name);
    return qualify_name(p ? p->unit_name : 0, name);
}

static void parser_skip_to_enclosing_block_end(Parser *p)
{
    int nested_braces = 0;
    while (cur(p)->type != TOK_EOF)
    {
        TokenType t = cur(p)->type;
        if (t == TOK_LBRACE)
        {
            nested_braces++;
            p->pos++;
            continue;
        }
        if (t == TOK_RBRACE)
        {
            if (nested_braces == 0)
                break;
            nested_braces--;
            p->pos++;
            continue;
        }
        p->pos++;
    }
}

static bool parser_enter_stmt(Parser *p, Token *site)
{
    if (!p) return false;
    p->stmt_depth++;
    if (p->stmt_depth <= KN_PARSER_STMT_DEPTH_LIMIT)
        return true;

    if (!p->saw_stmt_depth_limit)
    {
        p->saw_stmt_depth_limit = 1;
        kn_diag_report(p->src, KN_STAGE_PARSER,
                       site ? site->line : cur(p)->line,
                       site ? site->col : cur(p)->col,
                       (int)((site && site->length) ? site->length : 1),
                       "Statement Too Deep",
                       "Statement nesting exceeds parser recursion limit",
                       tok_got(p));
    }
    parser_skip_to_enclosing_block_end(p);
    p->stmt_depth--;
    return false;
}

static void parser_leave_stmt(Parser *p)
{
    if (p && p->stmt_depth > 0)
        p->stmt_depth--;
}


#include "parser/kn_parser_type_expr.inc"
#include "parser/kn_parser_stmt.inc"
#include "parser/kn_parser_decl.inc"

void parse_program(const KnSource *src, MetaList *metas, FuncList *funcs, ImportList *imports, ClassList *classes,
                   InterfaceList *interfaces, StructList *structs, EnumList *enums, StmtList *globals)
{
    Lexer l;
    lex_init(&l, src ? src->text : "");
    Token *toks = 0;
    int count = 0, cap = 0;
    for (;;)
    {
        Token t = lex_next(&l);
        tok_push(&toks, &count, &cap, t);
        if (t.type == TOK_EOF) break;
    }
    int has_unsafe_alias = 0;
    preprocess_unsafe_aliases(src, &toks, &count, &cap, &has_unsafe_alias);
    Parser p;
    p.toks = toks;
    p.count = count;
    p.pos = 0;
    p.src = src;
    p.unit_name = 0;
    p.decl_prefix = 0;
    p.metas_out = metas;
    p.classes_out = classes;
    p.interfaces_out = interfaces;
    p.structs_out = structs;
    p.enums_out = enums;
    p.in_block_literal = 0;
    p.stmt_depth = 0;
    p.saw_stmt_depth_limit = 0;
    p.next_anon_func_id = 0;
    p.next_block_id = 0;
    p.package_literal_depth = 0;

    if (has_unsafe_alias && cur(&p)->type == TOK_UNIT)
    {
        Token *u = cur(&p);
        kn_diag_report(src, KN_STAGE_PARSER, u->line, u->col, (int)(u->length ? u->length : 1),
                       "Invalid Unit With Unsafe Alias",
                       "Files that use Unsafe Alias cannot declare Unit",
                       0);
        skip_top_level_directive(&p);
    }

    if (!has_unsafe_alias && cur(&p)->type == TOK_UNIT)
    {
        p.pos++;
        p.unit_name = parse_qualified_name(&p);
        expect(&p, TOK_SEMI, "Expected Semicolon", "Missing ';' after Unit");
    }

    while (cur(&p)->type == TOK_GET || cur(&p)->type == TOK_ALIAS)
    {
        if (cur(&p)->type == TOK_GET)
            parse_get(&p, imports);
        else
            parse_alias(&p, imports);
    }

    while (cur(&p)->type != TOK_EOF)
    {
        if (looks_like_unsafe_alias_directive(&p))
        {
            Token *u = cur(&p);
            kn_diag_report(src, KN_STAGE_PARSER, u->line, u->col, (int)(u->length ? u->length : 1),
                           "Invalid Unsafe Alias Position",
                           "Unsafe Alias must appear in the file prologue before Unit, Get, Alias, and declarations",
                           0);
            skip_top_level_directive(&p);
            continue;
        }
        if (cur(&p)->type == TOK_GET || cur(&p)->type == TOK_ALIAS)
        {
            Token *g = cur(&p);
            kn_diag_report(src, KN_STAGE_PARSER, g->line, g->col, (int)g->length,
                cur(&p)->type == TOK_GET ? "Invalid Get Position" : "Invalid Alias Position",
                cur(&p)->type == TOK_GET
                    ? "Get must appear at top-level after Unit and before declarations"
                    : "Alias must appear at top-level after Unit/Get and before declarations",
                cur(&p)->type == TOK_GET ? "Get" : "Alias");
        }
        AttrList attrs = parse_attributes(&p);
        TokenType t0 = cur(&p)->type;
        TokenType t1 = peek(&p, 1)->type;
        if (starts_invalid_top_level_global_modifier(&p))
        {
            Token *g = cur(&p);
            kn_diag_report(src, KN_STAGE_PARSER, g->line, g->col, (int)g->length,
                           "Invalid Global Modifier",
                           "Top-level global variables cannot use function modifiers like Static/Extern/Delegate/Safe/Trusted/Unsafe/Async",
                           tok_got(&p));
            sync_top_level(&p);
            continue;
        }
        if (cur(&p)->type == TOK_ID && token_text_eq(cur(&p), "Meta") &&
            peek(&p, 1)->type == TOK_ID && peek(&p, 2)->type == TOK_LPAREN)
        {
            if (attrs.count > 0)
            {
                Token *g = cur(&p);
                kn_diag_report(src, KN_STAGE_PARSER, g->line, g->col, (int)g->length,
                               "Invalid Attribute", "Attributes are not supported on Meta declarations", tok_got(&p));
            }
            if (metas)
            {
                MetaDecl m = parse_meta_decl(&p);
                metalist_push(metas, m);
            }
            else
            {
                (void)parse_meta_decl(&p);
            }
            continue;
        }
        if (t0 == TOK_ENUM)
        {
            EnumDecl e = parse_enum(&p);
            e.attrs = attrs;
            enumlist_push(enums, e);
            continue;
        }
        if (t0 == TOK_STRUCT)
        {
            StructDecl s = parse_struct(&p);
            s.attrs = attrs;
            structlist_push(structs, s);
            continue;
        }
        if (t0 == TOK_INTERFACE || ((t0 == TOK_PUBLIC || t0 == TOK_PRIVATE || t0 == TOK_PROTECTED || t0 == TOK_INTERNAL) && t1 == TOK_INTERFACE))
        {
            InterfaceDecl i = parse_interface(&p);
            i.attrs = attrs;
            interfacelist_push(interfaces, i);
            continue;
        }
        if (looks_like_class_decl(&p))
        {
            // try class parse
            ClassDecl c = parse_class(&p);
            c.attrs = attrs;
            classlist_push(classes, c);
            continue;
        }
        if (looks_like_type_decl(&p))
        {
            int errs_before = kn_diag_error_count();
            if (attrs.count > 0)
            {
                Token *g = cur(&p);
                kn_diag_report(src, KN_STAGE_PARSER, g->line, g->col, (int)g->length,
                    "Invalid Attribute", "Attributes are not supported on global variables", tok_got(&p));
            }
            int is_const = 0;
            if (match(&p, TOK_CONST)) is_const = 1;
            Type ty = type_make(TY_UNKNOWN);
            if (match(&p, TOK_VAR)) ty = type_make(TY_UNKNOWN);
            else ty = parse_type(&p);
            Token *name_tok = cur(&p);
            expect(&p, TOK_ID, "Expected Identifier", "Expected variable name");
            const char *name = tok_text(name_tok);
            NameList package_fields;
            kn_memset(&package_fields, 0, sizeof(package_fields));
            if (cur(&p)->type == TOK_LT)
                package_fields = parse_package_field_alias_list(&p);
            (void)parse_legacy_array_suffix(&p, &ty, name_tok);
            int skip_semi = 0;
            Expr *init = parse_optional_decl_initializer(&p, name_tok, &skip_semi);
            if (!skip_semi)
                expect(&p, TOK_SEMI, "Expected Semicolon", "Missing ';' after variable declaration");
            if (kn_diag_error_count() > errs_before)
                continue;
            Stmt *gv = new_stmt(ST_VAR, name_tok->line, name_tok->col);
            gv->v.var.type = ty;
            gv->v.var.name = name;
            gv->v.var.name_line = name_tok->line;
            gv->v.var.name_col = name_tok->col;
            gv->v.var.package_fields = package_fields;
            gv->v.var.init = init;
            gv->v.var.is_const = is_const;
            gv->v.var.src = src;
            gv->v.var.unit = p.unit_name;
            if (globals)
                stmtlist_push(globals, gv);
            continue;
        }
        if (!is_top_level_func_start(t0))
        {
            Token *g = cur(&p);
            kn_diag_report(src, KN_STAGE_PARSER, g->line, g->col, (int)g->length,
                           "Unexpected Top-Level Token",
                           "Expected a top-level declaration (Function/Class/Struct/Enum/Interface/variable declaration)",
                           tok_got(&p));
            sync_top_level(&p);
            continue;
        }
        Func f = parse_func(&p);
        f.attrs = attrs;
        funclist_push(funcs, f);
    }
}



