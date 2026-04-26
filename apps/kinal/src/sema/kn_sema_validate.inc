// Internal sema fragment: type validation, lookup, and declaration checks

static bool eval_const_expr_scoped(Sema *s, Expr *e, int64_t *out)
{
    if (!e || !out) return false;
    switch (e->kind)
    {
    case EXPR_VAR:
    {
        VarSym *v = scope_find(s ? s->scope : 0, e->v.var.name);
        if (!v || !v->is_const || !v->origin)
            return false;
        return eval_const_expr_scoped(s, v->origin, out);
    }
    case EXPR_UNARY:
    {
        int64_t v = 0;
        if (!eval_const_expr_scoped(s, e->v.unary.expr, &v)) return false;
        if (e->v.unary.op == TOK_MINUS) { *out = -v; return true; }
        if (e->v.unary.op == TOK_NOT) { *out = !v; return true; }
        return false;
    }
    case EXPR_BINARY:
    {
        int64_t l = 0, r = 0;
        if (!eval_const_expr_scoped(s, e->v.binary.left, &l)) return false;
        if (!eval_const_expr_scoped(s, e->v.binary.right, &r)) return false;
        switch ((TokenType)e->v.binary.op)
        {
        case TOK_PLUS: *out = l + r; return true;
        case TOK_MINUS: *out = l - r; return true;
        case TOK_STAR: *out = l * r; return true;
        case TOK_SLASH: if (r == 0) return false; *out = l / r; return true;
        case TOK_PERCENT: if (r == 0) return false; *out = l % r; return true;
        case TOK_EQ: *out = (l == r); return true;
        case TOK_NE: *out = (l != r); return true;
        case TOK_LT: *out = (l < r); return true;
        case TOK_LE: *out = (l <= r); return true;
        case TOK_GT: *out = (l > r); return true;
        case TOK_GE: *out = (l >= r); return true;
        case TOK_ANDAND: *out = (l && r); return true;
        case TOK_OROR: *out = (l || r); return true;
        case TOK_AMP: *out = (l & r); return true;
        case TOK_BITOR: *out = (l | r); return true;
        case TOK_XOR: *out = (l ^ r); return true;
        case TOK_SHL: *out = (l << r); return true;
        case TOK_SHR: *out = (l >> r); return true;
        default: return false;
        }
    }
    default:
        return eval_const_expr(e, out);
    }
}

static void resolve_array_len_expr(Sema *s, Type *t, int line, int col)
{
    if (!s || !t || t->kind != TY_ARRAY || t->array_len >= 0 || !t->array_len_expr)
        return;

    Type it = sema_expr_with_expected(s, t->array_len_expr, type_make(TY_INT), "Array length requires integer type");
    if (!is_integer_kind(it.kind))
    {
        sema_error(s, line, col, 1, "Array Length", "Array length must be an integer constant expression");
        t->array_len_expr = 0;
        return;
    }

    int64_t len = 0;
    if (!eval_const_expr_scoped(s, t->array_len_expr, &len))
    {
        sema_error(s, line, col, 1, "Array Length", "Array length must be a constant integer expression");
        t->array_len_expr = 0;
        return;
    }

    if (len < 0)
        sema_error(s, line, col, 1, "Array Length", "Array length cannot be negative");
    else
        t->array_len = len;
    t->array_len_expr = 0;
}

static void validate_type_ref(Sema *s, Type *t, int line, int col, int allow_void)
{
    int hidden_class_alias = (t && t->kind == TY_CLASS && t->name && qname_uses_hidden_module_alias(s, t->name));
    int hidden_struct_alias = (t && t->kind == TY_STRUCT && t->name && qname_uses_hidden_module_alias(s, t->name));
    int hidden_enum_alias = (t && t->kind == TY_ENUM && t->name && qname_uses_hidden_module_alias(s, t->name));
    int hidden_ptr_class_alias = (t && (t->kind == TY_PTR || t->kind == TY_ARRAY) && t->elem == TY_CLASS &&
                                  t->name && qname_uses_hidden_module_alias(s, t->name));
    int hidden_ptr_struct_alias = (t && (t->kind == TY_PTR || t->kind == TY_ARRAY) && t->elem == TY_STRUCT &&
                                   t->name && qname_uses_hidden_module_alias(s, t->name));
    int hidden_ptr_enum_alias = (t && (t->kind == TY_PTR || t->kind == TY_ARRAY) && t->elem == TY_ENUM &&
                                 t->name && qname_uses_hidden_module_alias(s, t->name));
    resolve_array_len_expr(s, t, line, col);
    resolve_named_type(s, t);
    if (t->kind == TY_UNKNOWN)
        sema_error(s, line, col, 1, "Invalid Type", "Type not specified");
    if (!allow_void && t->kind == TY_VOID)
        sema_error(s, line, col, 1, "Invalid Type", "Void not allowed here");
    if (t->kind == TY_ARRAY && t->elem == TY_UNKNOWN)
        sema_error(s, line, col, 1, "Invalid Type", "Array element type unknown");
    if (t->kind == TY_ARRAY && t->elem == TY_ANY && t->array_len < 0)
        sema_error(s, line, col, 1, "Invalid Type", "any[] requires explicit length");
    if (t->kind == TY_CLASS)
    {
        if (hidden_class_alias)
        {
            sema_error(s, line, col, 1, "Unknown Type", "Class or interface not found");
            t->kind = TY_UNKNOWN;
            t->name = 0;
            return;
        }
        ClassDecl *c = find_class(s, t->name);
        InterfaceDecl *it = find_interface(s, t->name);
        if (!c && !it)
            sema_error(s, line, col, 1, "Unknown Type", "Class or interface not found");
        else if (c && c->is_static_class)
            sema_error(s, line, col, 1, "Invalid Type", "Static class cannot be used as a value type");
        else if (c)
            t->name = class_qname(c);
        else
            t->name = interface_qname(it);
    }
    if (t->kind == TY_STRUCT)
    {
        if (hidden_struct_alias)
        {
            sema_error(s, line, col, 1, "Unknown Type", "Struct not found");
            t->kind = TY_UNKNOWN;
            t->name = 0;
            return;
        }
        StructDecl *st = find_struct(s, t->name);
        if (!st)
            sema_error(s, line, col, 1, "Unknown Type", "Struct not found");
        else
            t->name = struct_qname(st);
    }
    if (t->kind == TY_ENUM)
    {
        if (hidden_enum_alias)
        {
            sema_error(s, line, col, 1, "Unknown Type", "Enum not found");
            t->kind = TY_UNKNOWN;
            t->name = 0;
            return;
        }
        EnumDecl *en = find_enum(s, t->name);
        if (!en)
            sema_error(s, line, col, 1, "Unknown Type", "Enum not found");
        else
            t->name = enum_qname(en);
    }
    if ((t->kind == TY_PTR || t->kind == TY_ARRAY) && t->name)
    {
        if (t->elem == TY_CLASS)
        {
            if (hidden_ptr_class_alias)
            {
                sema_error(s, line, col, 1, "Unknown Type", "Class or interface not found");
                t->elem = TY_UNKNOWN;
                t->name = 0;
                return;
            }
            ClassDecl *c = find_class(s, t->name);
            InterfaceDecl *it = find_interface(s, t->name);
            if (!c && !it)
                sema_error(s, line, col, 1, "Unknown Type", "Class or interface not found");
            else if (c && c->is_static_class)
                sema_error(s, line, col, 1, "Invalid Type", "Static class cannot be used as a value type");
            else if (c)
                t->name = class_qname(c);
            else
                t->name = interface_qname(it);
        }
        if (t->elem == TY_STRUCT)
        {
            if (hidden_ptr_struct_alias)
            {
                sema_error(s, line, col, 1, "Unknown Type", "Struct not found");
                t->elem = TY_UNKNOWN;
                t->name = 0;
                return;
            }
            StructDecl *st = find_struct(s, t->name);
            if (!st)
                sema_error(s, line, col, 1, "Unknown Type", "Struct not found");
            else
                t->name = struct_qname(st);
        }
        if (t->elem == TY_ENUM)
        {
            if (hidden_ptr_enum_alias)
            {
                sema_error(s, line, col, 1, "Unknown Type", "Enum not found");
                t->elem = TY_UNKNOWN;
                t->name = 0;
                return;
            }
            EnumDecl *en = find_enum(s, t->name);
            if (!en)
                sema_error(s, line, col, 1, "Unknown Type", "Enum not found");
            else
                t->name = enum_qname(en);
        }
    }
    if (t->kind == TY_ARRAY && (t->elem == TY_CLASS || t->elem == TY_STRUCT || t->elem == TY_ENUM) && !t->name)
        sema_error(s, line, col, 1, "Invalid Type", "Array element name missing");
    if (!type_supported(*t))
        sema_error(s, line, col, 1, "Unsupported Type", "Type not supported yet");
}

static void validate_param_defaults(Sema *s, ParamList *params, int line, int col)
{
    if (!s || !params) return;
    int saw_default = 0;
    for (int i = 0; i < params->count; i++)
    {
        Param *pr = &params->items[i];
        int pr_line = pr->line > 0 ? pr->line : line;
        int pr_col = pr->col > 0 ? pr->col : col;
        validate_type_ref(s, &pr->type, pr_line, pr_col, 0);
        if (pr->default_value)
        {
            saw_default = 1;
            bind_default_literal_type(s, pr->default_value, pr->type, "Default literal requires parameter type");
            {
                Type dt = sema_expr_with_expected(s, pr->default_value, pr->type, "Default literal requires parameter type");
                check_int_to_ptr_safety(s, pr->type, dt, pr->default_value);
                if (!type_assignable(s, pr->type, dt))
                    sema_error(s, pr->default_value->line, pr->default_value->col, 1,
                               "Type Mismatch", type_mismatch_detail(pr->type, dt));
                check_byte_literal_range(s, pr->type, pr->default_value);
            }
        }
        else if (saw_default)
        {
            sema_error(s, pr_line, pr_col, 1, "Invalid Default Parameter",
                       "Parameters with default values must appear after required parameters");
        }
    }
}

typedef struct
{
    const char *name;
    uint32_t target_mask;
    int keep_kind;
    int repeatable;
    int builtin_kind;
} BuiltinMetaSpec;

enum
{
    KN_BUILTIN_META_LINK_NAME = 1,
    KN_BUILTIN_META_LINK_LIB = 2,
    KN_BUILTIN_META_LINK_FILE = 3,
    KN_BUILTIN_META_SYMBOL_NAME = 4,
    KN_BUILTIN_META_LINK_LIB_TARGET = 5,
    KN_BUILTIN_META_LINK_FILE_TARGET = 6
};

typedef struct
{
    const BuiltinMetaSpec *builtin;
    MetaDecl *meta;
    const char *qname;
    const char *short_name;
    uint32_t target_mask;
    int keep_kind;
    int repeatable;
} ResolvedMetaSpec;

static bool is_float_kind(TypeKind k);

static const BuiltinMetaSpec g_builtin_metas[] = {
    { "LinkName",   KN_META_TARGET_EXTERN_FUNCTION,   KN_META_KEEP_COMPILE, 0, KN_BUILTIN_META_LINK_NAME },
    { "LinkLib",    KN_META_TARGET_EXTERN_FUNCTION,   KN_META_KEEP_COMPILE, 1, KN_BUILTIN_META_LINK_LIB },
    { "LinkFile",   KN_META_TARGET_EXTERN_FUNCTION,   KN_META_KEEP_COMPILE, 1, KN_BUILTIN_META_LINK_FILE },
    { "LinkLibTarget",  KN_META_TARGET_EXTERN_FUNCTION, KN_META_KEEP_COMPILE, 1, KN_BUILTIN_META_LINK_LIB_TARGET },
    { "LinkFileTarget", KN_META_TARGET_EXTERN_FUNCTION, KN_META_KEEP_COMPILE, 1, KN_BUILTIN_META_LINK_FILE_TARGET },
    { "SymbolName", KN_META_TARGET_DELEGATE_FUNCTION, KN_META_KEEP_COMPILE, 0, KN_BUILTIN_META_SYMBOL_NAME }
};

static const char *meta_short_name(const char *name)
{
    if (!name) return 0;
    const char *last = name;
    for (const char *p = name; *p; p++)
        if (*p == '.')
            last = p + 1;
    return last;
}

static void attr_error_at(Sema *s, const KnSource *src, int owner_line, int owner_col, const Attribute *a,
                          const char *title, const char *detail)
{
    int line = owner_line;
    int col = owner_col;
    if (a && a->args.count > 0 && a->args.items[0])
    {
        line = a->args.items[0]->line;
        col = a->args.items[0]->col;
    }
    const KnSource *prev_src = s ? s->src : 0;
    if (s)
        s->src = src ? src : prev_src;
    sema_error(s, line, col, 1, title, detail);
    if (s)
        s->src = prev_src;
}

static bool meta_literal_expr(Expr *e)
{
    if (!e) return false;
    switch (e->kind)
    {
    case EXPR_INT:
    case EXPR_FLOAT:
    case EXPR_BOOL:
    case EXPR_CHAR:
    case EXPR_STRING:
        return true;
    case EXPR_MEMBER:
        return e->v.member.is_enum_const != 0;
    default:
        return false;
    }
}

static bool meta_param_type_supported(Type t)
{
    if (t.kind == TY_BOOL || t.kind == TY_CHAR || t.kind == TY_STRING)
        return true;
    if (is_integer_kind(t.kind) || is_float_kind(t.kind))
        return true;
    if (t.kind == TY_ENUM)
        return true;
    return false;
}

static const BuiltinMetaSpec *find_builtin_meta(const char *name)
{
    const char *short_name = meta_short_name(name);
    if (!short_name || short_name != name)
        return 0;
    for (int i = 0; i < (int)(sizeof(g_builtin_metas) / sizeof(g_builtin_metas[0])); i++)
        if (kn_strcmp(g_builtin_metas[i].name, short_name) == 0)
            return &g_builtin_metas[i];
    return 0;
}

static MetaDecl *find_user_meta(Sema *s, const char *name)
{
    if (!s || !s->metas || !name) return 0;

    const char *short_name = meta_short_name(name);
    int has_qualifier = short_name != name;
    if (has_qualifier)
    {
        for (int i = 0; i < s->metas->count; i++)
        {
            MetaDecl *m = &s->metas->items[i];
            if (m->qname && kn_strcmp(m->qname, name) == 0)
                return m;
        }
        return 0;
    }

    MetaDecl *fallback = 0;
    for (int i = 0; i < s->metas->count; i++)
    {
        MetaDecl *m = &s->metas->items[i];
        if (!m->name || kn_strcmp(m->name, name) != 0)
            continue;
        if (m->unit && s->current_unit && kn_strcmp(m->unit, s->current_unit) == 0)
            return m;
        if (!fallback)
            fallback = m;
    }
    return fallback;
}

static int meta_target_matches(uint32_t spec_mask, uint32_t owner_mask)
{
    if (owner_mask == KN_META_TARGET_EXTERN_FUNCTION)
        return (spec_mask & (KN_META_TARGET_EXTERN_FUNCTION | KN_META_TARGET_FUNCTION)) != 0;
    if (owner_mask == KN_META_TARGET_DELEGATE_FUNCTION)
        return (spec_mask & (KN_META_TARGET_DELEGATE_FUNCTION | KN_META_TARGET_FUNCTION)) != 0;
    return (spec_mask & owner_mask) != 0;
}

static int count_required_meta_params(const ParamList *params)
{
    int required = 0;
    if (!params) return 0;
    for (int i = 0; i < params->count; i++)
        if (!params->items[i].default_value)
            required++;
    return required;
}

static int resolve_meta_spec(Sema *s, const char *name, ResolvedMetaSpec *out)
{
    if (!out) return 0;
    kn_memset(out, 0, sizeof(*out));
    if (!name) return 0;

    {
        const BuiltinMetaSpec *builtin = find_builtin_meta(name);
        if (builtin)
        {
            out->builtin = builtin;
            out->qname = builtin->name;
            out->short_name = builtin->name;
            out->target_mask = builtin->target_mask;
            out->keep_kind = builtin->keep_kind;
            out->repeatable = builtin->repeatable;
            return 1;
        }
    }

    {
        MetaDecl *meta = find_user_meta(s, name);
        if (!meta)
            return 0;
        out->meta = meta;
        out->qname = meta->qname ? meta->qname : meta->name;
        out->short_name = meta->name;
        out->target_mask = meta->target_mask;
        out->keep_kind = meta->keep_kind;
        out->repeatable = meta->repeatable;
        return 1;
    }
}

static void validate_meta_constant_expr(Sema *s, const KnSource *src, int owner_line, int owner_col,
                                        const Attribute *a, Expr *expr, Type expected, const char *meta_name)
{
    if (!expr)
        return;
    bind_default_literal_type(s, expr, expected, "Meta argument requires declared parameter type");
    {
        Type got = sema_expr_with_expected(s, expr, expected, "Meta argument requires declared parameter type");
        check_int_to_ptr_safety(s, expected, got, expr);
        if (!type_assignable(s, expected, got))
        {
            attr_error_at(s, src, owner_line, owner_col, a, "Invalid Attribute", type_mismatch_detail(expected, got));
            return;
        }
        check_byte_literal_range(s, expected, expr);
    }
    if (!meta_literal_expr(expr))
    {
        char detail[160];
        size_t off = 0;
        detail[0] = 0;
        kn_append(detail, sizeof(detail), &off, meta_name ? meta_name : "Meta");
        kn_append(detail, sizeof(detail), &off, " requires compile-time literal arguments");
        attr_error_at(s, src, owner_line, owner_col, a, "Invalid Attribute", detail);
    }
}

static void validate_meta_decl(Sema *s, MetaDecl *m)
{
    if (!s || !m) return;
    const KnSource *prev_src = s->src;
    s->src = m->src ? m->src : prev_src;

    if (find_builtin_meta(m->name))
        sema_error(s, m->line, m->col, 1, "Invalid Meta", "Built-in Meta names cannot be redefined");
    if (m->target_mask == KN_META_TARGET_NONE)
        sema_error(s, m->line, m->col, 1, "Invalid Meta", "Meta declaration must define at least one On target");
    if (m->keep_kind == KN_META_KEEP_NONE)
        sema_error(s, m->line, m->col, 1, "Invalid Meta", "Meta declaration must define a Keep mode");

    for (int i = 0; i < m->params.count; i++)
    {
        Param *pr = &m->params.items[i];
        validate_type_ref(s, &pr->type, m->line, m->col, 0);
        if (!meta_param_type_supported(pr->type))
            sema_error(s, m->line, m->col, 1, "Invalid Meta", "Meta parameters only support literal-friendly types");
        for (int j = 0; j < i; j++)
        {
            if (kn_strcmp(m->params.items[j].name, pr->name) == 0)
                sema_error(s, m->line, m->col, 1, "Duplicate Parameter", "Meta parameter already defined");
        }
        if (pr->default_value)
            validate_meta_constant_expr(s, m->src, m->line, m->col, 0, pr->default_value, pr->type, m->name);
    }

    s->src = prev_src;
}

