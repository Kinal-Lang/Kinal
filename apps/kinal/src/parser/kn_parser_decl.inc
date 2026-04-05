// Internal parser fragment: declarations

static AttrList parse_attributes(Parser *p)
{
    AttrList list;
    kn_memset(&list, 0, sizeof(list));
    while (match(p, TOK_LBRACKET))
    {
        for (;;)
        {
            const char *name = parse_qualified_name(p);
            Attribute a;
            kn_memset(&a, 0, sizeof(a));
            a.name = name;
            if (match(p, TOK_LPAREN))
            {
                if (!match(p, TOK_RPAREN))
                {
                    for (;;)
                    {
                        Expr *arg = parse_expr(p);
                        exprlist_push(&a.args, arg);
                        if (match(p, TOK_COMMA)) continue;
                        expect(p, TOK_RPAREN, "Expected ')'", "Missing ')' in attribute");
                        break;
                    }
                }
            }
            attrlist_push(&list, a);
            if (match(p, TOK_COMMA)) continue;
            expect(p, TOK_RBRACKET, "Expected ']'", "Missing ']' after attribute");
            break;
        }
    }
    return list;
}

static const char *parse_qualified_name(Parser *p)
{
    const char *first = parse_name_token(p, "Expected Identifier", "Expected name");
    const char *parts[16];
    int part_count = 0;
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

static const char *qualify_name(const char *unit, const char *name)
{
    if (!unit || !name) return name;
    size_t ulen = kn_strlen(unit);
    size_t nlen = kn_strlen(name);
    char *out = (char *)kn_malloc(ulen + 1 + nlen + 1);
    if (!out) kn_die("out of memory");
    kn_memcpy(out, unit, ulen);
    out[ulen] = '.';
    kn_memcpy(out + ulen + 1, name, nlen);
    out[ulen + 1 + nlen] = 0;
    return out;
}

static bool starts_with_io(const char *name)
{
    if (!name) return false;
    return kn_strncmp(name, "IO.", 3) == 0;
}

static bool split_module_member(const char *qname, const char **module_out, const char **member_out)
{
    if (!qname || !module_out || !member_out) return false;
    const char *last = 0;
    for (const char *p = qname; *p; p++)
        if (*p == '.') last = p;
    if (!last || last == qname || !last[1]) return false;

    size_t mlen = (size_t)(last - qname);
    char *mod = (char *)kn_malloc(mlen + 1);
    if (!mod) kn_die("out of memory");
    kn_memcpy(mod, qname, mlen);
    mod[mlen] = 0;

    *module_out = mod;
    *member_out = last + 1;
    return true;
}

static void parse_get(Parser *p, ImportList *imports)
{
    expect(p, TOK_GET, "Expected Get", "Expected 'Get' keyword");

    Token *first_tok = cur(p);
    const char *first = parse_qualified_name(p);
    int first_is_qualified = 0;
    if (first)
    {
        for (const char *q = first; *q; q++)
        {
            if (*q == '.')
            {
                first_is_qualified = 1;
                break;
            }
        }
    }

    if (match(p, TOK_BY))
    {
        const char *target = parse_qualified_name(p);
        if (first_is_qualified)
        {
            kn_diag_report(p->src, KN_STAGE_PARSER, first_tok->line, first_tok->col,
                           (int)(first_tok->length ? first_tok->length : 1),
                           "Invalid Get Alias",
                           "Alias before By must be a simple identifier",
                           0);
            expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after Get");
            return;
        }
        const char *module = 0;
        const char *member = 0;
        bool split_ok = split_module_member(target, &module, &member);
        bool target_is_module = starts_with_io(target) && kn_std_module_has(target);
        bool by_symbol = split_ok && starts_with_io(target) && !target_is_module && kn_std_module_has(module);

        ImportMap m;
        m.line = first_tok->line;
        m.col = first_tok->col;
        m.unit = p->unit_name;
        m.global = 0;
        if (by_symbol)
        {
            // Get Alias By IO.Console.PrintLine;
            m.kind = IMPORT_SYMBOL;
            m.module = module;
            m.local = first;
            m.remote = member;
            m.global = 1;
        }
        else
        {
            // Get Alias By IO.Console;
            m.kind = IMPORT_ALIAS;
            m.module = target;
            m.local = first;
            m.remote = 0;
        }
        importlist_push(imports, m);
        expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after Get");
        return;
    }

    if (match(p, TOK_LBRACE))
    {
        const char *module = first;
        for (;;)
        {
            if (match(p, TOK_RBRACE))
                break;
            Token *local = cur(p);
            expect(p, TOK_ID, "Expected Identifier", "Expected name");
            const char *local_name = tok_text(local);
            const char *remote_name = local_name;
            if (match(p, TOK_BY))
            {
                Token *rem = cur(p);
                expect(p, TOK_ID, "Expected Identifier", "Expected remote name");
                remote_name = tok_text(rem);
            }
            ImportMap m;
            m.kind = IMPORT_SYMBOL;
            m.module = module;
            m.local = local_name;
            m.remote = remote_name;
            m.unit = p->unit_name;
            m.global = 0;
            m.line = local->line;
            m.col = local->col;
            importlist_push(imports, m);
            if (match(p, TOK_COMMA)) continue;
            expect(p, TOK_RBRACE, "Expected '}'", "Missing '}'");
            break;
        }
        expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after Get");
        return;
    }

    // Get IO.Console.PrintLine; -> module-scoped symbol import
    // Get App.Test;            -> open module import
    const char *module = 0;
    const char *member = 0;
    bool split_ok = split_module_member(first, &module, &member);
    bool first_is_module = starts_with_io(first) && kn_std_module_has(first);
    bool has_member = split_ok && starts_with_io(first) && !first_is_module && kn_std_module_has(module);
    if (has_member)
    {
        ImportMap m;
        m.kind = IMPORT_SYMBOL;
        m.module = module;
        m.local = member;
        m.remote = member;
        m.unit = p->unit_name;
        m.global = 0;
        m.line = first_tok->line;
        m.col = first_tok->col;
        importlist_push(imports, m);
    }
    else
    {
        // open module: Get IO.Console; / Get App.Test;
        ImportMap m;
        m.kind = IMPORT_OPEN;
        m.module = first;
        m.local = 0;
        m.remote = 0;
        m.unit = p->unit_name;
        m.global = 0;
        m.line = first_tok->line;
        m.col = first_tok->col;
        importlist_push(imports, m);
    }
    expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after Get");
}

static Access parse_access(Parser *p, Access def)
{
    if (match(p, TOK_PUBLIC)) return ACC_PUBLIC;
    if (match(p, TOK_PRIVATE)) return ACC_PRIVATE;
    if (match(p, TOK_PROTECTED)) return ACC_PROTECTED;
    if (match(p, TOK_INTERNAL)) return ACC_INTERNAL;
    return def;
}

static bool match_contextual(Parser *p, const char *text)
{
    if (!p || !text) return false;
    if (cur(p)->type == TOK_ID && token_text_eq(cur(p), text))
    {
        p->pos++;
        return true;
    }
    return false;
}

static uint32_t parse_meta_target_atom(Parser *p)
{
    Token *t = cur(p);
    if (!match(p, TOK_AT))
    {
        kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col,
                       (int)(t->length ? t->length : 1),
                       "Invalid Meta Target",
                       "Meta targets must use built-in constants like @function, @method, @class, @field",
                       tok_got(p));
        if (cur(p)->type != TOK_EOF)
            p->pos++;
        return KN_META_TARGET_NONE;
    }

    t = cur(p);
    if (cur(p)->type == TOK_ID && token_text_eq(cur(p), "function"))
    {
        p->pos++;
        return KN_META_TARGET_FUNCTION;
    }
    if (cur(p)->type == TOK_ID && token_text_eq(cur(p), "extern_function"))
    {
        p->pos++;
        return KN_META_TARGET_EXTERN_FUNCTION;
    }
    if (cur(p)->type == TOK_ID && token_text_eq(cur(p), "delegate_function"))
    {
        p->pos++;
        return KN_META_TARGET_DELEGATE_FUNCTION;
    }
    if (cur(p)->type == TOK_ID && token_text_eq(cur(p), "class"))
    {
        p->pos++;
        return KN_META_TARGET_CLASS;
    }
    if (cur(p)->type == TOK_ID && token_text_eq(cur(p), "struct"))
    {
        p->pos++;
        return KN_META_TARGET_STRUCT;
    }
    if (cur(p)->type == TOK_ID && token_text_eq(cur(p), "enum"))
    {
        p->pos++;
        return KN_META_TARGET_ENUM;
    }
    if (cur(p)->type == TOK_ID && token_text_eq(cur(p), "interface"))
    {
        p->pos++;
        return KN_META_TARGET_INTERFACE;
    }
    if (cur(p)->type == TOK_ID && token_text_eq(cur(p), "method"))
    {
        p->pos++;
        return KN_META_TARGET_METHOD;
    }
    if (cur(p)->type == TOK_ID && token_text_eq(cur(p), "field"))
    {
        p->pos++;
        return KN_META_TARGET_FIELD;
    }
    kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col,
                   (int)(t->length ? t->length : 1),
                   "Invalid Meta Target",
                   "Supported Meta targets are @function, @extern_function, @delegate_function, @class, @struct, @enum, @interface, @method, @field",
                   tok_got(p));
    if (cur(p)->type != TOK_EOF)
        p->pos++;
    return KN_META_TARGET_NONE;
}

