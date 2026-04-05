// Internal sema fragment: statement analysis and entrypoints

{
    if (!st) return false;
    switch (st->kind)
    {
    case ST_RETURN:
    case ST_THROW:
        return true;
    case ST_BLOCK:
        for (int i = 0; i < st->v.block.stmts.count; i++)
        {
            if (stmt_guarantees_return(st->v.block.stmts.items[i]))
                return true;
        }
        return false;
    case ST_IF:
        if (st->v.ifs.is_const)
        {
            if (st->v.ifs.const_value)
                return stmt_guarantees_return(st->v.ifs.then_s);
            if (st->v.ifs.else_s)
                return stmt_guarantees_return(st->v.ifs.else_s);
            return false;
        }
        if (st->v.ifs.then_s && st->v.ifs.else_s)
            return stmt_guarantees_return(st->v.ifs.then_s) && stmt_guarantees_return(st->v.ifs.else_s);
        return false;
    case ST_SWITCH:
    {
        int has_default = 0;
        if (st->v.switchs.cases.count <= 0)
            return false;
        for (int i = 0; i < st->v.switchs.cases.count; i++)
        {
            StmtSwitchCase *c = &st->v.switchs.cases.items[i];
            if (c->is_default)
                has_default = 1;
            if (!stmt_guarantees_return(c->body))
                return false;
        }
        return has_default;
    }
    case ST_TRY:
        if (st->v.trys.try_block && st->v.trys.catch_block)
            return stmt_guarantees_return(st->v.trys.try_block) && stmt_guarantees_return(st->v.trys.catch_block);
        return false;
    default:
        return false;
    }
}

static bool expr_contains_unsafe_block(Expr *e)
{
    if (!e) return false;
    switch (e->kind)
    {
    case EXPR_CALL:
        for (int i = 0; i < e->v.call.args.count; i++)
            if (expr_contains_unsafe_block(e->v.call.args.items[i])) return true;
        return false;
    case EXPR_MEMBER_CALL:
        if (expr_contains_unsafe_block(e->v.member_call.recv)) return true;
        for (int i = 0; i < e->v.member_call.args.count; i++)
            if (expr_contains_unsafe_block(e->v.member_call.args.items[i])) return true;
        return false;
    case EXPR_INVOKE:
        if (expr_contains_unsafe_block(e->v.invoke.callee)) return true;
        for (int i = 0; i < e->v.invoke.args.count; i++)
            if (expr_contains_unsafe_block(e->v.invoke.args.items[i])) return true;
        return false;
    case EXPR_BINARY:
        return expr_contains_unsafe_block(e->v.binary.left) || expr_contains_unsafe_block(e->v.binary.right);
    case EXPR_UNARY:
        return expr_contains_unsafe_block(e->v.unary.expr);
    case EXPR_ARRAY:
        for (int i = 0; i < e->v.array.items.count; i++)
            if (expr_contains_unsafe_block(e->v.array.items.items[i])) return true;
        return false;
    case EXPR_DICT:
        for (int i = 0; i < e->v.dict.keys.count; i++)
        {
            if (expr_contains_unsafe_block(e->v.dict.keys.items[i])) return true;
            if (expr_contains_unsafe_block(e->v.dict.values.items[i])) return true;
        }
        return false;
    case EXPR_ASSIGN:
        return expr_contains_unsafe_block(e->v.assign.target) || expr_contains_unsafe_block(e->v.assign.value);
    case EXPR_IF:
        return expr_contains_unsafe_block(e->v.if_expr.cond) ||
               expr_contains_unsafe_block(e->v.if_expr.then_expr) ||
               expr_contains_unsafe_block(e->v.if_expr.else_expr);
    case EXPR_SWITCH:
        if (expr_contains_unsafe_block(e->v.switch_expr.value)) return true;
        for (int i = 0; i < e->v.switch_expr.cases.count; i++)
        {
            if (expr_contains_unsafe_block(e->v.switch_expr.cases.items[i].match)) return true;
            if (expr_contains_unsafe_block(e->v.switch_expr.cases.items[i].body)) return true;
        }
        return false;
    case EXPR_IS:
        return expr_contains_unsafe_block(e->v.is_expr.expr);
    case EXPR_MEMBER:
        return expr_contains_unsafe_block(e->v.member.recv);
    case EXPR_INDEX:
        return expr_contains_unsafe_block(e->v.index.recv) || expr_contains_unsafe_block(e->v.index.index);
    case EXPR_NEW:
        for (int i = 0; i < e->v.new_expr.args.count; i++)
            if (expr_contains_unsafe_block(e->v.new_expr.args.items[i])) return true;
        return false;
    case EXPR_CAST:
        return expr_contains_unsafe_block(e->v.cast.expr);
    case EXPR_ANON_FUNC:
        return stmt_contains_unsafe_block(e->v.anon_func.body);
    case EXPR_BLOCK_LITERAL:
        if (e->v.block_lit.safety == SAFETY_UNSAFE) return true;
        return stmt_contains_unsafe_block(e->v.block_lit.body);
    default:
        return false;
    }
}

