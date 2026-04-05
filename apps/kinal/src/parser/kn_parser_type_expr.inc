// Internal parser fragment: type parsing and expressions

static Type parse_type(Parser *p)
{
    Type t = type_make(TY_UNKNOWN);
    if (is_type_tok(cur(p)->type))
    {
        TokenType tt = cur(p)->type;
        p->pos++;
        switch (tt)
        {
        case TOK_TYPE_VOID: t = type_make(TY_VOID); break;
        case TOK_TYPE_BOOL: t = type_make(TY_BOOL); break;
        case TOK_TYPE_BYTE: t = type_make(TY_BYTE); break;
        case TOK_TYPE_CHAR: t = type_make(TY_CHAR); break;
        case TOK_TYPE_INT: t = type_make(TY_INT); break;
        case TOK_TYPE_FLOAT: t = type_make(TY_FLOAT); break;
        case TOK_TYPE_F16: t = type_make(TY_F16); break;
        case TOK_TYPE_F32: t = type_make(TY_F32); break;
        case TOK_TYPE_F64: t = type_make(TY_F64); break;
        case TOK_TYPE_F128: t = type_make(TY_F128); break;
        case TOK_TYPE_I8: t = type_make(TY_I8); break;
        case TOK_TYPE_I16: t = type_make(TY_I16); break;
        case TOK_TYPE_I32: t = type_make(TY_I32); break;
        case TOK_TYPE_I64: t = type_make(TY_I64); break;
        case TOK_TYPE_U8: t = type_make(TY_U8); break;
        case TOK_TYPE_U16: t = type_make(TY_U16); break;
        case TOK_TYPE_U32: t = type_make(TY_U32); break;
        case TOK_TYPE_U64: t = type_make(TY_U64); break;
        case TOK_TYPE_ISIZE: t = type_make(TY_ISIZE); break;
        case TOK_TYPE_USIZE: t = type_make(TY_USIZE); break;
        case TOK_TYPE_STRING: t = type_make(TY_STRING); break;
        case TOK_TYPE_ANY: t = type_make(TY_ANY); break;
        default: break;
        }
    }
    else if (cur(p)->type == TOK_ID)
    {
        const char *raw_name = parse_qualified_type_name(p);
        const char *qname = normalize_type_alias(raw_name);
        TypeKind k = qualified_builtin_kind(qname);
        if (k != TY_UNKNOWN)
            t = type_make(k);
        else
            t = type_class(qname);
    }

    // pointer declarator (multi-level)
    if (t.kind != TY_UNKNOWN)
    {
        int ptr_depth = 0;
        while (match(p, TOK_STAR))
            ptr_depth++;
        if (ptr_depth > 0)
        {
            Type base = t;
            t = type_ptr(base.kind);
            t.ptr_depth = ptr_depth;
            if (base.kind == TY_CLASS || base.kind == TY_STRUCT || base.kind == TY_ENUM)
                t.name = base.name;
        }
    }

    if (t.kind != TY_UNKNOWN && match(p, TOK_LBRACKET))
    {
        int64_t len = parse_array_len_suffix(p);
        apply_array_type(&t, len);
    }

    return t;
}

static bool type_name_segment_tok(TokenType t)
{
    if (t == TOK_ID || is_type_tok(t))
        return true;
    return t == TOK_FUNCTION || t == TOK_BLOCK || t == TOK_CLASS ||
           t == TOK_INTERFACE || t == TOK_STRUCT || t == TOK_ENUM ||
           t == TOK_ASYNC ||
           t == TOK_RECORD || t == TOK_JUMP;
}

static bool looks_like_type_arg_list(Parser *p, int off, int require_call_lparen, int *end_off)
{
    if (peek(p, off)->type != TOK_LT)
        return false;
    off++;

    int count = 0;
    for (;;)
    {
        TokenType tt = peek(p, off)->type;
        if (!type_name_segment_tok(tt))
            return false;
        off++;

        while (peek(p, off)->type == TOK_DOT)
        {
            off++;
            if (!type_name_segment_tok(peek(p, off)->type))
                return false;
            off++;
        }

        while (peek(p, off)->type == TOK_STAR)
            off++;

        if (peek(p, off)->type == TOK_LBRACKET)
        {
            off++;
            if (peek(p, off)->type == TOK_NUMBER)
                off++;
            if (peek(p, off)->type != TOK_RBRACKET)
                return false;
            off++;
        }

        count++;
        if (peek(p, off)->type == TOK_COMMA)
        {
            off++;
            continue;
        }
        if (peek(p, off)->type == TOK_GT)
        {
            off++;
            break;
        }
        return false;
    }

    if (count <= 0)
        return false;

    if (require_call_lparen && peek(p, off)->type != TOK_LPAREN)
        return false;

    if (end_off) *end_off = off;
    return true;
}

static TypeList parse_type_arg_list(Parser *p)
{
    TypeList list;
    kn_memset(&list, 0, sizeof(list));
    expect(p, TOK_LT, "Expected '<'", "Missing '<' before generic type arguments");
    for (;;)
    {
        Type ty = parse_type(p);
        if (ty.kind == TY_UNKNOWN)
            kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                           "Invalid Generic Type", "Expected a type argument", tok_got(p));
        typelist_push(&list, ty);
        if (match(p, TOK_COMMA))
            continue;
        expect(p, TOK_GT, "Expected '>'", "Missing '>' after generic type arguments");
        break;
    }
    return list;
}

static void parse_type_param_list(Parser *p, const char ***out_items, int *out_count, int *out_cap)
{
    if (!out_items || !out_count || !out_cap)
        return;
    *out_items = 0;
    *out_count = 0;
    *out_cap = 0;
    if (!match(p, TOK_LT))
        return;

    for (;;)
    {
        Token *tp = cur(p);
        expect(p, TOK_ID, "Expected Identifier", "Expected generic type parameter name");
        const char *name = tok_text(tp);
        if (*out_count + 1 > *out_cap)
        {
            int new_cap = *out_cap ? *out_cap * 2 : 4;
            const char **n = (const char **)kn_malloc(sizeof(char *) * (size_t)new_cap);
            if (*out_items)
            {
                kn_memcpy(n, *out_items, sizeof(char *) * (size_t)(*out_count));
                kn_free((void *)*out_items);
            }
            *out_items = n;
            *out_cap = new_cap;
        }
        ((const char **)(*out_items))[(*out_count)++] = name;
        if (match(p, TOK_COMMA))
            continue;
        expect(p, TOK_GT, "Expected '>'", "Missing '>' after generic type parameter list");
        break;
    }
}