static void validate_attr_list(Sema *s, AttrList *attrs, uint32_t owner_mask,
                               const KnSource *owner_src, int owner_line, int owner_col, Func *func_owner)
{
    if (!s || !attrs) return;

    for (int i = 0; i < attrs->count; i++)
    {
        Attribute *a = &attrs->items[i];
        ResolvedMetaSpec spec;
        if (!resolve_meta_spec(s, a->name, &spec))
        {
            attr_error_at(s, owner_src, owner_line, owner_col, a, "Unknown Attribute", "Meta not declared");
            continue;
        }

        a->resolved_name = spec.qname;

        if (!meta_target_matches(spec.target_mask, owner_mask))
        {
            char detail[200];
            size_t off = 0;
            detail[0] = 0;
            kn_append(detail, sizeof(detail), &off, spec.short_name ? spec.short_name : "Meta");
            kn_append(detail, sizeof(detail), &off, " is not valid on this declaration");
            attr_error_at(s, owner_src, owner_line, owner_col, a, "Invalid Attribute", detail);
            continue;
        }

        if (func_owner && spec.builtin && spec.builtin->builtin_kind == KN_BUILTIN_META_SYMBOL_NAME &&
            func_owner->delegate_abi != KN_DELEGATE_ABI_C)
        {
            attr_error_at(s, owner_src, owner_line, owner_col, a, "Invalid Attribute",
                          "SymbolName requires Delegate Function ... By C");
            continue;
        }

        if (!spec.repeatable)
        {
            for (int j = 0; j < i; j++)
            {
                Attribute *prev = &attrs->items[j];
                if (prev->resolved_name && a->resolved_name &&
                    kn_strcmp(prev->resolved_name, a->resolved_name) == 0)
                {
                    attr_error_at(s, owner_src, owner_line, owner_col, a, "Invalid Attribute",
                                  "Meta is not Repeatable on this declaration");
                    break;
                }
            }
        }

        if (spec.builtin)
        {
            if (spec.builtin->builtin_kind == KN_BUILTIN_META_LINK_LIB ||
                spec.builtin->builtin_kind == KN_BUILTIN_META_LINK_FILE)
            {
                if (a->args.count <= 0)
                {
                    attr_error_at(s, owner_src, owner_line, owner_col, a, "Invalid Attribute",
                                  "LinkLib and LinkFile require at least one string literal argument");
                    continue;
                }
                for (int ai = 0; ai < a->args.count; ai++)
                    validate_meta_constant_expr(s, owner_src, owner_line, owner_col, a, a->args.items[ai], type_make(TY_STRING), spec.short_name);
                continue;
            }

            if (spec.builtin->builtin_kind == KN_BUILTIN_META_LINK_LIB_TARGET ||
                spec.builtin->builtin_kind == KN_BUILTIN_META_LINK_FILE_TARGET)
            {
                if (a->args.count < 2)
                {
                    attr_error_at(s, owner_src, owner_line, owner_col, a, "Invalid Attribute",
                                  "LinkLibTarget and LinkFileTarget require a target string followed by one or more string literal arguments");
                    continue;
                }
                for (int ai = 0; ai < a->args.count; ai++)
                    validate_meta_constant_expr(s, owner_src, owner_line, owner_col, a, a->args.items[ai], type_make(TY_STRING), spec.short_name);
                continue;
            }

            if (a->args.count != 1)
            {
                attr_error_at(s, owner_src, owner_line, owner_col, a, "Invalid Attribute",
                              "Built-in Meta requires exactly one string literal argument");
                continue;
            }
            validate_meta_constant_expr(s, owner_src, owner_line, owner_col, a, a->args.items[0], type_make(TY_STRING), spec.short_name);
            if (func_owner && a->args.items[0] && a->args.items[0]->kind == EXPR_STRING)
            {
                const char *value = a->args.items[0]->v.str_val.ptr;
                if (spec.builtin->builtin_kind == KN_BUILTIN_META_LINK_NAME)
                    func_owner->extern_symbol = value;
                else if (spec.builtin->builtin_kind == KN_BUILTIN_META_SYMBOL_NAME)
                    func_owner->delegate_symbol_name = value;
            }
            continue;
        }

        if (spec.meta)
        {
            int required = count_required_meta_params(&spec.meta->params);
            if (a->args.count < required || a->args.count > spec.meta->params.count)
            {
                attr_error_at(s, owner_src, owner_line, owner_col, a, "Invalid Attribute",
                              "Meta argument count does not match declaration");
                continue;
            }
            for (int ai = 0; ai < a->args.count; ai++)
            {
                Param *pr = &spec.meta->params.items[ai];
                validate_meta_constant_expr(s, owner_src, owner_line, owner_col, a, a->args.items[ai], pr->type, spec.short_name);
            }
        }
    }
}

static void validate_func_attrs(Sema *s, Func *f)
{
    if (!s || !f)
        return;
    f->extern_symbol = 0;
    f->delegate_symbol_name = 0;
    validate_attr_list(s, &f->attrs,
                       f->is_extern ? KN_META_TARGET_EXTERN_FUNCTION :
                       (f->is_delegate ? KN_META_TARGET_DELEGATE_FUNCTION : KN_META_TARGET_FUNCTION),
                       f->src, f->line, f->col, f);
}

static void validate_interface_decl(Sema *s, InterfaceDecl *i)
{
    if (!i) return;
    s->src = i->src;
    ClassDecl *prev_class = s->current_class;
    const char *prev_unit = s->current_unit;
    int prev_static = s->current_is_static;
    s->current_class = 0;
    s->current_unit = i->unit;
    s->current_is_static = 1;
    validate_attr_list(s, &i->attrs, KN_META_TARGET_INTERFACE, i->src, i->line, i->col, 0);
    for (int a = 0; a < i->methods.count; a++)
    {
        Method *m = &i->methods.items[a];
        validate_attr_list(s, &m->attrs, KN_META_TARGET_METHOD,
                           m->src ? m->src : i->src, m->line, m->col, 0);
        validate_type_ref(s, &m->ret_type, m->line, m->col, 1);
        validate_param_defaults(s, &m->params, m->line, m->col);
        if (!m->is_abstract)
            sema_error(s, m->line, m->col, 1, "Invalid Interface", "Interface methods must be abstract");
        for (int b = 0; b < a; b++)
        {
            Method *prev = &i->methods.items[b];
            if (kn_strcmp(prev->name, m->name) == 0)
                sema_error(s, m->line, m->col, 1, "Duplicate Method", "Interface method already defined");
        }
    }
    s->current_class = prev_class;
    s->current_unit = prev_unit;
    s->current_is_static = prev_static;
}

static void validate_enum_decl(Sema *s, EnumDecl *e)
{
    if (!e) return;
    s->src = e->src;
    validate_attr_list(s, &e->attrs, KN_META_TARGET_ENUM, e->src, e->line, e->col, 0);
    validate_type_ref(s, &e->underlying, e->line, e->col, 0);
    if (!is_integer_kind(e->underlying.kind))
        sema_error(s, e->line, e->col, 1, "Invalid Enum", "Enum underlying type must be integer");
    for (int i = 0; i < e->items.count; i++)
    {
        EnumItem *it = &e->items.items[i];
        for (int j = 0; j < i; j++)
            if (kn_strcmp(e->items.items[j].name, it->name) == 0)
                sema_error(s, it->line, it->col, 1, "Duplicate Enum", "Enum member already defined");
    }
}

static void validate_struct_decl(Sema *s, StructDecl *st)
{
    if (!st) return;
    s->src = st->src;
    validate_attr_list(s, &st->attrs, KN_META_TARGET_STRUCT, st->src, st->line, st->col, 0);
    if (st->align > 0 && (st->align & (st->align - 1)) != 0)
        sema_error(s, st->line, st->col, 1, "Invalid Align", "Align must be power of two");
    for (int i = 0; i < st->fields.count; i++)
    {
        Field *f = &st->fields.items[i];
        validate_attr_list(s, &f->attrs, KN_META_TARGET_FIELD,
                           f->src ? f->src : st->src, f->line, f->col, 0);
        validate_type_ref(s, &f->type, f->line, f->col, 0);
        for (int j = 0; j < i; j++)
            if (kn_strcmp(st->fields.items[j].name, f->name) == 0)
                sema_error(s, f->line, f->col, 1, "Duplicate Field", "Field already defined");
    }
}

static void normalize_class_bases(Sema *s, ClassDecl *c)
{
    if (!c || !c->base) return;
    ClassDecl *base = kn_strcmp(c->base, "IO.Type.Object.Class") == 0
        ? find_class(s, c->base)
        : find_visible_class(s, c->base);
    if (base)
    {
        c->base = class_qname(base);
        return;
    }
    InterfaceDecl *iface = find_visible_interface(s, c->base);
    if (iface)
    {
        class_add_interface(c, interface_qname(iface));
        c->base = 0;
        return;
    }
    sema_error(s, c->line, c->col, 1, "Unknown Base", "Base class or interface not found");
}

static void apply_default_object_base(Sema *s)
{
    if (!s || !s->classes) return;
    if (!find_class(s, "IO.Type.Object.Class"))
        return;
    for (int i = 0; i < s->classes->count; i++)
    {
        ClassDecl *c = &s->classes->items[i];
        if (kn_strcmp(c->name, "IO.Type.Object.Class") == 0)
            continue;
        if (c->is_static_class)
            continue;
        if (!c->base)
            c->base = "IO.Type.Object.Class";
    }
}

static bool class_has_impl(Sema *s, ClassDecl *c, const Method *sig)
{
    ClassDecl *owner = 0;
    int idx = -1;
    Method *m = find_method_sig_hierarchy(s, c, sig, &owner, &idx);
    return m && !m->is_abstract;
}

static void validate_class_decl(Sema *s, ClassDecl *c)
{
    if (!c) return;
    s->src = c->src;
    ClassDecl *prev_class = s->current_class;
    const char *prev_unit = s->current_unit;
    int prev_static = s->current_is_static;
    s->current_class = c;
    s->current_unit = c->unit;
    s->current_is_static = 1;
    validate_attr_list(s, &c->attrs, KN_META_TARGET_CLASS, c->src, c->line, c->col, 0);
    normalize_class_bases(s, c);

    if (c->is_static_class)
    {
        if (c->base)
            sema_error(s, c->line, c->col, 1, "Invalid Static Class", "Static class cannot inherit from a base class");
        if (c->interface_count > 0)
            sema_error(s, c->line, c->col, 1, "Invalid Static Class", "Static class cannot implement interfaces");
        if (c->is_abstract || c->is_sealed)
            sema_error(s, c->line, c->col, 1, "Invalid Static Class", "Static class cannot be abstract or sealed");
    }

    if (c->base)
    {
        ClassDecl *b = find_class(s, c->base);
        if (!b)
            sema_error(s, c->line, c->col, 1, "Unknown Base", "Base class not found");
        if (b && b->is_sealed)
            sema_error(s, c->line, c->col, 1, "Invalid Inheritance", "Cannot inherit from sealed class");
        if (b == c)
            sema_error(s, c->line, c->col, 1, "Invalid Inheritance", "Class cannot inherit from itself");
    }

    // interfaces
    for (int i = 0; i < c->interface_count; i++)
    {
        const char *iface = c->interfaces[i];
        InterfaceDecl *iface_decl = find_visible_interface(s, iface);
        if (!iface_decl)
            sema_error(s, c->line, c->col, 1, "Unknown Interface", "Interface not found");
        else
            c->interfaces[i] = interface_qname(iface_decl);
        for (int j = 0; j < i; j++)
            if (kn_strcmp(c->interfaces[j], c->interfaces[i]) == 0)
                sema_error(s, c->line, c->col, 1, "Duplicate Interface", "Interface already listed");
    }

    // fields
    for (int i = 0; i < c->fields.count; i++)
    {
        Field *f = &c->fields.items[i];
        validate_attr_list(s, &f->attrs, KN_META_TARGET_FIELD,
                           f->src ? f->src : c->src, f->line, f->col, 0);
        validate_type_ref(s, &f->type, f->line, f->col, 0);
        if (c->is_static_class && !f->is_static)
            sema_error(s, f->line, f->col, 1, "Invalid Static Class", "Static class fields must be static");
        if (f->init)
        {
            int prev_field_static = s->current_is_static;
            s->current_is_static = f->is_static ? 1 : 0;
            Type init_t = sema_expr(s, f->init);
            s->current_is_static = prev_field_static;
            if (!f->is_static)
                sema_error(s, f->line, f->col, 1, "Field Initializer", "Only static field initializers are supported");
            else if (!type_assignable(s, f->type, init_t))
                sema_error(s, f->init->line, f->init->col, 1, "Type Mismatch", type_mismatch_detail(f->type, init_t));
        }
        for (int j = 0; j < i; j++)
            if (kn_strcmp(c->fields.items[j].name, f->name) == 0)
                sema_error(s, f->line, f->col, 1, "Duplicate Field", "Field already defined");
    }

    // methods
    for (int i = 0; i < c->methods.count; i++)
    {
        Method *m = &c->methods.items[i];
        validate_attr_list(s, &m->attrs, KN_META_TARGET_METHOD,
                           m->src ? m->src : c->src, m->line, m->col, 0);
        validate_type_ref(s, &m->ret_type, m->line, m->col, 1);
        validate_param_defaults(s, &m->params, m->line, m->col);

        if (m->is_constructor)
        {
            if (c->is_static_class)
                sema_error(s, m->line, m->col, 1, "Invalid Static Class", "Static class cannot declare constructors");
            if (m->is_static || m->is_virtual || m->is_override || m->is_abstract || m->is_sealed)
                sema_error(s, m->line, m->col, 1, "Invalid Constructor", "Constructor cannot be static/virtual/override/abstract/sealed");
            // duplicate ctor signature
            for (int j = 0; j < i; j++)
            {
                Method *prev = &c->methods.items[j];
                if (prev->is_constructor && params_equal(&prev->params, &m->params))
                    sema_error(s, m->line, m->col, 1, "Duplicate Constructor", "Constructor already defined");
            }
            continue;
        }

        if (c->is_static_class)
        {
            if (!m->is_static)
                sema_error(s, m->line, m->col, 1, "Invalid Static Class", "Static class methods must be static");
            if (m->is_virtual || m->is_override || m->is_abstract || m->is_sealed)
                sema_error(s, m->line, m->col, 1, "Invalid Static Class", "Static class methods cannot be virtual/override/abstract/sealed");
        }

        // duplicate method name in same class (no overloads for now)
        for (int j = 0; j < i; j++)
        {
            Method *prev = &c->methods.items[j];
            if (!prev->is_constructor && prev->is_static == m->is_static && kn_strcmp(prev->name, m->name) == 0)
                sema_error(s, m->line, m->col, 1, "Duplicate Method", "Method already defined");
        }

        if (m->is_static && (m->is_virtual || m->is_override || m->is_abstract))
            sema_error(s, m->line, m->col, 1, "Invalid Modifier", "Static methods cannot be virtual/override/abstract");
        if (m->is_sealed && !m->is_override)
            sema_error(s, m->line, m->col, 1, "Invalid Modifier", "Sealed is only valid on override methods");
        if (m->is_abstract && !c->is_abstract)
            sema_error(s, m->line, m->col, 1, "Invalid Abstract", "Abstract method requires abstract class");

        ClassDecl *base_owner = 0;
        int base_idx = -1;
        Method *base_virtual = find_virtual_in_base(s, c, m, &base_owner, &base_idx);
        if (m->is_override)
        {
            if (!base_virtual)
                sema_error(s, m->line, m->col, 1, "Invalid Override", "No virtual base method to override");
            if (base_virtual && base_virtual->is_sealed)
                sema_error(s, m->line, m->col, 1, "Invalid Override", "Base method is sealed");
        }
        else
        {
            if (base_virtual)
                sema_error(s, m->line, m->col, 1, "Missing Override", "Method hides a virtual base method");
        }
    }

    // abstract requirements for concrete classes
    if (!c->is_static_class && !c->is_abstract)
    {
        // base abstract methods
        ClassDecl *base = c->base ? find_class(s, c->base) : 0;
        while (base)
        {
            for (int i = 0; i < base->methods.count; i++)
            {
                Method *bm = &base->methods.items[i];
                if (bm->is_constructor) continue;
                if (bm->is_abstract && !class_has_impl(s, c, bm))
                    sema_error(s, c->line, c->col, 1, "Abstract Method", "Class must implement abstract base method");
            }
            if (!base->base) break;
            base = find_class(s, base->base);
        }

        // interface methods
        for (int i = 0; i < c->interface_count; i++)
        {
            InterfaceDecl *iface = find_interface(s, c->interfaces[i]);
            if (!iface) continue;
            for (int m = 0; m < iface->methods.count; m++)
            {
                Method *im = &iface->methods.items[m];
                if (!class_has_impl(s, c, im))
                    sema_error(s, c->line, c->col, 1, "Interface Method", "Class must implement interface method");
                else
                {
                    ClassDecl *owner = 0;
                    int idx = -1;
                    Method *impl = find_method_sig_hierarchy(s, c, im, &owner, &idx);
                    if (impl && impl->access != ACC_PUBLIC)
                        sema_error(s, impl->line, impl->col, 1, "Interface Method", "Interface implementation must be public");
                }
            }
        }
    }
    s->current_class = prev_class;
    s->current_unit = prev_unit;
    s->current_is_static = prev_static;
}

static void append_i64(char *buf, size_t cap, size_t *off, int64_t v)
{
    char tmp[32];
    int n = 0;
    if (v == 0)
        tmp[n++] = '0';
    else
    {
        int64_t x = v;
        if (x < 0) x = -x;
        while (x && n < (int)sizeof(tmp))
        {
            tmp[n++] = (char)('0' + (x % 10));
            x /= 10;
        }
        if (v < 0 && n < (int)sizeof(tmp))
            tmp[n++] = '-';
    }
    for (int i = n - 1; i >= 0; i--)
    {
        if (*off + 1 >= cap) break;
        buf[(*off)++] = tmp[i];
    }
    buf[*off] = 0;
}

