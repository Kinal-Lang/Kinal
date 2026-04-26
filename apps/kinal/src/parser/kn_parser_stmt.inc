// Internal parser fragment: statements

static Stmt *parse_try(Parser *p)
{
    Token *t = cur(p);
    expect(p, TOK_TRY, "Expected Try", "Expected 'Try' keyword");
    Stmt *try_block = parse_block(p);
    expect(p, TOK_CATCH, "Expected Catch", "Missing 'Catch' after Try block");
    Type catch_type = type_make(TY_UNKNOWN);
    const char *catch_name = 0;
    int has_param = 0;
    if (match(p, TOK_LPAREN))
    {
        has_param = 1;
        catch_type = parse_type(p);
        Token *name_tok = cur(p);
        expect(p, TOK_ID, "Expected Identifier", "Expected catch variable name");
        catch_name = tok_text(name_tok);
        expect(p, TOK_RPAREN, "Expected ')'", "Missing ')' after catch parameter");
    }
    Stmt *catch_block = parse_block(p);
    Stmt *s = new_stmt(ST_TRY, t->line, t->col);
    s->v.trys.try_block = try_block;
    s->v.trys.catch_block = catch_block;
    s->v.trys.catch_type = catch_type;
    s->v.trys.catch_name = catch_name;
    s->v.trys.has_param = has_param;
    return s;
}

static Stmt *parse_throw(Parser *p)
{
    Token *t = cur(p);
    expect(p, TOK_THROW, "Expected Throw", "Expected 'Throw' keyword");
    Stmt *s = new_stmt(ST_THROW, t->line, t->col);
    s->v.throws.expr = 0;
    if (cur(p)->type != TOK_SEMI)
        s->v.throws.expr = parse_expr(p);
    expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after Throw");
    return s;
}

static Stmt *parse_if(Parser *p)
{
    Token *t = cur(p);
    expect(p, TOK_IF, "Expected If", "Expected 'If' keyword");
    expect(p, TOK_LPAREN, "Expected '('", "Missing '('");
    Expr *cond = parse_expr(p);
    const char *pattern_name = 0;
    int pattern_line = 0;
    int pattern_col = 0;
    if (cond && cond->kind == EXPR_IS && cur(p)->type == TOK_ID)
    {
        Token *name_tok = cur(p);
        pattern_name = tok_text(name_tok);
        pattern_line = name_tok->line;
        pattern_col = name_tok->col;
        p->pos++;
    }
    expect(p, TOK_RPAREN, "Expected ')'", "Missing ')'");
    Stmt *then_s = parse_stmt(p);
    Stmt *else_s = 0;
    if (match(p, TOK_ELSE))
    {
        if (cur(p)->type == TOK_IF)
            else_s = parse_if(p);
        else
            else_s = parse_stmt(p);
    }
    Stmt *s = new_stmt(ST_IF, t->line, t->col);
    s->v.ifs.cond = cond;
    s->v.ifs.then_s = then_s;
    s->v.ifs.else_s = else_s;
    s->v.ifs.pattern_name = pattern_name;
    s->v.ifs.pattern_line = pattern_line;
    s->v.ifs.pattern_col = pattern_col;
    return s;
}

static Stmt *parse_switch(Parser *p)
{
    Token *t = cur(p);
    expect(p, TOK_SWITCH, "Expected Switch", "Expected 'Switch' keyword");
    expect(p, TOK_LPAREN, "Expected '('", "Missing '(' after Switch");
    Expr *value = parse_expr(p);
    expect(p, TOK_RPAREN, "Expected ')'", "Missing ')' after Switch value");
    expect(p, TOK_LBRACE, "Expected '{'", "Missing '{' in Switch");

    Stmt *s = new_stmt(ST_SWITCH, t->line, t->col);
    s->v.switchs.value = value;
    while (!match(p, TOK_RBRACE))
    {
        if (cur(p)->type == TOK_EOF)
        {
            kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                           "Expected '}'", "Unexpected end of file while parsing Switch", "<end-of-file>");
            break;
        }

        Token *case_tok = cur(p);
        expect(p, TOK_CASE, "Expected Case", "Expected 'Case' in Switch");
        expect(p, TOK_LPAREN, "Expected '('", "Missing '(' after Case");

        StmtSwitchCase c;
        kn_memset(&c, 0, sizeof(c));
        c.line = case_tok->line;
        c.col = case_tok->col;
        if (match(p, TOK_DEFAULT))
        {
            c.is_default = 1;
        }
        else
        {
            c.match = parse_expr(p);
        }
        expect(p, TOK_RPAREN, "Expected ')'", "Missing ')' after Case");
        c.body = parse_stmt(p);
        stmtswitchcaselist_push(&s->v.switchs.cases, c);
    }
    return s;
}