static Type parse_param_type(Parser *p)
{
    return parse_type(p);
}

static Param parse_param_decl(Parser *p, int allow_default_value)
{
    Param prm;
    kn_memset(&prm, 0, sizeof(prm));
    Token *type_tok = cur(p);
    prm.type = parse_param_type(p);
    Token *pname = cur(p);
    expect(p, TOK_ID, "Expected Identifier", "Expected parameter name");
    prm.name = tok_text(pname);
    prm.src = p->src;
    prm.line = type_tok ? type_tok->line : 1;
    prm.col = type_tok ? type_tok->col : 1;
    (void)parse_legacy_array_suffix(p, &prm.type, pname);
    if (allow_default_value && match(p, TOK_ASSIGN))
        prm.default_value = parse_expr(p);
    return prm;
}

static ParamList parse_param_list(Parser *p, int allow_default_value, const char *missing_rparen_detail)
{
    ParamList params;
    kn_memset(&params, 0, sizeof(params));
    int saw_default = 0;
    if (!match(p, TOK_RPAREN))
    {
        for (;;)
        {
            Param prm = parse_param_decl(p, allow_default_value);
            if (prm.default_value)
                saw_default = 1;
            else if (allow_default_value && saw_default)
            {
                Token *bad = cur(p);
                kn_diag_report(p->src, KN_STAGE_PARSER, bad->line, bad->col, (int)(bad->length ? bad->length : 1),
                               "Invalid Default Parameter",
                               "Parameters with default values must appear after required parameters",
                               tok_got(p));
            }
            paramlist_push(&params, prm);
            if (match(p, TOK_COMMA)) continue;
            expect(p, TOK_RPAREN, "Expected ')'", missing_rparen_detail);
            break;
        }
    }
    return params;
}

static Expr *parse_expr(Parser *p);
static Expr *parse_block_literal_expr(Parser *p, Safety safety, const char *debug_name);
static Expr *parse_anon_function_expr(Parser *p, Safety safety);
static Expr *parse_if_expr(Parser *p);
static Expr *parse_switch_expr(Parser *p);

static Expr *parse_optional_decl_initializer(Parser *p, Token *anchor, int *skip_semi)
{
    if (skip_semi) *skip_semi = 0;
    if (!match(p, TOK_ASSIGN))
        return 0;

    if (cur(p)->type == TOK_EOF)
    {
        Token *e = cur(p);
        int line = anchor ? anchor->line : e->line;
        int col = anchor ? anchor->col : e->col;
        kn_diag_report(p->src, KN_STAGE_PARSER, line, col, 1,
                       "Unexpected End Of File",
                       "Unexpected end of file while parsing variable declaration",
                       "<end-of-file>");
        if (skip_semi) *skip_semi = 1;
        return 0;
    }

    int errs_before = kn_diag_error_count();
    Expr *init = parse_expr(p);
    if (skip_semi && cur(p)->type == TOK_EOF && kn_diag_error_count() > errs_before)
        *skip_semi = 1;
    return init;
}

static Safety parse_optional_safety(Parser *p, Safety def)
{
    Safety s = def;
    if (match(p, TOK_SAFE)) s = SAFETY_SAFE;
    else if (match(p, TOK_TRUSTED)) s = SAFETY_TRUSTED;
    else if (match(p, TOK_UNSAFE)) s = SAFETY_UNSAFE;
    return s;
}

static bool is_block_closer(Parser *p)
{
    return cur(p)->type == TOK_SLASH && peek(p, 1)->type == TOK_GT;
}

static Stmt *parse_block_region(Parser *p)
{
    Token *t = cur(p);
    expect(p, TOK_LT, "Expected '<'", "Missing '</' before block body");
    expect(p, TOK_SLASH, "Expected '/'", "Missing '</' before block body");

    int prev = p->in_block_literal;
    p->in_block_literal = 1;

    Stmt *s = new_stmt(ST_BLOCK, t->line, t->col);
    while (!is_block_closer(p))
    {
        if (cur(p)->type == TOK_EOF)
        {
            kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                           "Expected '/>'", "Missing '/>' after block body", "<end-of-file>");
            break;
        }
        Stmt *st = parse_stmt(p);
        stmtlist_push(&s->v.block.stmts, st);
    }

    expect(p, TOK_SLASH, "Expected '/'", "Missing '/>' after block body");
    expect(p, TOK_GT, "Expected '>'", "Missing '/>' after block body");
    p->in_block_literal = prev;
    return s;
}

static void collect_block_records_stmt(Stmt *st, int depth, int *next_ip, BlockRecordList *out)
{
    if (!st || !next_ip || !out) return;
    switch (st->kind)
    {
    case ST_BLOCK:
        for (int i = 0; i < st->v.block.stmts.count; i++)
            collect_block_records_stmt(st->v.block.stmts.items[i], depth, next_ip, out);
        return;
    case ST_IF:
        if (st->v.ifs.then_s) collect_block_records_stmt(st->v.ifs.then_s, depth + 1, next_ip, out);
        if (st->v.ifs.else_s) collect_block_records_stmt(st->v.ifs.else_s, depth + 1, next_ip, out);
        return;
    case ST_SWITCH:
        for (int i = 0; i < st->v.switchs.cases.count; i++)
            if (st->v.switchs.cases.items[i].body)
                collect_block_records_stmt(st->v.switchs.cases.items[i].body, depth + 1, next_ip, out);
        return;
    case ST_WHILE:
        if (st->v.whiles.body) collect_block_records_stmt(st->v.whiles.body, depth + 1, next_ip, out);
        return;
    case ST_FOR:
        if (st->v.fors.init) collect_block_records_stmt(st->v.fors.init, depth + 1, next_ip, out);
        if (st->v.fors.body) collect_block_records_stmt(st->v.fors.body, depth + 1, next_ip, out);
        return;
    case ST_TRY:
        if (st->v.trys.try_block) collect_block_records_stmt(st->v.trys.try_block, depth + 1, next_ip, out);
        if (st->v.trys.catch_block) collect_block_records_stmt(st->v.trys.catch_block, depth + 1, next_ip, out);
        return;
    case ST_BLOCK_RECORD:
    {
        st->v.record.ip = (*next_ip)++;
        st->v.record.nested_depth = depth;
        BlockRecord r;
        r.name = st->v.record.name;
        r.ip = st->v.record.ip;
        r.nested_depth = depth;
        blockrecordlist_push(out, r);
        return;
    }
    default:
        return;
    }
}