static int parse_meta_keep_kind(Parser *p)
{
    Token *t = cur(p);
    if (!match(p, TOK_AT))
    {
        kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col,
                       (int)(t->length ? t->length : 1),
                       "Invalid Meta Keep",
                       "Meta keep modes must use built-in constants like @source, @compile, @runtime",
                       tok_got(p));
        if (cur(p)->type != TOK_EOF)
            p->pos++;
        return KN_META_KEEP_NONE;
    }

    t = cur(p);
    if (cur(p)->type == TOK_ID && token_text_eq(cur(p), "source")) { p->pos++; return KN_META_KEEP_SOURCE; }
    if (cur(p)->type == TOK_ID && token_text_eq(cur(p), "compile")) { p->pos++; return KN_META_KEEP_COMPILE; }
    if (cur(p)->type == TOK_ID && token_text_eq(cur(p), "runtime")) { p->pos++; return KN_META_KEEP_RUNTIME; }
    kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col,
                   (int)(t->length ? t->length : 1),
                   "Invalid Meta Keep",
                   "Supported Meta keep modes are @source, @compile, @runtime",
                   tok_got(p));
    if (cur(p)->type != TOK_EOF)
        p->pos++;
    return KN_META_KEEP_NONE;
}

static MetaDecl parse_meta_decl(Parser *p)
{
    Token *meta_tok = cur(p);
    if (!match_contextual(p, "Meta"))
        expect(p, TOK_ID, "Expected Meta", "Expected Meta declaration");

    Token *name_tok = cur(p);
    expect(p, TOK_ID, "Expected Identifier", "Expected Meta name");
    const char *name = tok_text(name_tok);

    expect(p, TOK_LPAREN, "Expected '('", "Missing '(' after Meta name");
    ParamList params = parse_param_list(p, 1, "Missing ')' after Meta parameters");

    MetaDecl m;
    kn_memset(&m, 0, sizeof(m));
    m.name = name;
    m.qname = qualify_name(p->unit_name, name);
    m.params = params;
    m.keep_kind = KN_META_KEEP_NONE;
    m.src = p->src;
    m.line = meta_tok->line;
    m.col = meta_tok->col;
    m.unit = p->unit_name;

    expect(p, TOK_LBRACE, "Expected '{'", "Missing '{' in Meta declaration");

    int saw_on = 0;
    int saw_keep = 0;
    while (!match(p, TOK_RBRACE))
    {
        if (cur(p)->type == TOK_EOF)
        {
            kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                           "Expected '}'", "Unexpected end of file while parsing Meta declaration", "<end-of-file>");
            break;
        }
        if (match_contextual(p, "On"))
        {
            if (saw_on)
            {
                kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                               "Duplicate Meta Clause", "Meta declaration can only define one On clause", "On");
            }
            saw_on = 1;
            for (;;)
            {
                m.target_mask |= parse_meta_target_atom(p);
                if (match(p, TOK_COMMA)) continue;
                expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after Meta On clause");
                break;
            }
            continue;
        }
        if (match_contextual(p, "Keep"))
        {
            if (saw_keep)
            {
                kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                               "Duplicate Meta Clause", "Meta declaration can only define one Keep clause", "Keep");
            }
            saw_keep = 1;
            m.keep_kind = parse_meta_keep_kind(p);
            expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after Meta Keep clause");
            continue;
        }
        if (match_contextual(p, "Repeatable"))
        {
            if (m.repeatable)
            {
                kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                               "Duplicate Meta Clause", "Meta declaration can only define Repeatable once", "Repeatable");
            }
            m.repeatable = 1;
            expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after Repeatable");
            continue;
        }

        {
            Token *bad = cur(p);
            kn_diag_report(p->src, KN_STAGE_PARSER, bad->line, bad->col,
                           (int)(bad->length ? bad->length : 1),
                           "Invalid Meta Clause",
                           "Meta body only supports On, Keep, and Repeatable clauses",
                           tok_got(p));
            p->pos++;
        }
    }

    if (!saw_on)
        kn_diag_report(p->src, KN_STAGE_PARSER, meta_tok->line, meta_tok->col,
                       (int)(meta_tok->length ? meta_tok->length : 1),
                       "Missing Meta Clause", "Meta declaration requires an On clause", name);
    if (!saw_keep)
        kn_diag_report(p->src, KN_STAGE_PARSER, meta_tok->line, meta_tok->col,
                       (int)(meta_tok->length ? meta_tok->length : 1),
                       "Missing Meta Clause", "Meta declaration requires a Keep clause", name);

    return m;
}