static bool stmt_contains_unsafe_block(Stmt *st)
{
    if (!st) return false;
    switch (st->kind)
    {
    case ST_BLOCK:
        for (int i = 0; i < st->v.block.stmts.count; i++)
            if (stmt_contains_unsafe_block(st->v.block.stmts.items[i])) return true;
        return false;
    case ST_VAR:
        return expr_contains_unsafe_block(st->v.var.init);
    case ST_ASSIGN:
        return expr_contains_unsafe_block(st->v.assign.value);
    case ST_EXPR:
        return expr_contains_unsafe_block(st->v.expr.expr);
    case ST_IF:
        return expr_contains_unsafe_block(st->v.ifs.cond) ||
               stmt_contains_unsafe_block(st->v.ifs.then_s) ||
               stmt_contains_unsafe_block(st->v.ifs.else_s);
    case ST_SWITCH:
        if (expr_contains_unsafe_block(st->v.switchs.value)) return true;
        for (int i = 0; i < st->v.switchs.cases.count; i++)
        {
            if (expr_contains_unsafe_block(st->v.switchs.cases.items[i].match)) return true;
            if (stmt_contains_unsafe_block(st->v.switchs.cases.items[i].body)) return true;
        }
        return false;
    case ST_WHILE:
        return expr_contains_unsafe_block(st->v.whiles.cond) ||
               stmt_contains_unsafe_block(st->v.whiles.body);
    case ST_FOR:
        return stmt_contains_unsafe_block(st->v.fors.init) ||
               expr_contains_unsafe_block(st->v.fors.cond) ||
               expr_contains_unsafe_block(st->v.fors.post) ||
               stmt_contains_unsafe_block(st->v.fors.body);
    case ST_RETURN:
        return expr_contains_unsafe_block(st->v.ret.expr);
    case ST_TRY:
        return stmt_contains_unsafe_block(st->v.trys.try_block) ||
               stmt_contains_unsafe_block(st->v.trys.catch_block);
    case ST_THROW:
        return expr_contains_unsafe_block(st->v.throws.expr);
    case ST_BLOCK_JUMP:
        return expr_contains_unsafe_block(st->v.jump.target);
    default:
        return false;
    }
}