static Expr *parse_block_literal_expr(Parser *p, Safety safety, const char *debug_name)
{
    Token *t = cur(p);
    Expr *e = new_expr(EXPR_BLOCK_LITERAL, t->line, t->col);
    e->v.block_lit.safety = safety;
    e->v.block_lit.debug_name = debug_name ? debug_name : "";
    e->v.block_lit.id = p->next_block_id++;
    e->v.block_lit.body = parse_block_region(p);
    int next_ip = 1; // ip=0 reserved for block entry
    collect_block_records_stmt(e->v.block_lit.body, 0, &next_ip, &e->v.block_lit.records);
    return e;
}

static Expr *parse_anon_function_expr(Parser *p, Safety safety)
{
    Token *t = cur(p);
    expect(p, TOK_FUNCTION, "Expected Function", "Expected 'Function' keyword");
    Type ret = parse_type(p);
    expect(p, TOK_LPAREN, "Expected '('", "Missing '(' after function return type");
    ParamList params = parse_param_list(p, 1, "Missing ')' after parameters");
    Stmt *body = parse_block(p);
    Expr *e = new_expr(EXPR_ANON_FUNC, t->line, t->col);
    e->v.anon_func.ret_type = ret;
    e->v.anon_func.params = params.items;
    e->v.anon_func.param_count = params.count;
    e->v.anon_func.param_cap = params.cap;
    e->v.anon_func.body = body;
    e->v.anon_func.safety = safety;
    e->v.anon_func.id = p->next_anon_func_id++;
    e->v.anon_func.debug_name = "lambda";
    e->v.anon_func.qname = 0;
    return e;
}

// Check if current position looks like a type cast: [type](...) 
static bool looks_like_cast(Parser *p)
{
    if (cur(p)->type != TOK_LBRACKET) return false;
    
    // Peek ahead to check for type pattern: [type]*...]
    int off = 1;
    Token *peek_t = peek(p, off);
    
    // Check for type token
    if (is_type_tok(peek_t->type))
    {
        off++;
        // Check for pointer depth (*)
        while (peek(p, off)->type == TOK_STAR)
            off++;
        // Should be followed by ]
        if (peek(p, off)->type == TOK_RBRACKET)
        {
            off++;
            // Should be followed by (
            if (peek(p, off)->type == TOK_LPAREN)
                return true;
        }
    }
    // Check for qualified type name
    else if (type_name_segment_tok(peek_t->type))
    {
        off++;
        // Handle qualified names (Module.Type)
        while (peek(p, off)->type == TOK_DOT)
        {
            off++;
            if (type_name_segment_tok(peek(p, off)->type))
                off++;
            else
                return false;
        }
        // Check for pointer depth
        while (peek(p, off)->type == TOK_STAR)
            off++;
        // Should be followed by ]
        if (peek(p, off)->type == TOK_RBRACKET)
        {
            off++;
            // Should be followed by (
            if (peek(p, off)->type == TOK_LPAREN)
                return true;
        }
    }
    
    return false;
}

static Expr *parse_cast_expr(Parser *p)
{
    Token *t = cur(p);
    expect(p, TOK_LBRACKET, "Expected '['", "Missing '[' in cast expression");
    
    Type cast_type = parse_type(p);
    
    expect(p, TOK_RBRACKET, "Expected ']'", "Missing ']' in cast expression");
    expect(p, TOK_LPAREN, "Expected '('", "Missing '(' after cast type");
    
    Expr *value = parse_expr(p);
    
    expect(p, TOK_RPAREN, "Expected ')'", "Missing ')' in cast expression");
    
    Expr *e = new_expr(EXPR_CAST, t->line, t->col);
    e->v.cast.type = cast_type;
    e->v.cast.expr = value;
    
    return e;
}

static int parse_named_arg_prefix(Parser *p, const char **out_name)
{
    if (out_name) *out_name = 0;
    if (!p || !out_name) return 0;
    if (cur(p)->type == TOK_ID && peek(p, 1)->type == TOK_ASSIGN)
    {
        *out_name = tok_text(cur(p));
        p->pos += 2; // consume "name ="
        return 1;
    }
    return 0;
}

static void parse_call_arg_list(Parser *p, ExprList *args, NameList *arg_names, const char *missing_rparen_detail)
{
    int saw_named = 0;
    if (!match(p, TOK_RPAREN))
    {
        for (;;)
        {
            const char *arg_name = 0;
            int is_named = parse_named_arg_prefix(p, &arg_name);
            if (is_named) saw_named = 1;
            else if (saw_named)
            {
                Token *bad = cur(p);
                kn_diag_report(p->src, KN_STAGE_PARSER, bad->line, bad->col, (int)(bad->length ? bad->length : 1),
                               "Invalid Named Argument", "Positional argument cannot follow named argument", tok_got(p));
            }

            Expr *arg = parse_expr(p);
            exprlist_push(args, arg);
            namelist_push(arg_names, arg_name);

            if (match(p, TOK_COMMA)) continue;
            expect(p, TOK_RPAREN, "Expected ')'", missing_rparen_detail);
            break;
        }
    }
}

#define KN_STRING_PREFIX_RAW    0x01
#define KN_STRING_PREFIX_FORMAT 0x02
#define KN_STRING_PREFIX_CHARS  0x04

static Expr *build_string_literal_expr(int line, int col, const char *ptr, int len)
{
    Expr *e = new_expr(EXPR_STRING, line, col);
    e->v.str_val.ptr = ptr ? ptr : "";
    e->v.str_val.len = len >= 0 ? len : 0;
    return e;
}

static Expr *build_empty_string_expr(int line, int col)
{
    return build_string_literal_expr(line, col, "", 0);
}

static Expr *build_char_array_literal_expr(int line, int col, const char *ptr, int len)
{
    Expr *arr = new_expr(EXPR_ARRAY, line, col);
    for (int i = 0; i < len; i++)
    {
        Expr *ch = new_expr(EXPR_CHAR, line, col);
        ch->v.int_val = (unsigned char)ptr[i];
        exprlist_push(&arr->v.array.items, ch);
    }
    return arr;
}

static Expr *build_string_concat_expr(Expr *left, Expr *right)
{
    Expr *e = new_expr(EXPR_BINARY, left ? left->line : (right ? right->line : 1),
                       left ? left->col : (right ? right->col : 1));
    e->v.binary.op = TOK_PLUS;
    e->v.binary.left = left;
    e->v.binary.right = right;
    return e;
}