static void parse_member_mods(Parser *p, Access *access, int *is_static, int *is_virtual,
                              int *is_override, int *is_abstract, int *is_sealed,
                              int *is_async, Safety *safety)
{
    bool progress = true;
    while (progress)
    {
        progress = false;
        Access acc = parse_access(p, *access);
        if (acc != *access)
        {
            *access = acc;
            progress = true;
        }
        if (match(p, TOK_STATIC)) { *is_static = 1; progress = true; }
        if (match(p, TOK_VIRTUAL)) { *is_virtual = 1; progress = true; }
        if (match(p, TOK_OVERRIDE)) { *is_override = 1; progress = true; }
        if (match(p, TOK_ABSTRACT)) { *is_abstract = 1; progress = true; }
        if (match(p, TOK_SEALED)) { *is_sealed = 1; progress = true; }
        if (match(p, TOK_ASYNC)) { *is_async = 1; progress = true; }
        if (match(p, TOK_SAFE)) { *safety = SAFETY_SAFE; progress = true; }
        if (match(p, TOK_TRUSTED)) { *safety = SAFETY_TRUSTED; progress = true; }
        if (match(p, TOK_UNSAFE)) { *safety = SAFETY_UNSAFE; progress = true; }
    }
}

static void parse_class_mods(Parser *p, Access *access, int *is_static_class, int *is_abstract, int *is_sealed)
{
    bool progress = true;
    while (progress)
    {
        progress = false;
        Access acc = parse_access(p, *access);
        if (acc != *access)
        {
            *access = acc;
            progress = true;
        }
        if (match(p, TOK_STATIC)) { *is_static_class = 1; progress = true; }
        if (match(p, TOK_ABSTRACT)) { *is_abstract = 1; progress = true; }
        if (match(p, TOK_SEALED)) { *is_sealed = 1; progress = true; }
    }
}

typedef enum
{
    KN_NESTED_DECL_NONE = 0,
    KN_NESTED_DECL_CLASS,
    KN_NESTED_DECL_INTERFACE,
    KN_NESTED_DECL_STRUCT,
    KN_NESTED_DECL_ENUM
} KnNestedDeclKind;

static KnNestedDeclKind preview_nested_type_decl(Parser *p)
{
    if (!p) return KN_NESTED_DECL_NONE;
    int pos = p->pos;

    while (1)
    {
        TokenType t = p->toks[pos].type;
        if (t == TOK_PUBLIC || t == TOK_PRIVATE || t == TOK_PROTECTED || t == TOK_INTERNAL)
        {
            pos++;
            continue;
        }
        break;
    }

    int pos_after_access = pos;
    while (1)
    {
        TokenType t = p->toks[pos].type;
        if (t == TOK_STATIC || t == TOK_ABSTRACT || t == TOK_SEALED)
        {
            pos++;
            continue;
        }
        break;
    }

    if (p->toks[pos].type == TOK_CLASS)
        return KN_NESTED_DECL_CLASS;
    if (p->toks[pos_after_access].type == TOK_INTERFACE)
        return KN_NESTED_DECL_INTERFACE;
    if (p->toks[pos_after_access].type == TOK_STRUCT)
        return KN_NESTED_DECL_STRUCT;
    if (p->toks[pos_after_access].type == TOK_ENUM)
        return KN_NESTED_DECL_ENUM;
    return KN_NESTED_DECL_NONE;
}

static void parse_and_store_nested_type(Parser *p, AttrList attrs)
{
    if (!p) return;
    switch (preview_nested_type_decl(p))
    {
    case KN_NESTED_DECL_CLASS:
    {
        ClassDecl c = parse_class(p);
        c.attrs = attrs;
        if (p->classes_out)
            classlist_push(p->classes_out, c);
        return;
    }
    case KN_NESTED_DECL_INTERFACE:
    {
        (void)parse_access(p, ACC_PRIVATE);
        InterfaceDecl i = parse_interface(p);
        i.attrs = attrs;
        if (p->interfaces_out)
            interfacelist_push(p->interfaces_out, i);
        return;
    }
    case KN_NESTED_DECL_STRUCT:
    {
        (void)parse_access(p, ACC_PRIVATE);
        StructDecl s = parse_struct(p);
        s.attrs = attrs;
        if (p->structs_out)
            structlist_push(p->structs_out, s);
        return;
    }
    case KN_NESTED_DECL_ENUM:
    {
        (void)parse_access(p, ACC_PRIVATE);
        EnumDecl e = parse_enum(p);
        e.attrs = attrs;
        if (p->enums_out)
            enumlist_push(p->enums_out, e);
        return;
    }
    default:
        return;
    }
}

