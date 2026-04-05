// Internal sema fragment: expression analysis

static bool sema_lower_info_string_const(Expr *e, const char *value)
{
    if (!e || !value) return false;
    e->kind = EXPR_STRING;
    e->v.str_val.ptr = value;
    e->v.str_val.len = (int)kn_strlen(value);
    e->type = type_make(TY_STRING);
    return true;
}

static bool sema_lower_info_int_const(Expr *e, int64_t value)
{
    if (!e) return false;
    e->kind = EXPR_INT;
    e->v.int_val = value;
    e->type = type_make(TY_INT);
    return true;
}

static bool sema_lower_info_bool_const(Expr *e, int value)
{
    if (!e) return false;
    e->kind = EXPR_BOOL;
    e->v.int_val = value ? 1 : 0;
    e->type = type_make(TY_BOOL);
    return true;
}

static bool sema_try_lower_target_or_host_constant(Sema *s, Expr *e, const char *qname)
{
    if (!s || !e || !qname || !qname[0])
        return false;

    if (kn_strcmp(qname, "IO.Target") == 0 ||
        kn_strcmp(qname, "IO.Target.Current") == 0 ||
        kn_strcmp(qname, "IO.Target.OS") == 0 ||
        kn_strcmp(qname, "IO.Target.OS.Current") == 0)
        return sema_lower_info_int_const(e, sema_target_os_value(s));
    if (kn_strcmp(qname, "IO.Target.Windows") == 0 || kn_strcmp(qname, "IO.Target.OS.Windows") == 0)
        return sema_lower_info_int_const(e, KN_INFO_OS_WINDOWS);
    if (kn_strcmp(qname, "IO.Target.Linux") == 0 || kn_strcmp(qname, "IO.Target.OS.Linux") == 0)
        return sema_lower_info_int_const(e, KN_INFO_OS_LINUX);
    if (kn_strcmp(qname, "IO.Target.MacOS") == 0 || kn_strcmp(qname, "IO.Target.OS.MacOS") == 0)
        return sema_lower_info_int_const(e, KN_INFO_OS_MACOS);
    if (kn_strcmp(qname, "IO.Target.Unknown") == 0 || kn_strcmp(qname, "IO.Target.OS.Unknown") == 0)
        return sema_lower_info_int_const(e, KN_INFO_OS_UNKNOWN);

    if (kn_strcmp(qname, "IO.Target.Arch") == 0 || kn_strcmp(qname, "IO.Target.Arch.Current") == 0)
        return sema_lower_info_int_const(e, sema_target_arch_value(s));
    if (kn_strcmp(qname, "IO.Target.X86") == 0 || kn_strcmp(qname, "IO.Target.Arch.X86") == 0)
        return sema_lower_info_int_const(e, KN_INFO_ARCH_X86);
    if (kn_strcmp(qname, "IO.Target.X64") == 0 || kn_strcmp(qname, "IO.Target.Arch.X64") == 0)
        return sema_lower_info_int_const(e, KN_INFO_ARCH_X64);
    if (kn_strcmp(qname, "IO.Target.Arm64") == 0 || kn_strcmp(qname, "IO.Target.Arch.Arm64") == 0)
        return sema_lower_info_int_const(e, KN_INFO_ARCH_ARM64);
    if (kn_strcmp(qname, "IO.Target.Arch.Unknown") == 0)
        return sema_lower_info_int_const(e, KN_INFO_ARCH_UNKNOWN);

    if (kn_strcmp(qname, "IO.Target.Env") == 0 || kn_strcmp(qname, "IO.Target.Env.Current") == 0)
        return sema_lower_info_int_const(e, sema_target_env_value(s));
    if (kn_strcmp(qname, "IO.Target.Hosted") == 0 || kn_strcmp(qname, "IO.Target.Env.Hosted") == 0)
        return sema_lower_info_int_const(e, KN_INFO_ENV_HOSTED);
    if (kn_strcmp(qname, "IO.Target.Freestanding") == 0 || kn_strcmp(qname, "IO.Target.Env.Freestanding") == 0)
        return sema_lower_info_int_const(e, KN_INFO_ENV_FREESTANDING);
    if (kn_strcmp(qname, "IO.Target.Env.Unknown") == 0)
        return sema_lower_info_int_const(e, KN_INFO_ENV_UNKNOWN);

    if (kn_strcmp(qname, "IO.Target.PointerBits") == 0)
        return sema_lower_info_int_const(e, sema_target_pointer_bits_value(s));
    if (kn_strcmp(qname, "IO.Target.PathSeparator") == 0)
        return sema_lower_info_string_const(e, sema_target_path_separator(s));
    if (kn_strcmp(qname, "IO.Target.NewLine") == 0)
        return sema_lower_info_string_const(e, sema_target_new_line(s));
    if (kn_strcmp(qname, "IO.Target.ExeSuffix") == 0)
        return sema_lower_info_string_const(e, sema_target_exe_suffix(s));
    if (kn_strcmp(qname, "IO.Target.ObjectSuffix") == 0)
        return sema_lower_info_string_const(e, sema_target_object_suffix(s));
    if (kn_strcmp(qname, "IO.Target.DynamicLibrarySuffix") == 0)
        return sema_lower_info_string_const(e, sema_target_dynamic_library_suffix(s));
    if (kn_strcmp(qname, "IO.Target.StaticLibrarySuffix") == 0)
        return sema_lower_info_string_const(e, sema_target_static_library_suffix(s));

    if (kn_strcmp(qname, "IO.Host") == 0 ||
        kn_strcmp(qname, "IO.Host.Current") == 0 ||
        kn_strcmp(qname, "IO.Host.OS") == 0 ||
        kn_strcmp(qname, "IO.Host.OS.Current") == 0)
        return sema_lower_info_int_const(e, sema_host_os_value(s));
    if (kn_strcmp(qname, "IO.Host.Windows") == 0 || kn_strcmp(qname, "IO.Host.OS.Windows") == 0)
        return sema_lower_info_int_const(e, KN_INFO_OS_WINDOWS);
    if (kn_strcmp(qname, "IO.Host.Linux") == 0 || kn_strcmp(qname, "IO.Host.OS.Linux") == 0)
        return sema_lower_info_int_const(e, KN_INFO_OS_LINUX);
    if (kn_strcmp(qname, "IO.Host.MacOS") == 0 || kn_strcmp(qname, "IO.Host.OS.MacOS") == 0)
        return sema_lower_info_int_const(e, KN_INFO_OS_MACOS);
    if (kn_strcmp(qname, "IO.Host.Unknown") == 0 || kn_strcmp(qname, "IO.Host.OS.Unknown") == 0)
        return sema_lower_info_int_const(e, KN_INFO_OS_UNKNOWN);

    if (kn_strcmp(qname, "IO.Host.Arch") == 0 || kn_strcmp(qname, "IO.Host.Arch.Current") == 0)
        return sema_lower_info_int_const(e, sema_host_arch_value(s));
    if (kn_strcmp(qname, "IO.Host.X86") == 0 || kn_strcmp(qname, "IO.Host.Arch.X86") == 0)
        return sema_lower_info_int_const(e, KN_INFO_ARCH_X86);
    if (kn_strcmp(qname, "IO.Host.X64") == 0 || kn_strcmp(qname, "IO.Host.Arch.X64") == 0)
        return sema_lower_info_int_const(e, KN_INFO_ARCH_X64);
    if (kn_strcmp(qname, "IO.Host.Arm64") == 0 || kn_strcmp(qname, "IO.Host.Arch.Arm64") == 0)
        return sema_lower_info_int_const(e, KN_INFO_ARCH_ARM64);
    if (kn_strcmp(qname, "IO.Host.Arch.Unknown") == 0)
        return sema_lower_info_int_const(e, KN_INFO_ARCH_UNKNOWN);

    if (kn_strcmp(qname, "IO.Host.Env") == 0 || kn_strcmp(qname, "IO.Host.Env.Current") == 0)
        return sema_lower_info_int_const(e, sema_host_env_value(s));
    if (kn_strcmp(qname, "IO.Host.Hosted") == 0 || kn_strcmp(qname, "IO.Host.Env.Hosted") == 0)
        return sema_lower_info_int_const(e, KN_INFO_ENV_HOSTED);
    if (kn_strcmp(qname, "IO.Host.Freestanding") == 0 || kn_strcmp(qname, "IO.Host.Env.Freestanding") == 0)
        return sema_lower_info_int_const(e, KN_INFO_ENV_FREESTANDING);
    if (kn_strcmp(qname, "IO.Host.Env.Unknown") == 0)
        return sema_lower_info_int_const(e, KN_INFO_ENV_UNKNOWN);

    if (kn_strcmp(qname, "IO.Host.PointerBits") == 0)
        return sema_lower_info_int_const(e, sema_host_pointer_bits_value(s));
    if (kn_strcmp(qname, "IO.Host.PathSeparator") == 0)
        return sema_lower_info_string_const(e, sema_host_path_separator(s));
    if (kn_strcmp(qname, "IO.Host.NewLine") == 0)
        return sema_lower_info_string_const(e, sema_host_new_line(s));
    if (kn_strcmp(qname, "IO.Host.ExeSuffix") == 0)
        return sema_lower_info_string_const(e, sema_host_exe_suffix(s));
    if (kn_strcmp(qname, "IO.Host.ObjectSuffix") == 0)
        return sema_lower_info_string_const(e, sema_host_object_suffix(s));
    if (kn_strcmp(qname, "IO.Host.DynamicLibrarySuffix") == 0)
        return sema_lower_info_string_const(e, sema_host_dynamic_library_suffix(s));
    if (kn_strcmp(qname, "IO.Host.StaticLibrarySuffix") == 0)
        return sema_lower_info_string_const(e, sema_host_static_library_suffix(s));

    // IO.Runtime.*
    if (kn_strcmp(qname, "IO.Runtime") == 0 || kn_strcmp(qname, "IO.Runtime.Current") == 0 ||
        kn_strcmp(qname, "IO.Runtime.Kind") == 0 || kn_strcmp(qname, "IO.Runtime.Kind.Current") == 0)
        return sema_lower_info_int_const(e, s ? s->runtime_backend : KN_INFO_RUNTIME_UNKNOWN);

    if (kn_strcmp(qname, "IO.Runtime.Unknown") == 0 || kn_strcmp(qname, "IO.Runtime.Kind.Unknown") == 0)
        return sema_lower_info_int_const(e, KN_INFO_RUNTIME_UNKNOWN);
    if (kn_strcmp(qname, "IO.Runtime.Native") == 0 || kn_strcmp(qname, "IO.Runtime.Kind.Native") == 0)
        return sema_lower_info_int_const(e, KN_INFO_RUNTIME_NATIVE);
    if (kn_strcmp(qname, "IO.Runtime.VM") == 0 || kn_strcmp(qname, "IO.Runtime.Kind.VM") == 0)
        return sema_lower_info_int_const(e, KN_INFO_RUNTIME_VM);

    if (kn_strcmp(qname, "IO.Runtime.Name") == 0)
    {
        int rb = s ? s->runtime_backend : KN_INFO_RUNTIME_UNKNOWN;
        return sema_lower_info_string_const(e, rb == KN_INFO_RUNTIME_VM ? "Kinal.VM" : "Kinal.Native");
    }

    if (kn_strcmp(qname, "IO.Runtime.IsNative") == 0)
        return sema_lower_info_bool_const(e, s && s->runtime_backend == KN_INFO_RUNTIME_NATIVE);
    if (kn_strcmp(qname, "IO.Runtime.IsVM") == 0)
        return sema_lower_info_bool_const(e, s && s->runtime_backend == KN_INFO_RUNTIME_VM);

    // IO.Version.*
    if (kn_strcmp(qname, "IO.Version.Major") == 0)
        return sema_lower_info_int_const(e, KN_VERSION_MAJOR);
    if (kn_strcmp(qname, "IO.Version.Minor") == 0)
        return sema_lower_info_int_const(e, KN_VERSION_MINOR);
    if (kn_strcmp(qname, "IO.Version.Patch") == 0)
        return sema_lower_info_int_const(e, KN_VERSION_PATCH);
    if (kn_strcmp(qname, "IO.Version") == 0 || kn_strcmp(qname, "IO.Version.String") == 0)
        return sema_lower_info_string_const(e, KN_VERSION_STRING);

    // IO.Version.VM.*
    if (kn_strcmp(qname, "IO.Version.VM") == 0 || kn_strcmp(qname, "IO.Version.VM.String") == 0)
    {
        int rb = s ? s->runtime_backend : KN_INFO_RUNTIME_UNKNOWN;
        return sema_lower_info_string_const(e, rb == KN_INFO_RUNTIME_VM ? KN_VM_VERSION_STRING : "");
    }
    if (kn_strcmp(qname, "IO.Version.VM.Major") == 0)
        return sema_lower_info_int_const(e, s && s->runtime_backend == KN_INFO_RUNTIME_VM ? KN_VM_VERSION_MAJOR : 0);
    if (kn_strcmp(qname, "IO.Version.VM.Minor") == 0)
        return sema_lower_info_int_const(e, s && s->runtime_backend == KN_INFO_RUNTIME_VM ? KN_VM_VERSION_MINOR : 0);
    if (kn_strcmp(qname, "IO.Version.VM.Patch") == 0)
        return sema_lower_info_int_const(e, s && s->runtime_backend == KN_INFO_RUNTIME_VM ? KN_VM_VERSION_PATCH : 0);

    return false;
}

static bool sema_is_in_async(Sema *s)
{
    if (!s) return false;
    if (s->current && s->current->is_async) return true;
    if (s->current_method && s->current_method->is_async) return true;
    return false;
}