static Expr *append_formatted_string_part(Expr *acc, Expr *part, int line, int col)
{
    if (!part)
        return acc ? acc : build_empty_string_expr(line, col);
    if (!acc)
    {
        if (part->kind == EXPR_STRING)
            return part;
        acc = build_empty_string_expr(line, col);
    }
    return build_string_concat_expr(acc, part);
}

static Expr *build_string_to_chars_expr(Expr *recv)
{
    Expr *mc = new_expr(EXPR_MEMBER_CALL, recv ? recv->line : 1, recv ? recv->col : 1);
    mc->v.member_call.recv = recv;
    mc->v.member_call.name = "ToChars";
    mc->v.member_call.builtin_id = KN_BUILTIN_NONE;
    mc->v.member_call.is_static = 0;
    mc->v.member_call.static_type = 0;
    mc->v.member_call.method_index = -1;
    mc->v.member_call.method_owner = 0;
    mc->v.member_call.is_qual_call = 0;
    mc->v.member_call.qual_name = 0;
    return mc;
}

static Expr *build_string_cast_expr(Expr *value)
{
    Expr *e = new_expr(EXPR_CAST, value ? value->line : 1, value ? value->col : 1);
    e->v.cast.type = type_make(TY_STRING);
    e->v.cast.expr = value;
    return e;
}

static int string_prefix_flag_from_token(const Token *t)
{
    if (!t || t->type != TOK_ID)
        return 0;
    if (token_text_eq(t, "r") || token_text_eq(t, "raw"))
        return KN_STRING_PREFIX_RAW;
    if (token_text_eq(t, "f") || token_text_eq(t, "format"))
        return KN_STRING_PREFIX_FORMAT;
    if (token_text_eq(t, "c") || token_text_eq(t, "chars"))
        return KN_STRING_PREFIX_CHARS;
    return 0;
}

static int scan_prefixed_string(Parser *p, int *out_flags, int *out_count, Token **out_string_tok)
{
    if (out_flags) *out_flags = 0;
    if (out_count) *out_count = 0;
    if (out_string_tok) *out_string_tok = 0;
    if (!p || cur(p)->type != TOK_LBRACKET)
        return 0;

    int off = 0;
    int flags = 0;
    int count = 0;
    for (;;)
    {
        if (peek(p, off)->type != TOK_LBRACKET)
            break;
        Token *name_tok = peek(p, off + 1);
        if (name_tok->type != TOK_ID || peek(p, off + 2)->type != TOK_RBRACKET)
            break;
        int flag = string_prefix_flag_from_token(name_tok);
        if (!flag)
            return -1;
        flags |= flag;
        count++;
        off += 3;
    }

    if (count <= 0)
        return 0;
    if (peek(p, off)->type != TOK_STRING && peek(p, off)->type != TOK_BAD_STRING)
        return 0;

    if (out_flags) *out_flags = flags;
    if (out_count) *out_count = count;
    if (out_string_tok) *out_string_tok = peek(p, off);
    return 1;
}

static Expr *parse_expr_from_slice(Parser *p, const char *text, int len, int line, int col)
{
    if (len <= 0)
    {
        kn_diag_report(p->src, KN_STAGE_PARSER, line, col, 1,
                       "Empty Interpolation",
                       "Formatted string placeholder requires an expression",
                       "{}");
        return build_empty_string_expr(line, col);
    }

    char *buf = (char *)kn_malloc((size_t)len + 1);
    if (!buf) kn_die("out of memory");
    kn_memcpy(buf, text, (size_t)len);
    buf[len] = 0;

    Lexer l;
    lex_init(&l, buf);
    Token *toks = 0;
    int tok_count = 0;
    int tok_cap = 0;
    for (;;)
    {
        Token tok = lex_next(&l);
        tok.line = line + (tok.line - 1);
        if (tok.line == line)
            tok.col = col + (tok.col - 1);
        tok_push(&toks, &tok_count, &tok_cap, tok);
        if (tok.type == TOK_EOF)
            break;
    }

    Parser sub;
    sub.toks = toks;
    sub.count = tok_count;
    sub.pos = 0;
    sub.src = p->src;
    sub.unit_name = p->unit_name;
    sub.in_block_literal = p->in_block_literal;
    sub.next_anon_func_id = p->next_anon_func_id;
    sub.next_block_id = p->next_block_id;

    Expr *expr = parse_expr(&sub);
    if (cur(&sub)->type != TOK_EOF)
    {
        Token *bad = cur(&sub);
        kn_diag_report(p->src, KN_STAGE_PARSER, bad->line, bad->col, (int)(bad->length ? bad->length : 1),
                       "Invalid Interpolation",
                       "Formatted string placeholder must contain exactly one expression",
                       tok_got(&sub));
    }

    p->next_anon_func_id = sub.next_anon_func_id;
    p->next_block_id = sub.next_block_id;

    if (toks) kn_free(toks);
    kn_free(buf);
    return expr;
}

static int find_interpolation_close(Parser *p, const Token *t, int start)
{
    if (!t || t->length < 2)
        return -1;
    const char *s = t->start + 1;
    int src_len = (int)t->length - 2;
    int depth = 0;
    for (int i = start; i < src_len; i++)
    {
        char c = s[i];
        if (c == '"')
        {
            i++;
            while (i < src_len)
            {
                if (s[i] == '\\')
                {
                    i += 2;
                    continue;
                }
                if (s[i] == '"')
                    break;
                i++;
            }
            continue;
        }
        if (c == '\'')
        {
            i++;
            while (i < src_len)
            {
                if (s[i] == '\\')
                {
                    i += 2;
                    continue;
                }
                if (s[i] == '\'')
                    break;
                i++;
            }
            continue;
        }
        if (c == '{')
        {
            depth++;
            continue;
        }
        if (c == '}')
        {
            if (depth == 0)
                return i;
            depth--;
        }
    }

    kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col + 1 + start, 1,
                   "Invalid Interpolation",
                   "Formatted string placeholder is missing a closing '}'",
                   "<unterminated-interpolation>");
    return src_len;
}