static bool looks_like_class_decl(Parser *p)
{
    if (!p) return false;
    int pos = p->pos;
    bool progress = true;
    while (progress)
    {
        progress = false;
        TokenType t = p->toks[pos].type;
        switch (t)
        {
        case TOK_PUBLIC:
        case TOK_PRIVATE:
        case TOK_PROTECTED:
        case TOK_INTERNAL:
        case TOK_STATIC:
        case TOK_ABSTRACT:
        case TOK_SEALED:
            pos++;
            progress = true;
            break;
        default:
            break;
        }
    }
    return p->toks[pos].type == TOK_CLASS;
}

static bool looks_like_named_constructor(Parser *p, const char *class_name)
{
    return cur(p)->type == TOK_FUNCTION &&
           peek(p, 1)->type == TOK_ID &&
           token_text_eq(peek(p, 1), class_name) &&
           peek(p, 2)->type == TOK_LPAREN;
}

static const char *property_accessor_name(const char *prefix, const char *name)
{
    if (!prefix || !name) return name;
    size_t plen = kn_strlen(prefix);
    size_t nlen = kn_strlen(name);
    char *out = (char *)kn_malloc(plen + nlen + 1);
    if (!out) kn_die("out of memory");
    kn_memcpy(out, prefix, plen);
    kn_memcpy(out + plen, name, nlen);
    out[plen + nlen] = 0;
    return out;
}

static const char *property_getter_name(const char *name)
{
    return property_accessor_name("__propget$", name);
}

static const char *property_setter_name(const char *name)
{
    return property_accessor_name("__propset$", name);
}

static Stmt *wrap_stmt_in_block(Stmt *body, int line, int col)
{
    if (!body)
    {
        Stmt *block = new_stmt(ST_BLOCK, line, col);
        return block;
    }
    if (body->kind == ST_BLOCK)
        return body;

    Stmt *block = new_stmt(ST_BLOCK, line, col);
    stmtlist_push(&block->v.block.stmts, body);
    return block;
}

static Stmt *append_setter_return(Stmt *body, int line, int col)
{
    Stmt *block = wrap_stmt_in_block(body, line, col);
    Stmt *ret = new_stmt(ST_RETURN, line, col);
    Expr *value = new_expr(EXPR_VAR, line, col);
    value->v.var.name = "value";
    ret->v.ret.expr = value;
    stmtlist_push(&block->v.block.stmts, ret);
    return block;
}

static Stmt *parse_property_accessor_body(Parser *p, int is_abstract, const char *kind)
{
    Token *t = cur(p);
    if (match(p, TOK_SEMI))
    {
        if (!is_abstract)
        {
            kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col,
                           (int)(t->length ? t->length : 1),
                           "Property Accessor Body",
                           "Non-abstract property accessor requires a block body",
                           kind);
        }
        return 0;
    }
    if (is_abstract)
    {
        kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col,
                       (int)(t->length ? t->length : 1),
                       "Abstract Property Accessor",
                       "Abstract property accessor must end with ';'",
                       kind);
    }
    return parse_block(p);
}

static Method make_property_getter(Parser *p, Token *decl_tok, const char *name, Type type,
                                   Access access, int is_static, int is_virtual,
                                   int is_override, int is_abstract, int is_sealed,
                                   Safety safety, Stmt *body)
{
    Method m;
    kn_memset(&m, 0, sizeof(m));
    m.name = property_getter_name(name);
    m.ret_type = type;
    m.params.items = 0;
    m.params.count = 0;
    m.params.cap = 0;
    m.body = body;
    m.safety = safety;
    m.access = access;
    m.is_static = is_static;
    m.is_virtual = is_virtual;
    m.is_override = is_override;
    m.is_abstract = body ? 0 : is_abstract;
    m.is_sealed = is_sealed;
    m.is_constructor = 0;
    m.src = p->src;
    m.line = decl_tok->line;
    m.col = decl_tok->col;
    return m;
}

static Method make_property_setter(Parser *p, Token *decl_tok, const char *name, Type type,
                                   Access access, int is_static, int is_virtual,
                                   int is_override, int is_abstract, int is_sealed,
                                   Safety safety, Stmt *body)
{
    Method m;
    kn_memset(&m, 0, sizeof(m));
    m.name = property_setter_name(name);
    m.ret_type = type;
    m.params.items = (Param *)kn_malloc(sizeof(Param));
    if (!m.params.items) kn_die("out of memory");
    m.params.count = 1;
    m.params.cap = 1;
    kn_memset(m.params.items, 0, sizeof(Param));
    m.params.items[0].name = "value";
    m.params.items[0].type = type;
    m.params.items[0].src = p->src;
    m.body = body ? append_setter_return(body, decl_tok->line, decl_tok->col) : 0;
    m.safety = safety;
    m.access = access;
    m.is_static = is_static;
    m.is_virtual = is_virtual;
    m.is_override = is_override;
    m.is_abstract = body ? 0 : is_abstract;
    m.is_sealed = is_sealed;
    m.is_constructor = 0;
    m.src = p->src;
    m.line = decl_tok->line;
    m.col = decl_tok->col;
    return m;
}