static Stmt *parse_while(Parser *p)
{
    Token *t = cur(p);
    expect(p, TOK_WHILE, "Expected While", "Expected 'While' keyword");
    expect(p, TOK_LPAREN, "Expected '('", "Missing '('");
    Expr *cond = parse_expr(p);
    expect(p, TOK_RPAREN, "Expected ')'", "Missing ')'");
    Stmt *body = parse_stmt(p);
    Stmt *s = new_stmt(ST_WHILE, t->line, t->col);
    s->v.whiles.cond = cond;
    s->v.whiles.body = body;
    return s;
}

static Stmt *parse_for(Parser *p)
{
    Token *t = cur(p);
    expect(p, TOK_FOR, "Expected For", "Expected 'For' keyword");
    expect(p, TOK_LPAREN, "Expected '('", "Missing '('");
    Stmt *init = 0;
    if (!match(p, TOK_SEMI))
    {
        if (looks_like_type_decl(p))
        {
            init = parse_stmt(p);
        }
        else
        {
            Expr *e = parse_expr(p);
            expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' in for initializer");
            Stmt *st = new_stmt(ST_EXPR, t->line, t->col);
            st->v.expr.expr = e;
            init = st;
        }
    }
    Expr *cond = 0;
    if (!match(p, TOK_SEMI))
    {
        cond = parse_expr(p);
        expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after for condition");
    }
    Expr *post = 0;
    if (!match(p, TOK_RPAREN))
    {
        post = parse_expr(p);
        expect(p, TOK_RPAREN, "Expected ')'", "Missing ')' after for clause");
    }
    Stmt *body = parse_stmt(p);
    Stmt *s = new_stmt(ST_FOR, t->line, t->col);
    s->v.fors.init = init;
    s->v.fors.cond = cond;
    s->v.fors.post = post;
    s->v.fors.body = body;
    return s;
}

static bool is_type_tok(TokenType t)
{
    switch (t)
    {
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
        return true;
    default:
        return false;
    }
}

static bool looks_like_type_decl(Parser *p)
{
    if (!p) return false;
    if (cur(p)->type == TOK_VAR || cur(p)->type == TOK_CONST)
        return true;
    if (is_type_tok(cur(p)->type) || token_is_builtin_type_name(cur(p)))
    {
        if (peek(p, 1)->type == TOK_LPAREN || peek(p, 1)->type == TOK_DOT)
            return false;
        return true;
    }
    if (cur(p)->type != TOK_ID)
        return false;
    int off = 1;
    while (peek(p, off)->type == TOK_DOT)
    {
        off++;
        TokenType tt = peek(p, off)->type;
        if (tt == TOK_ID || is_type_tok(tt) || tt == TOK_FUNCTION || tt == TOK_BLOCK ||
            tt == TOK_CLASS || tt == TOK_INTERFACE || tt == TOK_ASYNC)
        {
            off++;
            continue;
        }
        return false;
    }
    while (peek(p, off)->type == TOK_STAR)
        off++;
    if (peek(p, off)->type == TOK_LBRACKET)
    {
        off++;
        int depth = 0;
        while ((peek(p, off)->type != TOK_RBRACKET || depth > 0) && peek(p, off)->type != TOK_EOF)
        {
            if (peek(p, off)->type == TOK_LPAREN || peek(p, off)->type == TOK_LBRACKET)
                depth++;
            else if (peek(p, off)->type == TOK_RPAREN || peek(p, off)->type == TOK_RBRACKET)
                depth--;
            off++;
        }
        if (peek(p, off)->type != TOK_RBRACKET)
            return false;
        off++;
    }
    if (peek(p, off)->type == TOK_LPAREN)
        return false;
    if (peek(p, off)->type == TOK_ID)
    {
        TokenType after_name = peek(p, off + 1)->type;
        return decl_name_follow_tok(after_name);
    }
    return false;
}