static Expr *parse_formatted_string_expr(Parser *p, const Token *t, int raw)
{
    if (!t || t->length < 2)
        return build_empty_string_expr(t ? t->line : 1, t ? t->col : 1);

    const char *s = t->start + 1;
    int src_len = (int)t->length - 2;
    Expr *result = 0;
    int seg_start = 0;
    int saw_placeholder = 0;

    for (int i = 0; i < src_len; i++)
    {
        char c = s[i];
        if (c == '{' && i + 1 < src_len && s[i + 1] == '{')
        {
            i++;
            continue;
        }
        if (c == '{')
        {
            if (i > seg_start)
            {
                const char *seg_ptr = 0;
                int seg_len = 0;
                parse_string_lit_range(p, t, seg_start, i - seg_start, raw, 1, &seg_ptr, &seg_len);
                if (seg_len > 0 || !result)
                    result = append_formatted_string_part(result,
                                                          build_string_literal_expr(t->line, t->col + 1 + seg_start, seg_ptr, seg_len),
                                                          t->line, t->col);
            }

            int close = find_interpolation_close(p, t, i + 1);
            Expr *part = parse_expr_from_slice(p, s + i + 1, close - (i + 1), t->line, t->col + 1 + i + 1);
            part = build_string_cast_expr(part);
            result = append_formatted_string_part(result, part, t->line, t->col);
            saw_placeholder = 1;
            i = close;
            seg_start = close + 1;
        }
        else if (c == '}' && i + 1 < src_len && s[i + 1] == '}')
        {
            i++;
        }
    }

    if (seg_start < src_len || !saw_placeholder)
    {
        const char *seg_ptr = 0;
        int seg_len = 0;
        parse_string_lit_range(p, t, seg_start, src_len - seg_start, raw, 1, &seg_ptr, &seg_len);
        if (saw_placeholder)
            result = append_formatted_string_part(result,
                                                  build_string_literal_expr(t->line, t->col + 1 + seg_start, seg_ptr, seg_len),
                                                  t->line, t->col);
        else
            return build_string_literal_expr(t->line, t->col, seg_ptr, seg_len);
    }

    return result ? result : build_empty_string_expr(t->line, t->col);
}

static Expr *parse_prefixed_string_expr(Parser *p, int prefix_flags, int prefix_count)
{
    Token *start_tok = cur(p);
    for (int i = 0; i < prefix_count; i++)
    {
        p->pos += 3;
    }

    Token *t = cur(p);
    if (t->type == TOK_BAD_STRING)
    {
        p->pos++;
        kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col, (int)t->length,
                       "Unterminated String Literal",
                       "String literal is missing a closing quote",
                       "<unterminated-string>");
        if (prefix_flags & KN_STRING_PREFIX_CHARS)
            return new_expr(EXPR_ARRAY, start_tok->line, start_tok->col);
        return build_empty_string_expr(start_tok->line, start_tok->col);
    }

    expect(p, TOK_STRING, "Expected String Literal", "String prefix must be followed by a string literal");

    Expr *base = 0;
    if (prefix_flags & KN_STRING_PREFIX_FORMAT)
    {
        base = parse_formatted_string_expr(p, t, (prefix_flags & KN_STRING_PREFIX_RAW) != 0);
    }
    else
    {
        const char *ptr = 0;
        int len = 0;
        parse_string_lit_range(p, t, 0, -1, (prefix_flags & KN_STRING_PREFIX_RAW) != 0, 0, &ptr, &len);
        base = build_string_literal_expr(t->line, t->col, ptr, len);
    }

    if (!(prefix_flags & KN_STRING_PREFIX_CHARS))
        return base;
    if (base->kind == EXPR_STRING)
        return build_char_array_literal_expr(start_tok->line, start_tok->col, base->v.str_val.ptr, base->v.str_val.len);
    return build_string_to_chars_expr(base);
}