static const char *type_name(Type t)
{
    char buf[96];
    size_t off = 0;
    if ((t.kind == TY_CLASS || t.kind == TY_STRUCT || t.kind == TY_ENUM) && t.name)
    {
        kn_append(buf, sizeof(buf), &off, t.name);
    }
    else if (t.name && t.kind != TY_PTR && t.kind != TY_ARRAY)
    {
        kn_append(buf, sizeof(buf), &off, t.name);
    }
    else if (t.kind == TY_PTR)
    {
        if ((t.elem == TY_CLASS || t.elem == TY_STRUCT || t.elem == TY_ENUM) && t.name)
            kn_append(buf, sizeof(buf), &off, t.name);
        else
            kn_append(buf, sizeof(buf), &off, base_type_name(t.elem));
        int depth = t.ptr_depth > 0 ? t.ptr_depth : 1;
        for (int i = 0; i < depth; i++)
            kn_append(buf, sizeof(buf), &off, "*");
    }
    else if (t.kind == TY_ARRAY)
    {
        if ((t.elem == TY_CLASS || t.elem == TY_STRUCT || t.elem == TY_ENUM) && t.name)
            kn_append(buf, sizeof(buf), &off, t.name);
        else
            kn_append(buf, sizeof(buf), &off, base_type_name(t.elem));
        kn_append(buf, sizeof(buf), &off, "[");
        if (t.array_len >= 0)
            append_i64(buf, sizeof(buf), &off, t.array_len);
        kn_append(buf, sizeof(buf), &off, "]");
    }
    else
    {
        kn_append(buf, sizeof(buf), &off, base_type_name(t.kind));
    }

    char *out = (char *)kn_malloc(off + 1);
    if (!out) return "unknown";
    kn_memcpy(out, buf, off + 1);
    return out;
}

static const char *type_mismatch_detail(Type expected, Type got)
{
    const char *et = type_name(expected);
    const char *gt = type_name(got);
    char buf[160];
    size_t off = 0;
    kn_append(buf, sizeof(buf), &off, "Expected ");
    kn_append(buf, sizeof(buf), &off, et);
    kn_append(buf, sizeof(buf), &off, ", got ");
    kn_append(buf, sizeof(buf), &off, gt);
    char *out = (char *)kn_malloc(off + 1);
    if (!out) return "Type mismatch";
    kn_memcpy(out, buf, off + 1);
    return out;
}

static bool type_equal(Type a, Type b)
{
    if (a.kind != b.kind) return false;
    if (a.kind == TY_ARRAY)
    {
        if (a.elem != b.elem || a.array_len != b.array_len) return false;
        if ((a.elem == TY_CLASS || a.elem == TY_STRUCT || a.elem == TY_ENUM) && a.name && b.name)
            return kn_strcmp(a.name, b.name) == 0;
        return a.elem == b.elem;
    }
    if (a.kind == TY_PTR)
    {
        if (a.elem != b.elem) return false;
        if (a.ptr_depth != b.ptr_depth) return false;
        if ((a.elem == TY_CLASS || a.elem == TY_STRUCT || a.elem == TY_ENUM) && a.name && b.name)
            return kn_strcmp(a.name, b.name) == 0;
        return true;
    }
    if (a.kind == TY_CLASS || a.kind == TY_STRUCT || a.kind == TY_ENUM)
        return a.name && b.name && kn_strcmp(a.name, b.name) == 0;
    return true;
}

static bool type_supported(Type t)
{
    switch (t.kind)
    {
    case TY_VOID:
    case TY_BOOL:
    case TY_BYTE:
    case TY_CHAR:
    case TY_INT:
    case TY_FLOAT:
    case TY_F16:
    case TY_F32:
    case TY_F64:
    case TY_F128:
    case TY_I8:
    case TY_I16:
    case TY_I32:
    case TY_I64:
    case TY_U8:
    case TY_U16:
    case TY_U32:
    case TY_U64:
    case TY_ISIZE:
    case TY_USIZE:
    case TY_STRING:
    case TY_ANY:
    case TY_CLASS:
    case TY_STRUCT:
    case TY_ENUM:
    case TY_PTR:
        return true;
    case TY_ARRAY:
        return t.elem != TY_VOID && t.elem != TY_UNKNOWN;
    default:
        break;
    }
    return false;
}

static bool is_integer_kind(TypeKind k)
{
    switch (k)
    {
    case TY_BYTE:
    case TY_CHAR:
    case TY_INT:
    case TY_I8:
    case TY_I16:
    case TY_I32:
    case TY_I64:
    case TY_U8:
    case TY_U16:
    case TY_U32:
    case TY_U64:
    case TY_ISIZE:
    case TY_USIZE:
        return true;
    default:
        return false;
    }
}

static bool is_error_type(Type t)
{
    return t.kind == TY_CLASS && t.name && kn_strcmp(t.name, "IO.Error") == 0;
}

static bool is_async_task_type(Type t)
{
    return t.kind == TY_CLASS && t.name && kn_strcmp(t.name, "IO.Async.Task") == 0;
}

static bool builtin_is_vararg(KnBuiltinKind kind)
{
    switch (kind)
    {
    case KN_BUILTIN_IO_CONSOLE_PRINT:
    case KN_BUILTIN_IO_CONSOLE_PRINTLINE:
        return true;
    default:
        return false;
    }
}

static bool builtin_conv_target(KnBuiltinKind kind, Type *out)
{
    if (!out) return false;
    switch (kind)
    {
    case KN_BUILTIN_IO_TYPE_BOOL: *out = type_make(TY_BOOL); return true;
    case KN_BUILTIN_IO_TYPE_BYTE: *out = type_make(TY_BYTE); return true;
    case KN_BUILTIN_IO_TYPE_CHAR: *out = type_make(TY_CHAR); return true;
    case KN_BUILTIN_IO_TYPE_INT: *out = type_make(TY_INT); return true;
    case KN_BUILTIN_IO_TYPE_FLOAT: *out = type_make(TY_FLOAT); return true;
    case KN_BUILTIN_IO_TYPE_F16: *out = type_make(TY_F16); return true;
    case KN_BUILTIN_IO_TYPE_F32: *out = type_make(TY_F32); return true;
    case KN_BUILTIN_IO_TYPE_F64: *out = type_make(TY_F64); return true;
    case KN_BUILTIN_IO_TYPE_F128: *out = type_make(TY_F128); return true;
    case KN_BUILTIN_IO_TYPE_I8: *out = type_make(TY_I8); return true;
    case KN_BUILTIN_IO_TYPE_I16: *out = type_make(TY_I16); return true;
    case KN_BUILTIN_IO_TYPE_I32: *out = type_make(TY_I32); return true;
    case KN_BUILTIN_IO_TYPE_I64: *out = type_make(TY_I64); return true;
    case KN_BUILTIN_IO_TYPE_U8: *out = type_make(TY_U8); return true;
    case KN_BUILTIN_IO_TYPE_U16: *out = type_make(TY_U16); return true;
    case KN_BUILTIN_IO_TYPE_U32: *out = type_make(TY_U32); return true;
    case KN_BUILTIN_IO_TYPE_U64: *out = type_make(TY_U64); return true;
    case KN_BUILTIN_IO_TYPE_ISIZE: *out = type_make(TY_ISIZE); return true;
    case KN_BUILTIN_IO_TYPE_USIZE: *out = type_make(TY_USIZE); return true;
    case KN_BUILTIN_IO_TYPE_STRING: *out = type_make(TY_STRING); return true;
    case KN_BUILTIN_IO_TYPE_ANY: *out = type_make(TY_ANY); return true;
    default: return false;
    }
}

static bool is_unsigned_kind(TypeKind k)
{
    switch (k)
    {
    case TY_BYTE:
    case TY_CHAR:
    case TY_U8:
    case TY_U16:
    case TY_U32:
    case TY_U64:
    case TY_USIZE:
        return true;
    default:
        return false;
    }
}

static int int_bits(TypeKind k)
{
    switch (k)
    {
    case TY_BYTE:
    case TY_CHAR:
    case TY_I8:
    case TY_U8: return 8;
    case TY_I16:
    case TY_U16: return 16;
    case TY_I32:
    case TY_U32: return 32;
    case TY_INT:
    case TY_I64:
    case TY_U64:
    case TY_ISIZE:
    case TY_USIZE: return 64;
    default: return 0;
    }
}

static bool is_float_kind(TypeKind k)
{
    return k == TY_FLOAT || k == TY_F16 || k == TY_F32 || k == TY_F64 || k == TY_F128;
}

static int float_bits(TypeKind k)
{
    switch (k)
    {
    case TY_F16: return 16;
    case TY_F32: return 32;
    case TY_FLOAT:
    case TY_F64: return 64;
    case TY_F128: return 128;
    default: return 0;
    }
}

static bool is_numeric_kind(TypeKind k)
{
    return is_integer_kind(k) || is_float_kind(k);
}

static bool is_builtin_type_name(const char *name)
{
    if (!name) return false;
    return kn_strcmp(name, "bool") == 0 || kn_strcmp(name, "byte") == 0 || kn_strcmp(name, "char") == 0 ||
           kn_strcmp(name, "int") == 0 || kn_strcmp(name, "float") == 0 || kn_strcmp(name, "f16") == 0 ||
           kn_strcmp(name, "f32") == 0 || kn_strcmp(name, "f64") == 0 || kn_strcmp(name, "f128") == 0 ||
           kn_strcmp(name, "i8") == 0 ||
           kn_strcmp(name, "i16") == 0 || kn_strcmp(name, "i32") == 0 || kn_strcmp(name, "i64") == 0 ||
           kn_strcmp(name, "u8") == 0 || kn_strcmp(name, "u16") == 0 || kn_strcmp(name, "u32") == 0 ||
           kn_strcmp(name, "u64") == 0 || kn_strcmp(name, "isize") == 0 || kn_strcmp(name, "usize") == 0 ||
           kn_strcmp(name, "string") == 0 || kn_strcmp(name, "any") == 0;
}

static int64_t sema_target_os_value(const Sema *s)
{
    return s ? s->target_os : KN_INFO_OS_UNKNOWN;
}

static int64_t sema_target_arch_value(const Sema *s)
{
    return s ? s->target_arch : KN_INFO_ARCH_UNKNOWN;
}

static int64_t sema_target_env_value(const Sema *s)
{
    return s ? s->target_env : KN_INFO_ENV_UNKNOWN;
}

static int64_t sema_target_pointer_bits_value(const Sema *s)
{
    return s ? s->target_pointer_bits : 0;
}

static const char *sema_target_path_separator(const Sema *s)
{
    return (s && s->target_path_separator) ? s->target_path_separator : "/";
}

static const char *sema_target_new_line(const Sema *s)
{
    return (s && s->target_new_line) ? s->target_new_line : "\n";
}

static const char *sema_target_exe_suffix(const Sema *s)
{
    return (s && s->target_exe_suffix) ? s->target_exe_suffix : "";
}

static const char *sema_target_object_suffix(const Sema *s)
{
    return (s && s->target_object_suffix) ? s->target_object_suffix : ".o";
}

static const char *sema_target_dynamic_library_suffix(const Sema *s)
{
    return (s && s->target_dynamic_library_suffix) ? s->target_dynamic_library_suffix : ".so";
}

static const char *sema_target_static_library_suffix(const Sema *s)
{
    return (s && s->target_static_library_suffix) ? s->target_static_library_suffix : ".a";
}

static int64_t sema_host_os_value(const Sema *s)
{
    return s ? s->host_os : KN_INFO_OS_UNKNOWN;
}

static int64_t sema_host_arch_value(const Sema *s)
{
    return s ? s->host_arch : KN_INFO_ARCH_UNKNOWN;
}

static int64_t sema_host_env_value(const Sema *s)
{
    return s ? s->host_env : KN_INFO_ENV_UNKNOWN;
}

static int64_t sema_host_pointer_bits_value(const Sema *s)
{
    return s ? s->host_pointer_bits : 0;
}

static const char *sema_host_path_separator(const Sema *s)
{
    return (s && s->host_path_separator) ? s->host_path_separator : "/";
}

static const char *sema_host_new_line(const Sema *s)
{
    return (s && s->host_new_line) ? s->host_new_line : "\n";
}

static const char *sema_host_exe_suffix(const Sema *s)
{
    return (s && s->host_exe_suffix) ? s->host_exe_suffix : "";
}

static const char *sema_host_object_suffix(const Sema *s)
{
    return (s && s->host_object_suffix) ? s->host_object_suffix : ".o";
}

static const char *sema_host_dynamic_library_suffix(const Sema *s)
{
    return (s && s->host_dynamic_library_suffix) ? s->host_dynamic_library_suffix : ".so";
}

static const char *sema_host_static_library_suffix(const Sema *s)
{
    return (s && s->host_static_library_suffix) ? s->host_static_library_suffix : ".a";
}

static bool eval_const_expr(Expr *e, int64_t *out)
{
    if (!e || !out) return false;
    switch (e->kind)
    {
    case EXPR_INT:
        *out = e->v.int_val;
        return true;
    case EXPR_CHAR:
        *out = e->v.int_val;
        return true;
    case EXPR_BOOL:
        *out = e->v.int_val ? 1 : 0;
        return true;
    case EXPR_UNARY:
    {
        int64_t v = 0;
        if (!eval_const_expr(e->v.unary.expr, &v)) return false;
        if (e->v.unary.op == TOK_MINUS) { *out = -v; return true; }
        if (e->v.unary.op == TOK_NOT) { *out = !v; return true; }
        return false;
    }
    case EXPR_BINARY:
    {
        int64_t l = 0, r = 0;
        if (!eval_const_expr(e->v.binary.left, &l)) return false;
        if (!eval_const_expr(e->v.binary.right, &r)) return false;
        switch ((TokenType)e->v.binary.op)
        {
        case TOK_PLUS: *out = l + r; return true;
        case TOK_MINUS: *out = l - r; return true;
        case TOK_STAR: *out = l * r; return true;
        case TOK_SLASH: if (r == 0) return false; *out = l / r; return true;
        case TOK_PERCENT: if (r == 0) return false; *out = l % r; return true;
        case TOK_EQ: *out = (l == r); return true;
        case TOK_NE: *out = (l != r); return true;
        case TOK_LT: *out = (l < r); return true;
        case TOK_LE: *out = (l <= r); return true;
        case TOK_GT: *out = (l > r); return true;
        case TOK_GE: *out = (l >= r); return true;
        case TOK_ANDAND: *out = (l && r); return true;
        case TOK_OROR: *out = (l || r); return true;
        case TOK_AMP: *out = (l & r); return true;
        case TOK_BITOR: *out = (l | r); return true;
        case TOK_XOR: *out = (l ^ r); return true;
        case TOK_SHL: *out = (l << r); return true;
        case TOK_SHR: *out = (l >> r); return true;
        default: return false;
        }
    }
    default:
        return false;
    }
}

static bool method_is_virtual(const Method *m)
{
    if (!m) return false;
    return m->is_virtual || m->is_override || m->is_abstract;
}

static bool params_equal(const ParamList *a, const ParamList *b)
{
    if (a->count != b->count) return false;
    for (int i = 0; i < a->count; i++)
    {
        if (!type_equal(a->items[i].type, b->items[i].type))
            return false;
    }
    return true;
}

static bool method_sig_equal(const Method *a, const Method *b)
{
    if (!a || !b) return false;
    if (a->is_static != b->is_static) return false;
    if (kn_strcmp(a->name, b->name) != 0) return false;
    if (!type_equal(a->ret_type, b->ret_type)) return false;
    return params_equal(&a->params, &b->params);
}

static VarSym *scope_find(Scope *s, const char *name)
{
    for (Scope *c = s; c; c = c->parent)
    {
        VarSym *v = c->vars;
        while (v)
        {
            if (kn_strcmp(v->name, name) == 0) return v;
            v = v->next;
        }
    }
    return 0;
}

static void scope_add_ex(Sema *s, Scope *sc, const char *name, Type type, int is_const,
                         int line, int col, Expr *origin, int is_param)
{
    if (sc)
    {
        for (VarSym *v = sc->vars; v; v = v->next)
        {
            if (kn_strcmp(v->name, name) == 0)
                sema_error(s, line, col, (int)kn_strlen(name), "Duplicate Symbol", "Variable already defined in this scope");
        }
    }

    VarSym *v = (VarSym *)kn_malloc(sizeof(VarSym));
    if (!v) kn_die("out of memory");
    v->name = name;
    v->type = type;
    v->async_result_type = (origin && origin->async_result_type.kind != TY_UNKNOWN)
        ? origin->async_result_type
        : type_make(TY_UNKNOWN);
    v->is_const = is_const;
    v->origin = origin;
    v->used = 0;
    v->is_param = is_param ? 1 : 0;
    v->line = line;
    v->col = col;
    v->src = s ? s->src : 0;
    v->next = sc->vars;
    sc->vars = v;
}

static void scope_add(Sema *s, Scope *sc, const char *name, Type type, int is_const, int line, int col, Expr *origin)
{
    scope_add_ex(s, sc, name, type, is_const, line, col, origin, 0);
}

static void scope_add_param(Sema *s, Scope *sc, const char *name, Type type, int line, int col)
{
    scope_add_ex(s, sc, name, type, 0, line, col, 0, 1);
}

static void warn_unused_scope(Sema *s, Scope *sc)
{
    if (!s || !sc)
        return;
    for (VarSym *v = sc->vars; v; v = v->next)
        sema_warn_unused_var(s, v);
}

static void functable_add(FuncTable *t, FuncSym s)
{
    if (t->count + 1 > t->cap)
    {
        int new_cap = t->cap ? t->cap * 2 : 16;
        FuncSym *n = (FuncSym *)kn_malloc(sizeof(FuncSym) * (size_t)new_cap);
        if (t->items)
        {
            kn_memcpy(n, t->items, sizeof(FuncSym) * (size_t)t->count);
            kn_free(t->items);
        }
        t->items = n;
        t->cap = new_cap;
    }
    t->items[t->count++] = s;
}

static FuncSym *functable_find(FuncTable *t, const char *name)
{
    for (int i = 0; i < t->count; i++)
        if (kn_strcmp(t->items[i].qname, name) == 0) return &t->items[i];
    return 0;
}