static void parse_property(Parser *p, Access access, int is_static, int is_virtual,
                           int is_override, int is_abstract, int is_sealed,
                           Safety safety, AttrList attrs, MethodList *methods)
{
    Token *decl_tok = cur(p);
    expect(p, TOK_PROPERTY, "Expected Property", "Expected 'Property' keyword");
    Type type = parse_type(p);
    Token *name_tok = cur(p);
    expect(p, TOK_ID, "Expected Identifier", "Expected property name");
    const char *name = tok_text(name_tok);
    (void)parse_legacy_array_suffix(p, &type, name_tok);
    expect(p, TOK_LBRACE, "Expected '{'", "Missing '{' in property");

    int saw_get = 0;
    int saw_set = 0;
    while (!match(p, TOK_RBRACE))
    {
        if (cur(p)->type == TOK_EOF)
        {
            kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                           "Expected '}'", "Unexpected end of file while parsing property body", "<end-of-file>");
            break;
        }

        if (match(p, TOK_GET))
        {
            Stmt *body = parse_property_accessor_body(p, is_abstract, "Get");
            if (saw_get)
            {
                kn_diag_report(p->src, KN_STAGE_PARSER, decl_tok->line, decl_tok->col,
                               (int)(name_tok->length ? name_tok->length : 1),
                               "Duplicate Property Accessor",
                               "Property already defines a Get accessor",
                               name);
                continue;
            }
            saw_get = 1;
            Method getter = make_property_getter(p, decl_tok, name, type, access, is_static,
                                                 is_virtual, is_override, is_abstract,
                                                 is_sealed, safety, body);
            getter.attrs = attrs;
            methodlist_push(methods, getter);
            continue;
        }

        if (match(p, TOK_SET))
        {
            Stmt *body = parse_property_accessor_body(p, is_abstract, "Set");
            if (saw_set)
            {
                kn_diag_report(p->src, KN_STAGE_PARSER, decl_tok->line, decl_tok->col,
                               (int)(name_tok->length ? name_tok->length : 1),
                               "Duplicate Property Accessor",
                               "Property already defines a Set accessor",
                               name);
                continue;
            }
            saw_set = 1;
            Method setter = make_property_setter(p, decl_tok, name, type, access, is_static,
                                                 is_virtual, is_override, is_abstract,
                                                 is_sealed, safety, body);
            setter.attrs = attrs;
            methodlist_push(methods, setter);
            continue;
        }

        Token *bad = cur(p);
        kn_diag_report(p->src, KN_STAGE_PARSER, bad->line, bad->col,
                       (int)(bad->length ? bad->length : 1),
                       "Invalid Property Accessor",
                       "Property body only supports Get and Set accessors",
                       tok_got(p));
        p->pos++;
    }

    if (!saw_get && !saw_set)
    {
        kn_diag_report(p->src, KN_STAGE_PARSER, decl_tok->line, decl_tok->col,
                       (int)(name_tok->length ? name_tok->length : 1),
                       "Missing Property Accessor",
                       "Property must define at least one accessor",
                       name);
    }
}

static Method parse_method(Parser *p, Access access, int is_static, int is_virtual,
                           int is_override, int is_abstract, int is_sealed, int is_async,
                           Safety safety)
{
    expect(p, TOK_FUNCTION, "Expected Function", "Expected 'Function' keyword");
    Token *ret_tok = cur(p);
    Type ret = parse_type(p);
    Token *name_tok = cur(p);
    expect(p, TOK_ID, "Expected Identifier", "Expected method name");
    const char *name = tok_text(name_tok);
    if (cur(p)->type == TOK_LT)
    {
        const char **tp = 0;
        int tcount = 0;
        int tcap = 0;
        parse_type_param_list(p, &tp, &tcount, &tcap);
        if (tcount > 0)
            kn_diag_report(p->src, KN_STAGE_PARSER, name_tok->line, name_tok->col, (int)name_tok->length,
                           "Unsupported Generic Method", "Generic methods are not supported yet", name);
    }
    expect(p, TOK_LPAREN, "Expected '('", "Missing '(' after method name");
    ParamList params = parse_param_list(p, 1, "Missing ')' after parameters");
    Method m;
    kn_memset(&m, 0, sizeof(m));
    m.name = name;
    m.ret_type = ret;
    m.params = params;
    m.safety = safety;
    m.access = access;
    m.is_static = is_static;
    m.is_virtual = is_virtual;
    m.is_override = is_override;
    m.is_abstract = is_abstract;
    m.is_sealed = is_sealed;
    m.is_constructor = 0;
    m.is_async = is_async;
    m.async_result_type = ret;
    m.src = p->src;
    m.line = ret_tok->line;
    m.col = ret_tok->col;

    if (is_abstract)
    {
        expect(p, TOK_SEMI, "Expected Semicolon", "Abstract method requires ';'");
        m.body = 0;
    }
    else
    {
        m.body = parse_block(p);
    }
    return m;
}

static Method parse_constructor_core(Parser *p, Access access, int is_static, int is_virtual,
                                     int is_override, int is_abstract, int is_sealed,
                                     Safety safety, Token *t, const char *name)
{
    expect(p, TOK_LPAREN, "Expected '('", "Missing '(' after constructor name");
    ParamList params = parse_param_list(p, 1, "Missing ')' after parameters");
    Method m;
    kn_memset(&m, 0, sizeof(m));
    m.name = name;
    m.ret_type = type_make(TY_VOID);
    m.params = params;
    m.body = parse_block(p);
    m.safety = safety;
    m.access = access;
    m.is_static = is_static;
    m.is_virtual = is_virtual;
    m.is_override = is_override;
    m.is_abstract = is_abstract;
    m.is_sealed = is_sealed;
    m.is_constructor = 1;
    m.src = p->src;
    m.line = t->line;
    m.col = t->col;
    return m;
}

static Method parse_constructor(Parser *p, Access access, int is_static, int is_virtual,
                                int is_override, int is_abstract, int is_sealed, Safety safety)
{
    Token *t = cur(p);
    expect(p, TOK_CONSTRUCTOR, "Expected Constructor", "Expected 'Constructor' keyword");
    return parse_constructor_core(p, access, is_static, is_virtual, is_override, is_abstract,
                                  is_sealed, safety, t, "Constructor");
}

static Method parse_named_constructor(Parser *p, Access access, int is_static, int is_virtual,
                                      int is_override, int is_abstract, int is_sealed,
                                      Safety safety, const char *class_name)
{
    Token *t = cur(p);
    expect(p, TOK_FUNCTION, "Expected Function", "Expected 'Function' keyword");
    expect(p, TOK_ID, "Expected Identifier", "Expected constructor name");
    kn_diag_warn(p->src, KN_STAGE_PARSER, 1, t->line, t->col, (int)(t->length ? t->length : 1),
                 "Legacy Constructor Syntax",
                 "Use 'Constructor(...)' instead of 'Function ClassName(...)'");
    return parse_constructor_core(p, access, is_static, is_virtual, is_override, is_abstract,
                                  is_sealed, safety, t, class_name);
}