static Expr *parse_primary(Parser *p)
{
    Token *t = cur(p);
    Expr *expr = 0;

    {
        int prefix_flags = 0;
        int prefix_count = 0;
        Token *string_tok = 0;
        int prefixed = scan_prefixed_string(p, &prefix_flags, &prefix_count, &string_tok);
        if (prefixed == 1)
        {
            expr = parse_prefixed_string_expr(p, prefix_flags, prefix_count);
            goto postfix;
        }
        if (prefixed == -1)
        {
            Token *bad = peek(p, 1);
            kn_diag_report(p->src, KN_STAGE_PARSER, bad->line, bad->col, (int)(bad->length ? bad->length : 1),
                           "Unknown String Prefix",
                           "Supported string prefixes are [r]/[raw], [f]/[format], and [c]/[chars]",
                           tok_text(bad));
        }
        (void)string_tok;
    }

    // Handle type cast: [type](value)
    if (looks_like_cast(p))
    {
        expr = parse_cast_expr(p);
        // Fall through to postfix loop so that (args) / .member are handled.
        goto postfix;
    }

    if (t->type == TOK_IF)
        return parse_if_expr(p);
    if (t->type == TOK_SWITCH)
        return parse_switch_expr(p);

    if ((t->type == TOK_SAFE || t->type == TOK_TRUSTED || t->type == TOK_UNSAFE) && peek(p, 1)->type == TOK_FUNCTION)
    {
        Safety safety = parse_optional_safety(p, SAFETY_SAFE);
        return parse_anon_function_expr(p, safety);
    }
    if (t->type == TOK_FUNCTION)
        return parse_anon_function_expr(p, SAFETY_SAFE);
    if ((t->type == TOK_SAFE || t->type == TOK_TRUSTED || t->type == TOK_UNSAFE) && peek(p, 1)->type == TOK_BLOCK)
    {
        Safety safety = parse_optional_safety(p, SAFETY_SAFE);
        expect(p, TOK_BLOCK, "Expected Block", "Expected 'Block' keyword");
        const char *name = 0;
        if (cur(p)->type == TOK_ID)
        {
            Token *nt = cur(p);
            expect(p, TOK_ID, "Expected Identifier", "Expected block name");
            name = tok_text(nt);
        }
        return parse_block_literal_expr(p, safety, name);
    }
    if (t->type == TOK_BLOCK)
    {
        expect(p, TOK_BLOCK, "Expected Block", "Expected 'Block' keyword");
        const char *name = 0;
        if (cur(p)->type == TOK_ID)
        {
            Token *nt = cur(p);
            expect(p, TOK_ID, "Expected Identifier", "Expected block name");
            name = tok_text(nt);
        }
        return parse_block_literal_expr(p, SAFETY_SAFE, name);
    }

    if (match(p, TOK_LBRACE))
    {
        if (match(p, TOK_RBRACE))
        {
            Expr *e = new_expr(EXPR_ARRAY, t->line, t->col);
            e->v.array.items.items = 0;
            e->v.array.items.count = 0;
            e->v.array.items.cap = 0;
            expr = e;
        }
        else
        {
            Expr *first = parse_expr(p);
            if (match(p, TOK_COLON))
            {
                Expr *e = new_expr(EXPR_DICT, t->line, t->col);
                Expr *value = parse_expr(p);
                exprlist_push(&e->v.dict.keys, first);
                exprlist_push(&e->v.dict.values, value);
                while (match(p, TOK_COMMA))
                {
                    Expr *key = parse_expr(p);
                    expect(p, TOK_COLON, "Expected ':'", "Missing ':' in dict literal");
                    Expr *val = parse_expr(p);
                    exprlist_push(&e->v.dict.keys, key);
                    exprlist_push(&e->v.dict.values, val);
                }
                expect(p, TOK_RBRACE, "Expected '}'", "Missing '}' in dict literal");
                expr = e;
            }
            else
            {
                Expr *e = new_expr(EXPR_ARRAY, t->line, t->col);
                exprlist_push(&e->v.array.items, first);
                while (match(p, TOK_COMMA))
                {
                    Expr *item = parse_expr(p);
                    exprlist_push(&e->v.array.items, item);
                }
                expect(p, TOK_RBRACE, "Expected '}'", "Missing '}' in array literal");
                expr = e;
            }
        }
    }
    else if (match(p, TOK_NUMBER))
    {
        bool is_float = false;
        bool is_hex = (t->length >= 2 &&
            t->start[0] == '0' &&
            (t->start[1] == 'x' || t->start[1] == 'X'));
        for (size_t i = 0; i < t->length; i++)
            if (t->start[i] == '.') { is_float = true; break; }
        if (!is_float && !is_hex && t->length > 0)
        {
            char tail = t->start[t->length - 1];
            if (tail == 'f' || tail == 'F')
                is_float = true;
        }
        if (is_float)
        {
            Expr *e = new_expr(EXPR_FLOAT, t->line, t->col);
            e->v.float_val = parse_float_lit(t);
            expr = e;
        }
        else
        {
            Expr *e = new_expr(EXPR_INT, t->line, t->col);
            e->v.int_val = parse_int_lit(t);
            expr = e;
        }
    }
    else if (match(p, TOK_STRING))
    {
        Expr *e = new_expr(EXPR_STRING, t->line, t->col);
        parse_string_lit(p, t, &e->v.str_val.ptr, &e->v.str_val.len);
        expr = e;
    }
    else if (match(p, TOK_CHAR_LIT))
    {
        Expr *e = new_expr(EXPR_CHAR, t->line, t->col);
        e->v.int_val = parse_char_lit(t);
        expr = e;
    }
    else if (match(p, TOK_BAD_STRING))
    {
        kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col, (int)t->length,
                       "Unterminated String Literal",
                       "String literal is missing a closing quote",
                       "<unterminated-string>");
        Expr *e = new_expr(EXPR_STRING, t->line, t->col);
        e->v.str_val.ptr = "";
        e->v.str_val.len = 0;
        expr = e;
    }
    else if (match(p, TOK_BAD_CHAR))
    {
        int terminated = t->length > 0 && t->start[t->length - 1] == '\'';
        kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col, (int)t->length,
                       terminated ? "Invalid Character Literal" : "Unterminated Character Literal",
                       terminated
                           ? "Character literal must contain exactly one character or a supported escape sequence"
                           : "Character literal is missing a closing quote",
                       "<invalid-char>");
        Expr *e = new_expr(EXPR_CHAR, t->line, t->col);
        e->v.int_val = 0;
        expr = e;
    }
    else if (t->type == TOK_TRUE || t->type == TOK_FALSE)
    {
        p->pos++;
        Expr *e = new_expr(EXPR_BOOL, t->line, t->col);
        e->v.int_val = (t->type == TOK_TRUE) ? 1 : 0;
        expr = e;
    }
    else if (match(p, TOK_DEFAULT))
    {
        Expr *e = new_expr(EXPR_DEFAULT, t->line, t->col);
        expr = e;
    }
    else if (match(p, TOK_NULL))
    {
        Expr *e = new_expr(EXPR_NULL, t->line, t->col);
        expr = e;
    }
    else if (match(p, TOK_THIS))
    {
        Expr *e = new_expr(EXPR_THIS, t->line, t->col);
        expr = e;
    }
    else if (match(p, TOK_BASE))
    {
        Expr *e = new_expr(EXPR_BASE, t->line, t->col);
        expr = e;
    }
    else if (match(p, TOK_NEW))
    {
        const char *qname = normalize_type_alias(parse_qualified_name(p));
        Expr *e = new_expr(EXPR_NEW, t->line, t->col);
        e->v.new_expr.class_name = qname;
        e->v.new_expr.ctor_index = -1;
        expect(p, TOK_LPAREN, "Expected '('", "Missing '(' after class name");
        parse_call_arg_list(p, &e->v.new_expr.args, &e->v.new_expr.arg_names, "Missing ')' in constructor call");
        expr = e;
    }
    else if (match(p, TOK_ID))
    {
        Expr *e = new_expr(EXPR_VAR, t->line, t->col);
        e->v.var.name = tok_text(t);
        expr = e;
    }
    else if (is_type_tok(t->type))
    {
        p->pos++;
        Expr *e = new_expr(EXPR_VAR, t->line, t->col);
        e->v.var.name = type_token_name(t->type);
        expr = e;
    }
    else if (match(p, TOK_LPAREN))
    {
        Expr *e = parse_expr(p);
        expect(p, TOK_RPAREN, "Expected ')'", "Missing ')'");
        expr = e;
    }
    else
    {
        if (t->type == TOK_EOF)
            kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col, 1,
                           "Unexpected End Of File", "Unexpected end of file while parsing expression", "<end-of-file>");
        else
            kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col, (int)t->length,
                           "Unexpected Token", "Unexpected token in expression", tok_got(p));
        if (t->type != TOK_EOF)
            p->pos++;
        Expr *dummy = new_expr(EXPR_INT, t->line, t->col);
        dummy->v.int_val = 0;
        return dummy;
    }

    // postfix: calls and member access