static void sema_stmt(Sema *s, Stmt *st)
{
    if (!st) return;
    switch (st->kind)
    {
    case ST_BLOCK:
    {
        Scope sc;
        sc.vars = 0;
        sc.parent = s->scope;
        s->scope = &sc;
        int terminated = 0;
        for (int i = 0; i < st->v.block.stmts.count; i++)
        {
            Stmt *cur_st = st->v.block.stmts.items[i];
            if (terminated && cur_st && !stmt_contains_unsafe_block(cur_st))
                sema_warn_unreachable(s, cur_st->line, cur_st->col, 1,
                                      "statement after a control-flow terminator");
            sema_stmt(s, cur_st);
            if (!terminated && stmt_guarantees_return(cur_st))
                terminated = 1;
        }
        warn_unused_scope(s, &sc);
        s->scope = sc.parent;
        return;
    }
    case ST_VAR:
    {
        Type t = st->v.var.type;
        if (st->v.var.is_const && !st->v.var.init)
            sema_error(s, st->line, st->col, 1, "Const Requires Init", "Const variable must have initializer");
        if (t.kind == TY_UNKNOWN)
        {
            if (!st->v.var.init)
                sema_error(s, st->line, st->col, 1, "Type Inference Failed", "Var requires initializer");
            Type it = sema_expr(s, st->v.var.init);
            if (it.kind == TY_NULL)
                t = type_make(TY_ANY);
            else
            t = it;
            if (t.kind == TY_ARRAY && t.elem == TY_UNKNOWN)
                sema_error(s, st->line, st->col, 1, "Type Inference Failed", "Cannot infer element type from empty array");
        }
        else if (t.kind == TY_ARRAY && t.elem == TY_UNKNOWN)
        {
            if (!st->v.var.init || st->v.var.init->kind != EXPR_ARRAY)
                sema_error(s, st->line, st->col, 1, "Type Inference Failed", "Array element type requires initializer");
            Type it = sema_array_literal(s, st->v.var.init, type_make(TY_UNKNOWN), false);
            t.elem = it.elem;
            t.name = it.name;
            if (t.elem == TY_UNKNOWN)
                sema_error(s, st->line, st->col, 1, "Type Inference Failed", "Cannot infer element type from empty array");
        }
        validate_type_ref(s, &t, st->line, st->col, 0);

        if (st->v.var.init)
        {
            if (st->v.var.init->kind == EXPR_ARRAY && t.kind == TY_ARRAY)
            {
                Type elem = type_make(t.elem);
                elem.name = t.name;
                Type lit = sema_array_literal(s, st->v.var.init, elem, true);
                if (t.array_len >= 0 && lit.array_len > t.array_len)
                    sema_error(s, st->line, st->col, 1, "Array Length", "Initializer has too many elements");
            }
            else
            {
                bind_default_literal_type(s, st->v.var.init, t, "Default literal requires variable type");
                Type it = sema_expr_with_expected(s, st->v.var.init, t, "Default literal requires variable type");
                check_int_to_ptr_safety(s, t, it, st->v.var.init);
                if (!type_assignable(s, t, it))
                    sema_error(s, st->v.var.init->line, st->v.var.init->col, 1, "Type Mismatch", type_mismatch_detail(t, it));
                check_byte_literal_range(s, t, st->v.var.init);
            }
        }

        st->v.var.type = t;
        Expr *origin = st->v.var.init;
        if ((is_function_object_type(s, t) || is_block_object_type(t)) &&
            origin && origin->kind == EXPR_VAR)
        {
            VarSym *srcv = scope_find(s->scope, origin->v.var.name);
            if (srcv && srcv->origin)
                origin = srcv->origin;
        }
        int nline = st->v.var.name_line > 0 ? st->v.var.name_line : st->line;
        int ncol = st->v.var.name_col > 0 ? st->v.var.name_col : st->col;
        scope_add(s, s->scope, st->v.var.name, t, st->v.var.is_const, nline, ncol, origin);
        return;
    }
    case ST_ASSIGN:
    {
        VarSym *v = scope_find(s->scope, st->v.assign.name);
        if (!v)
        {
            sema_error(s, st->line, st->col, 1, "Unknown Variable", "Assignment target not found");
            return;
        }
        if (v->is_const)
            sema_error(s, st->line, st->col, 1, "Const Assignment", "Cannot assign to const variable");
        Type it = sema_expr_with_expected(s, st->v.assign.value, v->type, "Default literal requires assignment target type");
        check_int_to_ptr_safety(s, v->type, it, st->v.assign.value);
        if (!type_assignable(s, v->type, it))
            sema_error(s, st->v.assign.value->line, st->v.assign.value->col, 1, "Type Mismatch", type_mismatch_detail(v->type, it));
        check_byte_literal_range(s, v->type, st->v.assign.value);
        if (is_function_object_type(s, v->type) || is_block_object_type(v->type))
        {
            Expr *origin = st->v.assign.value;
            if (origin && origin->kind == EXPR_VAR)
            {
                VarSym *srcv = scope_find(s->scope, origin->v.var.name);
                if (srcv && srcv->origin)
                    origin = srcv->origin;
            }
            v->origin = origin;
        }
        if (is_async_task_type(v->type))
            v->async_result_type = st->v.assign.value ? st->v.assign.value->async_result_type : type_make(TY_UNKNOWN);
        return;
    }
    case ST_EXPR:
        (void)sema_expr(s, st->v.expr.expr);
        if (st->v.expr.expr && st->v.expr.expr->kind == EXPR_DEFAULT && st->v.expr.expr->type.kind == TY_UNKNOWN)
            sema_error(s, st->v.expr.expr->line, st->v.expr.expr->col, 1, "Default Literal",
                       "Default literal requires a target type");
        return;
    case ST_BLOCK_RECORD:
        return;
    case ST_BLOCK_JUMP:
    {
        Type jt = sema_expr(s, st->v.jump.target);
        if (!is_integer_kind(jt.kind))
            sema_error_type_must_be_named(s, st->line, st->col, 1, "Jump", "target", "integer ip");
        return;
    }
    case ST_IF:
    {
        Type ct = sema_expr(s, st->v.ifs.cond);
        if (!(ct.kind == TY_BOOL || is_integer_kind(ct.kind)))
            sema_error(s, st->line, st->col, 1, "Type Mismatch", "If condition must be bool or integer");
        int64_t cval = 0;
        if (eval_const_expr(st->v.ifs.cond, &cval))
        {
            st->v.ifs.is_const = 1;
            st->v.ifs.const_value = cval != 0;
            if (st->v.ifs.const_value)
            {
                if (st->v.ifs.else_s && !stmt_contains_unsafe_block(st->v.ifs.else_s))
                    sema_warn_unreachable(s, st->v.ifs.else_s->line, st->v.ifs.else_s->col, 1,
                                          "else branch is never taken (condition is always true)");
                sema_if_then_branch(s, st);
            }
            else if (st->v.ifs.else_s)
            {
                if (st->v.ifs.then_s && !stmt_contains_unsafe_block(st->v.ifs.then_s))
                    sema_warn_unreachable(s, st->v.ifs.then_s->line, st->v.ifs.then_s->col, 1,
                                          "then branch is never taken (condition is always false)");
                sema_stmt(s, st->v.ifs.else_s);
            }
            else if (st->v.ifs.then_s && !stmt_contains_unsafe_block(st->v.ifs.then_s))
            {
                sema_warn_unreachable(s, st->v.ifs.then_s->line, st->v.ifs.then_s->col, 1,
                                      "then branch is never taken (condition is always false)");
            }
            return;
        }
        sema_if_then_branch(s, st);
        if (st->v.ifs.else_s) sema_stmt(s, st->v.ifs.else_s);
        return;
    }
    case ST_SWITCH:
    {
        Type vt = sema_expr(s, st->v.switchs.value);
        int default_count = 0;
        for (int i = 0; i < st->v.switchs.cases.count; i++)
        {
            StmtSwitchCase *c = &st->v.switchs.cases.items[i];
            if (c->is_default)
            {
                default_count++;
            }
            else if (c->match)
            {
                Type mt = sema_expr(s, c->match);
                if (!types_support_switch_match(vt, mt))
                    sema_error(s, c->match->line, c->match->col, 1, "Type Mismatch",
                               "Switch case value is not comparable to Switch value");
            }
            sema_stmt(s, c->body);
        }
        if (default_count > 1)
            sema_error(s, st->line, st->col, 1, "Duplicate Default Case", "Switch can only have one Case(default)");
        return;
    }
    case ST_WHILE:
    {
        Type ct = sema_expr(s, st->v.whiles.cond);
        if (!(ct.kind == TY_BOOL || is_integer_kind(ct.kind)))
            sema_error(s, st->line, st->col, 1, "Type Mismatch", "While condition must be bool or integer");
        s->loop_depth++;
        sema_stmt(s, st->v.whiles.body);
        s->loop_depth--;
        return;
    }
    case ST_FOR:
    {
        if (st->v.fors.init) sema_stmt(s, st->v.fors.init);
        if (st->v.fors.cond)
        {
            Type ct = sema_expr(s, st->v.fors.cond);
            if (!(ct.kind == TY_BOOL || is_integer_kind(ct.kind)))
                sema_error(s, st->line, st->col, 1, "Type Mismatch", "For condition must be bool or integer");
        }
        if (st->v.fors.post) (void)sema_expr(s, st->v.fors.post);
        s->loop_depth++;
        sema_stmt(s, st->v.fors.body);
        s->loop_depth--;
        return;
    }
    case ST_TRY:
    {
        if (sema_is_freestanding_core())
            sema_error(s, st->line, st->col, 1, "Freestanding Core", "Try/Catch is not available in freestanding core");
        s->try_depth++;
        if (st->v.trys.try_block)
            sema_stmt(s, st->v.trys.try_block);

        Scope sc;
        sc.vars = 0;
        sc.parent = s->scope;
        s->scope = &sc;
        if (st->v.trys.has_param)
        {
            Type ct = st->v.trys.catch_type;
            validate_type_ref(s, &ct, st->line, st->col, 0);
            if (!(ct.kind == TY_STRING || is_error_type(ct)))
                sema_error(s, st->line, st->col, 1, "Invalid Catch", "Catch parameter must be string or IO.Error");
            scope_add(s, &sc, st->v.trys.catch_name, ct, 0, st->line, st->col, 0);
        }
        if (st->v.trys.catch_block)
            sema_stmt(s, st->v.trys.catch_block);
        warn_unused_scope(s, &sc);
        s->scope = sc.parent;
        s->try_depth--;
        return;
    }
    case ST_BREAK:
    case ST_CONTINUE:
        if (s->loop_depth <= 0)
            sema_error(s, st->line, st->col, 1, "Invalid Control Flow", "break/continue not in loop");
        return;
    case ST_THROW:
    {
        if (sema_is_freestanding_core())
            sema_error(s, st->line, st->col, 1, "Freestanding Core", "Throw is not available in freestanding core");
        if (!s->current && !s->current_method)
            sema_error(s, st->line, st->col, 1, "Throw Outside Function", "Throw not allowed here");
        if (!st->v.throws.expr)
            sema_error(s, st->line, st->col, 1, "Invalid Throw", "Throw requires an expression");
        Type et = type_make(TY_UNKNOWN);
        if (st->v.throws.expr)
            et = sema_expr(s, st->v.throws.expr);
        if (!(et.kind == TY_STRING || is_error_type(et)))
            sema_error(s, st->line, st->col, 1, "Invalid Throw", "Throw expression must be string or IO.Error");
        return;
    }
    case ST_RETURN:
    {
        if (s->in_block_literal && s->block_records)
            sema_error(s, st->line, st->col, 1, "Invalid Return", "Return is not allowed inside Block");
        Func *cur = s->current;
        Method *curm = s->current_method;
        if (!cur && !curm)
            sema_error(s, st->line, st->col, 1, "Return Outside Function", "Return not allowed here");
        Type ret_type = curm ? curm->ret_type : cur->ret_type;
        if (ret_type.kind == TY_VOID)
        {
            if (st->v.ret.expr)
                sema_error(s, st->line, st->col, 1, "Return Type", "Void function cannot return a value");
            return;
        }
        if (!st->v.ret.expr)
        {
            sema_error(s, st->line, st->col, 1, "Return Type", "Non-void function must return a value");
            return;
        }
        Type rt = sema_expr_with_expected(s, st->v.ret.expr, ret_type, "Default literal requires return type");
        if (!type_assignable(s, ret_type, rt))
            sema_error(s, st->v.ret.expr->line, st->v.ret.expr->col, 1, "Return Type", type_mismatch_detail(ret_type, rt));
        check_byte_literal_range(s, ret_type, st->v.ret.expr);
        return;
    }
    default:
        break;
    }
}