static Field parse_field(Parser *p, Access access, int is_static)
{
    Token *t = cur(p);
    Type ty = parse_type(p);
    Token *name_tok = cur(p);
    expect(p, TOK_ID, "Expected Identifier", "Expected field name");
    const char *name = tok_text(name_tok);
    (void)parse_legacy_array_suffix(p, &ty, name_tok);
    Expr *init = 0;
    if (match(p, TOK_ASSIGN))
        init = parse_expr(p);
    expect(p, TOK_SEMI, "Expected Semicolon", "Missing ';' after field");
    Field f;
    kn_memset(&f, 0, sizeof(f));
    f.name = name;
    f.type = ty;
    f.init = init;
    f.access = access;
    f.is_static = is_static;
    f.src = p->src;
    f.line = t->line;
    f.col = t->col;
    return f;
}

static ClassDecl parse_class(Parser *p)
{
    Access access = ACC_PUBLIC;
    int is_static_class = 0;
    int is_abstract = 0;
    int is_sealed = 0;
    parse_class_mods(p, &access, &is_static_class, &is_abstract, &is_sealed);

    Token *t = cur(p);
    expect(p, TOK_CLASS, "Expected Class", "Expected 'Class' keyword");
    Token *name_tok = cur(p);
    expect(p, TOK_ID, "Expected Identifier", "Expected class name");
    const char *name = tok_text(name_tok);
    const char *owner_type_qname = p->decl_prefix;
    const char *class_qname = qualify_decl_name(p, name);

    const char *base = 0;
    const char **ifaces = 0;
    int iface_count = 0, iface_cap = 0;
    if (match(p, TOK_BY))
    {
        const char *first = parse_qualified_name(p);
        base = first;
        while (match(p, TOK_COMMA))
        {
            const char *iname = parse_qualified_name(p);
            if (iface_count + 1 > iface_cap)
            {
                int new_cap = iface_cap ? iface_cap * 2 : 4;
                const char **n = (const char **)kn_malloc(sizeof(char *) * (size_t)new_cap);
                if (ifaces)
                {
                    kn_memcpy(n, ifaces, sizeof(char *) * (size_t)iface_count);
                    kn_free(ifaces);
                }
                ifaces = n;
                iface_cap = new_cap;
            }
            ifaces[iface_count++] = iname;
        }
    }

    expect(p, TOK_LBRACE, "Expected '{'", "Missing '{' in class");
    FieldList fields = {0};
    MethodList methods = {0};
    const char *prev_decl_prefix = p->decl_prefix;
    p->decl_prefix = class_qname;
    while (!match(p, TOK_RBRACE))
    {
        if (cur(p)->type == TOK_EOF)
        {
            kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                           "Expected '}'", "Unexpected end of file while parsing class body", "<end-of-file>");
            break;
        }
        AttrList attrs = parse_attributes(p);
        if (preview_nested_type_decl(p) != KN_NESTED_DECL_NONE)
        {
            parse_and_store_nested_type(p, attrs);
            continue;
        }
        Access m_access = ACC_PRIVATE;
        int is_static = 0, is_virtual = 0, is_override = 0, is_abs = 0, is_sealed = 0;
        Safety safety = SAFETY_SAFE;
        int is_async = 0;
        parse_member_mods(p, &m_access, &is_static, &is_virtual, &is_override, &is_abs, &is_sealed, &is_async, &safety);
        if (is_static_class)
            is_static = 1;

        if (cur(p)->type == TOK_CONSTRUCTOR)
        {
            Method m = parse_constructor(p, m_access, is_static, is_virtual, is_override,
                                         is_abs, is_sealed, safety);
            m.attrs = attrs;
            methodlist_push(&methods, m);
            continue;
        }
        if (looks_like_named_constructor(p, name))
        {
            Method m = parse_named_constructor(p, m_access, is_static, is_virtual, is_override,
                                               is_abs, is_sealed, safety, name);
            m.attrs = attrs;
            methodlist_push(&methods, m);
            continue;
        }
        if (cur(p)->type == TOK_FUNCTION)
        {
            Method m = parse_method(p, m_access, is_static, is_virtual, is_override, is_abs, is_sealed, is_async, safety);
            m.attrs = attrs;
            methodlist_push(&methods, m);
            continue;
        }
        if (cur(p)->type == TOK_PROPERTY)
        {
            parse_property(p, m_access, is_static, is_virtual, is_override, is_abs, is_sealed,
                           safety, attrs, &methods);
            continue;
        }
        // field
        Field f = parse_field(p, m_access, is_static_class ? 1 : is_static);
        f.attrs = attrs;
        fieldlist_push(&fields, f);
    }
    p->decl_prefix = prev_decl_prefix;

    ClassDecl c;
    kn_memset(&c, 0, sizeof(c));
    c.name = name;
    c.qname = class_qname;
    c.owner_type_qname = owner_type_qname;
    c.base = base;
    c.interfaces = ifaces;
    c.interface_count = iface_count;
    c.fields = fields;
    c.methods = methods;
    c.is_static_class = is_static_class;
    c.is_abstract = is_abstract;
    c.is_sealed = is_sealed;
    c.src = p->src;
    c.line = t->line;
    c.col = t->col;
    c.unit = p->unit_name;
    return c;
}

static InterfaceDecl parse_interface(Parser *p)
{
    Access access = parse_access(p, ACC_PUBLIC);
    Token *t = cur(p);
    expect(p, TOK_INTERFACE, "Expected Interface", "Expected 'Interface' keyword");
    Token *name_tok = cur(p);
    expect(p, TOK_ID, "Expected Identifier", "Expected interface name");
    const char *name = tok_text(name_tok);
    const char *owner_type_qname = p->decl_prefix;
    const char *iface_qname = qualify_decl_name(p, name);
    expect(p, TOK_LBRACE, "Expected '{'", "Missing '{' in interface");

    MethodList methods = {0};
    while (!match(p, TOK_RBRACE))
    {
        if (cur(p)->type == TOK_EOF)
        {
            kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                           "Expected '}'", "Unexpected end of file while parsing interface body", "<end-of-file>");
            break;
        }
        AttrList attrs = parse_attributes(p);
        Access m_access = parse_access(p, ACC_PUBLIC);
        Safety safety = SAFETY_SAFE;
        if (match(p, TOK_SAFE)) safety = SAFETY_SAFE;
        else if (match(p, TOK_TRUSTED)) safety = SAFETY_TRUSTED;
        else if (match(p, TOK_UNSAFE)) safety = SAFETY_UNSAFE;

        if (cur(p)->type == TOK_PROPERTY)
        {
            parse_property(p, m_access, 0, 0, 0, 1, 0, safety, attrs, &methods);
            continue;
        }

        Method m = parse_method(p, m_access, 0, 0, 0, 1, 0, 0, safety);
        m.attrs = attrs;
        methodlist_push(&methods, m);
    }

    InterfaceDecl i;
    kn_memset(&i, 0, sizeof(i));
    i.name = name;
    i.qname = iface_qname;
    i.owner_type_qname = owner_type_qname;
    i.methods = methods;
    i.src = p->src;
    i.line = t->line;
    i.col = t->col;
    i.unit = p->unit_name;
    (void)access;
    return i;
}

