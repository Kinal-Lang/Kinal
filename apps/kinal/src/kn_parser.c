#include "kn/parser.h"
#include "kn/diag.h"
#include "kn/source.h"
#include "kn/std.h"

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
    int next_anon_func_id;
    int next_block_id;
} Parser;

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
    if (t->type == TOK_ID)
    {
        p->pos++;
        return tok_text(t);
    }
    if (is_type_tok(t->type))
    {
        p->pos++;
        return type_token_name(t->type);
    }
    // Allow keyword segments in qualified names for builtin runtime types/modules,
    // e.g. IO.Type.Object.Function / IO.Type.Object.Block / IO.Async.Task.
    if (t->type == TOK_FUNCTION || t->type == TOK_BLOCK || t->type == TOK_CLASS ||
        t->type == TOK_INTERFACE || t->type == TOK_JUMP || t->type == TOK_RECORD ||
        t->type == TOK_ASYNC ||
        t->type == TOK_GET || t->type == TOK_SET)
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
    if (ty->kind == TY_VOID || ty->kind == TY_ARRAY)
        return;
    Type base = *ty;
    *ty = type_array(base.kind, len);
    ty->name = base.name;
}

static int parse_legacy_array_suffix(Parser *p, Type *ty, const Token *name_tok)
{
    if (!p || !ty || ty->kind == TY_ARRAY || !match(p, TOK_LBRACKET))
        return 0;
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
    return 1;
}

static const char *qualify_decl_name(Parser *p, const char *name)
{
    if (!name) return 0;
    if (p && p->decl_prefix && p->decl_prefix[0])
        return qualify_name(p->decl_prefix, name);
    return qualify_name(p ? p->unit_name : 0, name);
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
    p.next_anon_func_id = 0;
    p.next_block_id = 0;

    if (cur(&p)->type == TOK_UNIT)
    {
        p.pos++;
        p.unit_name = parse_qualified_name(&p);
        expect(&p, TOK_SEMI, "Expected Semicolon", "Missing ';' after Unit");
    }

    while (cur(&p)->type == TOK_GET)
        parse_get(&p, imports);

    while (cur(&p)->type != TOK_EOF)
    {
        if (cur(&p)->type == TOK_GET)
        {
            Token *g = cur(&p);
            kn_diag_report(src, KN_STAGE_PARSER, g->line, g->col, (int)g->length,
                "Invalid Get Position", "Get must appear at top-level after Unit and before declarations", "Get");
        }
        AttrList attrs = parse_attributes(&p);
        TokenType t0 = cur(&p)->type;
        TokenType t1 = peek(&p, 1)->type;
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
            // Parse Static/Const/Trusted/Unsafe modifiers for global variables
            int is_static = 0;
            Safety safety = SAFETY_SAFE;
            if (match(&p, TOK_STATIC)) is_static = 1;
            if (cur(&p)->type == TOK_SAFE || cur(&p)->type == TOK_TRUSTED || cur(&p)->type == TOK_UNSAFE)
                safety = parse_optional_safety(&p, SAFETY_SAFE);
            int is_const = 0;
            if (match(&p, TOK_CONST)) is_const = 1;
            Type ty = type_make(TY_UNKNOWN);
            if (match(&p, TOK_VAR)) ty = type_make(TY_UNKNOWN);
            else ty = parse_type(&p);
            Token *name_tok = cur(&p);
            expect(&p, TOK_ID, "Expected Identifier", "Expected variable name");
            const char *name = tok_text(name_tok);
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