static int64_t parse_int_lit(const Token *t)
{
    if (t->length >= 2 && t->start[0] == '0' && (t->start[1] == 'x' || t->start[1] == 'X'))
    {
        int64_t v = 0;
        for (size_t i = 2; i < t->length; i++)
        {
            char c = t->start[i];
            int d = -1;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = 10 + (c - 'a');
            else if (c >= 'A' && c <= 'F') d = 10 + (c - 'A');
            else break;
            v = (v << 4) | d;
        }
        return v;
    }

    int64_t v = 0;
    for (size_t i = 0; i < t->length; i++)
    {
        char c = t->start[i];
        if (c < '0' || c > '9') break;
        v = v * 10 + (c - '0');
    }
    return v;
}

static double parse_float_lit(const Token *t)
{
    double v = 0.0;
    double frac = 0.1;
    int seen_dot = 0;
    size_t end = t->length;
    if (end > 0 && (t->start[end - 1] == 'f' || t->start[end - 1] == 'F'))
        end--;
    for (size_t i = 0; i < end; i++)
    {
        char c = t->start[i];
        if (c == '.')
        {
            if (seen_dot) break;
            seen_dot = 1;
            continue;
        }
        if (c < '0' || c > '9') break;
        int digit = c - '0';
        if (!seen_dot)
            v = v * 10.0 + (double)digit;
        else
        {
            v = v + frac * (double)digit;
            frac *= 0.1;
        }
    }
    return v;
}

static int decode_literal_escape(char esc, int64_t *out)
{
    if (!out)
        return 0;
    switch (esc)
    {
    case '0': *out = 0; return 1;
    case 'b': *out = '\b'; return 1;
    case 'n': *out = '\n'; return 1;
    case 'r': *out = '\r'; return 1;
    case 't': *out = '\t'; return 1;
    case '\\': *out = '\\'; return 1;
    case '\'': *out = '\''; return 1;
    case '"': *out = '"'; return 1;
    default:
        return 0;
    }
}

static int64_t parse_char_lit(const Token *t)
{
    if (!t || t->length < 3)
        return 0;
    const char *s = t->start + 1;
    size_t len = t->length - 2;
    if (len == 1)
        return (unsigned char)s[0];
    if (len == 2 && s[0] == '\\')
    {
        int64_t out = 0;
        if (decode_literal_escape(s[1], &out))
            return out;
        return 0;
    }
    return 0;
}

static void parse_string_lit_range(Parser *p, const Token *t, int start, int len, int raw, int fold_braces,
                                   const char **out_ptr, int *out_len)
{
    if (out_ptr) *out_ptr = "";
    if (out_len) *out_len = 0;
    if (!t || t->length < 2)
        return;

    int src_len = (int)t->length - 2;
    if (start < 0) start = 0;
    if (start > src_len) start = src_len;
    if (len < 0 || start + len > src_len)
        len = src_len - start;

    const char *s = t->start + 1 + start;
    char *buf = (char *)kn_malloc((size_t)len + 1);
    if (!buf) kn_die("out of memory");

    int out = 0;
    for (int i = 0; i < len; i++)
    {
        char c = s[i];
        if (fold_braces && c == '{' && i + 1 < len && s[i + 1] == '{')
        {
            buf[out++] = '{';
            i++;
            continue;
        }
        if (fold_braces && c == '}' && i + 1 < len && s[i + 1] == '}')
        {
            buf[out++] = '}';
            i++;
            continue;
        }
        if (raw || c != '\\')
        {
            buf[out++] = c;
            continue;
        }

        if (i + 1 >= len)
        {
            kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col + 1 + start + i, 1,
                           "Invalid String Escape",
                           "String literal ends with an incomplete escape sequence",
                           "<invalid-string-escape>");
            buf[out++] = '\\';
            continue;
        }

        int64_t esc = 0;
        if (!decode_literal_escape(s[i + 1], &esc))
        {
            kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col + 1 + start + i, 2,
                           "Invalid String Escape",
                           "Unsupported escape sequence in string literal",
                           "<invalid-string-escape>");
            buf[out++] = s[i + 1];
            i++;
            continue;
        }

        buf[out++] = (char)esc;
        i++;
    }

    buf[out] = 0;
    if (out_ptr) *out_ptr = buf;
    if (out_len) *out_len = out;
}

static void parse_string_lit(Parser *p, const Token *t, const char **out_ptr, int *out_len)
{
    parse_string_lit_range(p, t, 0, -1, 0, 0, out_ptr, out_len);
}