postfix:
    for (;;)
    {
        TypeList call_type_args;
        kn_memset(&call_type_args, 0, sizeof(call_type_args));
        if (expr->kind == EXPR_VAR && cur(p)->type == TOK_LT && looks_like_type_arg_list(p, 0, 1, 0))
            call_type_args = parse_type_arg_list(p);

        if (match(p, TOK_LPAREN))
        {
            Expr *call = 0;
            if (expr->kind == EXPR_VAR)
            {
                call = new_expr(EXPR_CALL, expr->line, expr->col);
                call->v.call.name = expr->v.var.name;
                call->v.call.builtin_id = KN_BUILTIN_NONE;
                call->v.call.type_args = call_type_args;
            }
            else
            {
                if (call_type_args.count > 0)
                    kn_diag_report(p->src, KN_STAGE_PARSER, expr->line, expr->col, 1,
                                   "Invalid Generic Call", "Generic type arguments are only supported on named function calls", tok_got(p));
                call = new_expr(EXPR_INVOKE, expr->line, expr->col);
                call->v.invoke.callee = expr;
            }
            if (call->kind == EXPR_CALL)
                parse_call_arg_list(p, &call->v.call.args, &call->v.call.arg_names, "Missing ')' in call");
            else
                parse_call_arg_list(p, &call->v.invoke.args, &call->v.invoke.arg_names, "Missing ')' in call");
            expr = call;
            continue;
        }
        if (match(p, TOK_DOT))
        {
            const char *mname = parse_name_token(p, "Expected Identifier", "Expected member name");
            TypeList member_type_args;
            kn_memset(&member_type_args, 0, sizeof(member_type_args));
            if (cur(p)->type == TOK_LT && looks_like_type_arg_list(p, 0, 1, 0))
                member_type_args = parse_type_arg_list(p);
            if (match(p, TOK_LPAREN))
            {
                Expr *mc = new_expr(EXPR_MEMBER_CALL, expr->line, expr->col);
                mc->v.member_call.recv = expr;
                mc->v.member_call.name = mname;
                mc->v.member_call.builtin_id = KN_BUILTIN_NONE;
                mc->v.member_call.type_args = member_type_args;
                mc->v.member_call.is_static = 0;
                mc->v.member_call.static_type = 0;
                mc->v.member_call.method_index = -1;
                mc->v.member_call.method_owner = 0;
                mc->v.member_call.is_qual_call = 0;
                mc->v.member_call.qual_name = 0;
                parse_call_arg_list(p, &mc->v.member_call.args, &mc->v.member_call.arg_names, "Missing ')' in member call");
                expr = mc;
            }
            else
            {
                if (member_type_args.count > 0)
                    kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                                   "Invalid Generic Member", "Generic type arguments on member access require a call", tok_got(p));
                Expr *mem = new_expr(EXPR_MEMBER, expr->line, expr->col);
                mem->v.member.recv = expr;
                mem->v.member.name = mname;
                mem->v.member.is_static = 0;
                mem->v.member.static_type = 0;
                mem->v.member.field_index = -1;
                mem->v.member.field_owner = 0;
                expr = mem;
            }
            continue;
        }
        if (match(p, TOK_LBRACKET))
        {
            Expr *idx = parse_expr(p);
            expect(p, TOK_RBRACKET, "Expected ']'", "Missing ']' after index");
            Expr *ix = new_expr(EXPR_INDEX, expr->line, expr->col);
            ix->v.index.recv = expr;
            ix->v.index.index = idx;
            expr = ix;
            continue;
        }
        if (cur(p)->type == TOK_PLUSPLUS || cur(p)->type == TOK_MINUSMINUS)
        {
            Token *op = cur(p);
            p->pos++;
            Expr *u = new_expr(EXPR_UNARY, expr->line, expr->col);
            u->v.unary.op = (int)op->type;
            u->v.unary.expr = expr;
            u->v.unary.is_postfix = 1;
            expr = u;
            continue;
        }
        break;
    }

    return expr;
}

static Expr *parse_unary(Parser *p)
{
    Token *t = cur(p);
    if (match(p, TOK_AWAIT))
    {
        Expr *e = new_expr(EXPR_AWAIT, t->line, t->col);
        e->v.await_expr.expr = parse_unary(p);
        return e;
    }
    if (match(p, TOK_PLUSPLUS) || match(p, TOK_MINUSMINUS) ||
        match(p, TOK_MINUS) || match(p, TOK_NOT) || match(p, TOK_STAR) || match(p, TOK_AMP) || match(p, TOK_TILDE))
    {
        Expr *e = new_expr(EXPR_UNARY, t->line, t->col);
        e->v.unary.op = (int)t->type;
        e->v.unary.expr = parse_unary(p);
        e->v.unary.is_postfix = 0;
        return e;
    }
    return parse_primary(p);
}

static int precedence(TokenType t)
{
    switch (t)
    {
    case TOK_OROR: return 1;
    case TOK_ANDAND: return 2;
    case TOK_BITOR: return 3;
    case TOK_XOR: return 4;
    case TOK_AMP: return 5;
    case TOK_EQ:
    case TOK_NE: return 6;
    case TOK_LT:
    case TOK_LE:
    case TOK_GT:
    case TOK_GE:
    case TOK_IS: return 7;
    case TOK_SHL:
    case TOK_SHR: return 8;
    case TOK_PLUS:
    case TOK_MINUS: return 9;
    case TOK_STAR:
    case TOK_SLASH:
    case TOK_PERCENT: return 10;
    default: return 0;
    }
}

static Expr *parse_bin_rhs(Parser *p, int prec, Expr *lhs)
{
    for (;;)
    {
        int tok_prec = precedence(cur(p)->type);
        if (tok_prec < prec) return lhs;
        Token *op = cur(p);
        p->pos++;
        if (op->type == TOK_IS)
        {
            Type target = parse_type(p);
            if (target.kind == TY_UNKNOWN)
                kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                               "Expected Type", "Expected type after Is", tok_got(p));
            Expr *is_expr = new_expr(EXPR_IS, op->line, op->col);
            is_expr->v.is_expr.expr = lhs;
            is_expr->v.is_expr.target = target;
            lhs = is_expr;
            continue;
        }
        Expr *rhs = parse_unary(p);
        int next_prec = precedence(cur(p)->type);
        if (tok_prec < next_prec)
            rhs = parse_bin_rhs(p, tok_prec + 1, rhs);
        Expr *bin = new_expr(EXPR_BINARY, op->line, op->col);
        bin->v.binary.op = (int)op->type;
        bin->v.binary.left = lhs;
        bin->v.binary.right = rhs;
        lhs = bin;
    }
}