static FuncSym *functable_find_by_name_in_unit(FuncTable *t, const char *name, const char *unit)
{
    if (!t || !name || !unit) return 0;
    for (int i = 0; i < t->count; i++)
    {
        Func *f = t->items[i].func;
        if (!f) continue;
        if (!f->unit || kn_strcmp(f->unit, unit) != 0) continue;
        if (kn_strcmp(f->name, name) == 0)
            return &t->items[i];
    }
    return 0;
}

static FuncSym *functable_find_by_name_without_unit(FuncTable *t, const char *name)
{
    if (!t || !name) return 0;
    FuncSym *best = 0;
    for (int i = 0; i < t->count; i++)
    {
        Func *f = t->items[i].func;
        if (!f) continue;
        if (f->unit && f->unit[0]) continue;
        if (kn_strcmp(f->name, name) != 0) continue;
        if (best) return 0;
        best = &t->items[i];
    }
    return best;
}

static FuncSym *functable_find_visible_qualified(FuncTable *t, Sema *s, const char *name)
{
    if (!t || !s || !name || !qname_has_dot(name)) return 0;

    {
        const char *expanded = expand_alias_qualified_name(s, name);
        if (expanded != name)
            return functable_find(t, expanded);
    }

    {
        FuncSym *fn = functable_find(t, name);
        if (!fn || !fn->func || !fn->func->unit)
            return 0;
        return module_accessible_by_namespace(s, fn->func->unit) ? fn : 0;
    }
}

static const char *find_required_namespace_module_for_func(Sema *s, const char *name)
{
    if (!s || !s->imports || !name || qname_has_dot(name)) return 0;
    for (int i = 0; i < s->imports->count; i++)
    {
        ImportMap *m = &s->imports->items[i];
        if (m->unit && m->unit[0] && s->current_unit && s->current_unit[0] &&
            kn_strcmp(m->unit, s->current_unit) != 0)
            continue;
        if (m->kind != IMPORT_OPEN || !m->module) continue;
        if (functable_find_by_name_in_unit(&s->funcs, name, m->module))
            return m->module;
    }
    return 0;
}

typedef struct
{
    const Func *tmpl;
    Type *types;
    int count;
} TypeSubst;

static void genericinst_add(GenericInstTable *t, GenericInst s)
{
    if (t->count + 1 > t->cap)
    {
        int new_cap = t->cap ? t->cap * 2 : 16;
        GenericInst *n = (GenericInst *)kn_malloc(sizeof(GenericInst) * (size_t)new_cap);
        if (t->items)
        {
            kn_memcpy(n, t->items, sizeof(GenericInst) * (size_t)t->count);
            kn_free(t->items);
        }
        t->items = n;
        t->cap = new_cap;
    }
    t->items[t->count++] = s;
}

static GenericInst *genericinst_find(GenericInstTable *t, const char *key)
{
    if (!t || !key) return 0;
    for (int i = 0; i < t->count; i++)
        if (kn_strcmp(t->items[i].key, key) == 0) return &t->items[i];
    return 0;
}

static const char *dup_cstr(const char *s)
{
    if (!s) return "";
    size_t n = kn_strlen(s);
    char *out = (char *)kn_malloc(n + 1);
    if (!out) kn_die("out of memory");
    kn_memcpy(out, s, n);
    out[n] = 0;
    return out;
}

static int type_param_index(const Func *f, const char *name)
{
    if (!f || !name || !f->type_params) return -1;
    for (int i = 0; i < f->type_param_count; i++)
    {
        if (f->type_params[i] && kn_strcmp(f->type_params[i], name) == 0)
            return i;
    }
    return -1;
}

static void append_char(char *buf, size_t cap, size_t *off, char c)
{
    if (!buf || !off || *off + 1 >= cap) return;
    buf[(*off)++] = c;
    buf[*off] = 0;
}

static void append_i64_dec(char *buf, size_t cap, size_t *off, int64_t v)
{
    char tmp[32];
    int n = 0;
    if (v == 0)
    {
        tmp[n++] = '0';
    }
    else
    {
        int64_t x = v;
        if (x < 0) x = -x;
        while (x && n < (int)sizeof(tmp))
        {
            tmp[n++] = (char)('0' + (x % 10));
            x /= 10;
        }
        if (v < 0 && n < (int)sizeof(tmp))
            tmp[n++] = '-';
    }
    for (int i = n - 1; i >= 0; i--)
        append_char(buf, cap, off, tmp[i]);
}