static Type sema_mark_async_task_expr(Expr *e, Type result_type)
{
    if (!e) return type_make(TY_UNKNOWN);
    e->type = type_class("IO.Async.Task");
    e->async_result_type = result_type;
    return e->type;
}

static Type sema_expr(Sema *s, Expr *e)
{
    switch (e->kind)
    {
    case EXPR_INT:
        e->type = type_make(TY_INT);
        return e->type;
    case EXPR_CHAR:
        e->type = type_make(TY_CHAR);
        return e->type;
    case EXPR_FLOAT:
        e->type = type_make(TY_FLOAT);
        return e->type;
    case EXPR_BOOL:
        e->type = type_make(TY_BOOL);
        return e->type;
    case EXPR_DEFAULT:
        return e->type;
    case EXPR_NULL:
        e->type = type_make(TY_NULL);
        return e->type;
    case EXPR_STRING:
        e->type = type_make(TY_STRING);
        return e->type;
    case EXPR_AWAIT:
    {
        if (!sema_is_in_async(s))
        {
            sema_error(s, e->line, e->col, 1, "Await Outside Async", "Await is only allowed inside Async Function");
            e->type = type_make(TY_UNKNOWN);
            return e->type;
        }
        if (s->try_depth > 0)
        {
            sema_error(s, e->line, e->col, 1, "Await In Try", "Await inside Try/Catch is not supported yet");
            e->type = type_make(TY_UNKNOWN);
            return e->type;
        }
        Type awaited = sema_expr(s, e->v.await_expr.expr);
        if (!is_async_task_type(awaited))
        {
            sema_error(s, e->line, e->col, 1, "Await Requires Task", "Await operand must be IO.Async.Task");
            e->type = type_make(TY_UNKNOWN);
            return e->type;
        }
        if (e->v.await_expr.expr->async_result_type.kind == TY_UNKNOWN)
        {
            sema_error(s, e->line, e->col, 1, "Unknown Task Result", "Task result type is not known here");
            e->type = type_make(TY_UNKNOWN);
            return e->type;
        }
        e->type = e->v.await_expr.expr->async_result_type;
        return e->type;
    }
    case EXPR_VAR:
    {
        if (sema_try_lower_target_or_host_constant(s, e, e->v.var.name))
            return e->type;

        if (s->in_block_literal && s->block_records)
        {
            int ip = block_record_ip(s->block_records, e->v.var.name);
            if (ip >= 0)
            {
                e->kind = EXPR_INT;
                e->v.int_val = ip;
                e->type = type_make(TY_INT);
                return e->type;
            }
        }
        VarSym *v = scope_find(s->scope, e->v.var.name);
        if (v)
        {
            v->used = 1;
            e->type = v->type;
            e->async_result_type = v->async_result_type;
            return e->type;
        }
        {
            Type implicit_field = type_make(TY_UNKNOWN);
            if (sema_try_current_class_field_expr(s, e, &implicit_field))
            {
                if (e->kind != EXPR_VAR)
                    return sema_expr(s, e);
                return implicit_field;
            }
        }

        FuncSym *fn = 0;
        if (name_has_dot(e->v.var.name))
        {
            fn = functable_find_visible_qualified(&s->funcs, s, e->v.var.name);
        }
        else
        {
            if (s->current_unit && s->current_unit[0])
                fn = functable_find_by_name_in_unit(&s->funcs, e->v.var.name, s->current_unit);
            else
                fn = functable_find_by_name_without_unit(&s->funcs, e->v.var.name);
        }
        if (fn)
        {
            if (func_is_generic_template(fn->func))
            {
                sema_error(s, e->line, e->col, 1, "Generic Function", "Generic function reference requires explicit type arguments");
                e->type = type_make(TY_UNKNOWN);
                return e->type;
            }
            e->kind = EXPR_FUNC_REF;
            e->v.func_ref.qname = fn->func->qname ? fn->func->qname : fn->func->name;
            e->v.func_ref.builtin_id = KN_BUILTIN_NONE;
            e->type = type_class("IO.Type.Object.Function");
            return e->type;
        }

        const KnStdFunc *std_ref = 0;
        if (name_has_dot(e->v.var.name))
            std_ref = resolve_std_by_qualified_name(s, e->v.var.name);
        else
            std_ref = resolve_std_by_local(s, e->v.var.name);
        if (std_ref)
        {
            e->kind = EXPR_FUNC_REF;
            e->v.func_ref.qname = e->v.var.name;
            e->v.func_ref.builtin_id = (int)std_ref->kind;
            e->type = type_class("IO.Type.Object.Function");
            return e->type;
        }

        if (find_scoped_symbol_module(s, e->v.var.name))
            sema_error(s, e->line, e->col, 1, "Scoped Symbol Required", "Imported symbol must be called with module or alias qualifier");
        else if (find_required_namespace_module_for_func(s, e->v.var.name))
            sema_error(s, e->line, e->col, 1, "Scoped Symbol Required", "Imported module function must be called with module or alias qualifier");
        else if (s->current_class && !name_has_dot(e->v.var.name))
        {
            int midx = -1;
            Method *sm = find_static_method_by_name(s->current_class, e->v.var.name, &midx);
            if (sm)
            {
                e->kind = EXPR_FUNC_REF;
                e->v.func_ref.qname = build_method_ref_qname(s->current_class, sm->name, midx);
                e->v.func_ref.builtin_id = KN_BUILTIN_NONE;
                e->type = type_class("IO.Type.Object.Function");
                return e->type;
            }
            sema_error(s, e->line, e->col, 1, "Unknown Variable", "Variable not defined");
        }
        else
            sema_error(s, e->line, e->col, 1, "Unknown Variable", "Variable not defined");
        e->type = type_make(TY_UNKNOWN);
        return e->type;
    }
    case EXPR_FUNC_REF:
        e->type = type_class("IO.Type.Object.Function");
        return e->type;
    case EXPR_ANON_FUNC:
    {
        validate_type_ref(s, &e->v.anon_func.ret_type, e->line, e->col, 1);
        {
            ParamList params;
            params.items = e->v.anon_func.params;
            params.count = e->v.anon_func.param_count;
            params.cap = e->v.anon_func.param_cap;
            validate_param_defaults(s, &params, e->line, e->col);
        }

        Func fake;
        kn_memset(&fake, 0, sizeof(fake));
        fake.ret_type = e->v.anon_func.ret_type;
        fake.safety = e->v.anon_func.safety;

        Func *prev_cur = s->current;
        Method *prev_method = s->current_method;
        Scope *prev_scope = s->scope;
        const char *prev_unit = s->current_unit;
        ClassDecl *prev_class = s->current_class;
        int prev_static = s->current_is_static;
        int prev_in_block = s->in_block_literal;
        BlockRecordList *prev_records = s->block_records;
        Safety prev_block_safety = s->block_safety;

        s->current = &fake;
        s->current_method = 0;
        s->current_unit = prev_unit;
        s->current_class = prev_class;
        s->current_is_static = prev_static;
        s->in_block_literal = 0;
        s->block_records = 0;
        s->block_safety = SAFETY_SAFE;

        Scope sc;
        sc.vars = 0;
        sc.parent = prev_scope;
        s->scope = &sc;
        for (int i = 0; i < e->v.anon_func.param_count; i++)
        {
            Param *pr = &e->v.anon_func.params[i];
            scope_add_param(s, &sc, pr->name, pr->type, e->line, e->col);
        }
        sema_stmt(s, e->v.anon_func.body);

        s->scope = prev_scope;
        s->current = prev_cur;
        s->current_method = prev_method;
        s->current_unit = prev_unit;
        s->current_class = prev_class;
        s->current_is_static = prev_static;
        s->in_block_literal = prev_in_block;
        s->block_records = prev_records;
        s->block_safety = prev_block_safety;

        e->type = type_class("IO.Type.Object.Function");
        return e->type;
    }
    case EXPR_BLOCK_LITERAL:
    {
        if (sema_is_freestanding_core())
            sema_error(s, e->line, e->col, 1, "Freestanding Core", "Block literals require freestanding gc runtime");
        if (e->v.block_lit.safety != SAFETY_UNSAFE)
        {
            for (int i = 0; i < e->v.block_lit.records.count; i++)
            {
                if (e->v.block_lit.records.items[i].nested_depth > 0)
                    sema_error(s, e->line, e->col, 1, "Unsafe Block", "Record inside nested control flow requires Unsafe Block");
            }
        }

        Func fake;
        kn_memset(&fake, 0, sizeof(fake));
        fake.ret_type = type_make(TY_VOID);
        fake.safety = e->v.block_lit.safety;

        Func *prev_cur = s->current;
        Method *prev_method = s->current_method;
        Scope *prev_scope = s->scope;
        const char *prev_unit = s->current_unit;
        ClassDecl *prev_class = s->current_class;
        int prev_static = s->current_is_static;
        int prev_in_block = s->in_block_literal;
        BlockRecordList *prev_records = s->block_records;
        Safety prev_block_safety = s->block_safety;

        s->current = &fake;
        s->current_method = 0;
        s->current_unit = prev_unit;
        s->current_class = prev_class;
        s->current_is_static = prev_static;
        s->in_block_literal = 1;
        s->block_records = &e->v.block_lit.records;
        s->block_safety = e->v.block_lit.safety;

        Scope sc;
        sc.vars = 0;
        sc.parent = prev_scope;
        s->scope = &sc;
        sema_stmt(s, e->v.block_lit.body);

        s->scope = prev_scope;
        s->current = prev_cur;
        s->current_method = prev_method;
        s->current_unit = prev_unit;
        s->current_class = prev_class;
        s->current_is_static = prev_static;
        s->in_block_literal = prev_in_block;
        s->block_records = prev_records;
        s->block_safety = prev_block_safety;

        e->type = type_class("IO.Type.Object.Block");
        return e->type;
    }
    case EXPR_INVOKE:
    {
        Type ct = sema_expr(s, e->v.invoke.callee);
        Type ret = type_make(TY_ANY);
        ParamList args = {0};
        args.count = e->v.invoke.args.count;
        args.cap = e->v.invoke.args.count;
        if (args.count > 0)
        {
            args.items = (Param *)kn_malloc(sizeof(Param) * (size_t)args.count);
            for (int i = 0; i < args.count; i++)
            {
                Type at = sema_expr(s, e->v.invoke.args.items[i]);
                args.items[i].type = at;
                args.items[i].name = "";
                args.items[i].src = s->src;
            }
        }
        if (is_block_object_type(ct))
            sema_error(s, e->line, e->col, 1, "Invalid Call", "Block object must use Run/Jump APIs");
        else if (!is_function_object_type(s, ct))
            sema_error(s, e->line, e->col, 1, "Invalid Call", "Call target is not a function object");
        else if (e->v.invoke.callee->kind == EXPR_FUNC_REF &&
                 e->v.invoke.callee->v.func_ref.builtin_id != KN_BUILTIN_NONE)
        {
            if (call_has_named_args(&e->v.invoke.arg_names))
                sema_error(s, e->line, e->col, 1, "Invalid Named Argument", "Named arguments are not supported for this callable");
            KnBuiltinKind bk = (KnBuiltinKind)e->v.invoke.callee->v.func_ref.builtin_id;
            return lower_invoke_builtin_ref(s, e, bk, &args);
        }
        else if (e->v.invoke.callee->kind == EXPR_VAR)
        {
            VarSym *v = scope_find(s->scope, e->v.invoke.callee->v.var.name);
            if (v && v->origin)
            {
                if (v->origin->kind == EXPR_FUNC_REF &&
                    v->origin->v.func_ref.builtin_id != KN_BUILTIN_NONE)
                {
                    if (call_has_named_args(&e->v.invoke.arg_names))
                        sema_error(s, e->line, e->col, 1, "Invalid Named Argument", "Named arguments are not supported for this callable");
                    KnBuiltinKind bk = (KnBuiltinKind)v->origin->v.func_ref.builtin_id;
                    return lower_invoke_builtin_ref(s, e, bk, &args);
                }
                if (v->origin->kind == EXPR_FUNC_REF && v->origin->v.func_ref.qname)
                {
                    FuncSym *fs = functable_find(&s->funcs, v->origin->v.func_ref.qname);
                    if (fs && fs->func)
                    {
                        const char **param_names = alloc_param_names_from_paramlist(&fs->func->params);
                        (void)prepare_call_args(s, e->line, e->col, &e->v.invoke.args, &e->v.invoke.arg_names,
                                                &args, fs->func->params.items, fs->func->params.count,
                                                param_names, fs->func->name, 1, 1);
                        retype_prepared_args(s, &e->v.invoke.args, &args, fs->func->params.items, fs->func->params.count,
                                             "Default literal requires parameter type");
                        if (param_names) kn_free((void *)param_names);
                        for (int i = 0; i < fs->func->params.count; i++)
                        {
                            Type pt = fs->func->params.items[i].type;
                            Type at = args.items[i].type;
                            check_int_to_ptr_safety(s, pt, at, e->v.invoke.args.items[i]);
                            if (!type_assignable(s, pt, at))
                                sema_error(s, e->v.invoke.args.items[i]->line, e->v.invoke.args.items[i]->col, 1, "Type Mismatch", type_mismatch_detail(pt, at));
                        }
                        check_call_safety(s, fs->func->safety, e->line, e->col, 1);
                        ret = fs->func->ret_type;
                    }
                }
                else if (v->origin->kind == EXPR_ANON_FUNC)
                {
                    const char **param_names = alloc_param_names_from_params(v->origin->v.anon_func.params,
                                                                             v->origin->v.anon_func.param_count);
                    (void)prepare_call_args(s, e->line, e->col, &e->v.invoke.args, &e->v.invoke.arg_names,
                                            &args, v->origin->v.anon_func.params,
                                            v->origin->v.anon_func.param_count, param_names,
                                            e->v.invoke.callee->v.var.name, 1, 1);
                    retype_prepared_args(s, &e->v.invoke.args, &args, v->origin->v.anon_func.params,
                                         v->origin->v.anon_func.param_count,
                                         "Default literal requires parameter type");
                    if (param_names) kn_free((void *)param_names);
                    for (int i = 0; i < v->origin->v.anon_func.param_count; i++)
                    {
                        Type pt = v->origin->v.anon_func.params[i].type;
                        Type at = args.items[i].type;
                        check_int_to_ptr_safety(s, pt, at, e->v.invoke.args.items[i]);
                        if (!type_assignable(s, pt, at))
                            sema_error(s, e->v.invoke.args.items[i]->line, e->v.invoke.args.items[i]->col, 1, "Type Mismatch", type_mismatch_detail(pt, at));
                    }
                    check_call_safety(s, v->origin->v.anon_func.safety, e->line, e->col, 1);
                    ret = v->origin->v.anon_func.ret_type;
                }
            }
        }
        e->type = ret;
        return e->type;
    }
    case EXPR_THIS:
    {
        if (!s->current_class || s->current_is_static)
            sema_error(s, e->line, e->col, 1, "Invalid This", "This is not available in static context");
        e->type = type_class(class_qname(s->current_class));
        return e->type;
    }
    case EXPR_BASE:
    {
        if (!s->current_class || s->current_is_static)
            sema_error(s, e->line, e->col, 1, "Invalid Base", "Base is not available in static context");
        if (!s->current_class->base)
            sema_error(s, e->line, e->col, 1, "Invalid Base", "Class has no base type");
        e->type = type_class(s->current_class->base);
        return e->type;
    }
    case EXPR_NEW:
    {
        if (sema_is_freestanding_core())
            sema_error(s, e->line, e->col, 1, "Freestanding Core", "New is not available in freestanding core");
        if (call_has_named_args(&e->v.new_expr.arg_names))
            sema_error(s, e->line, e->col, 1, "Invalid Named Argument", "Named arguments are not supported for constructors");
        ClassDecl *c = 0;
        if (e->type.kind == TY_CLASS && e->v.new_expr.class_name && name_has_dot(e->v.new_expr.class_name))
            c = find_class(s, e->v.new_expr.class_name);
        if (!c)
            c = find_visible_class(s, e->v.new_expr.class_name);
        if (!c)
        {
            sema_error(s, e->line, e->col, 1, "Unknown Type", "Class not found");
            e->type = type_make(TY_UNKNOWN);
            return e->type;
        }
        if (c->is_static_class)
            sema_error(s, e->line, e->col, 1, "Invalid New", "Cannot instantiate static class");
        if (c->is_abstract)
            sema_error(s, e->line, e->col, 1, "Invalid New", "Cannot instantiate abstract class");
        // resolve ctor by params
        ParamList params = {0};
        params.count = e->v.new_expr.args.count;
        params.cap = e->v.new_expr.args.count;
        if (params.count > 0)
        {
            params.items = (Param *)kn_malloc(sizeof(Param) * (size_t)params.count);
            for (int i = 0; i < params.count; i++)
            {
                Type at = sema_expr(s, e->v.new_expr.args.items[i]);
                params.items[i].type = at;
                params.items[i].name = "";
                params.items[i].src = s->src;
            }
        }
        int ctor_index = -1;
        bool has_explicit_ctor = class_has_explicit_ctor(c);
        Method *ctor = find_ctor(s, c, &e->v.new_expr.args, &e->v.new_expr.arg_names, &params, &ctor_index);
        if (!ctor && (has_explicit_ctor || params.count > 0))
            sema_error(s, e->line, e->col, 1, "Constructor", "Constructor not found");
        if (ctor)
        {
            const char **param_names = alloc_param_names_from_paramlist(&ctor->params);
            (void)prepare_call_args(s, e->line, e->col, &e->v.new_expr.args, &e->v.new_expr.arg_names,
                                    &params, ctor->params.items, ctor->params.count,
                                    param_names, c->name, 1, 1);
            retype_prepared_args(s, &e->v.new_expr.args, &params, ctor->params.items, ctor->params.count,
                                 "Default literal requires parameter type");
            if (param_names) kn_free((void *)param_names);
            for (int i = 0; i < ctor->params.count; i++)
            {
                Type pt = ctor->params.items[i].type;
                Type at = params.items[i].type;
                check_int_to_ptr_safety(s, pt, at, e->v.new_expr.args.items[i]);
                check_byte_literal_range(s, pt, e->v.new_expr.args.items[i]);
            }
            check_call_safety(s, ctor->safety, e->line, e->col, 1);
        }
        e->v.new_expr.ctor_index = ctor_index;
        e->v.new_expr.class_name = class_qname(c);
        e->type = type_class(class_qname(c));
        return e->type;
    }
    case EXPR_MEMBER:
    {
        Expr *recv = e->v.member.recv;
        // Parser recovery can produce member access nodes with missing member name
        // (e.g. `IO.Console.`). Avoid crashing in sema; parser has already reported
        // an error for the missing identifier.
        if (!e->v.member.name || !recv)
        {
            e->type = type_make(TY_UNKNOWN);
            return e->type;
        }
        {
            const char *qname = build_qualified_call_name(s, e, 0);
            if (!qname)
                qname = build_qualified_call_name(s, recv, e->v.member.name);
            if (qname)
            {
                if (sema_try_lower_target_or_host_constant(s, e, qname))
                    return e->type;

                FuncSym *qfn = functable_find_visible_qualified(&s->funcs, s, qname);
                if (qfn)
                {
                    e->kind = EXPR_FUNC_REF;
                    e->v.func_ref.qname = qfn->func && qfn->func->qname ? qfn->func->qname : qname;
                    e->v.func_ref.builtin_id = KN_BUILTIN_NONE;
                    e->type = type_class("IO.Type.Object.Function");
                    return e->type;
                }
                {
                    FuncSym *hidden = functable_find(&s->funcs, qname);
                    if (hidden && hidden->func && hidden->func->unit && import_has_alias_for_module(s, hidden->func->unit))
                    {
                        sema_error(s, e->line, e->col, 1, "Scoped Symbol Required", "Imported module function must be called with module or alias qualifier");
                        e->type = type_make(TY_UNKNOWN);
                        return e->type;
                    }
                }

                const KnStdFunc *qstd = resolve_std_by_qualified_name(s, qname);
                if (qstd)
                {
                    e->kind = EXPR_FUNC_REF;
                    e->v.func_ref.qname = qname;
                    e->v.func_ref.builtin_id = (int)qstd->kind;
                    e->type = type_class("IO.Type.Object.Function");
                    return e->type;
                }

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
                        if (qc)
                        {
                            ClassDecl *owner = 0;
                            int field_index = -1;
                            Field *f = find_field_hierarchy(s, qc, fname, &owner, &field_index);
                            if (!f)
                            {
                                int midx = -1;
                                Method *sm = find_static_method_by_name(qc, fname, &midx);
                                if (sm)
                                {
                                    e->kind = EXPR_FUNC_REF;
                                    e->v.func_ref.qname = build_method_ref_qname(qc, sm->name, midx);
                                    e->v.func_ref.builtin_id = KN_BUILTIN_NONE;
                                    e->type = type_class("IO.Type.Object.Function");
                                    return e->type;
                                }
                                sema_error(s, e->line, e->col, 1, "Unknown Field", "Field not found");
                                e->type = type_make(TY_UNKNOWN);
                                return e->type;
                            }
                            if (!f->is_static)
                            {
                                sema_error(s, e->line, e->col, 1, "Invalid Member", "Instance field requires class instance");
                                e->type = type_make(TY_UNKNOWN);
                                return e->type;
                            }
                            check_access(s, owner ? owner : qc, f->access, e->line, e->col);
                            e->v.member.is_static = 1;
                            e->v.member.static_type = owner ? class_qname(owner) : class_qname(qc);
                            e->v.member.field_index = field_index;
                            e->v.member.field_owner = owner ? class_qname(owner) : class_qname(qc);
                            e->type = f->type;
                            return e->type;
                        }
                    }
                }
            }
        }
        {
            Type property_type = type_make(TY_UNKNOWN);
            if (sema_try_rewrite_property_member_expr(s, e, &property_type))
                return property_type;
        }
        if (e->v.member.is_static && e->v.member.field_owner)
        {
            ClassDecl *owner = find_class(s, e->v.member.field_owner);
            if (!owner)
            {
                sema_error(s, e->line, e->col, 1, "Unknown Type", "Class not found");
                e->type = type_make(TY_UNKNOWN);
                return e->type;
            }
            ClassDecl *resolved_owner = 0;
            int field_index = -1;
            Field *f = find_field_hierarchy(s, owner, e->v.member.name, &resolved_owner, &field_index);
            if (!f)
            {
                sema_error(s, e->line, e->col, 1, "Unknown Field", "Field not found");
                e->type = type_make(TY_UNKNOWN);
                return e->type;
            }
            if (!f->is_static)
            {
                sema_error(s, e->line, e->col, 1, "Invalid Member", "Instance field requires class instance");
                e->type = type_make(TY_UNKNOWN);
                return e->type;
            }
            check_access(s, resolved_owner ? resolved_owner : owner, f->access, e->line, e->col);
            e->v.member.field_index = field_index;
            e->v.member.field_owner = resolved_owner ? class_qname(resolved_owner) : class_qname(owner);
            e->v.member.static_type = e->v.member.field_owner;
            e->type = f->type;
            return e->type;
        }
        Type rt = type_make(TY_UNKNOWN);
        Expr *block_origin = 0;
        {
            EnumDecl *en = resolve_enum_qual_expr(s, recv);
            if (en)
            {
                EnumItem *it = find_enum_item(en, e->v.member.name);
                if (!it)
                    sema_error(s, e->line, e->col, 1, "Unknown Enum Member", "Enum member not found");
                e->v.member.is_enum_const = 1;
                e->v.member.enum_value = it ? it->value : 0;
                e->v.member.enum_type = enum_qname(en);
                e->type = type_enum(enum_qname(en));
                return e->type;
            }
        }
        if (recv->kind == EXPR_VAR)
        {
            Type implicit_recv = type_make(TY_UNKNOWN);
            if (!scope_find(s->scope, recv->v.var.name) &&
                sema_try_current_class_field_expr(s, recv, &implicit_recv))
            {
                if (recv->kind == EXPR_VAR)
                {
                    e->type = implicit_recv;
                    return e->type;
                }
            }
        }
        if (recv->kind == EXPR_VAR)
        {
            VarSym *v = scope_find(s->scope, recv->v.var.name);
            if (!v)
            {
                sema_error(s, recv->line, recv->col, 1, "Unknown Variable", "Variable not defined");
                e->type = type_make(TY_UNKNOWN);
                return e->type;
            }
            v->used = 1;
            rt = v->type;
            if (v && v->origin && v->origin->kind == EXPR_BLOCK_LITERAL)
                block_origin = v->origin;
        }
        else
        {
            rt = sema_expr(s, recv);
            if (recv->kind == EXPR_BLOCK_LITERAL)
                block_origin = recv;
        }

        if (is_block_object_type(rt) && block_origin && block_origin->kind == EXPR_BLOCK_LITERAL)
        {
            int ip = block_record_ip(&block_origin->v.block_lit.records, e->v.member.name);
            if (ip >= 0)
            {
                e->kind = EXPR_INT;
                e->v.int_val = ip;
                e->type = type_make(TY_INT);
                return e->type;
            }
        }
        if (rt.kind == TY_STRUCT)
        {
            StructDecl *sd = find_struct(s, rt.name);
            if (!sd)
            {
                sema_error(s, e->line, e->col, 1, "Unknown Type", "Struct not found");
                e->type = type_make(TY_UNKNOWN);
                return e->type;
            }
            int field_index = -1;
            Field *f = find_struct_field(sd, e->v.member.name, &field_index);
            if (!f)
            {
                sema_error(s, e->line, e->col, 1, "Unknown Field", "Field not found");
                e->type = type_make(TY_UNKNOWN);
                return e->type;
            }
            e->v.member.field_index = field_index;
            e->v.member.field_owner = struct_qname(sd);
            e->type = f->type;
            return e->type;
        }
        if (rt.kind != TY_CLASS)
        {
            sema_error(s, e->line, e->col, 1, "Invalid Member", "Member access requires class instance");
            e->type = type_make(TY_UNKNOWN);
            return e->type;
        }
        ClassDecl *c = find_class(s, rt.name);
        if (!c)
        {
            if (find_interface(s, rt.name))
                sema_error(s, e->line, e->col, 1, "Invalid Member", "Interface members not supported yet");
            sema_error(s, e->line, e->col, 1, "Unknown Type", "Class not found");
            e->type = type_make(TY_UNKNOWN);
            return e->type;
        }
        ClassDecl *owner = 0;
        int field_index = -1;
        Field *f = find_field_hierarchy(s, c, e->v.member.name, &owner, &field_index);
        if (!f)
        {
            sema_error(s, e->line, e->col, 1, "Unknown Field", "Field not found");
            e->type = type_make(TY_UNKNOWN);
            return e->type;
        }
        if (f->is_static)
        {
            sema_error(s, e->line, e->col, 1, "Invalid Member", "Static field requires class name");
            e->v.member.is_static = 1;
            e->v.member.static_type = owner ? class_qname(owner) : class_qname(c);
        }
        check_access(s, owner, f->access, e->line, e->col);
        e->v.member.field_index = field_index;
        e->v.member.field_owner = owner ? class_qname(owner) : class_qname(c);
        e->type = f->type;
        return e->type;
    }
    case EXPR_ASSIGN:
    {
        Type property_type = type_make(TY_UNKNOWN);
        if (sema_try_rewrite_property_assign_expr(s, e, &property_type))
            return property_type;

        Expr *tgt = e->v.assign.target;
        Type vt;
        if (tgt->kind == EXPR_VAR)
        {
            VarSym *v = scope_find(s->scope, tgt->v.var.name);
            if (!v)
            {
                // Check if target is a delegate function
                FuncSym *dfn = 0;
                if (name_has_dot(tgt->v.var.name))
                    dfn = functable_find_visible_qualified(&s->funcs, s, tgt->v.var.name);
                else
                {
                    if (s->current_unit && s->current_unit[0])
                        dfn = functable_find_by_name_in_unit(&s->funcs, tgt->v.var.name, s->current_unit);
                    else
                        dfn = functable_find_by_name_without_unit(&s->funcs, tgt->v.var.name);
                }
                if (dfn && dfn->func && dfn->func->is_delegate)
                {
                    // Delegate assignment: RHS must be Object.Function (or ptr for By C)
                    Type rhs = sema_expr(s, e->v.assign.value);
                    if (dfn->func->delegate_abi == KN_DELEGATE_ABI_C)
                    {
                        if (!is_function_object_type(s, rhs) && rhs.kind != TY_NULL && rhs.kind != TY_PTR)
                            sema_error(s, e->v.assign.value->line, e->v.assign.value->col, 1,
                                       "Type Mismatch", "By C Delegate assignment requires Object.Function, pointer, or null");
                    }
                    else
                    {
                        if (!is_function_object_type(s, rhs) && rhs.kind != TY_NULL)
                            sema_error(s, e->v.assign.value->line, e->v.assign.value->col, 1,
                                       "Type Mismatch", "Delegate assignment requires Object.Function or null");
                    }
                    if (dfn->func->qname)
                        tgt->v.var.name = dfn->func->qname;
                    vt = type_class("IO.Type.Object.Function");
                    e->type = vt;
                    return e->type;
                }
                else if (dfn && dfn->func && !dfn->func->is_delegate)
                {
                    sema_error(s, e->line, e->col, 1, "Invalid Assignment",
                               "Cannot assign to a function with body; only Delegate functions are assignable");
                    vt = type_make(TY_UNKNOWN);
                }
                else
                {
                    Type implicit_target = type_make(TY_UNKNOWN);
                    if (sema_try_current_class_field_expr(s, tgt, &implicit_target))
                    {
                        if (tgt->kind != EXPR_VAR)
                            vt = sema_expr(s, tgt);
                        else
                            vt = implicit_target;
                    }
                    else
                    {
                        sema_error(s, e->line, e->col, 1, "Unknown Variable", "Assignment target not found");
                        vt = type_make(TY_UNKNOWN);
                    }
                }
            }
            else
            {
                if (v->is_const)
                sema_error(s, e->line, e->col, 1, "Const Assignment", "Cannot assign to const variable");
                vt = v->type;
            }
        }
        else if (tgt->kind == EXPR_MEMBER || tgt->kind == EXPR_INDEX ||
                 (tgt->kind == EXPR_UNARY && tgt->v.unary.op == TOK_STAR))
        {
            vt = sema_expr(s, tgt);
            if (tgt->kind == EXPR_MEMBER && tgt->v.member.is_enum_const)
                sema_error(s, tgt->line, tgt->col, 1, "Invalid Assignment", "Cannot assign to enum constant");
        }
        else
        {
            sema_error(s, e->line, e->col, 1, "Invalid Assignment", "Target is not assignable");
            vt = type_make(TY_UNKNOWN);
        }
        Type rhs = sema_expr_with_expected(s, e->v.assign.value, vt, "Default literal requires assignment target type");
        check_int_to_ptr_safety(s, vt, rhs, e->v.assign.value);
        if (!type_assignable(s, vt, rhs))
            sema_error(s, e->v.assign.value->line, e->v.assign.value->col, 1, "Type Mismatch", type_mismatch_detail(vt, rhs));
        check_byte_literal_range(s, vt, e->v.assign.value);
        e->type = vt;
        return e->type;
    }
    case EXPR_ARRAY:
        return sema_array_literal(s, e, type_make(TY_UNKNOWN), false);
    case EXPR_DICT:
        if (sema_is_freestanding_core())
            sema_error(s, e->line, e->col, 1, "Freestanding Core", "Dictionary literals require freestanding gc runtime");
        return sema_dict_literal(s, e);
    case EXPR_CALL:
    {
        VarSym *fv = scope_find(s->scope, e->v.call.name);
        if (fv && is_function_object_type(s, fv->type))
        {
            if (e->v.call.type_args.count > 0)
                sema_error(s, e->line, e->col, 1, "Invalid Generic Call", "Function object invocation does not accept generic type arguments");
            Expr *callee = new_expr(EXPR_VAR, e->line, e->col);
            callee->v.var.name = e->v.call.name;
            ExprList args = e->v.call.args;
            NameList arg_names = e->v.call.arg_names;
            e->kind = EXPR_INVOKE;
            e->v.invoke.callee = callee;
            e->v.invoke.args = args;
            e->v.invoke.arg_names = arg_names;
            return sema_expr(s, e);
        }

        const KnStdFunc *std = resolve_std_by_local(s, e->v.call.name);
        // No longer auto-resolve builtin type names as conversions
        // Use [type](value) syntax instead
        if (std)
        {
            if (e->v.call.type_args.count > 0)
                sema_error(s, e->line, e->col, 1, "Invalid Generic Call", "Builtin function does not accept generic type arguments");
            if (call_has_named_args(&e->v.call.arg_names))
                sema_error(s, e->line, e->col, 1, "Invalid Named Argument", "Named arguments are not supported for this callable");
            if (std->kind == KN_BUILTIN_IO_ASYNC_RUN)
            {
                if (e->v.call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, std->name, 1);
                if (e->v.call.args.count == 1)
                {
                    Type at = sema_expr(s, e->v.call.args.items[0]);
                    if (!is_async_task_type(at))
                        sema_error(s, e->line, e->col, 1, "Async Run", "IO.Async.Run expects IO.Async.Task");
                    else if (e->v.call.args.items[0]->async_result_type.kind == TY_UNKNOWN)
                        sema_error(s, e->line, e->col, 1, "Async Run", "Task result type is not known here");
                    e->v.call.builtin_id = (int)std->kind;
                    e->type = e->v.call.args.items[0]->async_result_type;
                    return e->type;
                }
            }
            if (std->kind == KN_BUILTIN_IO_ASYNC_SLEEP)
            {
                if (e->v.call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, std->name, 1);
                if (e->v.call.args.count == 1)
                {
                    Type at = sema_expr_with_expected(s, e->v.call.args.items[0], type_make(TY_INT), "Default literal requires parameter type");
                    if (!type_assignable(s, type_make(TY_INT), at))
                        sema_error(s, e->v.call.args.items[0]->line, e->v.call.args.items[0]->col, 1, "Type Mismatch", type_mismatch_detail(type_make(TY_INT), at));
                }
                e->v.call.builtin_id = (int)std->kind;
                check_call_safety(s, std->safety, e->line, e->col, 1);
                return sema_mark_async_task_expr(e, type_make(TY_VOID));
            }
            if (std->kind == KN_BUILTIN_IO_ASYNC_YIELD)
            {
                if (e->v.call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, std->name, 0);
                e->v.call.builtin_id = (int)std->kind;
                check_call_safety(s, std->safety, e->line, e->col, 1);
                return sema_mark_async_task_expr(e, type_make(TY_VOID));
            }
            Type conv_dst = type_make(TY_UNKNOWN);
            int is_conv = builtin_conv_target(std->kind, &conv_dst);
            int is_vararg = builtin_is_vararg(std->kind);
            if (!is_vararg && e->v.call.args.count != std->param_count)
                sema_error_arg_count_named(s, e->line, e->col, 1, std->name, std->param_count);
            for (int i = 0; i < e->v.call.args.count; i++)
            {
                Expr *arg = e->v.call.args.items[i];
                Type pt = is_vararg ? std->params[0] : std->params[i];
                Type at = sema_builtin_arg_with_expected(s, arg, pt, "Default literal requires parameter type",
                                                         std->kind, i);
                if (is_conv && at.kind == TY_PTR && is_integer_kind(conv_dst.kind))
                    check_ptr_safety(s, arg->line, arg->col, 1, "Safe function cannot cast pointer to integer");
                check_int_to_ptr_safety(s, pt, at, arg);
                if (!type_assignable(s, pt, at))
                    sema_error(s, arg->line, arg->col, 1, "Type Mismatch", type_mismatch_detail(pt, at));
                check_byte_literal_range(s, pt, arg);
            }
            e->v.call.builtin_id = (int)std->kind;
            e->type = std->ret_type;
            check_call_safety(s, std->safety, e->line, e->col, 1);
            return e->type;
        }
        
        // Check if user is trying to use old conversion syntax and provide helpful error
        if (is_builtin_type_name(e->v.call.name))
        {
            sema_error(s, e->line, e->col, 1, "Conversion Syntax Changed", 
                       "Type conversion now use [type](value) syntax instead of type(value). "
                       "For example: [int](\"123\") instead of int(\"123\")");
            return type_make(TY_UNKNOWN);
        }

        FuncSym *fn = 0;
        if (name_has_dot(e->v.call.name))
        {
            fn = functable_find_visible_qualified(&s->funcs, s, e->v.call.name);
        }
        else
        {
            if (s->current_unit && s->current_unit[0])
                fn = functable_find_by_name_in_unit(&s->funcs, e->v.call.name, s->current_unit);
            else
                fn = functable_find_by_name_without_unit(&s->funcs, e->v.call.name);
        }
        if (!fn)
        {
            if (s->current_class && !name_has_dot(e->v.call.name))
            {
                Type mt = type_make(TY_UNKNOWN);
                if (sema_try_current_class_method_call(s, e, &mt))
                    return mt;
            }
            if (find_scoped_symbol_module(s, e->v.call.name))
                sema_error(s, e->line, e->col, 1, "Scoped Symbol Required", "Imported symbol must be called with module or alias qualifier");
            else if (find_required_namespace_module_for_func(s, e->v.call.name))
                sema_error(s, e->line, e->col, 1, "Scoped Symbol Required", "Imported module function must be called with module or alias qualifier");
            else
                sema_error(s, e->line, e->col, 1, "Unknown Function", "Function not found");
            e->type = type_make(TY_UNKNOWN);
            return e->type;
        }

        if (func_is_generic_template(fn->func))
        {
            if (e->v.call.type_args.count <= 0)
            {
                sema_error(s, e->line, e->col, 1, "Generic Function", "Generic call requires explicit type arguments");
                e->type = type_make(TY_UNKNOWN);
                return e->type;
            }
            Type *type_args = copy_and_validate_type_args(s, &e->v.call.type_args, e->line, e->col);
            Func *inst = instantiate_generic_func(s, fn->func, type_args, e->v.call.type_args.count, e->line, e->col);
            if (!inst)
            {
                e->type = type_make(TY_UNKNOWN);
                return e->type;
            }
            fn = functable_find(&s->funcs, inst->qname ? inst->qname : inst->name);
            if (!fn || !fn->func)
            {
                sema_error(s, e->line, e->col, 1, "Generic Function", "Failed to resolve instantiated generic function");
                e->type = type_make(TY_UNKNOWN);
                return e->type;
            }
        }
        else if (e->v.call.type_args.count > 0)
        {
            sema_error(s, e->line, e->col, 1, "Invalid Generic Call", "Type arguments can only be used with generic functions");
        }

        {
            const char **param_names = alloc_param_names_from_paramlist(&fn->func->params);
            (void)prepare_call_args(s, e->line, e->col, &e->v.call.args, &e->v.call.arg_names,
                                    0, fn->func->params.items, fn->func->params.count,
                                    param_names, fn->func->name, 1, 1);
            if (param_names) kn_free((void *)param_names);
        }

        for (int i = 0; i < fn->func->params.count; i++)
        {
            Expr *arg = e->v.call.args.items[i];
            Type pt = fn->func->params.items[i].type;
            Type at = sema_expr_with_expected(s, arg, pt, "Default literal requires parameter type");
            check_int_to_ptr_safety(s, pt, at, arg);
            if (!type_assignable(s, pt, at))
                sema_error(s, arg->line, arg->col, 1, "Type Mismatch", type_mismatch_detail(pt, at));
            check_byte_literal_range(s, pt, arg);
        }
        check_call_safety(s, fn->func->safety, e->line, e->col, 1);
        if (fn->func->qname)
            e->v.call.name = fn->func->qname;
        if (fn->func->is_async)
            return sema_mark_async_task_expr(e, fn->func->async_result_type);
        e->type = fn->func->ret_type;
        return e->type;
    }
    case EXPR_MEMBER_CALL:
    {
        Expr *recv = e->v.member_call.recv;
        // Parser recovery can produce member call nodes with missing member name.
        if (!e->v.member_call.name || !recv)
        {
            e->type = type_make(TY_UNKNOWN);
            return e->type;
        }
        int has_named_member = call_has_named_args(&e->v.member_call.arg_names);
        ParamList args = {0};
        args.count = e->v.member_call.args.count;
        args.cap = e->v.member_call.args.count;
        if (args.count > 0)
        {
            args.items = (Param *)kn_malloc(sizeof(Param) * (size_t)args.count);
            for (int i = 0; i < args.count; i++)
            {
                Type at = sema_expr(s, e->v.member_call.args.items[i]);
                args.items[i].type = at;
                args.items[i].name = "";
                args.items[i].src = s->src;
            }
        }
        int recv_rewritten_to_field = 0;

        // If receiver is a name, it could be a class or stdlib alias.
        if (recv && recv->kind == EXPR_VAR)
        {
            VarSym *v = scope_find(s->scope, recv->v.var.name);
            if (!v)
            {
                Type implicit_recv = type_make(TY_UNKNOWN);
                if (sema_try_current_class_field_expr(s, recv, &implicit_recv))
                {
                    if (recv->kind == EXPR_VAR)
                    {
                        e->type = implicit_recv;
                        return e->type;
                    }
                    recv_rewritten_to_field = 1;
                }
                if (!recv_rewritten_to_field)
                {
                    const KnStdFunc *std = resolve_std_by_alias(s, recv->v.var.name, e->v.member_call.name);
                    if (std)
                    {
                        if (e->v.member_call.type_args.count > 0)
                            sema_error(s, e->line, e->col, 1, "Invalid Generic Call", "Builtin function does not accept generic type arguments");
                        if (has_named_member)
                            sema_error(s, e->line, e->col, 1, "Invalid Named Argument", "Named arguments are not supported for this callable");
                        if (std->kind == KN_BUILTIN_IO_ASYNC_RUN)
                        {
                            if (e->v.member_call.args.count != 1)
                                sema_error_arg_count_named(s, e->line, e->col, 1, std->name, 1);
                            if (e->v.member_call.args.count == 1)
                            {
                                Type at = sema_expr(s, e->v.member_call.args.items[0]);
                                if (!is_async_task_type(at))
                                    sema_error(s, e->line, e->col, 1, "Async Run", "IO.Async.Run expects IO.Async.Task");
                                else if (e->v.member_call.args.items[0]->async_result_type.kind == TY_UNKNOWN)
                                    sema_error(s, e->line, e->col, 1, "Async Run", "Task result type is not known here");
                            }
                            e->v.member_call.builtin_id = (int)std->kind;
                            e->v.member_call.is_static = 1;
                            e->type = (e->v.member_call.args.count == 1)
                                ? e->v.member_call.args.items[0]->async_result_type
                                : type_make(TY_UNKNOWN);
                            check_call_safety(s, std->safety, e->line, e->col, 1);
                            return e->type;
                        }
                        if (std->kind == KN_BUILTIN_IO_ASYNC_SLEEP)
                        {
                            if (e->v.member_call.args.count != 1)
                                sema_error_arg_count_named(s, e->line, e->col, 1, std->name, 1);
                            if (e->v.member_call.args.count == 1)
                            {
                                Type at = sema_expr_with_expected(s, e->v.member_call.args.items[0], type_make(TY_INT), "Default literal requires parameter type");
                                if (!type_assignable(s, type_make(TY_INT), at))
                                    sema_error(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "Type Mismatch", type_mismatch_detail(type_make(TY_INT), at));
                            }
                            e->v.member_call.builtin_id = (int)std->kind;
                            e->v.member_call.is_static = 1;
                            check_call_safety(s, std->safety, e->line, e->col, 1);
                            return sema_mark_async_task_expr(e, type_make(TY_VOID));
                        }
                        if (std->kind == KN_BUILTIN_IO_ASYNC_YIELD)
                        {
                            if (e->v.member_call.args.count != 0)
                                sema_error_arg_count_named(s, e->line, e->col, 1, std->name, 0);
                            e->v.member_call.builtin_id = (int)std->kind;
                            e->v.member_call.is_static = 1;
                            check_call_safety(s, std->safety, e->line, e->col, 1);
                            return sema_mark_async_task_expr(e, type_make(TY_VOID));
                        }
                        Type conv_dst = type_make(TY_UNKNOWN);
                        int is_conv = builtin_conv_target(std->kind, &conv_dst);
                        int is_vararg = builtin_is_vararg(std->kind);
                        if (!is_vararg && e->v.member_call.args.count != std->param_count)
                            sema_error_arg_count_named(s, e->line, e->col, 1, std->name, std->param_count);
                        for (int i = 0; i < e->v.member_call.args.count; i++)
                        {
                            Expr *arg = e->v.member_call.args.items[i];
                            Type pt = is_vararg ? std->params[0] : std->params[i];
                            Type at = sema_builtin_arg_with_expected(s, arg, pt, "Default literal requires parameter type",
                                                                     std->kind, i);
                            if (is_conv && at.kind == TY_PTR && is_integer_kind(conv_dst.kind))
                                check_ptr_safety(s, arg->line, arg->col, 1, "Safe function cannot cast pointer to integer");
                            check_int_to_ptr_safety(s, pt, at, arg);
                            if (!type_assignable(s, pt, at))
                                sema_error(s, arg->line, arg->col, 1, "Type Mismatch", type_mismatch_detail(pt, at));
                            check_byte_literal_range(s, pt, arg);
                        }
                        e->v.member_call.builtin_id = (int)std->kind;
                        e->v.member_call.is_static = 1;
                        e->type = std->ret_type;
                        check_call_safety(s, std->safety, e->line, e->col, 1);
                        return e->type;
                    }

                    ClassDecl *c = find_visible_class(s, recv->v.var.name);
                    if (c)
                    {
                        if (e->v.member_call.type_args.count > 0)
                            sema_error(s, e->line, e->col, 1, "Unsupported Generic Method", "Generic methods are not supported");
                        ClassDecl *owner = 0;
                        int midx = -1;
                        Method *m = find_method_hierarchy_call(s, c, e->v.member_call.name,
                                                               &e->v.member_call.args, &e->v.member_call.arg_names,
                                                               &args, 1, &owner, &midx);
                        if (!m)
                        {
                            sema_error(s, e->line, e->col, 1, "Unknown Method", "Static method not found");
                            e->type = type_make(TY_UNKNOWN);
                            return e->type;
                        }
                        else
                        {
                            const char **param_names = alloc_param_names_from_paramlist(&m->params);
                            (void)prepare_call_args(s, e->line, e->col, &e->v.member_call.args, &e->v.member_call.arg_names,
                                                    &args, m->params.items, m->params.count,
                                                    param_names, m->name, 1, 1);
                            retype_prepared_args(s, &e->v.member_call.args, &args, m->params.items, m->params.count,
                                                 "Default literal requires parameter type");
                            if (param_names) kn_free((void *)param_names);
                        }
                        check_access(s, owner, m->access, e->line, e->col);
                        check_call_safety(s, m->safety, e->line, e->col, 1);
                        e->v.member_call.is_static = 1;
                        e->v.member_call.static_type = class_qname(c);
                        e->v.member_call.method_index = midx;
                        e->v.member_call.method_owner = owner ? class_qname(owner) : class_qname(c);
                        if (m->is_async)
                            return sema_mark_async_task_expr(e, m->async_result_type);
                        e->type = m->ret_type;
                        return e->type;
                    }
                }
            }
            else
            {
                v->used = 1;
            }
        }

        // Qualified call like App.Test.Test()
        if (!recv_rewritten_to_field)
        {
            const char *qname = build_qualified_call_name(s, recv, e->v.member_call.name);
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
                        if (qc)
                        {
                            if (e->v.member_call.type_args.count > 0)
                                sema_error(s, e->line, e->col, 1, "Unsupported Generic Method", "Generic methods are not supported");
                            ClassDecl *owner = 0;
                            int midx = -1;
                            Method *m = find_method_hierarchy_call(s, qc, fname,
                                                                   &e->v.member_call.args, &e->v.member_call.arg_names,
                                                                   &args, 1, &owner, &midx);
                            if (m)
                            {
                                const char **param_names = alloc_param_names_from_paramlist(&m->params);
                                (void)prepare_call_args(s, e->line, e->col, &e->v.member_call.args, &e->v.member_call.arg_names,
                                                        &args, m->params.items, m->params.count,
                                                        param_names, m->name, 1, 1);
                                retype_prepared_args(s, &e->v.member_call.args, &args, m->params.items, m->params.count,
                                                     "Default literal requires parameter type");
                                if (param_names) kn_free((void *)param_names);

                                check_access(s, owner, m->access, e->line, e->col);
                                check_call_safety(s, m->safety, e->line, e->col, 1);
                                e->v.member_call.is_static = 1;
                                e->v.member_call.static_type = class_qname(qc);
                                e->v.member_call.method_index = midx;
                                e->v.member_call.method_owner = owner ? class_qname(owner) : class_qname(qc);
                                if (m->is_async)
                                    return sema_mark_async_task_expr(e, m->async_result_type);
                                e->type = m->ret_type;
                                return e->type;
                            }
                        }

                        const char *std_mod = qc ? class_qname(qc) : mod;
                        if (kn_std_module_has(std_mod) && !std_module_accessible(s, std_mod, fname))
                            sema_error(s, e->line, e->col, 1, "Missing Get", "Standard module not imported; add a Get statement");

                        const KnStdFunc *stdq = 0;
                        if (kn_std_module_has(std_mod))
                        {
                            const char *resolved = resolve_std_symbol_in_module(s, std_mod, fname);
                            stdq = kn_std_find(std_mod, resolved);
                            if (!stdq && std_module_accessible(s, std_mod, fname))
                                sema_error(s, e->line, e->col, 1, "Unknown Symbol", "Symbol not found in module");
                        }
                        else
                        {
                            stdq = resolve_std_by_module_member(s, std_mod, fname);
                        }
                        if (stdq)
                        {
                            if (e->v.member_call.type_args.count > 0)
                                sema_error(s, e->line, e->col, 1, "Invalid Generic Call", "Builtin function does not accept generic type arguments");
                            if (has_named_member)
                                sema_error(s, e->line, e->col, 1, "Invalid Named Argument", "Named arguments are not supported for this callable");
                            if (stdq->kind == KN_BUILTIN_IO_ASYNC_RUN)
                            {
                                if (e->v.member_call.args.count != 1)
                                    sema_error_arg_count_named(s, e->line, e->col, 1, stdq->name, 1);
                                if (e->v.member_call.args.count == 1)
                                {
                                    Type at = sema_expr(s, e->v.member_call.args.items[0]);
                                    if (!is_async_task_type(at))
                                        sema_error(s, e->line, e->col, 1, "Async Run", "IO.Async.Run expects IO.Async.Task");
                                    else if (e->v.member_call.args.items[0]->async_result_type.kind == TY_UNKNOWN)
                                        sema_error(s, e->line, e->col, 1, "Async Run", "Task result type is not known here");
                                }
                                e->v.member_call.builtin_id = (int)stdq->kind;
                                e->v.member_call.is_static = 1;
                                e->type = (e->v.member_call.args.count == 1)
                                    ? e->v.member_call.args.items[0]->async_result_type
                                    : type_make(TY_UNKNOWN);
                                check_call_safety(s, stdq->safety, e->line, e->col, 1);
                                return e->type;
                            }
                            if (stdq->kind == KN_BUILTIN_IO_ASYNC_SLEEP)
                            {
                                if (e->v.member_call.args.count != 1)
                                    sema_error_arg_count_named(s, e->line, e->col, 1, stdq->name, 1);
                                if (e->v.member_call.args.count == 1)
                                {
                                    Type at = sema_expr_with_expected(s, e->v.member_call.args.items[0], type_make(TY_INT), "Default literal requires parameter type");
                                    if (!type_assignable(s, type_make(TY_INT), at))
                                        sema_error(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "Type Mismatch", type_mismatch_detail(type_make(TY_INT), at));
                                }
                                e->v.member_call.builtin_id = (int)stdq->kind;
                                e->v.member_call.is_static = 1;
                                check_call_safety(s, stdq->safety, e->line, e->col, 1);
                                return sema_mark_async_task_expr(e, type_make(TY_VOID));
                            }
                            if (stdq->kind == KN_BUILTIN_IO_ASYNC_YIELD)
                            {
                                if (e->v.member_call.args.count != 0)
                                    sema_error_arg_count_named(s, e->line, e->col, 1, stdq->name, 0);
                                e->v.member_call.builtin_id = (int)stdq->kind;
                                e->v.member_call.is_static = 1;
                                check_call_safety(s, stdq->safety, e->line, e->col, 1);
                                return sema_mark_async_task_expr(e, type_make(TY_VOID));
                            }
                            Type conv_dst = type_make(TY_UNKNOWN);
                            int is_conv = builtin_conv_target(stdq->kind, &conv_dst);
                            int is_vararg = builtin_is_vararg(stdq->kind);
                            if (!is_vararg && e->v.member_call.args.count != stdq->param_count)
                                sema_error_arg_count_named(s, e->line, e->col, 1, stdq->name, stdq->param_count);
                            for (int i = 0; i < e->v.member_call.args.count; i++)
                            {
                                Expr *arg = e->v.member_call.args.items[i];
                                Type pt = is_vararg ? stdq->params[0] : stdq->params[i];
                                Type at = sema_builtin_arg_with_expected(s, arg, pt, "Default literal requires parameter type",
                                                                         stdq->kind, i);
                                if (is_conv && at.kind == TY_PTR && is_integer_kind(conv_dst.kind))
                                    check_ptr_safety(s, arg->line, arg->col, 1, "Safe function cannot cast pointer to integer");
                                check_int_to_ptr_safety(s, pt, at, arg);
                                if (!type_assignable(s, pt, at))
                                    sema_error(s, arg->line, arg->col, 1, "Type Mismatch", type_mismatch_detail(pt, at));
                                check_byte_literal_range(s, pt, arg);
                            }
                            e->v.member_call.builtin_id = (int)stdq->kind;
                            e->v.member_call.is_static = 1;
                            e->type = stdq->ret_type;
                            check_call_safety(s, stdq->safety, e->line, e->col, 1);
                            return e->type;
                        }
                        if (qc)
                        {
                            sema_error(s, e->line, e->col, 1, "Unknown Method", "Static method not found");
                            e->type = type_make(TY_UNKNOWN);
                            return e->type;
                        }
                    }
                }

                FuncSym *qfn = functable_find_visible_qualified(&s->funcs, s, qname);
                if (qfn)
                {
                    if (func_is_generic_template(qfn->func))
                    {
                        if (e->v.member_call.type_args.count <= 0)
                        {
                            sema_error(s, e->line, e->col, 1, "Generic Function", "Generic call requires explicit type arguments");
                            e->type = type_make(TY_UNKNOWN);
                            return e->type;
                        }
                        Type *type_args = copy_and_validate_type_args(s, &e->v.member_call.type_args, e->line, e->col);
                        Func *inst = instantiate_generic_func(s, qfn->func, type_args, e->v.member_call.type_args.count, e->line, e->col);
                        if (!inst)
                        {
                            e->type = type_make(TY_UNKNOWN);
                            return e->type;
                        }
                        qfn = functable_find(&s->funcs, inst->qname ? inst->qname : inst->name);
                        if (!qfn || !qfn->func)
                        {
                            sema_error(s, e->line, e->col, 1, "Generic Function", "Failed to resolve instantiated generic function");
                            e->type = type_make(TY_UNKNOWN);
                            return e->type;
                        }
                    }
                    else if (e->v.member_call.type_args.count > 0)
                    {
                        sema_error(s, e->line, e->col, 1, "Invalid Generic Call", "Type arguments can only be used with generic functions");
                    }

                    {
                        const char **param_names = alloc_param_names_from_paramlist(&qfn->func->params);
                        (void)prepare_call_args(s, e->line, e->col, &e->v.member_call.args, &e->v.member_call.arg_names,
                                                &args, qfn->func->params.items, qfn->func->params.count,
                                                param_names, qfn->func->name, 1, 1);
                        retype_prepared_args(s, &e->v.member_call.args, &args, qfn->func->params.items, qfn->func->params.count,
                                             "Default literal requires parameter type");
                        if (param_names) kn_free((void *)param_names);
                    }

                    for (int i = 0; i < qfn->func->params.count; i++)
                    {
                        Expr *arg = e->v.member_call.args.items[i];
                        Type pt = qfn->func->params.items[i].type;
                        Type at = args.items[i].type;
                        check_int_to_ptr_safety(s, pt, at, arg);
                        if (!type_assignable(s, pt, at))
                            sema_error(s, arg->line, arg->col, 1, "Type Mismatch", type_mismatch_detail(pt, at));
                        check_byte_literal_range(s, pt, arg);
                    }
                    check_call_safety(s, qfn->func->safety, e->line, e->col, 1);
                    e->v.member_call.is_qual_call = 1;
                    e->v.member_call.qual_name = qfn->func && qfn->func->qname ? qfn->func->qname : qname;
                    e->type = qfn->func->ret_type;
                    return e->type;
                }
                {
                    FuncSym *hidden = functable_find(&s->funcs, qname);
                    if (hidden && hidden->func && hidden->func->unit && import_has_alias_for_module(s, hidden->func->unit))
                    {
                        sema_error(s, e->line, e->col, 1, "Scoped Symbol Required", "Imported module function must be called with module or alias qualifier");
                        e->type = type_make(TY_UNKNOWN);
                        return e->type;
                    }
                }
            }
        }

        if (e->v.member_call.type_args.count > 0)
            sema_error(s, e->line, e->col, 1, "Invalid Generic Call", "Type arguments are only valid for generic function calls");

        Type rt = type_make(TY_UNKNOWN);
        if (recv)
        {
            if (recv->kind == EXPR_VAR)
            {
                Type implicit_recv = type_make(TY_UNKNOWN);
                if (!scope_find(s->scope, recv->v.var.name) &&
                    sema_try_current_class_field_expr(s, recv, &implicit_recv))
                {
                    if (recv->kind == EXPR_VAR)
                    {
                        e->type = implicit_recv;
                        return e->type;
                    }
                }
            }
            if (recv->kind == EXPR_VAR)
            {
                VarSym *v = scope_find(s->scope, recv->v.var.name);
                if (!v)
                {
                    sema_error(s, recv->line, recv->col, 1, "Unknown Variable", "Variable not defined");
                    e->type = type_make(TY_UNKNOWN);
                    return e->type;
                }
                rt = v->type;
            }
            else
            {
                rt = sema_expr(s, recv);
            }
        }

        if (is_block_object_type(rt))
        {
            if (kn_strcmp(e->v.member_call.name, "Run") == 0)
            {
                if (sema_is_freestanding_core())
                    sema_error(s, e->line, e->col, 1, "Freestanding Core", "Block runtime is not available in freestanding core");
                (void)remap_named_args(s, e->line, e->col, &e->v.member_call.args, &e->v.member_call.arg_names,
                                       &args, 0, 0);
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Run", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_BLOCK_RUN;
                e->type = type_make(TY_VOID);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Jump") == 0)
            {
                if (sema_is_freestanding_core())
                    sema_error(s, e->line, e->col, 1, "Freestanding Core", "Block runtime is not available in freestanding core");
                static const char *param_names[] = { "target" };
                (void)remap_named_args(s, e->line, e->col, &e->v.member_call.args, &e->v.member_call.arg_names,
                                       &args, 1, param_names);
                if (e->v.member_call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Jump", 1);
                if (e->v.member_call.args.count == 1 && !is_integer_kind(args.items[0].type.kind))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "Jump", "target", "integer ip");
                e->v.member_call.builtin_id = KN_BUILTIN_BLOCK_JUMP;
                e->type = type_make(TY_VOID);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "RunUntil") == 0)
            {
                if (sema_is_freestanding_core())
                    sema_error(s, e->line, e->col, 1, "Freestanding Core", "Block runtime is not available in freestanding core");
                static const char *param_names[] = { "target" };
                (void)remap_named_args(s, e->line, e->col, &e->v.member_call.args, &e->v.member_call.arg_names,
                                       &args, 1, param_names);
                if (e->v.member_call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "RunUntil", 1);
                if (e->v.member_call.args.count == 1 && !is_integer_kind(args.items[0].type.kind))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "RunUntil", "target", "integer ip");
                e->v.member_call.builtin_id = KN_BUILTIN_BLOCK_RUN_UNTIL;
                e->type = type_make(TY_VOID);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "JumpAndRunUntil") == 0)
            {
                if (sema_is_freestanding_core())
                    sema_error(s, e->line, e->col, 1, "Freestanding Core", "Block runtime is not available in freestanding core");
                static const char *param_names[] = { "start", "end" };
                (void)remap_named_args(s, e->line, e->col, &e->v.member_call.args, &e->v.member_call.arg_names,
                                       &args, 2, param_names);
                if (e->v.member_call.args.count != 2)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "JumpAndRunUntil", 2);
                if (e->v.member_call.args.count >= 1 && !is_integer_kind(args.items[0].type.kind))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "JumpAndRunUntil", "start", "integer ip");
                if (e->v.member_call.args.count >= 2 && !is_integer_kind(args.items[1].type.kind))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[1]->line, e->v.member_call.args.items[1]->col, 1, "JumpAndRunUntil", "end", "integer ip");
                e->v.member_call.builtin_id = KN_BUILTIN_BLOCK_JUMP_AND_RUN_UNTIL;
                e->type = type_make(TY_VOID);
                return e->type;
            }
        }

        if (has_named_member)
            sema_error(s, e->line, e->col, 1, "Invalid Named Argument", "Named arguments are not supported for this callable");

        if (kn_strcmp(e->v.member_call.name, "TypeOf") == 0)
        {
            if (e->v.member_call.args.count != 0)
                sema_error_arg_count_named(s, e->line, e->col, 1, "TypeOf", 0);
            const char *tn = type_name(rt);
            e->kind = EXPR_STRING;
            e->v.str_val.ptr = tn;
            e->v.str_val.len = (int)kn_strlen(tn);
            e->type = type_make(TY_STRING);
            return e->type;
        }

        if (kn_strcmp(e->v.member_call.name, "IsNull") == 0)
        {
            if (e->v.member_call.args.count != 0)
                sema_error_arg_count_named(s, e->line, e->col, 1, "IsNull", 0);
            e->v.member_call.builtin_id = KN_BUILTIN_IO_ANY_ISNULL;
            e->type = type_make(TY_BOOL);
            return e->type;
        }
        if (kn_strcmp(e->v.member_call.name, "ToString") == 0)
        {
            if (e->v.member_call.args.count != 0)
                sema_error_arg_count_named(s, e->line, e->col, 1, "ToString", 0);
            e->v.member_call.builtin_id = KN_BUILTIN_IO_ANY_TOSTRING;
            e->type = type_make(TY_STRING);
            return e->type;
        }
        if (kn_strcmp(e->v.member_call.name, "Equals") == 0)
        {
            if (e->v.member_call.args.count != 1)
                sema_error_arg_count_named(s, e->line, e->col, 1, "Equals", 1);
            e->v.member_call.builtin_id = KN_BUILTIN_IO_ANY_EQUALS;
            e->type = type_make(TY_BOOL);
            return e->type;
        }
        if (kn_strcmp(e->v.member_call.name, "TypeName") == 0)
        {
            if (e->v.member_call.args.count != 0)
                sema_error_arg_count_named(s, e->line, e->col, 1, "TypeName", 0);
            e->v.member_call.builtin_id = KN_BUILTIN_IO_ANY_TYPENAME;
            e->type = type_make(TY_STRING);
            return e->type;
        }

        if (rt.kind == TY_ANY)
        {
            if (kn_strcmp(e->v.member_call.name, "Tag") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Tag", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_ANY_TAG;
                e->type = type_make(TY_INT);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "IsNumber") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "IsNumber", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_ANY_ISNUMBER;
                e->type = type_make(TY_BOOL);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "IsPointer") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "IsPointer", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_ANY_ISPOINTER;
                e->type = type_make(TY_BOOL);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "IsInt") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "IsInt", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_ANY_ISINT;
                e->type = type_make(TY_BOOL);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "IsFloat") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "IsFloat", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_ANY_ISFLOAT;
                e->type = type_make(TY_BOOL);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "IsString") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "IsString", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_ANY_ISSTRING;
                e->type = type_make(TY_BOOL);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "IsBool") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "IsBool", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_ANY_ISBOOL;
                e->type = type_make(TY_BOOL);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "IsChar") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "IsChar", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_ANY_ISCHAR;
                e->type = type_make(TY_BOOL);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "IsObject") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "IsObject", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_ANY_ISOBJECT;
                e->type = type_make(TY_BOOL);
                return e->type;
            }
        }

        if (rt.kind == TY_STRING && kn_strcmp(e->v.member_call.name, "ToChars") == 0)
        {
            if (e->v.member_call.args.count != 0)
                sema_error_arg_count_named(s, e->line, e->col, 1, "ToChars", 0);
            if (sema_is_freestanding_core())
                sema_error(s, e->line, e->col, 1, "Freestanding Core", "ToChars requires allocation and is not available in freestanding core");
            e->v.member_call.builtin_id = KN_BUILTIN_IO_STRING_TOCHARS;
            e->type = type_array(TY_CHAR, -1);
            return e->type;
        }

        if (rt.kind == TY_STRING && kn_strcmp(e->v.member_call.name, "Length") == 0)
        {
            if (e->v.member_call.args.count != 0)
                sema_error_arg_count_named(s, e->line, e->col, 1, "Length", 0);
            e->v.member_call.builtin_id = KN_BUILTIN_IO_STRING_LENGTH;
            e->type = type_make(TY_INT);
            return e->type;
        }

        if (rt.kind == TY_STRING && kn_strcmp(e->v.member_call.name, "Index") == 0)
        {
            if (e->v.member_call.args.count != 1)
                sema_error_arg_count_named(s, e->line, e->col, 1, "Index", 1);
            Type idx_type = args.items[0].type;
            if (!is_integer_kind(idx_type.kind))
                sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "Index", "argument", "integer");
            e->v.member_call.builtin_id = KN_BUILTIN_STRING_INDEX;
            e->type = type_make(TY_CHAR);
            return e->type;
        }

        if (rt.kind == TY_ARRAY && kn_strcmp(e->v.member_call.name, "Length") == 0)
        {
            if (e->v.member_call.args.count != 0)
                sema_error_arg_count_named(s, e->line, e->col, 1, "Length", 0);
            e->v.member_call.builtin_id = KN_BUILTIN_ARRAY_LENGTH;
            e->type = type_make(TY_INT);
            return e->type;
        }
        if (rt.kind == TY_ARRAY && kn_strcmp(e->v.member_call.name, "Index") == 0)
        {
            if (e->v.member_call.args.count != 1)
                sema_error_arg_count_named(s, e->line, e->col, 1, "Index", 1);
            if (e->v.member_call.args.count == 1 && !is_integer_kind(args.items[0].type.kind))
                sema_error_type_must_be_named(s, e->line, e->col, 1, "Index", "argument", "integer");
            Expr *recv_expr = e->v.member_call.recv;
            Expr *idx_expr = e->v.member_call.args.items[0];
            e->kind = EXPR_INDEX;
            e->v.index.recv = recv_expr;
            e->v.index.index = idx_expr;
            e->type = type_make(rt.elem);
            e->type.name = rt.name;
            return e->type;
        }
        if (rt.kind == TY_ARRAY && kn_strcmp(e->v.member_call.name, "Add") == 0)
        {
            if (sema_is_freestanding_core())
                sema_error(s, e->line, e->col, 1, "Freestanding Core", "Dynamic array Add requires freestanding gc runtime");
            if (e->v.member_call.args.count != 1)
                sema_error_arg_count_named(s, e->line, e->col, 1, "Add", 1);
            if (e->v.member_call.args.count == 1)
            {
                Type elem = type_make(rt.elem);
                elem.name = rt.name;
                Type at = sema_expr_with_expected(s, e->v.member_call.args.items[0], elem,
                                                  "Default literal requires array element type");
                if (!type_assignable(s, elem, at))
                    sema_error(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1,
                               "Type Mismatch", type_mismatch_detail(elem, at));
                check_byte_literal_range(s, elem, e->v.member_call.args.items[0]);
            }
            e->v.member_call.builtin_id = KN_BUILTIN_ARRAY_ADD;
            e->type = rt;
            if (e->type.array_len >= 0)
                e->type.array_len += 1;
            return e->type;
        }

        if (rt.kind == TY_CLASS && rt.name && kn_strcmp(rt.name, "IO.Collection.dict") == 0)
        {
            if (kn_strcmp(e->v.member_call.name, "Set") == 0)
            {
                if (e->v.member_call.args.count != 2)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Set", 2);
                if (e->v.member_call.args.count >= 1 && !type_assignable(s, type_make(TY_STRING), args.items[0].type))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "Set", "key", "string");
                e->v.member_call.builtin_id = KN_BUILTIN_IO_DICT_SET;
                e->type = type_make(TY_VOID);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Fetch") == 0)
            {
                if (e->v.member_call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Fetch", 1);
                if (e->v.member_call.args.count == 1 && !type_assignable(s, type_make(TY_STRING), args.items[0].type))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "Fetch", "key", "string");
                e->v.member_call.builtin_id = KN_BUILTIN_IO_DICT_GET;
                e->type = type_make(TY_ANY);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Contains") == 0)
            {
                if (e->v.member_call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Contains", 1);
                if (e->v.member_call.args.count == 1 && !type_assignable(s, type_make(TY_STRING), args.items[0].type))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "Contains", "key", "string");
                e->v.member_call.builtin_id = KN_BUILTIN_IO_DICT_CONTAINS;
                e->type = type_make(TY_BOOL);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "TryFetch") == 0)
            {
                if (e->v.member_call.args.count != 2)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "TryFetch", 2);
                if (e->v.member_call.args.count >= 1 && !type_assignable(s, type_make(TY_STRING), args.items[0].type))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "TryFetch", "key", "string");
                e->v.member_call.builtin_id = KN_BUILTIN_IO_DICT_TRY_FETCH;
                e->type = type_make(TY_ANY);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Count") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Count", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_DICT_COUNT;
                e->type = type_make(TY_INT);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Remove") == 0)
            {
                if (e->v.member_call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Remove", 1);
                if (e->v.member_call.args.count == 1 && !type_assignable(s, type_make(TY_STRING), args.items[0].type))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "Remove", "key", "string");
                e->v.member_call.builtin_id = KN_BUILTIN_IO_DICT_REMOVE;
                e->type = type_make(TY_BOOL);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "IsEmpty") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "IsEmpty", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_DICT_IS_EMPTY;
                e->type = type_make(TY_BOOL);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Keys") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Keys", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_DICT_KEYS;
                e->type = type_class("IO.Collection.list");
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Values") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Values", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_DICT_VALUES;
                e->type = type_class("IO.Collection.list");
                lower_member_builtin_to_call(e);
                return e->type;
            }
        }

        if (rt.kind == TY_CLASS && rt.name && kn_strcmp(rt.name, "IO.Collection.list") == 0)
        {
            if (kn_strcmp(e->v.member_call.name, "Add") == 0)
            {
                if (e->v.member_call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Add", 1);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_LIST_ADD;
                e->type = type_make(TY_VOID);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Fetch") == 0)
            {
                if (e->v.member_call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Fetch", 1);
                if (e->v.member_call.args.count == 1 && !is_integer_kind(args.items[0].type.kind))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "Fetch", "index", "integer");
                e->v.member_call.builtin_id = KN_BUILTIN_IO_LIST_GET;
                e->type = type_make(TY_ANY);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Set") == 0)
            {
                if (e->v.member_call.args.count != 2)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Set", 2);
                if (e->v.member_call.args.count >= 1 && !is_integer_kind(args.items[0].type.kind))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "Set", "index", "integer");
                e->v.member_call.builtin_id = KN_BUILTIN_IO_LIST_SET;
                e->type = type_make(TY_VOID);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Insert") == 0)
            {
                if (e->v.member_call.args.count != 2)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Insert", 2);
                if (e->v.member_call.args.count >= 1 && !is_integer_kind(args.items[0].type.kind))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "Insert", "index", "integer");
                e->v.member_call.builtin_id = KN_BUILTIN_IO_LIST_INSERT;
                e->type = type_make(TY_VOID);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "TryFetch") == 0)
            {
                if (e->v.member_call.args.count != 2)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "TryFetch", 2);
                if (e->v.member_call.args.count >= 1 && !is_integer_kind(args.items[0].type.kind))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "TryFetch", "index", "integer");
                e->v.member_call.builtin_id = KN_BUILTIN_IO_LIST_TRY_FETCH;
                e->type = type_make(TY_ANY);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Count") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Count", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_LIST_COUNT;
                e->type = type_make(TY_INT);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Clear") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Clear", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_LIST_CLEAR;
                e->type = type_make(TY_VOID);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "RemoveAt") == 0)
            {
                if (e->v.member_call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "RemoveAt", 1);
                if (e->v.member_call.args.count == 1 && !is_integer_kind(args.items[0].type.kind))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "RemoveAt", "index", "integer");
                e->v.member_call.builtin_id = KN_BUILTIN_IO_LIST_REMOVE_AT;
                e->type = type_make(TY_ANY);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Contains") == 0)
            {
                if (e->v.member_call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Contains", 1);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_LIST_CONTAINS;
                e->type = type_make(TY_BOOL);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Pop") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Pop", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_LIST_POP;
                e->type = type_make(TY_ANY);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Peek") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Peek", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_LIST_PEEK;
                e->type = type_make(TY_ANY);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "IndexOf") == 0)
            {
                if (e->v.member_call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "IndexOf", 1);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_LIST_INDEX_OF;
                e->type = type_make(TY_INT);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "IsEmpty") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "IsEmpty", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_LIST_IS_EMPTY;
                e->type = type_make(TY_BOOL);
                lower_member_builtin_to_call(e);
                return e->type;
            }
        }

        if (rt.kind == TY_CLASS && rt.name && kn_strcmp(rt.name, "IO.Collection.set") == 0)
        {
            if (kn_strcmp(e->v.member_call.name, "Add") == 0)
            {
                if (e->v.member_call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Add", 1);
                if (e->v.member_call.args.count == 1 && !type_assignable(s, type_make(TY_STRING), args.items[0].type))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "Add", "value", "string");
                e->v.member_call.builtin_id = KN_BUILTIN_IO_SET_ADD;
                e->type = type_make(TY_VOID);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Contains") == 0)
            {
                if (e->v.member_call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Contains", 1);
                if (e->v.member_call.args.count == 1 && !type_assignable(s, type_make(TY_STRING), args.items[0].type))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "Contains", "value", "string");
                e->v.member_call.builtin_id = KN_BUILTIN_IO_SET_CONTAINS;
                e->type = type_make(TY_BOOL);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Count") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Count", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_SET_COUNT;
                e->type = type_make(TY_INT);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Remove") == 0)
            {
                if (e->v.member_call.args.count != 1)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Remove", 1);
                if (e->v.member_call.args.count == 1 && !type_assignable(s, type_make(TY_STRING), args.items[0].type))
                    sema_error_type_must_be_named(s, e->v.member_call.args.items[0]->line, e->v.member_call.args.items[0]->col, 1, "Remove", "value", "string");
                e->v.member_call.builtin_id = KN_BUILTIN_IO_SET_REMOVE;
                e->type = type_make(TY_BOOL);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Clear") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Clear", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_SET_CLEAR;
                e->type = type_make(TY_VOID);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "IsEmpty") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "IsEmpty", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_SET_IS_EMPTY;
                e->type = type_make(TY_BOOL);
                lower_member_builtin_to_call(e);
                return e->type;
            }
            if (kn_strcmp(e->v.member_call.name, "Values") == 0)
            {
                if (e->v.member_call.args.count != 0)
                    sema_error_arg_count_named(s, e->line, e->col, 1, "Values", 0);
                e->v.member_call.builtin_id = KN_BUILTIN_IO_SET_VALUES;
                e->type = type_class("IO.Collection.list");
                lower_member_builtin_to_call(e);
                return e->type;
            }
        }

        if (rt.kind != TY_CLASS)
        {
            sema_error(s, e->line, e->col, 1, "Invalid Member", "Member call requires class instance");
            e->type = type_make(TY_UNKNOWN);
            return e->type;
        }
        ClassDecl *c = find_class(s, rt.name);
        if (!c)
        {
            InterfaceDecl *iface = find_interface(s, rt.name);
            if (iface)
            {
                int midx = -1;
                Method *im = 0;
                for (int i = 0; i < iface->methods.count; i++)
                {
                    Method *m = &iface->methods.items[i];
                    if (kn_strcmp(m->name, e->v.member_call.name) != 0) continue;
                    if (call_args_match_params(s, e->line, e->col, &e->v.member_call.args,
                                               &e->v.member_call.arg_names, &args, &m->params))
                    {
                        im = m;
                        midx = i;
                        break;
                    }
                }
                if (!im)
                {
                    sema_error(s, e->line, e->col, 1, "Unknown Method", "Interface method not found");
                    e->type = type_make(TY_UNKNOWN);
                    return e->type;
                }
                {
                    const char **param_names = alloc_param_names_from_paramlist(&im->params);
                    (void)prepare_call_args(s, e->line, e->col, &e->v.member_call.args, &e->v.member_call.arg_names,
                                            &args, im->params.items, im->params.count,
                                            param_names, im->name, 1, 1);
                    retype_prepared_args(s, &e->v.member_call.args, &args, im->params.items, im->params.count,
                                         "Default literal requires parameter type");
                    if (param_names) kn_free((void *)param_names);
                }
                check_call_safety(s, im->safety, e->line, e->col, 1);
                e->v.member_call.is_static = 0;
                e->v.member_call.method_index = midx;
                e->v.member_call.method_owner = interface_qname(iface);
                if (im->is_async)
                    return sema_mark_async_task_expr(e, im->async_result_type);
                e->type = im->ret_type;
                return e->type;
            }
            sema_error(s, e->line, e->col, 1, "Unknown Type", "Class not found");
            e->type = type_make(TY_UNKNOWN);
            return e->type;
        }
        ClassDecl *owner = 0;
        int midx = -1;
        Method *m = find_method_hierarchy_call(s, c, e->v.member_call.name,
                                               &e->v.member_call.args, &e->v.member_call.arg_names,
                                               &args, 0, &owner, &midx);
        if (!m)
        {
            sema_error(s, e->line, e->col, 1, "Unknown Method", "Method not found");
            e->type = type_make(TY_UNKNOWN);
            return e->type;
        }
        {
            const char **param_names = alloc_param_names_from_paramlist(&m->params);
            (void)prepare_call_args(s, e->line, e->col, &e->v.member_call.args, &e->v.member_call.arg_names,
                                    &args, m->params.items, m->params.count,
                                    param_names, m->name, 1, 1);
            retype_prepared_args(s, &e->v.member_call.args, &args, m->params.items, m->params.count,
                                 "Default literal requires parameter type");
            if (param_names) kn_free((void *)param_names);
        }
        if (m->is_static)
            sema_error(s, e->line, e->col, 1, "Invalid Call", "Static method requires class name");
        check_access(s, owner, m->access, e->line, e->col);
        check_call_safety(s, m->safety, e->line, e->col, 1);
        e->v.member_call.is_static = 0;
        e->v.member_call.method_index = midx;
        e->v.member_call.method_owner = owner ? class_qname(owner) : class_qname(c);
        if (m->is_async)
            return sema_mark_async_task_expr(e, m->async_result_type);
        e->type = m->ret_type;
        return e->type;
    }
    case EXPR_BINARY:
    {
        Type lt = sema_expr(s, e->v.binary.left);
        Type rt = sema_expr(s, e->v.binary.right);
        TokenType op = (TokenType)e->v.binary.op;
        if (op == TOK_PLUS || op == TOK_MINUS)
        {
            if (lt.kind == TY_PTR && is_integer_kind(rt.kind))
            {
                check_ptr_safety(s, e->line, e->col, 1, "Safe function cannot use pointer arithmetic");
                e->type = lt;
                return e->type;
            }
            if (op == TOK_PLUS && rt.kind == TY_PTR && is_integer_kind(lt.kind))
            {
                check_ptr_safety(s, e->line, e->col, 1, "Safe function cannot use pointer arithmetic");
                e->type = rt;
                return e->type;
            }
        }
        if (op == TOK_PLUS || op == TOK_MINUS || op == TOK_STAR || op == TOK_SLASH || op == TOK_PERCENT)
        {
            bool lhs_cstr = (lt.kind == TY_PTR && lt.elem == TY_CHAR && lt.ptr_depth == 1);
            bool rhs_cstr = (rt.kind == TY_PTR && rt.elem == TY_CHAR && rt.ptr_depth == 1);
            bool lhs_concat_text = (lt.kind == TY_STRING) || lhs_cstr;
            bool rhs_concat_text = (rt.kind == TY_STRING) || rhs_cstr;
            bool lhs_stringify = lt.kind != TY_UNKNOWN && lt.kind != TY_VOID;
            bool rhs_stringify = rt.kind != TY_UNKNOWN && rt.kind != TY_VOID;
            if (op == TOK_PLUS &&
                ((lhs_concat_text && rhs_stringify) || (rhs_concat_text && lhs_stringify)) &&
                !(lhs_cstr && rhs_cstr))
            {
                e->type = type_make(TY_STRING);
                return e->type;
            }
            if (!is_numeric_kind(lt.kind) || !is_numeric_kind(rt.kind))
                sema_error(s, e->line, e->col, 1, "Type Mismatch", "Arithmetic operators require numeric types");
            if (lt.kind == rt.kind)
            {
                e->type = lt;
                return e->type;
            }
            if (is_float_kind(lt.kind) || is_float_kind(rt.kind))
            {
                if (is_float_kind(lt.kind) && is_float_kind(rt.kind))
                {
                    int lb = float_bits(lt.kind);
                    int rb = float_bits(rt.kind);
                    e->type = lb >= rb ? lt : rt;
                }
                else if (is_float_kind(lt.kind))
                    e->type = lt;
                else
                    e->type = rt;
                return e->type;
            }
            // allow mixed integer widths with implicit cast to left
            e->type = lt;
            return e->type;
        }
        if (op == TOK_AMP || op == TOK_BITOR || op == TOK_XOR || op == TOK_SHL || op == TOK_SHR)
        {
            if (!is_integer_kind(lt.kind) || !is_integer_kind(rt.kind))
                sema_error(s, e->line, e->col, 1, "Type Mismatch", "Bitwise operators require integer types");
            int lb = int_bits(lt.kind);
            int rb = int_bits(rt.kind);
            int bits = lb > rb ? lb : rb;
            bool u = is_unsigned_kind(lt.kind) || is_unsigned_kind(rt.kind);
            TypeKind rk;
            if (bits <= 8) rk = u ? TY_U8 : TY_I8;
            else if (bits <= 16) rk = u ? TY_U16 : TY_I16;
            else if (bits <= 32) rk = u ? TY_U32 : TY_I32;
            else rk = u ? TY_U64 : TY_INT;
            e->type = type_make(rk);
            return e->type;
        }
        if (op == TOK_EQ || op == TOK_NE)
        {
            if ((lt.kind == TY_NULL && (rt.kind == TY_PTR || rt.kind == TY_CLASS || rt.kind == TY_STRING || rt.kind == TY_NULL)) ||
                (rt.kind == TY_NULL && (lt.kind == TY_PTR || lt.kind == TY_CLASS || lt.kind == TY_STRING || lt.kind == TY_NULL)))
            {
                e->type = type_make(TY_BOOL);
                return e->type;
            }
            if (lt.kind == TY_STRING && rt.kind == TY_STRING)
            {
                e->type = type_make(TY_BOOL);
                return e->type;
            }
            if (lt.kind == TY_PTR && rt.kind == TY_PTR && type_equal(lt, rt))
            {
                e->type = type_make(TY_BOOL);
                return e->type;
            }
            if (!(type_equal(lt, rt) || (is_numeric_kind(lt.kind) && is_numeric_kind(rt.kind))))
                sema_error(s, e->line, e->col, 1, "Type Mismatch", type_mismatch_detail(lt, rt));
            e->type = type_make(TY_BOOL);
            return e->type;
        }
        if (op == TOK_LT || op == TOK_LE || op == TOK_GT || op == TOK_GE)
        {
            if (!is_numeric_kind(lt.kind) || !is_numeric_kind(rt.kind))
                sema_error(s, e->line, e->col, 1, "Type Mismatch", "Comparison requires numeric types");
            e->type = type_make(TY_BOOL);
            return e->type;
        }
        if (op == TOK_ANDAND || op == TOK_OROR)
        {
            if (lt.kind != TY_BOOL || rt.kind != TY_BOOL)
                sema_error(s, e->line, e->col, 1, "Type Mismatch", "Logical operators require bool");
            e->type = type_make(TY_BOOL);
            return e->type;
        }
        sema_error(s, e->line, e->col, 1, "Invalid Operator", "Unknown binary operator");
        return type_make(TY_UNKNOWN);
    }
    case EXPR_IF:
    {
        Type ct = sema_expr(s, e->v.if_expr.cond);
        if (!(ct.kind == TY_BOOL || is_integer_kind(ct.kind)))
            sema_error(s, e->line, e->col, 1, "Type Mismatch", "If expression condition must be bool or integer");
        Type tt = sema_expr(s, e->v.if_expr.then_expr);
        Type et = sema_expr(s, e->v.if_expr.else_expr);
        e->type = merge_branch_types(s, e->line, e->col,
                                     e->v.if_expr.then_expr, tt,
                                     e->v.if_expr.else_expr, et,
                                     "If expression branches must have compatible types");
        return e->type;
    }
    case EXPR_SWITCH:
    {
        Type vt = sema_expr(s, e->v.switch_expr.value);
        Type result = type_make(TY_UNKNOWN);
        int default_count = 0;
        for (int i = 0; i < e->v.switch_expr.cases.count; i++)
        {
            ExprSwitchCase *c = &e->v.switch_expr.cases.items[i];
            if (c->is_default)
            {
                default_count++;
            }
            else
            {
                Type mt = sema_expr(s, c->match);
                if (!types_support_switch_match(vt, mt))
                    sema_error(s, c->match->line, c->match->col, 1, "Type Mismatch",
                               "Switch case value is not comparable to Switch value");
            }

            Type bt = sema_expr(s, c->body);
            if (result.kind == TY_UNKNOWN)
                result = bt;
            else
                result = merge_branch_types(s, e->line, e->col, c->body, bt, 0, result,
                                            "Switch expression cases must have compatible types");
        }
        if (default_count == 0)
            sema_error(s, e->line, e->col, 1, "Missing Default Case", "Switch expression requires Case(default)");
        if (default_count > 1)
            sema_error(s, e->line, e->col, 1, "Duplicate Default Case", "Switch expression can only have one Case(default)");
        if (result.kind == TY_UNKNOWN)
            sema_error(s, e->line, e->col, 1, "Default Literal", "Switch expression requires at least one typed case body");
        if (result.kind != TY_UNKNOWN)
        {
            for (int i = 0; i < e->v.switch_expr.cases.count; i++)
                bind_default_literal_type(s, e->v.switch_expr.cases.items[i].body, result,
                                          "Default literal requires Switch expression result type");
        }
        e->type = result;
        return e->type;
    }
    case EXPR_IS:
    {
        Type value_type = sema_expr(s, e->v.is_expr.expr);
        Type target = e->v.is_expr.target;
        validate_type_ref(s, &target, e->line, e->col, 0);
        e->v.is_expr.target = target;
        if (target.kind == TY_VOID)
            sema_error(s, e->line, e->col, 1, "Invalid Is", "Is target type cannot be void");
        if (value_type.kind == TY_ANY && !any_is_supported_target(target))
            sema_error(s, e->line, e->col, 1, "Invalid Is",
                       "any only supports Is with primitive, enum, string, or any targets");
        e->type = type_make(TY_BOOL);
        return e->type;
    }
    case EXPR_UNARY:
    {
        Type vt = sema_expr(s, e->v.unary.expr);
        if (e->v.unary.op == TOK_PLUSPLUS || e->v.unary.op == TOK_MINUSMINUS)
        {
            if (!is_lvalue_expr(e->v.unary.expr))
                sema_error(s, e->line, e->col, 1, "Invalid Assignment", "Increment/decrement target must be assignable");
            if (e->v.unary.expr->kind == EXPR_VAR)
            {
                VarSym *v = scope_find(s->scope, e->v.unary.expr->v.var.name);
                if (v && v->is_const)
                    sema_error(s, e->line, e->col, 1, "Const Assignment", "Cannot modify const variable");
            }
            if (e->v.unary.expr->kind == EXPR_MEMBER && e->v.unary.expr->v.member.is_enum_const)
                sema_error(s, e->line, e->col, 1, "Invalid Assignment", "Cannot modify enum constant");
            if (!(is_numeric_kind(vt.kind) || vt.kind == TY_PTR))
                sema_error(s, e->line, e->col, 1, "Type Mismatch", "Increment/decrement requires numeric or pointer type");
            if (vt.kind == TY_PTR)
                check_ptr_safety(s, e->line, e->col, 1, "Safe function cannot modify pointer with increment/decrement");
            e->type = vt;
            return e->type;
        }
        if (e->v.unary.op == TOK_MINUS)
        {
            if (!is_numeric_kind(vt.kind))
                sema_error(s, e->line, e->col, 1, "Type Mismatch", "Unary '-' requires numeric type");
            e->type = vt;
            return e->type;
        }
        if (e->v.unary.op == TOK_NOT)
        {
            if (vt.kind != TY_BOOL)
                sema_error(s, e->line, e->col, 1, "Type Mismatch", "Unary '!' requires bool");
            e->type = type_make(TY_BOOL);
            return e->type;
        }
        if (e->v.unary.op == TOK_TILDE)
        {
            if (!is_integer_kind(vt.kind))
                sema_error(s, e->line, e->col, 1, "Type Mismatch", "Unary '~' requires integer type");
            e->type = vt;
            return e->type;
        }
        if (e->v.unary.op == TOK_STAR)
        {
            if (vt.kind != TY_PTR)
                sema_error(s, e->line, e->col, 1, "Type Mismatch", "Unary '*' requires pointer type");
            check_ptr_safety(s, e->line, e->col, 1, "Safe function cannot dereference pointer");
            Type elem;
            if (vt.ptr_depth > 1)
            {
                elem = type_ptr(vt.elem);
                elem.ptr_depth = vt.ptr_depth - 1;
                elem.name = vt.name;
            }
            else
            {
                elem = type_make(vt.elem);
                elem.name = vt.name;
            }
            e->type = elem;
            return e->type;
        }
        if (e->v.unary.op == TOK_AMP)
        {
            if (e->v.unary.expr && e->v.unary.expr->kind == EXPR_FUNC_REF)
            {
                Type ptr = type_ptr(TY_BYTE);
                ptr.ptr_depth = 1;
                e->type = ptr;
                return e->type;
            }
            if (!is_lvalue_expr(e->v.unary.expr))
                sema_error(s, e->line, e->col, 1, "Invalid Address", "Address-of requires an assignable expression");
            if (vt.kind == TY_ARRAY)
                sema_error(s, e->line, e->col, 1, "Invalid Address", "Address-of array not supported");
            Type ptr;
            if (vt.kind == TY_PTR)
            {
                ptr = type_ptr(vt.elem);
                ptr.ptr_depth = vt.ptr_depth + 1;
                ptr.name = vt.name;
            }
            else
            {
                ptr = type_ptr(vt.kind);
                ptr.name = vt.name;
            }
            e->type = ptr;
            return e->type;
        }
        sema_error(s, e->line, e->col, 1, "Invalid Operator", "Unknown unary operator");
        return type_make(TY_UNKNOWN);
    }
    case EXPR_INDEX:
    {
        Type rt = sema_expr(s, e->v.index.recv);
        Type it = sema_expr(s, e->v.index.index);
        if (!is_integer_kind(it.kind))
            sema_error(s, e->line, e->col, 1, "Type Mismatch", "Index requires integer type");
        if (rt.kind == TY_ARRAY)
        {
            Type elem = type_make(rt.elem);
            elem.name = rt.name;
            e->type = elem;
            return e->type;
        }
        if (rt.kind == TY_STRING)
        {
            e->type = type_make(TY_CHAR);
            return e->type;
        }
        if (rt.kind == TY_PTR)
        {
            check_ptr_safety(s, e->line, e->col, 1, "Safe function cannot index pointer");
            Type elem;
            if (rt.ptr_depth > 1)
            {
                elem = type_ptr(rt.elem);
                elem.ptr_depth = rt.ptr_depth - 1;
                elem.name = rt.name;
            }
            else
            {
                elem = type_make(rt.elem);
                elem.name = rt.name;
            }
            e->type = elem;
            return e->type;
        }
        sema_error(s, e->line, e->col, 1, "Invalid Index", "Indexing requires array, string or pointer");
        return type_make(TY_UNKNOWN);
    }
    case EXPR_CAST:
    {
        Type cast_type = e->v.cast.type;
        Type value_type = sema_expr(s, e->v.cast.expr);

        // Validate that cast_type is a valid type
        if (cast_type.kind == TY_UNKNOWN)
            sema_error(s, e->line, e->col, 1, "Unknown Type", "Cast target type is unknown");

        // Special case: allow [Object.Function](void*) — wraps a raw pointer as a callable object.
        if (cast_type.kind == TY_CLASS && is_function_object_type(s, cast_type) && value_type.kind == TY_PTR)
        {
            check_ptr_safety(s, e->line, e->col, 1, "Safe function cannot cast pointer to Object.Function");
            e->type = cast_type;
            return e->type;
        }

        // Disallow casting to Object types (only primitives and pointers)
        if (cast_type.kind == TY_CLASS || cast_type.kind == TY_STRUCT)
            sema_error(s, e->line, e->col, 1, "Invalid Cast", "Cannot cast to Object types; use Object conversions instead");

        // Object source values are also disallowed for explicit casts.
        if (value_type.kind == TY_CLASS || value_type.kind == TY_STRUCT)
            sema_error(s, e->line, e->col, 1, "Invalid Cast", "Cannot cast Object values; use Object conversions instead");

        // Explicit cast supports all non-object types.
        // Keep safety checks for raw pointer/integer reinterprets.
        if (cast_type.kind == TY_PTR && is_integer_kind(value_type.kind))
            check_int_to_ptr_safety(s, cast_type, value_type, e->v.cast.expr);
        if (is_integer_kind(cast_type.kind) && value_type.kind == TY_PTR)
            check_ptr_safety(s, e->line, e->col, 1, "Safe function cannot cast pointer to integer");

        e->type = cast_type;
        return e->type;
    }
    default: break;
    }
    sema_error(s, e->line, e->col, 1, "Unknown Expression", "Unsupported expression");
    return type_make(TY_UNKNOWN);
}

static bool stmt_guarantees_return(Stmt *st)