static Expr *parse_expr(Parser *p)
{
    Expr *lhs = parse_unary(p);
    lhs = parse_bin_rhs(p, 1, lhs);

    if (match(p, TOK_QUESTION))
    {
        Expr *then_expr = parse_expr(p);
        expect(p, TOK_COLON, "Expected ':'", "Missing ':' in conditional expression");
        Expr *else_expr = parse_expr(p);
        Expr *ie = new_expr(EXPR_IF, lhs->line, lhs->col);
        ie->v.if_expr.cond = lhs;
        ie->v.if_expr.then_expr = then_expr;
        ie->v.if_expr.else_expr = else_expr;
        lhs = ie;
    }

    TokenType assign_op = cur(p)->type;
    if (assign_op == TOK_ASSIGN || assign_op == TOK_PLUS_ASSIGN || assign_op == TOK_MINUS_ASSIGN ||
        assign_op == TOK_STAR_ASSIGN || assign_op == TOK_SLASH_ASSIGN || assign_op == TOK_PERCENT_ASSIGN)
    {
        p->pos++;
        if (lhs->kind != EXPR_VAR && lhs->kind != EXPR_MEMBER && lhs->kind != EXPR_INDEX &&
            !(lhs->kind == EXPR_UNARY && lhs->v.unary.op == TOK_STAR))
            kn_diag_report(p->src, KN_STAGE_PARSER, lhs->line, lhs->col, 1, "Invalid Assignment", "Left-hand side must be assignable", tok_got(p));
        Expr *rhs = parse_expr(p);
        Expr *value = rhs;
        if (assign_op != TOK_ASSIGN)
        {
            Expr *bin = new_expr(EXPR_BINARY, lhs->line, lhs->col);
            bin->v.binary.left = lhs;
            bin->v.binary.right = rhs;
            if (assign_op == TOK_PLUS_ASSIGN) bin->v.binary.op = TOK_PLUS;
            else if (assign_op == TOK_MINUS_ASSIGN) bin->v.binary.op = TOK_MINUS;
            else if (assign_op == TOK_STAR_ASSIGN) bin->v.binary.op = TOK_STAR;
            else if (assign_op == TOK_SLASH_ASSIGN) bin->v.binary.op = TOK_SLASH;
            else bin->v.binary.op = TOK_PERCENT;
            value = bin;
        }
        Expr *e = new_expr(EXPR_ASSIGN, lhs->line, lhs->col);
        e->v.assign.target = lhs;
        e->v.assign.value = value;
        return e;
    }

    return lhs;
}

static Expr *parse_if_expr(Parser *p)
{
    Token *t = cur(p);
    expect(p, TOK_IF, "Expected If", "Expected 'If' keyword in expression");
    expect(p, TOK_LPAREN, "Expected '('", "Missing '(' after If");
    Expr *cond = parse_expr(p);
    expect(p, TOK_RPAREN, "Expected ')'", "Missing ')' after If condition");
    expect(p, TOK_LBRACE, "Expected '{'", "Missing '{' in If expression");
    Expr *then_expr = parse_expr(p);
    (void)match(p, TOK_SEMI);
    expect(p, TOK_RBRACE, "Expected '}'", "Missing '}' in If expression");
    expect(p, TOK_ELSE, "Expected Else", "If expression requires Else branch");
    expect(p, TOK_LBRACE, "Expected '{'", "Missing '{' in Else expression");
    Expr *else_expr = parse_expr(p);
    (void)match(p, TOK_SEMI);
    expect(p, TOK_RBRACE, "Expected '}'", "Missing '}' in Else expression");

    Expr *e = new_expr(EXPR_IF, t->line, t->col);
    e->v.if_expr.cond = cond;
    e->v.if_expr.then_expr = then_expr;
    e->v.if_expr.else_expr = else_expr;
    return e;
}

static Expr *parse_switch_expr(Parser *p)
{
    Token *t = cur(p);
    expect(p, TOK_SWITCH, "Expected Switch", "Expected 'Switch' keyword in expression");
    expect(p, TOK_LPAREN, "Expected '('", "Missing '(' after Switch");
    Expr *value = parse_expr(p);
    expect(p, TOK_RPAREN, "Expected ')'", "Missing ')' after Switch value");
    expect(p, TOK_LBRACE, "Expected '{'", "Missing '{' in Switch expression");

    Expr *e = new_expr(EXPR_SWITCH, t->line, t->col);
    e->v.switch_expr.value = value;
    int saw_default = 0;

    while (!match(p, TOK_RBRACE))
    {
        if (cur(p)->type == TOK_EOF)
        {
            kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                           "Expected '}'", "Unexpected end of file while parsing Switch expression", "<end-of-file>");
            break;
        }

        Token *case_tok = cur(p);
        expect(p, TOK_CASE, "Expected Case", "Expected 'Case' in Switch expression");
        expect(p, TOK_LPAREN, "Expected '('", "Missing '(' after Case");

        ExprSwitchCase c;
        kn_memset(&c, 0, sizeof(c));
        c.line = case_tok->line;
        c.col = case_tok->col;
        if (match(p, TOK_DEFAULT))
        {
            c.is_default = 1;
            saw_default = 1;
        }
        else
        {
            c.match = parse_expr(p);
        }
        expect(p, TOK_RPAREN, "Expected ')'", "Missing ')' after Case");
        expect(p, TOK_LBRACE, "Expected '{'", "Missing '{' in Switch expression case");
        c.body = parse_expr(p);
        (void)match(p, TOK_SEMI);
        expect(p, TOK_RBRACE, "Expected '}'", "Missing '}' in Switch expression case");
        exprswitchcaselist_push(&e->v.switch_expr.cases, c);
    }

    if (!saw_default)
        kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col, (int)t->length,
                       "Missing Default Case", "Switch expression requires Case(default)", "Switch");
    return e;
}

static Stmt *parse_stmt(Parser *p);

static Stmt *parse_block(Parser *p)
{
    Token *t = cur(p);
    expect(p, TOK_LBRACE, "Expected '{'", "Missing '{'");
    Stmt *s = new_stmt(ST_BLOCK, t->line, t->col);
    while (!match(p, TOK_RBRACE))
    {
        if (cur(p)->type == TOK_EOF)
        {
            kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                           "Expected '}'", "Unexpected end of file while parsing block", "<end-of-file>");
            break;
        }
        Stmt *st = parse_stmt(p);
        stmtlist_push(&s->v.block.stmts, st);
    }
    return s;
}