static void append_sanitized(char *buf, size_t cap, size_t *off, const char *s)
{
    if (!s) return;
    for (const char *p = s; *p; p++)
    {
        char c = *p;
        bool ok = (c >= 'a' && c <= 'z') ||
                  (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9');
        append_char(buf, cap, off, ok ? c : '_');
    }
}

static void append_type_mangle(char *buf, size_t cap, size_t *off, Type t)
{
    switch (t.kind)
    {
    case TY_VOID: kn_append(buf, cap, off, "void"); return;
    case TY_BOOL: kn_append(buf, cap, off, "bool"); return;
    case TY_BYTE: kn_append(buf, cap, off, "byte"); return;
    case TY_CHAR: kn_append(buf, cap, off, "char"); return;
    case TY_INT: kn_append(buf, cap, off, "int"); return;
    case TY_FLOAT: kn_append(buf, cap, off, "float"); return;
    case TY_F16: kn_append(buf, cap, off, "f16"); return;
    case TY_F32: kn_append(buf, cap, off, "f32"); return;
    case TY_F64: kn_append(buf, cap, off, "f64"); return;
    case TY_F128: kn_append(buf, cap, off, "f128"); return;
    case TY_I8: kn_append(buf, cap, off, "i8"); return;
    case TY_I16: kn_append(buf, cap, off, "i16"); return;
    case TY_I32: kn_append(buf, cap, off, "i32"); return;
    case TY_I64: kn_append(buf, cap, off, "i64"); return;
    case TY_U8: kn_append(buf, cap, off, "u8"); return;
    case TY_U16: kn_append(buf, cap, off, "u16"); return;
    case TY_U32: kn_append(buf, cap, off, "u32"); return;
    case TY_U64: kn_append(buf, cap, off, "u64"); return;
    case TY_ISIZE: kn_append(buf, cap, off, "isize"); return;
    case TY_USIZE: kn_append(buf, cap, off, "usize"); return;
    case TY_STRING: kn_append(buf, cap, off, "string"); return;
    case TY_ANY: kn_append(buf, cap, off, "any"); return;
    case TY_NULL: kn_append(buf, cap, off, "null"); return;
    case TY_CLASS:
        kn_append(buf, cap, off, "c_");
        append_sanitized(buf, cap, off, t.name);
        return;
    case TY_STRUCT:
        kn_append(buf, cap, off, "s_");
        append_sanitized(buf, cap, off, t.name);
        return;
    case TY_ENUM:
        kn_append(buf, cap, off, "e_");
        append_sanitized(buf, cap, off, t.name);
        return;
    case TY_PTR:
    {
        kn_append(buf, cap, off, "p");
        append_i64_dec(buf, cap, off, (int64_t)(t.ptr_depth > 0 ? t.ptr_depth : 1));
        append_char(buf, cap, off, '_');
        Type base = type_make(t.elem);
        base.name = t.name;
        append_type_mangle(buf, cap, off, base);
        return;
    }
    case TY_ARRAY:
    {
        kn_append(buf, cap, off, "arr");
        append_i64_dec(buf, cap, off, t.array_len);
        append_char(buf, cap, off, '_');
        Type base = type_make(t.elem);
        base.name = t.name;
        append_type_mangle(buf, cap, off, base);
        return;
    }
    default:
        kn_append(buf, cap, off, "unknown");
        return;
    }
}

static const char *generic_instance_key(const Func *tmpl, Type *args, int arg_count)
{
    char buf[1024];
    size_t off = 0;
    buf[0] = 0;
    kn_append(buf, sizeof(buf), &off, tmpl && tmpl->qname ? tmpl->qname : (tmpl ? tmpl->name : ""));
    for (int i = 0; i < arg_count; i++)
    {
        append_char(buf, sizeof(buf), &off, '|');
        append_type_mangle(buf, sizeof(buf), &off, args[i]);
    }
    return dup_cstr(buf);
}

static const char *generic_instance_name(const Func *tmpl, Type *args, int arg_count)
{
    char buf[1024];
    size_t off = 0;
    buf[0] = 0;
    kn_append(buf, sizeof(buf), &off, tmpl && tmpl->name ? tmpl->name : "fn");
    kn_append(buf, sizeof(buf), &off, "$g");
    for (int i = 0; i < arg_count; i++)
    {
        append_char(buf, sizeof(buf), &off, '$');
        append_type_mangle(buf, sizeof(buf), &off, args[i]);
    }
    return dup_cstr(buf);
}

static const char *generic_instance_qname(const Func *tmpl, Type *args, int arg_count)
{
    char buf[1024];
    size_t off = 0;
    buf[0] = 0;
    kn_append(buf, sizeof(buf), &off, tmpl && tmpl->qname ? tmpl->qname : (tmpl ? tmpl->name : "fn"));
    kn_append(buf, sizeof(buf), &off, "$g");
    for (int i = 0; i < arg_count; i++)
    {
        append_char(buf, sizeof(buf), &off, '$');
        append_type_mangle(buf, sizeof(buf), &off, args[i]);
    }
    return dup_cstr(buf);
}

static Type subst_type(const TypeSubst *subst, Type t)
{
    if (!subst || !subst->tmpl || subst->count <= 0)
        return t;

    if (t.kind == TY_CLASS && t.name)
    {
        int idx = type_param_index(subst->tmpl, t.name);
        if (idx >= 0 && idx < subst->count)
            return subst->types[idx];
        return t;
    }
    if (t.kind == TY_PTR && t.elem == TY_CLASS && t.name)
    {
        int idx = type_param_index(subst->tmpl, t.name);
        if (idx >= 0 && idx < subst->count)
        {
            Type at = subst->types[idx];
            int add_depth = t.ptr_depth > 0 ? t.ptr_depth : 1;
            if (at.kind == TY_PTR)
            {
                at.ptr_depth += add_depth;
                return at;
            }
            Type out = type_ptr(at.kind);
            out.ptr_depth = add_depth;
            if (at.kind == TY_CLASS || at.kind == TY_STRUCT || at.kind == TY_ENUM)
                out.name = at.name;
            return out;
        }
    }
    if (t.kind == TY_ARRAY && t.elem == TY_CLASS && t.name)
    {
        int idx = type_param_index(subst->tmpl, t.name);
        if (idx >= 0 && idx < subst->count)
        {
            Type at = subst->types[idx];
            Type out = type_array(at.kind, t.array_len);
            out.array_len_expr = t.array_len_expr;
            if (at.kind == TY_CLASS || at.kind == TY_STRUCT || at.kind == TY_ENUM)
                out.name = at.name;
            return out;
        }
    }
    return t;
}

static Expr *clone_expr_subst(const Expr *e, const TypeSubst *subst);
static Stmt *clone_stmt_subst(const Stmt *st, const TypeSubst *subst);

static TypeList clone_typelist_subst(const TypeList *src, const TypeSubst *subst)
{
    TypeList out;
    kn_memset(&out, 0, sizeof(out));
    if (!src) return out;
    for (int i = 0; i < src->count; i++)
        typelist_push(&out, subst_type(subst, src->items[i]));
    return out;
}

static ExprList clone_exprlist_subst(const ExprList *src, const TypeSubst *subst)
{
    ExprList out;
    kn_memset(&out, 0, sizeof(out));
    if (!src) return out;
    for (int i = 0; i < src->count; i++)
        exprlist_push(&out, clone_expr_subst(src->items[i], subst));
    return out;
}

static NameList clone_namelist(const NameList *src)
{
    NameList out;
    kn_memset(&out, 0, sizeof(out));
    if (!src) return out;
    for (int i = 0; i < src->count; i++)
        namelist_push(&out, src->items[i]);
    return out;
}

static ParamList clone_paramlist_subst(const ParamList *src, const TypeSubst *subst)
{
    ParamList out;
    kn_memset(&out, 0, sizeof(out));
    if (!src) return out;
    for (int i = 0; i < src->count; i++)
    {
        Param p = src->items[i];
        p.type = subst_type(subst, p.type);
        p.default_value = clone_expr_subst(p.default_value, subst);
        paramlist_push(&out, p);
    }
    return out;
}

static BlockRecordList clone_block_records(const BlockRecordList *src)
{
    BlockRecordList out;
    kn_memset(&out, 0, sizeof(out));
    if (!src) return out;
    for (int i = 0; i < src->count; i++)
        blockrecordlist_push(&out, src->items[i]);
    return out;
}

static ExprSwitchCaseList clone_exprswitchcaselist_subst(const ExprSwitchCaseList *src, const TypeSubst *subst)
{
    ExprSwitchCaseList out;
    kn_memset(&out, 0, sizeof(out));
    if (!src) return out;
    for (int i = 0; i < src->count; i++)
    {
        ExprSwitchCase c = src->items[i];
        c.match = clone_expr_subst(c.match, subst);
        c.body = clone_expr_subst(c.body, subst);
        exprswitchcaselist_push(&out, c);
    }
    return out;
}

static StmtSwitchCaseList clone_stmtswitchcaselist_subst(const StmtSwitchCaseList *src, const TypeSubst *subst)
{
    StmtSwitchCaseList out;
    kn_memset(&out, 0, sizeof(out));
    if (!src) return out;
    for (int i = 0; i < src->count; i++)
    {
        StmtSwitchCase c = src->items[i];
        c.match = clone_expr_subst(c.match, subst);
        c.body = clone_stmt_subst(c.body, subst);
        stmtswitchcaselist_push(&out, c);
    }
    return out;
}

static Expr *clone_expr_subst(const Expr *e, const TypeSubst *subst)
{
    if (!e) return 0;
    Expr *n = new_expr(e->kind, e->line, e->col);
    n->type = subst_type(subst, e->type);
    switch (e->kind)
    {
    case EXPR_INT:
        n->v.int_val = e->v.int_val;
        break;
    case EXPR_CHAR:
        n->v.int_val = e->v.int_val;
        break;
    case EXPR_FLOAT:
        n->v.float_val = e->v.float_val;
        break;
    case EXPR_BOOL:
        n->v.int_val = e->v.int_val;
        break;
    case EXPR_DEFAULT:
    case EXPR_NULL:
        break;
    case EXPR_STRING:
        n->v.str_val.ptr = e->v.str_val.ptr;
        n->v.str_val.len = e->v.str_val.len;
        break;
    case EXPR_VAR:
        n->v.var.name = e->v.var.name;
        break;
    case EXPR_CALL:
        n->v.call.name = e->v.call.name;
        n->v.call.args = clone_exprlist_subst(&e->v.call.args, subst);
        n->v.call.arg_names = clone_namelist(&e->v.call.arg_names);
        n->v.call.builtin_id = e->v.call.builtin_id;
        n->v.call.type_args = clone_typelist_subst(&e->v.call.type_args, subst);
        break;
    case EXPR_MEMBER_CALL:
        n->v.member_call.recv = clone_expr_subst(e->v.member_call.recv, subst);
        n->v.member_call.name = e->v.member_call.name;
        n->v.member_call.args = clone_exprlist_subst(&e->v.member_call.args, subst);
        n->v.member_call.arg_names = clone_namelist(&e->v.member_call.arg_names);
        n->v.member_call.builtin_id = e->v.member_call.builtin_id;
        n->v.member_call.type_args = clone_typelist_subst(&e->v.member_call.type_args, subst);
        n->v.member_call.is_static = e->v.member_call.is_static;
        n->v.member_call.static_type = e->v.member_call.static_type;
        n->v.member_call.method_index = e->v.member_call.method_index;
        n->v.member_call.method_owner = e->v.member_call.method_owner;
        n->v.member_call.is_qual_call = e->v.member_call.is_qual_call;
        n->v.member_call.qual_name = e->v.member_call.qual_name;
        break;
    case EXPR_INVOKE:
        n->v.invoke.callee = clone_expr_subst(e->v.invoke.callee, subst);
        n->v.invoke.args = clone_exprlist_subst(&e->v.invoke.args, subst);
        n->v.invoke.arg_names = clone_namelist(&e->v.invoke.arg_names);
        break;
    case EXPR_BINARY:
        n->v.binary.op = e->v.binary.op;
        n->v.binary.left = clone_expr_subst(e->v.binary.left, subst);
        n->v.binary.right = clone_expr_subst(e->v.binary.right, subst);
        break;
    case EXPR_UNARY:
        n->v.unary.op = e->v.unary.op;
        n->v.unary.expr = clone_expr_subst(e->v.unary.expr, subst);
        n->v.unary.is_postfix = e->v.unary.is_postfix;
        break;
    case EXPR_ARRAY:
        n->v.array.items = clone_exprlist_subst(&e->v.array.items, subst);
        break;
    case EXPR_DICT:
        n->v.dict.keys = clone_exprlist_subst(&e->v.dict.keys, subst);
        n->v.dict.values = clone_exprlist_subst(&e->v.dict.values, subst);
        break;
    case EXPR_ASSIGN:
        n->v.assign.target = clone_expr_subst(e->v.assign.target, subst);
        n->v.assign.value = clone_expr_subst(e->v.assign.value, subst);
        break;
    case EXPR_IF:
        n->v.if_expr.cond = clone_expr_subst(e->v.if_expr.cond, subst);
        n->v.if_expr.then_expr = clone_expr_subst(e->v.if_expr.then_expr, subst);
        n->v.if_expr.else_expr = clone_expr_subst(e->v.if_expr.else_expr, subst);
        break;
    case EXPR_SWITCH:
        n->v.switch_expr.value = clone_expr_subst(e->v.switch_expr.value, subst);
        n->v.switch_expr.cases = clone_exprswitchcaselist_subst(&e->v.switch_expr.cases, subst);
        break;
    case EXPR_IS:
        n->v.is_expr.expr = clone_expr_subst(e->v.is_expr.expr, subst);
        n->v.is_expr.target = subst_type(subst, e->v.is_expr.target);
        break;
    case EXPR_MEMBER:
        n->v.member.recv = clone_expr_subst(e->v.member.recv, subst);
        n->v.member.name = e->v.member.name;
        n->v.member.is_static = e->v.member.is_static;
        n->v.member.static_type = e->v.member.static_type;
        n->v.member.field_index = e->v.member.field_index;
        n->v.member.field_owner = e->v.member.field_owner;
        n->v.member.is_enum_const = e->v.member.is_enum_const;
        n->v.member.enum_value = e->v.member.enum_value;
        n->v.member.enum_type = e->v.member.enum_type;
        break;
    case EXPR_INDEX:
        n->v.index.recv = clone_expr_subst(e->v.index.recv, subst);
        n->v.index.index = clone_expr_subst(e->v.index.index, subst);
        break;
    case EXPR_THIS:
    case EXPR_BASE:
        break;
    case EXPR_NEW:
    {
        Type cname = type_class(e->v.new_expr.class_name);
        cname = subst_type(subst, cname);
        n->v.new_expr.class_name = cname.name ? cname.name : e->v.new_expr.class_name;
        n->v.new_expr.args = clone_exprlist_subst(&e->v.new_expr.args, subst);
        n->v.new_expr.arg_names = clone_namelist(&e->v.new_expr.arg_names);
        n->v.new_expr.ctor_index = e->v.new_expr.ctor_index;
        break;
    }
    case EXPR_CAST:
        n->v.cast.type = subst_type(subst, e->v.cast.type);
        n->v.cast.expr = clone_expr_subst(e->v.cast.expr, subst);
        break;
    case EXPR_FUNC_REF:
        n->v.func_ref.qname = e->v.func_ref.qname;
        n->v.func_ref.builtin_id = e->v.func_ref.builtin_id;
        n->v.func_ref.type_args = clone_typelist_subst(&e->v.func_ref.type_args, subst);
        break;
    case EXPR_ANON_FUNC:
        n->v.anon_func.ret_type = subst_type(subst, e->v.anon_func.ret_type);
        n->v.anon_func.params = 0;
        n->v.anon_func.param_count = 0;
        n->v.anon_func.param_cap = 0;
        for (int i = 0; i < e->v.anon_func.param_count; i++)
        {
            Param p = e->v.anon_func.params[i];
            p.type = subst_type(subst, p.type);
            p.default_value = clone_expr_subst(p.default_value, subst);
            if (n->v.anon_func.param_count + 1 > n->v.anon_func.param_cap)
            {
                int new_cap = n->v.anon_func.param_cap ? n->v.anon_func.param_cap * 2 : 4;
                Param *arr = (Param *)kn_malloc(sizeof(Param) * (size_t)new_cap);
                if (n->v.anon_func.params)
                {
                    kn_memcpy(arr, n->v.anon_func.params, sizeof(Param) * (size_t)n->v.anon_func.param_count);
                    kn_free(n->v.anon_func.params);
                }
                n->v.anon_func.params = arr;
                n->v.anon_func.param_cap = new_cap;
            }
            n->v.anon_func.params[n->v.anon_func.param_count++] = p;
        }
        n->v.anon_func.body = clone_stmt_subst(e->v.anon_func.body, subst);
        n->v.anon_func.safety = e->v.anon_func.safety;
        n->v.anon_func.debug_name = e->v.anon_func.debug_name;
        n->v.anon_func.qname = e->v.anon_func.qname;
        n->v.anon_func.id = e->v.anon_func.id;
        break;
    case EXPR_BLOCK_LITERAL:
        n->v.block_lit.safety = e->v.block_lit.safety;
        n->v.block_lit.body = clone_stmt_subst(e->v.block_lit.body, subst);
        n->v.block_lit.records = clone_block_records(&e->v.block_lit.records);
        n->v.block_lit.debug_name = e->v.block_lit.debug_name;
        n->v.block_lit.id = e->v.block_lit.id;
        break;
    default:
        break;
    }
    return n;
}

static Stmt *clone_stmt_subst(const Stmt *st, const TypeSubst *subst)
{
    if (!st) return 0;
    Stmt *n = new_stmt(st->kind, st->line, st->col);
    switch (st->kind)
    {
    case ST_BLOCK:
        for (int i = 0; i < st->v.block.stmts.count; i++)
            stmtlist_push(&n->v.block.stmts, clone_stmt_subst(st->v.block.stmts.items[i], subst));
        break;
    case ST_VAR:
        n->v.var.type = subst_type(subst, st->v.var.type);
        n->v.var.name = st->v.var.name;
        n->v.var.name_line = st->v.var.name_line;
        n->v.var.name_col = st->v.var.name_col;
        n->v.var.init = clone_expr_subst(st->v.var.init, subst);
        n->v.var.is_const = st->v.var.is_const;
        n->v.var.src = st->v.var.src;
        n->v.var.unit = st->v.var.unit;
        break;
    case ST_ASSIGN:
        n->v.assign.name = st->v.assign.name;
        n->v.assign.value = clone_expr_subst(st->v.assign.value, subst);
        break;
    case ST_EXPR:
        n->v.expr.expr = clone_expr_subst(st->v.expr.expr, subst);
        break;
    case ST_IF:
        n->v.ifs.cond = clone_expr_subst(st->v.ifs.cond, subst);
        n->v.ifs.then_s = clone_stmt_subst(st->v.ifs.then_s, subst);
        n->v.ifs.else_s = clone_stmt_subst(st->v.ifs.else_s, subst);
        n->v.ifs.pattern_name = st->v.ifs.pattern_name;
        n->v.ifs.pattern_line = st->v.ifs.pattern_line;
        n->v.ifs.pattern_col = st->v.ifs.pattern_col;
        n->v.ifs.is_const = st->v.ifs.is_const;
        n->v.ifs.const_value = st->v.ifs.const_value;
        break;
    case ST_SWITCH:
        n->v.switchs.value = clone_expr_subst(st->v.switchs.value, subst);
        n->v.switchs.cases = clone_stmtswitchcaselist_subst(&st->v.switchs.cases, subst);
        break;
    case ST_WHILE:
        n->v.whiles.cond = clone_expr_subst(st->v.whiles.cond, subst);
        n->v.whiles.body = clone_stmt_subst(st->v.whiles.body, subst);
        break;
    case ST_FOR:
        n->v.fors.init = clone_stmt_subst(st->v.fors.init, subst);
        n->v.fors.cond = clone_expr_subst(st->v.fors.cond, subst);
        n->v.fors.post = clone_expr_subst(st->v.fors.post, subst);
        n->v.fors.body = clone_stmt_subst(st->v.fors.body, subst);
        break;
    case ST_RETURN:
        n->v.ret.expr = clone_expr_subst(st->v.ret.expr, subst);
        break;
    case ST_TRY:
        n->v.trys.try_block = clone_stmt_subst(st->v.trys.try_block, subst);
        n->v.trys.catch_block = clone_stmt_subst(st->v.trys.catch_block, subst);
        n->v.trys.catch_type = subst_type(subst, st->v.trys.catch_type);
        n->v.trys.catch_name = st->v.trys.catch_name;
        n->v.trys.has_param = st->v.trys.has_param;
        break;
    case ST_THROW:
        n->v.throws.expr = clone_expr_subst(st->v.throws.expr, subst);
        break;
    case ST_BLOCK_RECORD:
        n->v.record.name = st->v.record.name;
        n->v.record.ip = st->v.record.ip;
        n->v.record.nested_depth = st->v.record.nested_depth;
        break;
    case ST_BLOCK_JUMP:
        n->v.jump.target = clone_expr_subst(st->v.jump.target, subst);
        break;
    case ST_BREAK:
    case ST_CONTINUE:
        break;
    default:
        break;
    }
    return n;
}

static ExprList clone_exprlist_shallow(const ExprList *src)
{
    ExprList out;
    kn_memset(&out, 0, sizeof(out));
    if (!src || src->count <= 0) return out;
    out.items = (Expr **)kn_malloc(sizeof(Expr *) * (size_t)src->count);
    if (!out.items) kn_die("out of memory");
    kn_memcpy(out.items, src->items, sizeof(Expr *) * (size_t)src->count);
    out.count = src->count;
    out.cap = src->count;
    return out;
}

static ParamList clone_paramlist_shallow(const ParamList *src)
{
    ParamList out;
    kn_memset(&out, 0, sizeof(out));
    if (!src || src->count <= 0) return out;
    out.items = (Param *)kn_malloc(sizeof(Param) * (size_t)src->count);
    if (!out.items) kn_die("out of memory");
    kn_memcpy(out.items, src->items, sizeof(Param) * (size_t)src->count);
    out.count = src->count;
    out.cap = src->count;
    return out;
}

static int bind_default_literal_type(Sema *s, Expr *e, Type expected, const char *detail)
{
    if (!e || e->kind != EXPR_DEFAULT)
        return 0;
    if (expected.kind == TY_UNKNOWN || expected.kind == TY_VOID)
    {
        sema_error(s, e->line, e->col, 1, "Default Literal",
                   detail ? detail : "Default literal requires a target type");
        e->type = type_make(TY_UNKNOWN);
        return 1;
    }
    e->type = expected;
    return 1;
}

static int array_literal_matches_expected(Sema *s, Expr *e, Type expected)
{
    if (!e || e->kind != EXPR_ARRAY || expected.kind != TY_ARRAY)
        return 0;

    Type elem = type_make(expected.elem);
    elem.name = expected.name;
    for (int i = 0; i < e->v.array.items.count; i++)
    {
        Expr *item = e->v.array.items.items[i];
        if (!item || item->kind == EXPR_DEFAULT)
            continue;
        if (!type_assignable(s, elem, sema_expr(s, item)))
            return 0;
    }
    return 1;
}

static int call_arg_matches_param(Sema *s, Expr *arg, Type arg_type, Type param_type)
{
    if (arg && arg->kind == EXPR_DEFAULT)
        return param_type.kind != TY_UNKNOWN && param_type.kind != TY_VOID;
    if (arg && arg->kind == EXPR_ARRAY && param_type.kind == TY_ARRAY)
        return array_literal_matches_expected(s, arg, param_type);
    return type_assignable(s, param_type, arg_type);
}

static Type find_common_class_branch_type(Sema *s, Type lhs, Type rhs)
{
    Type out = type_make(TY_UNKNOWN);
    if (!s || lhs.kind != TY_CLASS || rhs.kind != TY_CLASS || !lhs.name || !rhs.name)
        return out;

    ClassDecl *lhs_c = find_class(s, lhs.name);
    ClassDecl *rhs_c = find_class(s, rhs.name);
    if (!lhs_c || !rhs_c)
        return out;

    for (ClassDecl *cur = lhs_c; cur; )
    {
        if (class_is_derived_from(s, rhs_c, cur))
        {
            out = type_class(class_qname(cur));
            return out;
        }
        if (!cur->base)
            break;
        cur = find_class(s, cur->base);
    }
    return out;
}

static Type merge_branch_types(Sema *s, int line, int col, Expr *lhs_expr, Type lhs, Expr *rhs_expr, Type rhs, const char *detail)
{
    if (lhs_expr && lhs_expr->kind == EXPR_DEFAULT && rhs.kind != TY_UNKNOWN)
    {
        lhs_expr->type = rhs;
        return rhs;
    }
    if (rhs_expr && rhs_expr->kind == EXPR_DEFAULT && lhs.kind != TY_UNKNOWN)
    {
        rhs_expr->type = lhs;
        return lhs;
    }
    if (lhs_expr && lhs_expr->kind == EXPR_DEFAULT && rhs_expr && rhs_expr->kind == EXPR_DEFAULT)
    {
        sema_error(s, line, col, 1, "Default Literal", "Default literal requires a target type");
        return type_make(TY_UNKNOWN);
    }
    if (type_equal(lhs, rhs))
        return lhs;
    if (type_assignable(s, lhs, rhs))
        return lhs;
    if (type_assignable(s, rhs, lhs))
        return rhs;
    {
        Type common = find_common_class_branch_type(s, lhs, rhs);
        if (common.kind != TY_UNKNOWN)
            return common;
    }
    if (is_numeric_kind(lhs.kind) && is_numeric_kind(rhs.kind))
    {
        if (is_float_kind(lhs.kind) || is_float_kind(rhs.kind))
        {
            if (is_float_kind(lhs.kind) && is_float_kind(rhs.kind))
            {
                int lb = float_bits(lhs.kind);
                int rb = float_bits(rhs.kind);
                return lb >= rb ? lhs : rhs;
            }
            else if (is_float_kind(lhs.kind))
                return lhs;
            else
                return rhs;
        }
        {
            int lb = int_bits(lhs.kind);
            int rb = int_bits(rhs.kind);
            int bits = lb > rb ? lb : rb;
            bool u = is_unsigned_kind(lhs.kind) || is_unsigned_kind(rhs.kind);
            TypeKind rk;
            if (bits <= 8) rk = u ? TY_U8 : TY_I8;
            else if (bits <= 16) rk = u ? TY_U16 : TY_I16;
            else if (bits <= 32) rk = u ? TY_U32 : TY_I32;
            else rk = u ? TY_U64 : TY_I64;
            return type_make(rk);
        }
    }
    if (lhs.kind == TY_NULL && (rhs.kind == TY_CLASS || rhs.kind == TY_PTR || rhs.kind == TY_STRING))
        return rhs;
    if (rhs.kind == TY_NULL && (lhs.kind == TY_CLASS || lhs.kind == TY_PTR || lhs.kind == TY_STRING))
        return lhs;
    sema_error(s, line, col, 1, "Type Mismatch",
               detail ? detail : "Branches must have compatible types");
    return lhs.kind != TY_UNKNOWN ? lhs : rhs;
}

static int types_support_switch_match(Type lhs, Type rhs)
{
    if ((lhs.kind == TY_NULL && (rhs.kind == TY_PTR || rhs.kind == TY_CLASS || rhs.kind == TY_STRING || rhs.kind == TY_NULL)) ||
        (rhs.kind == TY_NULL && (lhs.kind == TY_PTR || lhs.kind == TY_CLASS || lhs.kind == TY_STRING || lhs.kind == TY_NULL)))
        return 1;
    if (lhs.kind == TY_STRING && rhs.kind == TY_STRING)
        return 1;
    if (lhs.kind == TY_PTR && rhs.kind == TY_PTR && type_equal(lhs, rhs))
        return 1;
    if (lhs.kind == TY_CLASS && rhs.kind == TY_CLASS && type_equal(lhs, rhs))
        return 1;
    if (lhs.kind == TY_BOOL && rhs.kind == TY_BOOL)
        return 1;
    if (lhs.kind == TY_ENUM && rhs.kind == TY_ENUM && type_equal(lhs, rhs))
        return 1;
    if (is_numeric_kind(lhs.kind) && is_numeric_kind(rhs.kind))
        return 1;
    return 0;
}

static int prepare_call_args(Sema *s, int call_line, int call_col, ExprList *args, NameList *arg_names,
                             ParamList *typed_args, const Param *params, int param_count,
                             const char **param_names, const char *call_name,
                             int emit_errors, int materialize_defaults)
{
    if (!args || !arg_names || args->count != arg_names->count)
        return 1;
    if (typed_args && typed_args->count != args->count)
        return 1;

    int argc = args->count;
    if (param_count <= 0)
    {
        if (call_has_named_args(arg_names) && emit_errors)
            sema_error(s, call_line, call_col, 1, "Invalid Named Argument",
                       "Named arguments are not supported for this callable");
        if (argc != 0 && emit_errors)
            sema_error_arg_count_named(s, call_line, call_col, 1, call_name ? call_name : "callable", 0);
        return argc == 0;
    }

    if (!param_names && call_has_named_args(arg_names))
    {
        if (emit_errors)
            sema_error(s, call_line, call_col, 1, "Invalid Named Argument",
                       "Named arguments are not supported for this callable");
        return 0;
    }

    int ok = 1;
    int missing_required = 0;
    int too_many = 0;
    int *mapping = (int *)kn_malloc(sizeof(int) * (size_t)(argc > 0 ? argc : 1));
    int *used = (int *)kn_malloc(sizeof(int) * (size_t)param_count);
    Expr **new_items = (Expr **)kn_malloc(sizeof(Expr *) * (size_t)param_count);
    const char **new_names = (const char **)kn_malloc(sizeof(const char *) * (size_t)param_count);
    Param *new_typed = typed_args ? (Param *)kn_malloc(sizeof(Param) * (size_t)param_count) : 0;
    if (!mapping || !used || !new_items || !new_names || (typed_args && !new_typed))
    {
        if (mapping) kn_free(mapping);
        if (used) kn_free(used);
        if (new_items) kn_free(new_items);
        if (new_names) kn_free(new_names);
        if (new_typed) kn_free(new_typed);
        return 1;
    }

    for (int i = 0; i < argc; i++) mapping[i] = -1;
    for (int i = 0; i < param_count; i++)
    {
        used[i] = 0;
        new_items[i] = 0;
        new_names[i] = 0;
        if (new_typed) kn_memset(&new_typed[i], 0, sizeof(Param));
    }

    int next_pos = 0;
    int saw_named = 0;
    for (int i = 0; i < argc; i++)
    {
        const char *an = arg_names->items[i];
        if (an && an[0])
        {
            saw_named = 1;
            int idx = find_param_name_index(param_names, param_count, an);
            if (idx < 0)
            {
                if (emit_errors)
                    sema_error_named_arg_unknown(s, args->items[i]->line, args->items[i]->col, 1, an);
                ok = 0;
                continue;
            }
            if (used[idx])
            {
                if (emit_errors)
                    sema_error_named_arg_duplicate(s, args->items[i]->line, args->items[i]->col, 1, an);
                ok = 0;
                continue;
            }
            mapping[i] = idx;
            used[idx] = 1;
            continue;
        }

        if (saw_named)
        {
            if (emit_errors)
                sema_error(s, args->items[i]->line, args->items[i]->col, 1,
                           "Invalid Named Argument", "Positional argument cannot follow named argument");
            ok = 0;
        }
        while (next_pos < param_count && used[next_pos]) next_pos++;
        if (next_pos >= param_count)
        {
            too_many = 1;
            ok = 0;
            continue;
        }
        mapping[i] = next_pos;
        used[next_pos] = 1;
        next_pos++;
    }

    for (int i = 0; i < argc; i++)
    {
        int idx = mapping[i];
        if (idx < 0 || idx >= param_count)
            continue;
        new_items[idx] = args->items[i];
        new_names[idx] = 0;
        if (new_typed)
        {
            new_typed[idx] = typed_args->items[i];
            new_typed[idx].materialized_default = 0;
        }
    }

    for (int i = 0; i < param_count; i++)
    {
        if (!new_items[i])
        {
            if (params && params[i].default_value)
            {
                new_items[i] = materialize_defaults ? clone_expr_subst(params[i].default_value, 0) : params[i].default_value;
                if (new_typed)
                {
                    new_typed[i].name = params[i].name;
                    new_typed[i].type = params[i].default_value->type;
                    new_typed[i].src = params[i].src;
                    new_typed[i].materialized_default = 1;
                }
                continue;
            }

            missing_required = 1;
            ok = 0;
            {
                Expr *dummy = new_expr(EXPR_DEFAULT, call_line, call_col);
                dummy->type = params ? params[i].type : type_make(TY_UNKNOWN);
                new_items[i] = dummy;
                if (new_typed)
                {
                    new_typed[i].name = params ? params[i].name : "";
                    new_typed[i].type = params ? params[i].type : type_make(TY_UNKNOWN);
                    new_typed[i].src = params ? params[i].src : s->src;
                    new_typed[i].materialized_default = 0;
                }
            }
            continue;
        }

        if (new_items[i]->kind == EXPR_DEFAULT)
        {
            if (params && params[i].default_value)
            {
                new_items[i] = materialize_defaults ? clone_expr_subst(params[i].default_value, 0) : params[i].default_value;
                if (new_typed)
                {
                    new_typed[i].type = params[i].default_value->type;
                    new_typed[i].src = params[i].src;
                    new_typed[i].materialized_default = 1;
                }
            }
            else
            {
                ok = 0;
                if (params)
                    new_items[i]->type = params[i].type;
                if (emit_errors)
                {
                    char detail[256];
                    size_t off = 0;
                    detail[0] = 0;
                    kn_append(detail, sizeof(detail), &off, "Parameter '");
                    kn_append(detail, sizeof(detail), &off, params && params[i].name ? params[i].name : "?");
                    kn_append(detail, sizeof(detail), &off, "' does not have a default value");
                    sema_error(s, new_items[i]->line, new_items[i]->col, 1, "Default Argument", detail);
                }
                if (new_typed && params)
                {
                    new_typed[i].type = params[i].type;
                    new_typed[i].src = params[i].src;
                    new_typed[i].materialized_default = 0;
                }
            }
        }
    }

    if ((missing_required || too_many) && emit_errors)
        sema_error_arg_count_named(s, call_line, call_col, 1, call_name ? call_name : "callable", param_count);

    if (args->items) kn_free(args->items);
    if (arg_names->items) kn_free((void *)arg_names->items);
    args->items = new_items;
    args->count = param_count;
    args->cap = param_count;
    arg_names->items = new_names;
    arg_names->count = param_count;
    arg_names->cap = param_count;
    if (typed_args)
    {
        if (typed_args->items) kn_free(typed_args->items);
        typed_args->items = new_typed;
        typed_args->count = param_count;
        typed_args->cap = param_count;
    }
    else if (new_typed)
    {
        kn_free(new_typed);
    }

    kn_free(mapping);
    kn_free(used);
    return ok;
}

static int call_args_match_params(Sema *s, int call_line, int call_col, const ExprList *raw_args,
                                  const NameList *raw_names, const ParamList *raw_types,
                                  const ParamList *params)
{
    if (!params)
        return 0;

    ExprList args = clone_exprlist_shallow(raw_args);
    NameList names = clone_namelist(raw_names);
    ParamList types = clone_paramlist_shallow(raw_types);
    const char **param_names = alloc_param_names_from_paramlist(params);
    int ok = prepare_call_args(s, call_line, call_col, &args, &names, &types,
                               params->items, params->count, param_names, 0, 0, 0);
    if (param_names) kn_free((void *)param_names);
    if (!ok)
    {
        if (args.items) kn_free(args.items);
        if (names.items) kn_free((void *)names.items);
        if (types.items) kn_free(types.items);
        return 0;
    }

    for (int i = 0; i < params->count; i++)
    {
        if (!call_arg_matches_param(s, args.items[i], types.items[i].type, params->items[i].type))
        {
            ok = 0;
            break;
        }
    }

    if (args.items) kn_free(args.items);
    if (names.items) kn_free((void *)names.items);
    if (types.items) kn_free(types.items);
    return ok;
}

static Func *instantiate_generic_func(Sema *s, Func *tmpl, Type *type_args, int type_arg_count, int line, int col)
{
    if (!s || !tmpl)
        return 0;
    if (!func_is_generic_template(tmpl))
        return tmpl;

    if (type_arg_count != tmpl->type_param_count)
    {
        sema_error(s, line, col, 1, "Generic Arity", "Wrong number of generic type arguments");
        return 0;
    }

    const char *key = generic_instance_key(tmpl, type_args, type_arg_count);
    GenericInst *existing = genericinst_find(&s->generic_insts, key);
    if (existing && existing->func)
        return existing->func;

    TypeSubst subst;
    subst.tmpl = tmpl;
    subst.types = type_args;
    subst.count = type_arg_count;

    Func inst;
    kn_memset(&inst, 0, sizeof(inst));
    inst.name = generic_instance_name(tmpl, type_args, type_arg_count);
    inst.ret_type = subst_type(&subst, tmpl->ret_type);
    inst.params = clone_paramlist_subst(&tmpl->params, &subst);
    inst.type_params = 0;
    inst.type_param_count = 0;
    inst.type_param_cap = 0;
    inst.body = tmpl->body ? clone_stmt_subst(tmpl->body, &subst) : 0;
    inst.safety = tmpl->safety;
    inst.attrs = tmpl->attrs;
    inst.src = tmpl->src;
    inst.is_extern = tmpl->is_extern;
    inst.extern_abi = tmpl->extern_abi;
    inst.extern_symbol = tmpl->extern_symbol;
    inst.is_delegate = tmpl->is_delegate;
    inst.delegate_abi = tmpl->delegate_abi;
    inst.delegate_symbol_name = tmpl->delegate_symbol_name;
    inst.is_static = tmpl->is_static;
    inst.is_generic_instance = 1;
    inst.generic_template_qname = tmpl->qname ? tmpl->qname : tmpl->name;
    inst.generic_arg_count = type_arg_count;
    if (type_arg_count > 0)
    {
        inst.generic_args = (Type *)kn_malloc(sizeof(Type) * (size_t)type_arg_count);
        for (int i = 0; i < type_arg_count; i++)
            inst.generic_args[i] = type_args[i];
    }
    inst.unit = tmpl->unit;
    inst.qname = generic_instance_qname(tmpl, type_args, type_arg_count);
    inst.line = tmpl->line;
    inst.col = tmpl->col;

    funclist_push(s->func_list, inst);
    Func *out = &s->func_list->items[s->func_list->count - 1];

    FuncSym sym;
    sym.qname = out->qname ? out->qname : out->name;
    sym.func = out;
    functable_add(&s->funcs, sym);

    GenericInst gi;
    gi.key = key;
    gi.func = out;
    genericinst_add(&s->generic_insts, gi);
    return out;
}

static Type *copy_and_validate_type_args(Sema *s, const TypeList *src, int line, int col)
{
    if (!src || src->count <= 0)
        return 0;
    Type *out = (Type *)kn_malloc(sizeof(Type) * (size_t)src->count);
    if (!out) kn_die("out of memory");
    for (int i = 0; i < src->count; i++)
    {
        out[i] = src->items[i];
        validate_type_ref(s, &out[i], line, col, 0);
    }
    return out;
}

static bool name_has_dot(const char *name)
{
    if (!name) return false;
    for (const char *p = name; *p; p++)
        if (*p == '.') return true;
    return false;
}

static ClassDecl *find_class(Sema *s, const char *name)
{
    if (!s || !s->classes || !name) return 0;
    ClassDecl *best = 0;
    int best_score = 0;
    for (int i = 0; i < s->classes->count; i++)
    {
        ClassDecl *c = &s->classes->items[i];
        int score = named_decl_match_score(s, name,
                                           type_decl_lookup_name(c->name, c->qname, c->owner_type_qname),
                                           c->unit, c->qname);
        if (score > best_score)
        {
            best = c;
            best_score = score;
        }
    }
    return best;
}

static InterfaceDecl *find_interface(Sema *s, const char *name)
{
    if (!s || !s->interfaces || !name) return 0;
    InterfaceDecl *best = 0;
    int best_score = 0;
    for (int i = 0; i < s->interfaces->count; i++)
    {
        InterfaceDecl *it = &s->interfaces->items[i];
        int score = named_decl_match_score(s, name,
                                           type_decl_lookup_name(it->name, it->qname, it->owner_type_qname),
                                           it->unit, it->qname);
        if (score > best_score)
        {
            best = it;
            best_score = score;
        }
    }
    return best;
}

static StructDecl *find_struct(Sema *s, const char *name)
{
    if (!s || !s->structs || !name) return 0;
    StructDecl *best = 0;
    int best_score = 0;
    for (int i = 0; i < s->structs->count; i++)
    {
        StructDecl *st = &s->structs->items[i];
        int score = named_decl_match_score(s, name,
                                           type_decl_lookup_name(st->name, st->qname, st->owner_type_qname),
                                           st->unit, st->qname);
        if (score > best_score)
        {
            best = st;
            best_score = score;
        }
    }
    return best;
}

static EnumDecl *find_enum(Sema *s, const char *name)
{
    if (!s || !s->enums || !name) return 0;
    EnumDecl *best = 0;
    int best_score = 0;
    for (int i = 0; i < s->enums->count; i++)
    {
        EnumDecl *en = &s->enums->items[i];
        int score = named_decl_match_score(s, name,
                                           type_decl_lookup_name(en->name, en->qname, en->owner_type_qname),
                                           en->unit, en->qname);
        if (score > best_score)
        {
            best = en;
            best_score = score;
        }
    }
    return best;
}

static Field *find_field(ClassDecl *c, const char *name, int *out_index)
{
    if (!c || !name) return 0;
    for (int i = 0; i < c->fields.count; i++)
    {
        if (kn_strcmp(c->fields.items[i].name, name) == 0)
        {
            if (out_index) *out_index = i;
            return &c->fields.items[i];
        }
    }
    return 0;
}

static Field *find_struct_field(StructDecl *s, const char *name, int *out_index)
{
    if (!s || !name) return 0;
    for (int i = 0; i < s->fields.count; i++)
    {
        if (kn_strcmp(s->fields.items[i].name, name) == 0)
        {
            if (out_index) *out_index = i;
            return &s->fields.items[i];
        }
    }
    return 0;
}

static EnumItem *find_enum_item(EnumDecl *e, const char *name)
{
    if (!e || !name) return 0;
    for (int i = 0; i < e->items.count; i++)
        if (kn_strcmp(e->items.items[i].name, name) == 0)
            return &e->items.items[i];
    return 0;
}

static Method *find_method(ClassDecl *c, const char *name, const ParamList *params, int is_static, int *out_index)
{
    if (!c || !name) return 0;
    for (int i = 0; i < c->methods.count; i++)
    {
        Method *m = &c->methods.items[i];
        if (m->is_constructor) continue;
        if (kn_strcmp(m->name, name) != 0) continue;
        if (m->is_static != is_static) continue;
        if (!params_equal(&m->params, params)) continue;
        if (out_index) *out_index = i;
        return m;
    }
    return 0;
}

static Method *find_method_named_arity(ClassDecl *c, const char *name, int param_count, int is_static, int *out_index)
{
    if (!c || !name) return 0;
    for (int i = 0; i < c->methods.count; i++)
    {
        Method *m = &c->methods.items[i];
        if (m->is_constructor) continue;
        if (m->is_static != is_static) continue;
        if (m->params.count != param_count) continue;
        if (kn_strcmp(m->name, name) != 0) continue;
        if (out_index) *out_index = i;
        return m;
    }
    return 0;
}

static Method *find_static_method_by_name(ClassDecl *c, const char *name, int *out_index)
{
    if (!c || !name) return 0;
    for (int i = 0; i < c->methods.count; i++)
    {
        Method *m = &c->methods.items[i];
        if (m->is_constructor || !m->is_static) continue;
        if (kn_strcmp(m->name, name) != 0) continue;
        if (out_index) *out_index = i;
        return m;
    }
    return 0;
}

static const char *build_method_ref_qname(const ClassDecl *c, const char *method_name, int index)
{
    const char *cqn = c->qname ? c->qname : c->name;
    char idx_str[16];
    int n = 0, x = index;
    if (x == 0) idx_str[n++] = '0';
    else { while (x > 0 && n < 15) { idx_str[n++] = (char)('0' + (x % 10)); x /= 10; } }
    { int a = 0, b = n - 1; while (a < b) { char t = idx_str[a]; idx_str[a] = idx_str[b]; idx_str[b] = t; a++; b--; } }
    idx_str[n] = 0;
    size_t clen = kn_strlen(cqn);
    size_t mlen = kn_strlen(method_name);
    size_t ilen = (size_t)n;
    size_t total = clen + 1 + mlen + 1 + ilen + 1;
    char *out = (char *)kn_malloc(total);
    size_t off = 0;
    kn_memcpy(out + off, cqn, clen); off += clen;
    out[off++] = '$';
    kn_memcpy(out + off, method_name, mlen); off += mlen;
    out[off++] = '$';
    kn_memcpy(out + off, idx_str, ilen); off += ilen;
    out[off] = 0;
    return out;
}

static Method *find_ctor(Sema *s, ClassDecl *c, const ExprList *raw_args, const NameList *raw_names,
                        const ParamList *arg_types, int *out_index)
{
    if (!c) return 0;
    for (int i = 0; i < c->methods.count; i++)
    {
        Method *m = &c->methods.items[i];
        if (!m->is_constructor) continue;
        if (call_args_match_params(s, 0, 0, raw_args, raw_names, arg_types, &m->params))
        {
            if (out_index) *out_index = i;
            return m;
        }
    }
    return 0;
}

static bool class_has_explicit_ctor(ClassDecl *c)
{
    if (!c) return false;
    for (int i = 0; i < c->methods.count; i++)
        if (c->methods.items[i].is_constructor)
            return true;
    return false;
}

static Field *find_field_hierarchy(Sema *s, ClassDecl *c, const char *name, ClassDecl **owner_out, int *index_out)
{
    ClassDecl *cur = c;
    while (cur)
    {
        int idx = -1;
        Field *f = find_field(cur, name, &idx);
        if (f)
        {
            if (owner_out) *owner_out = cur;
            if (index_out) *index_out = idx;
            return f;
        }
        if (!cur->base) break;
        cur = find_class(s, cur->base);
    }
    return 0;
}

static Method *find_method_hierarchy(Sema *s, ClassDecl *c, const char *name, const ParamList *params, int is_static, ClassDecl **owner_out, int *index_out)
{
    ClassDecl *cur = c;
    while (cur)
    {
        int idx = -1;
        Method *m = find_method(cur, name, params, is_static, &idx);
        if (m)
        {
            if (owner_out) *owner_out = cur;
            if (index_out) *index_out = idx;
            return m;
        }
        if (!cur->base) break;
        cur = find_class(s, cur->base);
    }
    return 0;
}

static Method *find_method_hierarchy_call(Sema *s, ClassDecl *c, const char *name, const ExprList *raw_args,
                                          const NameList *raw_names, const ParamList *arg_types, int is_static,
                                          ClassDecl **owner_out, int *index_out)
{
    ClassDecl *cur = c;
    while (cur)
    {
        for (int i = 0; i < cur->methods.count; i++)
        {
            Method *m = &cur->methods.items[i];
            if (m->is_constructor) continue;
            if (m->is_static != is_static) continue;
            if (kn_strcmp(m->name, name) != 0) continue;
            if (!call_args_match_params(s, 0, 0, raw_args, raw_names, arg_types, &m->params))
                continue;
            if (owner_out) *owner_out = cur;
            if (index_out) *index_out = i;
            return m;
        }
        if (!cur->base) break;
        cur = find_class(s, cur->base);
    }
    return 0;
}

static Method *find_method_named_arity_hierarchy(Sema *s, ClassDecl *c, const char *name,
                                                 int param_count, int is_static,
                                                 ClassDecl **owner_out, int *index_out)
{
    ClassDecl *cur = c;
    while (cur)
    {
        int idx = -1;
        Method *m = find_method_named_arity(cur, name, param_count, is_static, &idx);
        if (m)
        {
            if (owner_out) *owner_out = cur;
            if (index_out) *index_out = idx;
            return m;
        }
        if (!cur->base) break;
        cur = find_class(s, cur->base);
    }
    return 0;
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

static Method *find_property_getter_hierarchy(Sema *s, ClassDecl *c, const char *name,
                                              int is_static, ClassDecl **owner_out, int *index_out)
{
    return find_method_named_arity_hierarchy(s, c, property_getter_name(name), 0,
                                             is_static, owner_out, index_out);
}

static Method *find_property_setter_hierarchy(Sema *s, ClassDecl *c, const char *name,
                                              int is_static, ClassDecl **owner_out, int *index_out)
{
    return find_method_named_arity_hierarchy(s, c, property_setter_name(name), 1,
                                             is_static, owner_out, index_out);
}

static ClassDecl *class_from_type(Sema *s, Type t)
{
    if (t.kind != TY_CLASS) return 0;
    return find_class(s, t.name);
}

static bool class_is_derived_from(Sema *s, ClassDecl *c, ClassDecl *base)
{
    if (!c || !base) return false;
    ClassDecl *cur = c;
    int guard = 0;
    while (cur && guard++ < 256)
    {
        if (cur == base) return true;
        if (!cur->base) break;
        cur = find_class(s, cur->base);
    }
    return false;
}

static bool class_implements_interface(Sema *s, ClassDecl *c, const char *iface)
{
    if (!c || !iface) return false;
    for (int i = 0; i < c->interface_count; i++)
        if (kn_strcmp(c->interfaces[i], iface) == 0) return true;
    if (c->base)
    {
        ClassDecl *b = find_class(s, c->base);
        if (b && class_implements_interface(s, b, iface))
            return true;
    }
    return false;
}

static bool any_is_supported_target(Type t)
{
    if (t.kind == TY_ANY || t.kind == TY_BOOL || t.kind == TY_CHAR ||
        is_float_kind(t.kind) || t.kind == TY_STRING || is_integer_kind(t.kind))
        return true;
    if (t.kind == TY_ENUM)
        return true;
    return false;
}

static bool is_function_object_type(Sema *s, Type t)
{
    if (t.kind != TY_CLASS || !t.name) return false;
    if (kn_strcmp(t.name, "IO.Type.Object.Function") == 0)
        return true;
    ClassDecl *c = find_class(s, t.name);
    ClassDecl *base = find_class(s, "IO.Type.Object.Function");
    if (!c || !base) return false;
    return class_is_derived_from(s, c, base);
}

static bool is_block_object_type(Type t)
{
    return t.kind == TY_CLASS && t.name && kn_strcmp(t.name, "IO.Type.Object.Block") == 0;
}

static int block_record_ip(BlockRecordList *records, const char *name)
{
    if (!records || !name) return -1;
    for (int i = 0; i < records->count; i++)
    {
        if (kn_strcmp(records->items[i].name, name) == 0)
            return records->items[i].ip;
    }
    return -1;
}

static bool collect_qualified_parts(Expr *e, const char **parts, int *count, int max)
{
    if (!e || !parts || !count) return false;
    if (e->kind == EXPR_VAR)
    {
        if (*count < max) parts[(*count)++] = e->v.var.name;
        return true;
    }
    if (e->kind == EXPR_MEMBER)
    {
        if (!collect_qualified_parts(e->v.member.recv, parts, count, max))
            return false;
        if (*count < max) parts[(*count)++] = e->v.member.name;
        return true;
    }
    return false;
}

static const char *build_qualified_call_name(Sema *s, Expr *recv, const char *member)
{
    const char *parts[32];
    int count = 0;
    if (!collect_qualified_parts(recv, parts, &count, (int)(sizeof(parts) / sizeof(parts[0]))))
        return 0;
    if (count <= 0) return 0;
    if (scope_find(s->scope, parts[0]))
        return 0;
    size_t total = 0;
    for (int i = 0; i < count; i++)
        total += kn_strlen(parts[i]) + (i > 0 ? 1 : 0);
    if (member)
        total += kn_strlen(member) + 1;
    char *out = (char *)kn_malloc(total + 1);
    if (!out) return 0;
    size_t off = 0;
    for (int i = 0; i < count; i++)
    {
        if (i > 0) out[off++] = '.';
        size_t len = kn_strlen(parts[i]);
        kn_memcpy(out + off, parts[i], len);
        off += len;
    }
    if (member)
    {
        if (off > 0) out[off++] = '.';
        size_t len = kn_strlen(member);
        kn_memcpy(out + off, member, len);
        off += len;
    }
    out[off] = 0;
    return out;
}

static EnumDecl *resolve_enum_qual_expr(Sema *s, Expr *e)
{
    if (!s || !e) return 0;

    if (e->kind == EXPR_VAR)
        return find_visible_enum(s, e->v.var.name);

    {
        const char *qname = build_qualified_call_name(s, e, 0);
        if (qname)
            return find_visible_enum(s, qname);
    }
    return 0;
}

static bool type_assignable(Sema *s, Type dst, Type src)
{
    if (type_equal(dst, src)) return true;
    if (dst.kind == TY_ARRAY && src.kind == TY_ARRAY)
    {
        if (dst.elem != src.elem)
            return false;
        if ((dst.elem == TY_CLASS || dst.elem == TY_STRUCT || dst.elem == TY_ENUM) &&
            (dst.name || src.name) &&
            (!dst.name || !src.name || kn_strcmp(dst.name, src.name) != 0))
            return false;
        if (dst.array_len < 0)
            return true;
        if (src.array_len < 0)
            return false;
        return src.array_len <= dst.array_len;
    }
    if (src.kind == TY_NULL)
    {
        if (dst.kind == TY_ANY || dst.kind == TY_PTR || dst.kind == TY_CLASS || dst.kind == TY_STRING)
            return true;
        return false;
    }
    if (dst.kind == TY_ANY && src.kind != TY_VOID)
        return true;
    if (src.kind == TY_ANY)
        return false;
    if (dst.kind == TY_PTR && dst.elem == TY_CHAR && src.kind == TY_STRING)
        return true;
    if (dst.kind == TY_STRING && src.kind == TY_CHAR)
        return true;
    if (is_integer_kind(dst.kind) && is_integer_kind(src.kind))
        return true;
    if (dst.kind == TY_PTR && is_integer_kind(src.kind))
        return true;
    if (is_float_kind(dst.kind) && is_numeric_kind(src.kind))
        return true;
    if (dst.kind == TY_CLASS && src.kind == TY_CLASS)
    {
        ClassDecl *dst_c = find_class(s, dst.name);
        ClassDecl *src_c = find_class(s, src.name);
        InterfaceDecl *dst_i = find_interface(s, dst.name);
        if (dst_c && src_c && class_is_derived_from(s, src_c, dst_c))
            return true;
        if (dst_i && src_c && class_implements_interface(s, src_c, dst.name))
            return true;
    }
    return false;
}

static void check_byte_literal_range(Sema *s, Type dst, Expr *src)
{
    if (!s || !src) return;
    if (dst.kind != TY_BYTE) return;
    if (src->kind != EXPR_INT) return;
    if (src->v.int_val < 0 || src->v.int_val > 255)
        sema_error(s, src->line, src->col, 1, "Byte Overflow", "byte range is 0..255");
}

static void check_access(Sema *s, ClassDecl *owner, Access access, int line, int col)
{
    if (!owner) return;
    switch (access)
    {
    case ACC_PUBLIC:
        return;
    case ACC_PRIVATE:
        if (!s->current_class || s->current_class != owner)
            sema_error(s, line, col, 1, "Access Violation", "Private member not accessible");
        return;
    case ACC_PROTECTED:
        if (!s->current_class || !(s->current_class == owner || class_is_derived_from(s, s->current_class, owner)))
            sema_error(s, line, col, 1, "Access Violation", "Protected member not accessible");
        return;
    case ACC_INTERNAL:
        // Treat Internal as Public for now (single-module compilation).
        return;
    default:
        return;
    }
}

static const char *builtin_std_module_alias(const char *alias);

static const KnStdFunc *resolve_std_by_local(Sema *s, const char *local)
{
    for (int i = 0; i < s->imports->count; i++)
    {
        ImportMap *m = &s->imports->items[i];
        if (m->unit && m->unit[0] && s->current_unit && s->current_unit[0] &&
            kn_strcmp(m->unit, s->current_unit) != 0)
            continue;
        if (m->kind == IMPORT_SYMBOL && m->global && kn_strcmp(m->local, local) == 0)
            return kn_std_find(m->module, m->remote);
    }
    return 0;
}

static const char *find_scoped_symbol_module(Sema *s, const char *local)
{
    if (!s || !local) return 0;
    for (int i = 0; i < s->imports->count; i++)
    {
        ImportMap *m = &s->imports->items[i];
        if (m->unit && m->unit[0] && s->current_unit && s->current_unit[0] &&
            kn_strcmp(m->unit, s->current_unit) != 0)
            continue;
        if (m->kind != IMPORT_SYMBOL) continue;
        if (m->global) continue;
        if (kn_strcmp(m->local, local) != 0) continue;
        return m->module;
    }
    return 0;
}

static const char *resolve_std_symbol_in_module(Sema *s, const char *module, const char *local)
{
    if (!module || !local) return local;
    for (int i = 0; i < s->imports->count; i++)
    {
        ImportMap *m = &s->imports->items[i];
        if (m->unit && m->unit[0] && s->current_unit && s->current_unit[0] &&
            kn_strcmp(m->unit, s->current_unit) != 0)
            continue;
        if (m->kind != IMPORT_SYMBOL) continue;
        if (m->global) continue;
        if (kn_strcmp(m->module, module) != 0) continue;
        if (kn_strcmp(m->local, local) != 0) continue;
        return m->remote;
    }
    return local;
}

static bool std_module_is_always_available(const char *module)
{
    if (!module) return false;
    // Base IO.Type conversions/types are treated as language-level facilities.
    return kn_strcmp(module, "IO.Type") == 0 ||
           kn_strcmp(module, "IO.Collection.dict") == 0 ||
           kn_strcmp(module, "IO.Collection.list") == 0 ||
           kn_strcmp(module, "IO.Collection.set") == 0;
}

static bool std_module_accessible(Sema *s, const char *module, const char *name)
{
    if (!s || !module || !name) return false;
    if (std_module_is_always_available(module))
        return true;
    if (import_has_alias_for_module(s, module))
        return false;
    for (int i = 0; i < s->imports->count; i++)
    {
        ImportMap *m = &s->imports->items[i];
        if (m->unit && m->unit[0] && s->current_unit && s->current_unit[0] &&
            kn_strcmp(m->unit, s->current_unit) != 0)
            continue;
        if (!m->module || kn_strcmp(m->module, module) != 0)
            continue;
        if (m->kind == IMPORT_OPEN)
            return true;
        if (m->kind == IMPORT_SYMBOL && !m->global && m->local && kn_strcmp(m->local, name) == 0)
            return true;
    }
    return false;
}

static const KnStdFunc *resolve_std_by_module_member(Sema *s, const char *module, const char *name)
{
    if (!module || !name) return 0;
    if (!std_module_accessible(s, module, name))
        return 0;
    const char *resolved = resolve_std_symbol_in_module(s, module, name);
    return kn_std_find(module, resolved);
}

static const KnStdFunc *resolve_std_by_alias(Sema *s, const char *alias, const char *name)
{
    const char *builtin_module = builtin_std_module_alias(alias);
    if (builtin_module)
        return kn_std_find(builtin_module, name);
    for (int i = 0; i < s->imports->count; i++)
    {
        ImportMap *m = &s->imports->items[i];
        if (m->unit && m->unit[0] && s->current_unit && s->current_unit[0] &&
            kn_strcmp(m->unit, s->current_unit) != 0)
            continue;
        if (m->kind == IMPORT_ALIAS && kn_strcmp(m->local, alias) == 0)
        {
            const char *resolved = resolve_std_symbol_in_module(s, m->module, name);
            return kn_std_find(m->module, resolved);
        }
    }
    return 0;
}

static const KnStdFunc *resolve_std_by_qualified_name(Sema *s, const char *qname)
{
    if (!s || !qname) return 0;
    const char *last = 0;
    for (const char *p = qname; *p; p++)
        if (*p == '.') last = p;
    if (!last || last == qname) return 0;

    size_t mlen = (size_t)(last - qname);
    char *module = (char *)kn_malloc(mlen + 1);
    if (!module) return 0;
    kn_memcpy(module, qname, mlen);
    module[mlen] = 0;
    const char *name = last + 1;

    const KnStdFunc *std = 0;
    if (kn_std_module_has(module))
        std = resolve_std_by_module_member(s, module, name);
    if (!std)
        std = resolve_std_by_alias(s, module, name);

    kn_free(module);
    return std;
}

static const char *func_runtime_symbol_name(const Func *f)
{
    if (!f)
        return 0;
    if (f->is_delegate)
    {
        if (f->delegate_symbol_name && f->delegate_symbol_name[0])
            return f->delegate_symbol_name;
        return f->name;
    }
    if (f->is_extern)
    {
        if (f->extern_symbol && f->extern_symbol[0])
            return f->extern_symbol;
        return f->name;
    }
    return 0;
}

static const char *resolve_builtin_symbol_arg_name(Sema *s, Expr *arg)
{
    if (!s || !arg || arg->kind != EXPR_FUNC_REF || !arg->v.func_ref.qname ||
        arg->v.func_ref.builtin_id != KN_BUILTIN_NONE)
        return 0;
    {
        FuncSym *fs = functable_find(&s->funcs, arg->v.func_ref.qname);
        if (!fs || !fs->func)
            return 0;
        return func_runtime_symbol_name(fs->func);
    }
}

static const char *resolve_builtin_owner_arg_name(Sema *s, Expr *arg)
{
    if (!s || !arg || arg->kind != EXPR_FUNC_REF || !arg->v.func_ref.qname ||
        arg->v.func_ref.builtin_id != KN_BUILTIN_NONE)
        return 0;
    {
        FuncSym *fs = functable_find(&s->funcs, arg->v.func_ref.qname);
        if (!fs || !fs->func)
            return 0;
        return fs->func->qname ? fs->func->qname : fs->func->name;
    }
}

static Type sema_builtin_arg_with_expected(Sema *s, Expr *arg, Type expected, const char *detail,
                                           KnBuiltinKind kind, int arg_index)
{
    Type at = sema_expr_with_expected(s, arg, expected, detail);
    if (kind == KN_BUILTIN_IO_SYSTEM_GET_SYMBOL && arg_index == 1)
    {
        const char *symbol = resolve_builtin_symbol_arg_name(s, arg);
        if (symbol && symbol[0])
        {
            arg->kind = EXPR_STRING;
            arg->v.str_val.ptr = symbol;
            arg->v.str_val.len = (int)kn_strlen(symbol);
            arg->type = type_make(TY_STRING);
            at = arg->type;
        }
    }
    if ((kind == KN_BUILTIN_IO_META_OF ||
         kind == KN_BUILTIN_IO_META_HAS ||
         kind == KN_BUILTIN_IO_META_FIND) && arg_index == 0)
    {
        const char *owner = resolve_builtin_owner_arg_name(s, arg);
        if (owner && owner[0])
        {
            arg->kind = EXPR_STRING;
            arg->v.str_val.ptr = owner;
            arg->v.str_val.len = (int)kn_strlen(owner);
            arg->type = type_make(TY_STRING);
            at = arg->type;
        }
    }
    return at;
}

static Type lower_invoke_builtin_ref(Sema *s, Expr *invoke, KnBuiltinKind kind, ParamList *args)
{
    const KnStdFunc *std = kn_std_find_by_kind(kind);
    if (!std)
        sema_error(s, invoke->line, invoke->col, 1, "Unknown Function", "Builtin function reference not found");

    Type conv_dst = type_make(TY_UNKNOWN);
    int is_conv = builtin_conv_target(std->kind, &conv_dst);
    int is_vararg = builtin_is_vararg(std->kind);
    if (!is_vararg && invoke->v.invoke.args.count != std->param_count)
        sema_error_arg_count_named(s, invoke->line, invoke->col, 1, std->name, std->param_count);

    for (int i = 0; i < invoke->v.invoke.args.count; i++)
    {
        Expr *arg = invoke->v.invoke.args.items[i];
        Type pt = is_vararg ? std->params[0] : std->params[i];
        Type at = sema_builtin_arg_with_expected(s, arg, pt, "Default literal requires parameter type", kind, i);
        if (args && i < args->count)
        {
            args->items[i].type = at;
            args->items[i].name = "";
            args->items[i].src = s->src;
            args->items[i].materialized_default = 0;
        }
        if (is_conv && at.kind == TY_PTR && is_integer_kind(conv_dst.kind))
            check_ptr_safety(s, arg->line, arg->col, 1, "Safe function cannot cast pointer to integer");
        check_int_to_ptr_safety(s, pt, at, arg);
        if (!type_assignable(s, pt, at))
            sema_error(s, arg->line, arg->col, 1, "Type Mismatch", type_mismatch_detail(pt, at));
        check_byte_literal_range(s, pt, arg);
    }
    check_call_safety(s, std->safety, invoke->line, invoke->col, 1);

    ExprList call_args = invoke->v.invoke.args;
    NameList call_arg_names = invoke->v.invoke.arg_names;
    invoke->kind = EXPR_CALL;
    invoke->v.call.name = std->name ? std->name : "";
    invoke->v.call.args = call_args;
    invoke->v.call.arg_names = call_arg_names;
    invoke->v.call.builtin_id = (int)std->kind;
    invoke->type = std->ret_type;
    return invoke->type;
}

static void check_call_safety(Sema *s, Safety callee, int line, int col, int len)
{
    Safety caller = SAFETY_SAFE;
    if (s->current_method) caller = s->current_method->safety;
    else if (s->current) caller = s->current->safety;
    if (caller == SAFETY_SAFE && callee == SAFETY_UNSAFE)
        sema_error(s, line, col, len, "Unsafe Call", "Safe function cannot call Unsafe");
}

static void check_ptr_safety(Sema *s, int line, int col, int len, const char *detail)
{
    Safety caller = SAFETY_SAFE;
    if (s->current_method) caller = s->current_method->safety;
    else if (s->current) caller = s->current->safety;
    if (caller == SAFETY_SAFE)
        sema_error(s, line, col, len, "Unsafe Pointer", detail);
}

static void check_int_to_ptr_safety(Sema *s, Type dst, Type src, Expr *expr)
{
    if (!expr) return;
    if (dst.kind == TY_PTR && is_integer_kind(src.kind))
        check_ptr_safety(s, expr->line, expr->col, 1, "Safe function cannot cast integer to pointer");
}

static bool is_lvalue_expr(const Expr *e)
{
    if (!e) return false;
    if (e->kind == EXPR_VAR || e->kind == EXPR_MEMBER || e->kind == EXPR_INDEX)
        return true;
    if (e->kind == EXPR_UNARY && e->v.unary.op == TOK_STAR)
        return true;
    return false;
}

static Type sema_expr(Sema *s, Expr *e);
static void sema_stmt(Sema *s, Stmt *st);
static bool stmt_guarantees_return(Stmt *st);

static void rewrite_expr_to_property_call(Expr *e, Expr *recv, const char *method_name, Expr *value)
{
    ExprList args = {0};
    NameList arg_names = {0};
    TypeList type_args = {0};
    if (value)
        exprlist_push(&args, value);

    e->kind = EXPR_MEMBER_CALL;
    e->v.member_call.recv = recv;
    e->v.member_call.name = method_name;
    e->v.member_call.args = args;
    e->v.member_call.arg_names = arg_names;
    e->v.member_call.builtin_id = KN_BUILTIN_NONE;
    e->v.member_call.type_args = type_args;
    e->v.member_call.is_static = 0;
    e->v.member_call.static_type = 0;
    e->v.member_call.method_index = -1;
    e->v.member_call.method_owner = 0;
    e->v.member_call.is_qual_call = 0;
    e->v.member_call.qual_name = 0;
}

static int sema_try_current_class_field_expr(Sema *s, Expr *e, Type *out_type);

static int sema_try_current_class_property_expr(Sema *s, Expr *e, Type *out_type)
{
    if (out_type) *out_type = type_make(TY_UNKNOWN);
    if (!s || !e || e->kind != EXPR_VAR || !s->current_class || !e->v.var.name || name_has_dot(e->v.var.name))
        return 0;

    ClassDecl *owner = 0;
    int midx = -1;
    Method *m = find_property_getter_hierarchy(s, s->current_class, e->v.var.name,
                                               s->current_is_static, &owner, &midx);
    if (!m)
        return 0;

    Expr *recv = new_expr(s->current_is_static ? EXPR_VAR : EXPR_THIS, e->line, e->col);
    if (s->current_is_static)
        recv->v.var.name = owner ? owner->name : s->current_class->name;

    rewrite_expr_to_property_call(e, recv, property_getter_name(e->v.var.name), 0);
    if (out_type) *out_type = sema_expr(s, e);
    return 1;
}

static int sema_try_rewrite_property_member_expr(Sema *s, Expr *e, Type *out_type)
{
    if (out_type) *out_type = type_make(TY_UNKNOWN);
    if (!s || !e || e->kind != EXPR_MEMBER || !e->v.member.recv || !e->v.member.name)
        return 0;

    Expr *recv = e->v.member.recv;
    if (recv->kind == EXPR_VAR && !scope_find(s->scope, recv->v.var.name))
    {
        Type implicit_recv = type_make(TY_UNKNOWN);
        if (sema_try_current_class_field_expr(s, recv, &implicit_recv))
        {
            if (recv->kind == EXPR_VAR)
                return 0;
        }
        else
        {
            const char *qname = build_qualified_call_name(s, recv, e->v.member.name);
            if (qname)
            {
                const char *last = 0;
                for (const char *p = qname; *p; p++)
                    if (*p == '.') last = p;
                if (last && last != qname)
                {
                    size_t mlen = (size_t)(last - qname);
                    char *mod = (char *)kn_malloc(mlen + 1);
                    if (mod)
                    {
                        kn_memcpy(mod, qname, mlen);
                        mod[mlen] = 0;
                        const char *fname = last + 1;
                        ClassDecl *qc = find_visible_class(s, mod);
                        if (qc && find_property_getter_hierarchy(s, qc, fname, 1, 0, 0))
                        {
                            rewrite_expr_to_property_call(e, recv, property_getter_name(fname), 0);
                            if (out_type) *out_type = sema_expr(s, e);
                            return 1;
                        }
                    }
                }
            }
            return 0;
        }
    }

    /* If recv forms a qualified module/type chain (root not in local scope),
       skip — the caller's EXPR_MEMBER handler resolves enums, static fields, etc. */
    if (recv->kind == EXPR_MEMBER)
    {
        const char *q = build_qualified_call_name(s, recv, 0);
        if (q) return 0;
    }

    Type rt = sema_expr(s, recv);
    if (rt.kind != TY_CLASS)
        return 0;

    ClassDecl *c = find_class(s, rt.name);
    if (!c)
        return 0;
    if (!find_property_getter_hierarchy(s, c, e->v.member.name, 0, 0, 0))
        return 0;

    rewrite_expr_to_property_call(e, recv, property_getter_name(e->v.member.name), 0);
    if (out_type) *out_type = sema_expr(s, e);
    return 1;
}

static int sema_try_rewrite_property_assign_expr(Sema *s, Expr *e, Type *out_type)
{
    if (out_type) *out_type = type_make(TY_UNKNOWN);
    if (!s || !e || e->kind != EXPR_ASSIGN || !e->v.assign.target)
        return 0;

    Expr *value = e->v.assign.value;
    Expr *target = e->v.assign.target;

    if (target->kind == EXPR_VAR)
    {
        if (!s->current_class || !target->v.var.name || name_has_dot(target->v.var.name) ||
            scope_find(s->scope, target->v.var.name))
            return 0;
        if (!find_property_setter_hierarchy(s, s->current_class, target->v.var.name,
                                            s->current_is_static, 0, 0))
            return 0;

        Expr *recv = new_expr(s->current_is_static ? EXPR_VAR : EXPR_THIS, e->line, e->col);
        if (s->current_is_static)
            recv->v.var.name = s->current_class->name;
        rewrite_expr_to_property_call(e, recv, property_setter_name(target->v.var.name), value);
        if (out_type) *out_type = sema_expr(s, e);
        return 1;
    }

    if (target->kind != EXPR_MEMBER || !target->v.member.recv || !target->v.member.name)
        return 0;

    Expr *recv = target->v.member.recv;
    if (recv->kind == EXPR_VAR && !scope_find(s->scope, recv->v.var.name))
    {
        Type implicit_recv = type_make(TY_UNKNOWN);
        if (sema_try_current_class_field_expr(s, recv, &implicit_recv))
        {
            if (recv->kind == EXPR_VAR)
                return 0;
        }
        else
        {
            const char *qname = build_qualified_call_name(s, recv, target->v.member.name);
            if (qname)
            {
                const char *last = 0;
                for (const char *p = qname; *p; p++)
                    if (*p == '.') last = p;
                if (last && last != qname)
                {
                    size_t mlen = (size_t)(last - qname);
                    char *mod = (char *)kn_malloc(mlen + 1);
                    if (mod)
                    {
                        kn_memcpy(mod, qname, mlen);
                        mod[mlen] = 0;
                        const char *fname = last + 1;
                        ClassDecl *qc = find_visible_class(s, mod);
                        if (qc && find_property_setter_hierarchy(s, qc, fname, 1, 0, 0))
                        {
                            rewrite_expr_to_property_call(e, recv, property_setter_name(fname), value);
                            if (out_type) *out_type = sema_expr(s, e);
                            return 1;
                        }
                    }
                }
            }
            return 0;
        }
    }

    /* If recv forms a qualified module/type chain (root not in local scope),
       skip — the caller handles static/enum paths. */
    if (recv->kind == EXPR_MEMBER)
    {
        const char *q = build_qualified_call_name(s, recv, 0);
        if (q) return 0;
    }

    Type rt = sema_expr(s, recv);
    if (rt.kind != TY_CLASS)
        return 0;

    ClassDecl *c = find_class(s, rt.name);
    if (!c)
        return 0;
    if (!find_property_setter_hierarchy(s, c, target->v.member.name, 0, 0, 0))
        return 0;

    rewrite_expr_to_property_call(e, recv, property_setter_name(target->v.member.name), value);
    if (out_type) *out_type = sema_expr(s, e);
    return 1;
}

static int if_pattern_binding_active(Stmt *st)
{
    return st && st->kind == ST_IF && st->v.ifs.pattern_name && st->v.ifs.pattern_name[0] &&
           st->v.ifs.cond && st->v.ifs.cond->kind == EXPR_IS;
}

static void sema_if_then_branch(Sema *s, Stmt *st)
{
    if (!s || !st || !st->v.ifs.then_s)
        return;
    if (!if_pattern_binding_active(st))
    {
        sema_stmt(s, st->v.ifs.then_s);
        return;
    }

    Scope sc;
    sc.vars = 0;
    sc.parent = s->scope;
    s->scope = &sc;
    scope_add(s, s->scope, st->v.ifs.pattern_name, st->v.ifs.cond->v.is_expr.target, 0,
              st->v.ifs.pattern_line > 0 ? st->v.ifs.pattern_line : st->line,
              st->v.ifs.pattern_col > 0 ? st->v.ifs.pattern_col : st->col,
              st->v.ifs.cond->v.is_expr.expr);
    sema_stmt(s, st->v.ifs.then_s);
    warn_unused_scope(s, &sc);
    s->scope = sc.parent;
}

static int current_class_has_method_named(Sema *s, const char *name, int is_static)
{
    if (!s || !s->current_class || !name) return 0;
    for (ClassDecl *cur = s->current_class; cur; )
    {
        for (int i = 0; i < cur->methods.count; i++)
        {
            Method *m = &cur->methods.items[i];
            if (m->is_constructor) continue;
            if (m->is_static != is_static) continue;
            if (kn_strcmp(m->name, name) == 0)
                return 1;
        }
        if (!cur->base) break;
        cur = find_class(s, cur->base);
    }
    return 0;
}

static const char *builtin_std_module_alias(const char *alias)
{
    if (!alias) return 0;
    if (kn_strcmp(alias, "dict") == 0)
        return "IO.Collection.dict";
    if (kn_strcmp(alias, "list") == 0)
        return "IO.Collection.list";
    if (kn_strcmp(alias, "set") == 0)
        return "IO.Collection.set";
    return 0;
}

static int sema_try_current_class_field_expr(Sema *s, Expr *e, Type *out_type)
{
    if (out_type) *out_type = type_make(TY_UNKNOWN);
    if (!s || !e || e->kind != EXPR_VAR || !s->current_class || !e->v.var.name || name_has_dot(e->v.var.name))
        return 0;

    ClassDecl *owner = 0;
    int fidx = -1;
    Field *f = find_field_hierarchy(s, s->current_class, e->v.var.name, &owner, &fidx);
    if (!f)
        return sema_try_current_class_property_expr(s, e, out_type);

    check_access(s, owner ? owner : s->current_class, f->access, e->line, e->col);
    if (f->is_static)
    {
        Expr *recv = new_expr(EXPR_VAR, e->line, e->col);
        recv->v.var.name = owner ? owner->name : s->current_class->name;
        recv->type = type_class(owner ? class_qname(owner) : class_qname(s->current_class));
        const char *field_name = e->v.var.name;
        e->kind = EXPR_MEMBER;
        e->v.member.recv = recv;
        e->v.member.name = field_name;
        e->v.member.is_static = 1;
        e->v.member.static_type = owner ? class_qname(owner) : class_qname(s->current_class);
        e->v.member.field_index = fidx;
        e->v.member.field_owner = owner ? class_qname(owner) : class_qname(s->current_class);
        e->v.member.is_enum_const = 0;
        e->v.member.enum_value = 0;
        e->v.member.enum_type = 0;
        e->type = f->type;
        if (out_type) *out_type = e->type;
        return 1;
    }

    if (s->current_is_static)
    {
        sema_error(s, e->line, e->col, 1, "Invalid This", "This is not available in static context");
        e->type = type_make(TY_UNKNOWN);
        if (out_type) *out_type = e->type;
        return 1;
    }

    Expr *recv = new_expr(EXPR_THIS, e->line, e->col);
    const char *field_name = e->v.var.name;
    e->kind = EXPR_MEMBER;
    e->v.member.recv = recv;
    e->v.member.name = field_name;
    e->v.member.is_static = 0;
    e->v.member.static_type = 0;
    e->v.member.field_index = fidx;
    e->v.member.field_owner = owner ? class_qname(owner) : class_qname(s->current_class);
    e->v.member.is_enum_const = 0;
    e->v.member.enum_value = 0;
    e->v.member.enum_type = 0;
    e->type = f->type;
    if (out_type) *out_type = e->type;
    return 1;
}

static int sema_try_current_class_method_call(Sema *s, Expr *e, Type *out_type)
{
    if (out_type) *out_type = type_make(TY_UNKNOWN);
    if (!s || !e || e->kind != EXPR_CALL || !s->current_class || !e->v.call.name || name_has_dot(e->v.call.name))
        return 0;
    ParamList args = {0};
    args.count = e->v.call.args.count;
    args.cap = e->v.call.args.count;
    if (args.count > 0)
    {
        args.items = (Param *)kn_malloc(sizeof(Param) * (size_t)args.count);
        for (int i = 0; i < args.count; i++)
        {
            Type at = sema_expr(s, e->v.call.args.items[i]);
            args.items[i].type = at;
            args.items[i].name = "";
            args.items[i].src = s->src;
        }
    }

    ClassDecl *owner = 0;
    int midx = -1;
    Method *m = find_method_hierarchy_call(s, s->current_class, e->v.call.name,
                                           &e->v.call.args, &e->v.call.arg_names,
                                           &args, s->current_is_static, &owner, &midx);
    if (!m)
    {
        if (!current_class_has_method_named(s, e->v.call.name, s->current_is_static))
            return 0;
        if (e->v.call.type_args.count > 0)
            sema_error(s, e->line, e->col, 1, "Unsupported Generic Method", "Generic methods are not supported");
        sema_error(s, e->line, e->col, 1, "Unknown Method",
                   s->current_is_static ? "Static method not found" : "Method not found");
        e->type = type_make(TY_UNKNOWN);
        if (out_type) *out_type = e->type;
        return 1;
    }

    if (e->v.call.type_args.count > 0)
        sema_error(s, e->line, e->col, 1, "Unsupported Generic Method", "Generic methods are not supported");

    {
        const char **param_names = alloc_param_names_from_paramlist(&m->params);
        (void)prepare_call_args(s, e->line, e->col, &e->v.call.args, &e->v.call.arg_names,
                                &args, m->params.items, m->params.count,
                                param_names, m->name, 1, 1);
        retype_prepared_args(s, &e->v.call.args, &args, m->params.items, m->params.count,
                             "Default literal requires parameter type");
        if (param_names) kn_free((void *)param_names);
    }
    check_access(s, owner, m->access, e->line, e->col);
    check_call_safety(s, m->safety, e->line, e->col, 1);

    Expr *recv = new_expr(s->current_is_static ? EXPR_VAR : EXPR_THIS, e->line, e->col);
    if (s->current_is_static)
        recv->v.var.name = s->current_class->name;

    const char *name = e->v.call.name;
    ExprList call_args = e->v.call.args;
    NameList arg_names = e->v.call.arg_names;
    TypeList type_args = e->v.call.type_args;

    e->kind = EXPR_MEMBER_CALL;
    e->v.member_call.recv = recv;
    e->v.member_call.name = name;
    e->v.member_call.args = call_args;
    e->v.member_call.arg_names = arg_names;
    e->v.member_call.type_args = type_args;
    e->v.member_call.builtin_id = KN_BUILTIN_NONE;
    e->v.member_call.is_static = s->current_is_static ? 1 : 0;
    e->v.member_call.static_type = s->current_is_static ? class_qname(s->current_class) : 0;
    e->v.member_call.method_index = midx;
    e->v.member_call.method_owner = owner ? class_qname(owner) : class_qname(s->current_class);
    e->v.member_call.is_qual_call = 0;
    e->v.member_call.qual_name = 0;
    e->type = m->ret_type;
    if (out_type) *out_type = e->type;
    return 1;
}

static void lower_member_builtin_to_call(Expr *e)
{
    (void)e;
}

static Type sema_array_literal(Sema *s, Expr *e, Type expected_elem, bool has_expected)
{
    int count = e->v.array.items.count;
    Type elem = type_make(TY_UNKNOWN);
    if (has_expected) elem = expected_elem;
    int saw_untyped_default = 0;

    for (int i = 0; i < count; i++)
    {
        Expr *item = e->v.array.items.items[i];
        Type it;
        if (item && item->kind == EXPR_DEFAULT && has_expected)
        {
            bind_default_literal_type(s, item, expected_elem, "Default literal requires array element type");
            it = item->type;
        }
        else
        {
            it = sema_expr(s, item);
        }
        if (item && item->kind == EXPR_DEFAULT && !has_expected && elem.kind == TY_UNKNOWN)
        {
            saw_untyped_default = 1;
            continue;
        }
        if (item && item->kind == EXPR_DEFAULT && elem.kind != TY_UNKNOWN)
        {
            item->type = elem;
            it = item->type;
        }
        if (elem.kind == TY_UNKNOWN)
            elem = it;
        else if (!type_assignable(s, elem, it))
        {
            sema_error(s, item->line, item->col, 1, "Type Mismatch", type_mismatch_detail(elem, it));
        }
    }
    if (elem.kind == TY_UNKNOWN)
    {
        if (has_expected)
            elem = expected_elem;
    }
    if (elem.kind != TY_UNKNOWN)
    {
        for (int i = 0; i < count; i++)
            if (e->v.array.items.items[i] && e->v.array.items.items[i]->kind == EXPR_DEFAULT)
                e->v.array.items.items[i]->type = elem;
    }
    else if (saw_untyped_default)
    {
        sema_error(s, e->line, e->col, 1, "Default Literal", "Default literal requires array element type");
    }
    Type out = type_array(elem.kind, (int64_t)count);
    out.name = elem.name;
    e->type = out;
    return out;
}

static int type_is_collection_dict(Type t)
{
    return t.kind == TY_CLASS && t.name && kn_strcmp(t.name, "IO.Collection.dict") == 0;
}

static Type sema_dict_literal(Sema *s, Expr *e)
{
    int count = e->v.dict.keys.count;
    for (int i = 0; i < count; i++)
    {
        Expr *key = e->v.dict.keys.items[i];
        Expr *value = e->v.dict.values.items[i];
        Type kt = sema_expr_with_expected(s, key, type_make(TY_STRING), "Dict literal keys must be string");
        if (!type_assignable(s, type_make(TY_STRING), kt))
            sema_error(s, key->line, key->col, 1, "Type Mismatch", type_mismatch_detail(type_make(TY_STRING), kt));
        (void)sema_expr(s, value);
    }
    e->type = type_class("IO.Collection.dict");
    return e->type;
}

static Type sema_expr_with_expected(Sema *s, Expr *e, Type expected, const char *detail)
{
    if (!e)
        return type_make(TY_UNKNOWN);
    if (e->kind == EXPR_DEFAULT)
    {
        bind_default_literal_type(s, e, expected, detail);
        return e->type;
    }
    if (e->kind == EXPR_ARRAY && expected.kind == TY_ARRAY)
    {
        Type elem = type_make(expected.elem);
        elem.name = expected.name;
        return sema_array_literal(s, e, elem, true);
    }
    if (e->kind == EXPR_ARRAY && e->v.array.items.count == 0 && type_is_collection_dict(expected))
    {
        e->kind = EXPR_DICT;
        e->v.dict.keys.items = 0;
        e->v.dict.keys.count = 0;
        e->v.dict.keys.cap = 0;
        e->v.dict.values.items = 0;
        e->v.dict.values.count = 0;
        e->v.dict.values.cap = 0;
        e->type = expected;
        return expected;
    }
    return sema_expr(s, e);
}

static void retype_prepared_args(Sema *s, ExprList *args, ParamList *typed_args,
                                 const Param *params, int param_count, const char *detail)
{
    if (!s || !args || !typed_args || !params)
        return;
    int count = args->count;
    if (typed_args->count < count)
        count = typed_args->count;
    if (param_count < count)
        count = param_count;
    for (int i = 0; i < count; i++)
    {
        if (typed_args->items[i].materialized_default && args->items[i] && args->items[i]->type.kind != TY_UNKNOWN)
        {
            typed_args->items[i].type = args->items[i]->type;
            typed_args->items[i].name = "";
            continue;
        }
        typed_args->items[i].type = sema_expr_with_expected(s, args->items[i], params[i].type, detail);
        typed_args->items[i].name = "";
        typed_args->items[i].src = s->src;
        typed_args->items[i].materialized_default = 0;
    }
}