static void validate_imports(Sema *s)
{
    for (int i = 0; i < s->imports->count; i++)
    {
        ImportMap *m = &s->imports->items[i];
        if (!kn_std_module_has(m->module))
            continue;
        if (m->kind == IMPORT_SYMBOL)
        {
            if (!kn_std_find(m->module, m->remote))
                sema_error(s, m->line, m->col, 1, "Unknown Symbol", "Symbol not found in module");
        }
    }
}

static void validate_entry(Sema *s, const KnSemaOptions *opts)
{
    if (!opts || !opts->require_entry)
        return;

    const char *entry_name = (opts->entry_name && opts->entry_name[0])
        ? opts->entry_name
        : (opts->env_kind == KN_ENV_FREESTANDING ? "KMain" : "Main");
    Func *entry_func = 0;
    int entry_count = 0;
    for (int i = 0; i < s->funcs.count; i++)
    {
        Func *f = s->funcs.items[i].func;
        if (!f) continue;
        if (func_is_generic_template(f) || func_is_generic_instance(f))
            continue;
        if (f && kn_strcmp(f->name, entry_name) == 0)
        {
            entry_func = f;
            entry_count++;
        }
    }
    if (entry_count == 0)
    {
        if (opts->env_kind == KN_ENV_FREESTANDING)
            sema_error(s, 1, 1, 1, "Missing Entry", "Freestanding entry function not found");
        else
            sema_error(s, 1, 1, 1, "Missing Entry", "Main function not found");
    }
    if (entry_count > 1)
        sema_error(s, 1, 1, 1, "Entry Ambiguous", "Multiple entry functions found");

    Func *f = entry_func;
    if (f && f->src) s->src = f->src;
    int decl_line = f ? (f->line > 0 ? f->line : 1) : 1;
    int decl_col = f ? (f->col > 0 ? f->col : 1) : 1;
    if (f && !f->is_static)
        sema_error(s, decl_line, decl_col, 1, "Entry Signature", "Entry point must be Static");

    if (opts->env_kind == KN_ENV_FREESTANDING)
    {
        if (f && f->ret_type.kind != TY_VOID)
            sema_error(s, decl_line, decl_col, 1, "Entry Signature", "Freestanding entry must return void");
        if (f && !(f->params.count == 0 || f->params.count == 1))
            sema_error(s, decl_line, decl_col, 1, "Entry Signature", "Freestanding entry must take zero or one parameter");
        if (f && f->params.count == 1)
        {
            Type t = f->params.items[0].type;
            if (t.kind != TY_PTR || t.ptr_depth <= 0)
                sema_error(s, decl_line, decl_col, 1, "Entry Signature", "Freestanding entry parameter must be a pointer");
        }
    }
    else
    {
        if (f && f->ret_type.kind != TY_INT)
            sema_error(s, decl_line, decl_col, 1, "Entry Signature", "Main must return int");
        if (f && !(f->params.count == 0 || f->params.count == 1))
            sema_error(s, decl_line, decl_col, 1, "Entry Signature", "Main must take zero or one parameter");
        if (f && f->params.count == 1)
        {
            Type t = f->params.items[0].type;
            if (!(t.kind == TY_ARRAY && t.elem == TY_STRING))
                sema_error(s, decl_line, decl_col, 1, "Entry Signature", "Main parameter must be string[]");
        }

        if (f && f->safety == SAFETY_UNSAFE)
        {
            kn_diag_warn(f->src, KN_STAGE_SEMA, 1, decl_line, decl_col, 6,
                         "Entry Unsafe",
                         "Entry point is marked as Unsafe. This disables safety guarantees for the entire program. "
                         "Consider using Safe Main and isolating Unsafe blocks locally.");
        }
    }
}