static void parse_struct_specifiers(Parser *p, int *is_packed, int *align)
{
    if (!match(p, TOK_BY))
        return;

    for (;;)
    {
        Token *t = cur(p);
        expect(p, TOK_ID, "Expected Identifier", "Expected specifier");
        const char *name = tok_text(t);
        if (kn_strcmp(name, "Packed") == 0)
        {
            *is_packed = 1;
        }
        else if (kn_strcmp(name, "Align") == 0)
        {
            expect(p, TOK_LPAREN, "Expected '('", "Missing '(' after Align");
            Token *num = cur(p);
            expect(p, TOK_NUMBER, "Expected Number", "Align requires a number");
            int64_t v = parse_int_lit(num);
            if (v > 0 && v < 1ll << 30)
                *align = (int)v;
            expect(p, TOK_RPAREN, "Expected ')'", "Missing ')' after Align");
        }
        else
        {
            kn_diag_report(p->src, KN_STAGE_PARSER, t->line, t->col, (int)t->length,
                           "Unknown Specifier", "Unsupported struct specifier", name);
        }
        if (match(p, TOK_COMMA)) continue;
        break;
    }
}

static StructDecl parse_struct(Parser *p)
{
    Token *t = cur(p);
    expect(p, TOK_STRUCT, "Expected Struct", "Expected 'Struct' keyword");
    Token *name_tok = cur(p);
    expect(p, TOK_ID, "Expected Identifier", "Expected struct name");
    const char *name = tok_text(name_tok);
    const char *owner_type_qname = p->decl_prefix;
    const char *struct_qname = qualify_decl_name(p, name);

    int is_packed = 0;
    int align = 0;
    parse_struct_specifiers(p, &is_packed, &align);

    expect(p, TOK_LBRACE, "Expected '{'", "Missing '{' in struct");
    FieldList fields = {0};
    while (!match(p, TOK_RBRACE))
    {
        if (cur(p)->type == TOK_EOF)
        {
            kn_diag_report(p->src, KN_STAGE_PARSER, cur(p)->line, cur(p)->col, 1,
                           "Expected '}'", "Unexpected end of file while parsing struct body", "<end-of-file>");
            break;
        }
        AttrList attrs = parse_attributes(p);
        Field f = parse_field(p, ACC_PUBLIC, 0);
        f.attrs = attrs;
        fieldlist_push(&fields, f);
    }

    StructDecl s;
    kn_memset(&s, 0, sizeof(s));
    s.name = name;
    s.qname = struct_qname;
    s.owner_type_qname = owner_type_qname;
    s.fields = fields;
    s.is_packed = is_packed;
    s.align = align;
    s.src = p->src;
    s.line = t->line;
    s.col = t->col;
    s.unit = p->unit_name;
    return s;
}

static EnumDecl parse_enum(Parser *p)
{
    Token *t = cur(p);
    expect(p, TOK_ENUM, "Expected Enum", "Expected 'Enum' keyword");
    Token *name_tok = cur(p);
    expect(p, TOK_ID, "Expected Identifier", "Expected enum name");
    const char *name = tok_text(name_tok);
    const char *owner_type_qname = p->decl_prefix;
    const char *enum_qname = qualify_decl_name(p, name);

    Type underlying = type_make(TY_INT);
    if (match(p, TOK_BY))
    {
        underlying = parse_type(p);
    }

    expect(p, TOK_LBRACE, "Expected '{'", "Missing '{' in enum");
    EnumItemList items = {0};
    int64_t next_val = 0;
    while (!match(p, TOK_RBRACE))
    {
        Token *it = cur(p);
        expect(p, TOK_ID, "Expected Identifier", "Expected enum member");
        const char *iname = tok_text(it);
        int64_t val = next_val;
        if (match(p, TOK_ASSIGN))
        {
            Token *num = cur(p);
            expect(p, TOK_NUMBER, "Expected Number", "Enum value requires a number");
            val = parse_int_lit(num);
        }
        next_val = val + 1;
        EnumItem item;
        item.name = iname;
        item.value = val;
        item.src = p->src;
        item.line = it->line;
        item.col = it->col;
        enumitemlist_push(&items, item);
        if (match(p, TOK_COMMA)) continue;
        expect(p, TOK_RBRACE, "Expected '}'", "Missing '}' in enum");
        break;
    }

    EnumDecl e;
    kn_memset(&e, 0, sizeof(e));
    e.name = name;
    e.qname = enum_qname;
    e.owner_type_qname = owner_type_qname;
    e.underlying = underlying;
    e.items = items;
    e.src = p->src;
    e.line = t->line;
    e.col = t->col;
    e.unit = p->unit_name;
    return e;
}