static bool is_access_modifier_tok(TokenType t)
{
    return t == TOK_PUBLIC || t == TOK_PRIVATE || t == TOK_PROTECTED || t == TOK_INTERNAL;
}

static Stmt *parse_local_named_function_stmt(Parser *p)
{
    Token *t = cur(p);
    Safety safety = SAFETY_SAFE;
    if (t->type == TOK_SAFE || t->type == TOK_TRUSTED || t->type == TOK_UNSAFE)
        safety = parse_optional_safety(p, SAFETY_SAFE);

    expect(p, TOK_FUNCTION, "Expected Function", "Expected 'Function' keyword");
    Type ret = parse_type(p);

    Token *name_tok = cur(p);
    expect(p, TOK_ID, "Expected Identifier", "Expected function name");
    const char *name = tok_text(name_tok);
    if (cur(p)->type == TOK_LT)
    {
        const char **tp = 0;
        int tcount = 0;
        int tcap = 0;
        parse_type_param_list(p, &tp, &tcount, &tcap);
        if (tcount > 0)
            kn_diag_report(p->src, KN_STAGE_PARSER, name_tok->line, name_tok->col, (int)name_tok->length,
                           "Unsupported Generic Local Function", "Generic local named functions are not supported yet", name);
    }

    expect(p, TOK_LPAREN, "Expected '('", "Missing '(' after function name");
    ParamList params = parse_param_list(p, 1, "Missing ')' after parameters");

    Stmt *body = parse_block(p);
    Expr *init = new_expr(EXPR_ANON_FUNC, t->line, t->col);
    init->v.anon_func.ret_type = ret;
    init->v.anon_func.params = params.items;
    init->v.anon_func.param_count = params.count;
    init->v.anon_func.param_cap = params.cap;
    init->v.anon_func.body = body;
    init->v.anon_func.safety = safety;
    init->v.anon_func.id = p->next_anon_func_id++;
    init->v.anon_func.debug_name = name;
    init->v.anon_func.qname = 0;

    Stmt *s = new_stmt(ST_VAR, t->line, t->col);
    s->v.var.type = type_class("IO.Type.Object.Function");
    s->v.var.name = name;
    s->v.var.name_line = name_tok->line;
    s->v.var.name_col = name_tok->col;
    s->v.var.init = init;
    s->v.var.is_const = 1;
    s->v.var.src = p->src;
    s->v.var.unit = p->unit_name;
    (void)match(p, TOK_SEMI);
    return s;
}