static int metas_has_name(MetaList *metas, const char *name)
{
    if (!metas || !name) return 0;
    for (int i = 0; i < metas->count; i++)
    {
        MetaDecl *m = &metas->items[i];
        if (m->qname && kn_strcmp(m->qname, name) == 0)
            return 1;
        if (m->name && kn_strcmp(m->name, name) == 0)
            return 1;
    }
    return 0;
}

static Param builtin_meta_param(const char *name)
{
    Param p;
    kn_memset(&p, 0, sizeof(p));
    p.name = name;
    p.type = type_make(TY_STRING);
    return p;
}

static void inject_builtin_meta_decl(MetaList *metas, const char *name, const char *param_name)
{
    if (!metas || !name || metas_has_name(metas, name))
        return;

    MetaDecl m;
    kn_memset(&m, 0, sizeof(m));
    m.name = name;
    m.qname = name;
    m.target_mask = KN_META_TARGET_FUNCTION |
                    KN_META_TARGET_EXTERN_FUNCTION |
                    KN_META_TARGET_DELEGATE_FUNCTION |
                    KN_META_TARGET_CLASS |
                    KN_META_TARGET_STRUCT |
                    KN_META_TARGET_ENUM |
                    KN_META_TARGET_INTERFACE |
                    KN_META_TARGET_METHOD |
                    KN_META_TARGET_FIELD;
    m.keep_kind = KN_META_KEEP_RUNTIME;
    m.repeatable = 0;
    if (param_name && param_name[0])
        paramlist_push(&m.params, builtin_meta_param(param_name));
    metalist_push(metas, m);
}