static Func parse_func(Parser *p)
{
    Token *decl_tok = cur(p);
    Token *unsafe_tok = 0;
    int is_extern = 0;
    int extern_abi = KN_EXTERN_ABI_C;
    int is_delegate = 0;
    int delegate_abi = KN_DELEGATE_ABI_KINAL;
    int is_static = 0;
    int is_async = 0;
    Safety safety = SAFETY_SAFE;
    // Parse all modifiers in any order: Extern, Delegate, Static, Safe, Trusted, Unsafe
    for (;;)
    {
        int progressed = 0;
        if (!is_extern && !is_delegate && match(p, TOK_EXTERN)) { is_extern = 1; progressed = 1; }
        else if (!is_extern && !is_delegate && match(p, TOK_DELEGATE)) { is_delegate = 1; progressed = 1; }
        if (match(p, TOK_STATIC)) { is_static = 1; progressed = 1; }
        if (match(p, TOK_ASYNC)) { is_async = 1; progressed = 1; }
        if (match(p, TOK_SAFE)) { safety = SAFETY_SAFE; progressed = 1; }
        else if (match(p, TOK_TRUSTED)) { safety = SAFETY_TRUSTED; progressed = 1; }
        else if (cur(p)->type == TOK_UNSAFE)
        {
            unsafe_tok = cur(p);
            p->pos++;
            safety = SAFETY_UNSAFE;
            progressed = 1;
        }
        if (!progressed) break;
    }

    expect(p, TOK_FUNCTION, "Expected Function", "Expected 'Function' keyword");
    Token *ret_tok = cur(p);
    Type ret = parse_type(p);
    Token *name_tok = cur(p);
    expect(p, TOK_ID, "Expected Identifier", "Expected function name");
    const char *name = tok_text(name_tok);
    const char **type_params = 0;
    int type_param_count = 0;
    int type_param_cap = 0;
    if (cur(p)->type == TOK_LT)
        parse_type_param_list(p, &type_params, &type_param_count, &type_param_cap);
    expect(p, TOK_LPAREN, "Expected '('", "Missing '(' after function name");
    ParamList params = parse_param_list(p, 1, "Missing ')' after parameters");
    Stmt *body = 0;
    if (is_extern)
    {
        if (type_param_count > 0)
            kn_diag_report(p->src, KN_STAGE_PARSER, name_tok->line, name_tok->col, (int)name_tok->length,
                           "Invalid Generic Function", "Extern function cannot be generic", name);
        if (match(p, TOK_BY))
        {
            Token *abi_tok = cur(p);
            expect(p, TOK_ID, "Expected Identifier", "Extern binding requires an identifier (C/System)");
            const char *abi = tok_text(abi_tok);
            if (kn_strcmp(abi, "System") == 0)
                extern_abi = KN_EXTERN_ABI_SYSTEM;
            else if (kn_strcmp(abi, "C") == 0)
                extern_abi = KN_EXTERN_ABI_C;
            else
                kn_diag_report(p->src, KN_STAGE_PARSER, abi_tok->line, abi_tok->col, (int)abi_tok->length,
                               "Unknown Extern Binding", "Supported bindings are C and System", abi);
        }
        expect(p, TOK_SEMI, "Expected Semicolon", "Extern function requires ';'");
        if (safety == SAFETY_SAFE)
            safety = SAFETY_UNSAFE;
    }
    else if (is_delegate)
    {
        if (type_param_count > 0)
            kn_diag_report(p->src, KN_STAGE_PARSER, name_tok->line, name_tok->col, (int)name_tok->length,
                           "Invalid Generic Function", "Delegate function cannot be generic", name);
        if (match(p, TOK_BY))
        {
            Token *abi_tok = cur(p);
            expect(p, TOK_ID, "Expected Identifier", "Delegate binding requires an identifier (C/Kinal)");
            const char *abi = tok_text(abi_tok);
            if (kn_strcmp(abi, "C") == 0)
                delegate_abi = KN_DELEGATE_ABI_C;
            else if (kn_strcmp(abi, "Kinal") == 0)
                delegate_abi = KN_DELEGATE_ABI_KINAL;
            else
                kn_diag_report(p->src, KN_STAGE_PARSER, abi_tok->line, abi_tok->col, (int)abi_tok->length,
                               "Unknown Delegate Binding", "Supported bindings are C and Kinal", abi);
        }
        expect(p, TOK_SEMI, "Expected Semicolon", "Delegate function requires ';'");
    }
    else
    {
        body = parse_block(p);
    }
    Func f;
    kn_memset(&f, 0, sizeof(f));
    f.name = name;
    f.ret_type = ret;
    f.params = params;
    f.type_params = type_params;
    f.type_param_count = type_param_count;
    f.type_param_cap = type_param_cap;
    f.body = body;
    f.safety = safety;
    f.src = p->src;
    f.is_extern = is_extern;
    f.extern_abi = extern_abi;
    f.extern_symbol = 0;
    f.is_delegate = is_delegate;
    f.delegate_abi = delegate_abi;
    f.delegate_symbol_name = 0;
    f.is_static = is_static;
    f.is_async = is_async;
    f.async_result_type = ret;
    f.is_generic_instance = 0;
    f.generic_template_qname = 0;
    f.generic_args = 0;
    f.generic_arg_count = 0;
    f.unit = p->unit_name;
    f.qname = qualify_name(p->unit_name, name);
    if (unsafe_tok)
    {
        f.line = unsafe_tok->line;
        f.col = unsafe_tok->col;
    }
    else if (decl_tok)
    {
        f.line = decl_tok->line;
        f.col = decl_tok->col;
    }
    else
    {
        f.line = ret_tok->line;
        f.col = ret_tok->col;
    }
    return f;
}

static bool is_top_level_func_start(TokenType t)
{
    return t == TOK_EXTERN || t == TOK_DELEGATE || t == TOK_FUNCTION || t == TOK_STATIC ||
           t == TOK_SAFE || t == TOK_TRUSTED || t == TOK_UNSAFE || t == TOK_ASYNC;
}

static bool is_top_level_decl_start(TokenType t)
{
    return t == TOK_ENUM || t == TOK_STRUCT || t == TOK_INTERFACE || t == TOK_CLASS ||
           t == TOK_PUBLIC || t == TOK_PRIVATE || t == TOK_PROTECTED || t == TOK_INTERNAL ||
           t == TOK_STATIC || t == TOK_ABSTRACT || t == TOK_SEALED || t == TOK_CONST || t == TOK_VAR ||
           is_top_level_func_start(t);
}

static void sync_top_level(Parser *p)
{
    int start_pos = p->pos;
    int start_line = cur(p)->line;
    while (cur(p)->type != TOK_EOF)
    {
        TokenType t = cur(p)->type;
        if (t == TOK_SEMI)
        {
            p->pos++;
            return;
        }
        if (p->pos > start_pos && (is_top_level_decl_start(t) || t == TOK_GET || t == TOK_UNIT))
            return;
        if (p->pos > start_pos && cur(p)->line > start_line && cur(p)->col == 1)
            return;
        p->pos++;
    }
}