static Stmt *parse_stmt(Parser *p)
{
    Token *t = cur(p);
    if (p->in_block_literal && t->type == TOK_RECORD)
    {
        expect(p, TOK_RECORD, "Expected Record", "Expected 'Record' keyword");
        Token *name_tok = cur(p);
        expect(p, TOK_ID, "Expected Identifier", "Expected record name");
        Stmt *s = new_stmt(ST_BLOCK_RECORD, t->line, t->col);
        s->v.record.name = tok_text(name_tok);
        s->v.record.ip = -1;
        s->v.record.nested_depth = 0;
        (void)match(p, TOK_SEMI);
        return s;
    }
    if (p->in_block_literal && t->type == TOK_JUMP)
    {
        expect(p, TOK_JUMP, "Expected Jump", "Expected 'Jump' keyword");
        Expr *target = parse_expr(p);
        expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after Jump");
        Stmt *s = new_stmt(ST_BLOCK_JUMP, t->line, t->col);
        s->v.jump.target = target;
        return s;
    }
    if (p->in_block_literal)
    {
        TokenType t0 = t->type;
        TokenType t1 = peek(p, 1)->type;
        if (t0 == TOK_STRUCT)
        {
            (void)parse_struct(p);
            return new_stmt(ST_BLOCK, t->line, t->col);
        }
        if (t0 == TOK_ENUM)
        {
            (void)parse_enum(p);
            return new_stmt(ST_BLOCK, t->line, t->col);
        }
        if (t0 == TOK_INTERFACE || (is_access_modifier_tok(t0) && t1 == TOK_INTERFACE))
        {
            (void)parse_interface(p);
            return new_stmt(ST_BLOCK, t->line, t->col);
        }
        if (t0 == TOK_CLASS || is_access_modifier_tok(t0) || t0 == TOK_ABSTRACT || t0 == TOK_SEALED)
        {
            (void)parse_class(p);
            return new_stmt(ST_BLOCK, t->line, t->col);
        }
        if (t0 == TOK_FUNCTION || ((t0 == TOK_SAFE || t0 == TOK_TRUSTED || t0 == TOK_UNSAFE) && t1 == TOK_FUNCTION))
            return parse_local_named_function_stmt(p);
    }
    if (t->type == TOK_BLOCK || ((t->type == TOK_SAFE || t->type == TOK_TRUSTED || t->type == TOK_UNSAFE) && peek(p, 1)->type == TOK_BLOCK))
    {
        Safety safety = SAFETY_SAFE;
        if (t->type == TOK_SAFE || t->type == TOK_TRUSTED || t->type == TOK_UNSAFE)
            safety = parse_optional_safety(p, SAFETY_SAFE);
        expect(p, TOK_BLOCK, "Expected Block", "Expected 'Block' keyword");
        Token *name_tok = cur(p);
        expect(p, TOK_ID, "Expected Identifier", "Expected block variable name");
        const char *name = tok_text(name_tok);
        Expr *init = parse_block_literal_expr(p, safety, name);
        Stmt *s = new_stmt(ST_VAR, t->line, t->col);
        s->v.var.type = type_class("IO.Type.Object.Block");
        s->v.var.name = name;
        s->v.var.name_line = name_tok->line;
        s->v.var.name_col = name_tok->col;
        s->v.var.init = init;
        s->v.var.is_const = 0;
        s->v.var.src = p->src;
        s->v.var.unit = p->unit_name;
        (void)match(p, TOK_SEMI);
        return s;
    }
    if (t->type == TOK_LBRACE) return parse_block(p);
    if (t->type == TOK_TRY) return parse_try(p);
    if (t->type == TOK_THROW) return parse_throw(p);
    if (t->type == TOK_IF) return parse_if(p);
    if (t->type == TOK_SWITCH) return parse_switch(p);
    if (t->type == TOK_WHILE) return parse_while(p);
    if (t->type == TOK_FOR) return parse_for(p);
    if (match(p, TOK_RETURN))
    {
        Stmt *s = new_stmt(ST_RETURN, t->line, t->col);
        if (cur(p)->type != TOK_SEMI)
            s->v.ret.expr = parse_expr(p);
        expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after return");
        return s;
    }
    if (match(p, TOK_BREAK))
    {
        Stmt *s = new_stmt(ST_BREAK, t->line, t->col);
        expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after break");
        return s;
    }
    if (match(p, TOK_CONTINUE))
    {
        Stmt *s = new_stmt(ST_CONTINUE, t->line, t->col);
        expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after continue");
        return s;
    }
    if (t->type == TOK_GET)
    {
        kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col, (int)t->length,
                       "Invalid Get Position", "Get must appear at top-level after Unit and before declarations", "Get");
    }

    // var decl (builtins, Var/Const, or qualified type)
    if (looks_like_type_decl(p))
    {
        int errs_before = kn_diag_error_count();
        int is_const = 0;
        if (match(p, TOK_CONST)) is_const = 1;
        Type ty = type_make(TY_UNKNOWN);
        if (match(p, TOK_VAR)) ty = type_make(TY_UNKNOWN);
        else ty = parse_type(p);
        Token *name_tok = cur(p);
        expect(p, TOK_ID, "Expected Identifier", "Expected variable name");
        const char *name = tok_text(name_tok);
        (void)parse_legacy_array_suffix(p, &ty, name_tok);
        int skip_semi = 0;
        Expr *init = parse_optional_decl_initializer(p, name_tok, &skip_semi);
        if (!skip_semi)
            expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after declaration");
        if (kn_diag_error_count() > errs_before)
            return new_stmt(ST_BLOCK, t->line, t->col);
        Stmt *s = new_stmt(ST_VAR, t->line, t->col);
        s->v.var.type = ty;
        s->v.var.name = name;
        s->v.var.name_line = name_tok->line;
        s->v.var.name_col = name_tok->col;
        s->v.var.init = init;
        s->v.var.is_const = is_const;
        s->v.var.src = p->src;
        s->v.var.unit = p->unit_name;
        return s;
    }

    Expr *e = parse_expr(p);
    expect_semi_after_expr(p, e);
    Stmt *s = new_stmt(ST_EXPR, t->line, t->col);
    s->v.expr.expr = e;
    return s;
}