static void inject_builtin_meta_decls(MetaList *metas)
{
    inject_builtin_meta_decl(metas, "Docs", "text");
    inject_builtin_meta_decl(metas, "Deprecated", "message");
    inject_builtin_meta_decl(metas, "Since", "version");
    inject_builtin_meta_decl(metas, "Example", "code");
    inject_builtin_meta_decl(metas, "Experimental", "message");
}

int kn_sema_check(const KnSource *src, MetaList *metas, FuncList *funcs, ImportList *imports, ClassList *classes,
                  InterfaceList *interfaces, StructList *structs, EnumList *enums, StmtList *globals,
                  const KnSemaOptions *opts)
{
    if (!metas || !funcs || !imports || !classes || !interfaces || !structs || !enums || !globals) return -1;

    inject_builtin_meta_decls(metas);

    Sema s;
    kn_memset(&s, 0, sizeof(s));
    s.src = src;
    s.metas = metas;
    s.imports = imports;
    s.classes = classes;
    s.interfaces = interfaces;
    s.structs = structs;
    s.enums = enums;
    s.func_list = funcs;
    if (opts)
    {
        s.target_os = opts->target_os;
        s.target_arch = opts->target_arch;
        s.target_env = opts->target_env;
        s.target_pointer_bits = opts->target_pointer_bits;
        s.target_path_separator = opts->target_path_separator;
        s.target_new_line = opts->target_new_line;
        s.target_exe_suffix = opts->target_exe_suffix;
        s.target_object_suffix = opts->target_object_suffix;
        s.target_dynamic_library_suffix = opts->target_dynamic_library_suffix;
        s.target_static_library_suffix = opts->target_static_library_suffix;
        s.host_os = opts->host_os;
        s.host_arch = opts->host_arch;
        s.host_env = opts->host_env;
        s.host_pointer_bits = opts->host_pointer_bits;
        s.host_path_separator = opts->host_path_separator;
        s.host_new_line = opts->host_new_line;
        s.host_exe_suffix = opts->host_exe_suffix;
        s.host_object_suffix = opts->host_object_suffix;
        s.host_dynamic_library_suffix = opts->host_dynamic_library_suffix;
        s.host_static_library_suffix = opts->host_static_library_suffix;
        s.runtime_backend = opts->runtime_backend;
    }

    // register functions
    for (int i = 0; i < funcs->count; i++)
    {
        Func *f = &funcs->items[i];
        if (f->qname && functable_find(&s.funcs, f->qname))
            sema_error(&s, f->body ? f->body->line : 1, f->body ? f->body->col : 1, 1, "Duplicate Function", "Function already defined");
        FuncSym sym;
        sym.qname = f->qname ? f->qname : f->name;
        sym.func = f;
        functable_add(&s.funcs, sym);
    }

    for (int i = 0; i < metas->count; i++)
    {
        MetaDecl *m = &metas->items[i];
        for (int j = 0; j < i; j++)
        {
            MetaDecl *prev = &metas->items[j];
            if (m->qname && prev->qname && kn_strcmp(m->qname, prev->qname) == 0)
                sema_error(&s, m->line, m->col, 1, "Duplicate Meta", "Meta already defined");
        }
        validate_meta_decl(&s, m);
    }

    // duplicate class/interface/struct/enum names
    for (int i = 0; i < classes->count; i++)
    {
        ClassDecl *c = &classes->items[i];
        for (int j = 0; j < i; j++)
            if (kn_strcmp(class_qname(&classes->items[j]), class_qname(c)) == 0)
                sema_error(&s, c->line, c->col, 1, "Duplicate Class", "Class already defined");
        for (int j = 0; j < interfaces->count; j++)
            if (kn_strcmp(interface_qname(&interfaces->items[j]), class_qname(c)) == 0)
                sema_error(&s, c->line, c->col, 1, "Name Conflict", "Class conflicts with interface name");
        for (int j = 0; j < structs->count; j++)
            if (kn_strcmp(struct_qname(&structs->items[j]), class_qname(c)) == 0)
                sema_error(&s, c->line, c->col, 1, "Name Conflict", "Class conflicts with struct name");
        for (int j = 0; j < enums->count; j++)
            if (kn_strcmp(enum_qname(&enums->items[j]), class_qname(c)) == 0)
                sema_error(&s, c->line, c->col, 1, "Name Conflict", "Class conflicts with enum name");
    }
    for (int i = 0; i < interfaces->count; i++)
    {
        InterfaceDecl *it = &interfaces->items[i];
        for (int j = 0; j < i; j++)
            if (kn_strcmp(interface_qname(&interfaces->items[j]), interface_qname(it)) == 0)
                sema_error(&s, it->line, it->col, 1, "Duplicate Interface", "Interface already defined");
        for (int j = 0; j < structs->count; j++)
            if (kn_strcmp(struct_qname(&structs->items[j]), interface_qname(it)) == 0)
                sema_error(&s, it->line, it->col, 1, "Name Conflict", "Interface conflicts with struct name");
        for (int j = 0; j < enums->count; j++)
            if (kn_strcmp(enum_qname(&enums->items[j]), interface_qname(it)) == 0)
                sema_error(&s, it->line, it->col, 1, "Name Conflict", "Interface conflicts with enum name");
    }
    for (int i = 0; i < structs->count; i++)
    {
        StructDecl *st = &structs->items[i];
        for (int j = 0; j < i; j++)
            if (kn_strcmp(struct_qname(&structs->items[j]), struct_qname(st)) == 0)
                sema_error(&s, st->line, st->col, 1, "Duplicate Struct", "Struct already defined");
        for (int j = 0; j < enums->count; j++)
            if (kn_strcmp(enum_qname(&enums->items[j]), struct_qname(st)) == 0)
                sema_error(&s, st->line, st->col, 1, "Name Conflict", "Struct conflicts with enum name");
    }
    for (int i = 0; i < enums->count; i++)
    {
        EnumDecl *en = &enums->items[i];
        for (int j = 0; j < i; j++)
            if (kn_strcmp(enum_qname(&enums->items[j]), enum_qname(en)) == 0)
                sema_error(&s, en->line, en->col, 1, "Duplicate Enum", "Enum already defined");
    }

    validate_imports(&s);

    apply_default_object_base(&s);

    for (int i = 0; i < interfaces->count; i++)
        validate_interface_decl(&s, &interfaces->items[i]);

    for (int i = 0; i < enums->count; i++)
        validate_enum_decl(&s, &enums->items[i]);

    for (int i = 0; i < structs->count; i++)
        validate_struct_decl(&s, &structs->items[i]);

    for (int i = 0; i < classes->count; i++)
        validate_class_decl(&s, &classes->items[i]);

    if (opts && opts->require_entry)
        validate_entry(&s, opts);

    for (int i = 0; i < funcs->count; i++)
    {
        Func *f = &funcs->items[i];
        if (func_is_generic_template(f))
            continue;
        s.src = f->src ? f->src : src;
        s.current = f;
        s.current_method = 0;
        s.current_class = 0;
        s.current_is_static = 1;
        s.current_unit = f->unit;
        s.scope = 0;
        validate_func_attrs(&s, f);
        if (f->is_async && f->is_extern)
            sema_error(&s, f->line > 0 ? f->line : 1, f->col > 0 ? f->col : 1, 1, "Invalid Async Function", "Extern function cannot be Async");
        if (f->is_async && f->is_delegate)
            sema_error(&s, f->line > 0 ? f->line : 1, f->col > 0 ? f->col : 1, 1, "Invalid Async Function", "Delegate function cannot be Async");
        validate_type_ref(&s, &f->ret_type, f->line > 0 ? f->line : 1, f->col > 0 ? f->col : 1, 1);
        validate_param_defaults(&s, &f->params, f->line > 0 ? f->line : 1, f->col > 0 ? f->col : 1);
    }

    Scope global_scope;
    global_scope.vars = 0;
    global_scope.parent = 0;
    s.scope = &global_scope;
    s.current = 0;
    s.current_method = 0;
    s.current_class = 0;
    s.current_is_static = 0;
    s.current_unit = 0;
    for (int i = 0; i < globals->count; i++)
    {
        Stmt *gv = globals->items[i];
        if (gv && gv->kind == ST_VAR)
        {
            s.src = gv->v.var.src ? gv->v.var.src : src;
            s.current_unit = gv->v.var.unit;
        }
        else
        {
            s.src = src;
            s.current_unit = 0;
        }
        if (!gv || gv->kind != ST_VAR)
            sema_error(&s, gv ? gv->line : 1, gv ? gv->col : 1, 1, "Invalid Global", "Only variable declarations are allowed at global scope");
        if (gv)
            sema_stmt(&s, gv);
    }
    s.src = src;
    s.current_unit = 0;

    // sema functions
    for (int i = 0; i < funcs->count; i++)
    {
        Func *f = &funcs->items[i];
        if (func_is_generic_template(f))
            continue;
        s.src = f->src ? f->src : src;
        s.current = f;
        s.current_method = 0;
        s.current_class = 0;
        s.current_is_static = 0;
        s.current_unit = f->unit;
        Scope sc;
        sc.vars = 0;
        sc.parent = &global_scope;
        s.scope = &sc;

        for (int p = 0; p < f->params.count; p++)
        {
            Param *pr = &f->params.items[p];
            scope_add_param(&s, &sc, pr->name, pr->type, f->body ? f->body->line : 1, f->body ? f->body->col : 1);
        }

        if (f->body)
        {
            sema_stmt(&s, f->body);
            if (f->ret_type.kind != TY_VOID && !stmt_guarantees_return(f->body))
                sema_error(&s, f->body ? f->body->line : f->line, f->body ? f->body->col : f->col, 1,
                           "Missing Return", "Non-void function must return a value on all paths");
        }
    }

    // sema methods
    for (int i = 0; i < classes->count; i++)
    {
        ClassDecl *c = &classes->items[i];
        for (int m = 0; m < c->methods.count; m++)
        {
            Method *md = &c->methods.items[m];
            if (!md->body) continue;
            s.src = md->src ? md->src : src;
            s.current = 0;
            s.current_method = md;
            s.current_class = c;
            s.current_is_static = md->is_static;
            s.current_unit = c->unit;
            if (md->is_async)
                sema_error(&s, md->line > 0 ? md->line : 1, md->col > 0 ? md->col : 1, 1, "Unsupported Async Method", "Async methods are not supported yet");

            Scope sc;
            sc.vars = 0;
            sc.parent = &global_scope;
            s.scope = &sc;

            if (!md->is_static)
            {
                Type this_type = type_class(class_qname(c));
                scope_add(&s, &sc, "this", this_type, 0, md->line, md->col, 0);
            }

            for (int p = 0; p < md->params.count; p++)
            {
                Param *pr = &md->params.items[p];
                scope_add_param(&s, &sc, pr->name, pr->type, md->line, md->col);
            }

            sema_stmt(&s, md->body);
            if (md->ret_type.kind != TY_VOID && !stmt_guarantees_return(md->body))
                sema_error(&s, md->body ? md->body->line : md->line, md->body ? md->body->col : md->col, 1,
                           "Missing Return", "Non-void method must return a value on all paths");
        }
    }

    warn_unused_scope(&s, &global_scope);

    return 0;
}
