// Internal codegen fragment: function/method declaration and module emission

static void codegen_func_body(Codegen *g,
                              LLVMValueRef fn,
                              ParamList *params,
                              Type ret_type,
                              Stmt *body,
                              const char *trace_name)
{
    if (!g || !fn || !body) return;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry");
    LLVMPositionBuilderAtEnd(g->builder, entry);

    g->current_class = 0;
    g->current_this = 0;
    g->current_ret_type = ret_type;
    g->current_func_name = trace_name;
    g->gc_frame = 0;

    if (g->fn_gc_push)
        g->gc_frame = LLVMBuildCall2(g->builder, g->fn_gc_push_ty, g->fn_gc_push, 0, 0, "gc.frame");

    Scope sc; sc.vars = 0; sc.parent = g->scope; g->scope = &sc;

    if (params)
    {
        for (int i = 0; i < params->count; i++)
        {
            Param *p = &params->items[i];
            LLVMValueRef param = LLVMGetParam(fn, (unsigned)i);
            LLVMValueRef alloca = build_entry_alloca(g, fn, llvm_type(g, p->type), p->name);
            LLVMBuildStore(g->builder, param, alloca);
            scope_add(g->scope, p->name, p->type, alloca);
            gc_add_root(g, alloca, p->type);
        }
    }

    maybe_call_global_init(g);

    gen_stmt(g, body);

    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(g->builder)))
    {
        if (ret_type.kind == TY_VOID)
        {
            if (g->gc_frame)
                LLVMBuildCall2(g->builder, g->fn_gc_pop_ty, g->fn_gc_pop, &g->gc_frame, 1, "");
            LLVMBuildRetVoid(g->builder);
        }
        else
        {
            if (g->gc_frame)
                LLVMBuildCall2(g->builder, g->fn_gc_pop_ty, g->fn_gc_pop, &g->gc_frame, 1, "");
            LLVMBuildRet(g->builder, LLVMConstNull(llvm_type(g, ret_type)));
        }
    }

    g->scope = sc.parent;
    g->gc_frame = 0;
    g->current_func_name = 0;
}

static void codegen_async_runner(Codegen *g, FuncSymbol *fs)
{
    Func *f = fs ? fs->func : 0;
    Type logical_ret = cg_func_logical_ret_type(f);
    LLVMValueRef fn = fs ? fs->async_runner_fn : 0;
    if (!g || !fs || !f || !fn || !fs->async_body_fn || !fs->async_env_ty) return;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry");
    LLVMPositionBuilderAtEnd(g->builder, entry);

    LLVMValueRef env_raw = LLVMGetParam(fn, 0);
    LLVMValueRef task_raw = LLVMGetParam(fn, 1);
    LLVMValueRef env_ptr = LLVMBuildBitCast(g->builder, env_raw, LLVMPointerType(fs->async_env_ty, 0), "async.env");
    LLVMValueRef *argv = f->params.count > 0
        ? (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)f->params.count)
        : 0;

    for (int i = 0; i < f->params.count; i++)
    {
        LLVMValueRef field_ptr = LLVMBuildStructGEP2(g->builder, fs->async_env_ty, env_ptr, (unsigned)i, "async.env.field");
        argv[i] = LLVMBuildLoad2(g->builder, llvm_type(g, f->params.items[i].type), field_ptr, "async.env.value");
    }

    LLVMValueRef retv = 0;
    if (logical_ret.kind == TY_VOID)
        LLVMBuildCall2(g->builder, fs->async_body_fn_ty, fs->async_body_fn, argv, (unsigned)f->params.count, "");
    else
        retv = LLVMBuildCall2(g->builder, fs->async_body_fn_ty, fs->async_body_fn, argv, (unsigned)f->params.count, "async.body");

    LLVMValueRef has = g->fn_exc_has
        ? LLVMBuildCall2(g->builder, g->fn_exc_has_ty, g->fn_exc_has, 0, 0, "async.exc.has")
        : LLVMConstInt(g->i1, 0, 0);
    LLVMValueRef cond = LLVMBuildICmp(g->builder, LLVMIntNE, has, LLVMConstInt(g->i1, 0, 0), "async.exc.cond");
    LLVMBasicBlockRef fail_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "async.fail");
    LLVMBasicBlockRef ok_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "async.ok");
    LLVMBuildCondBr(g->builder, cond, fail_bb, ok_bb);

    LLVMPositionBuilderAtEnd(g->builder, fail_bb);
    {
        LLVMValueRef errv = g->fn_exc_get
            ? LLVMBuildCall2(g->builder, g->fn_exc_get_ty, g->fn_exc_get, 0, 0, "async.exc.get")
            : LLVMConstNull(g->i8ptr);
        LLVMValueRef tracev = g->fn_exc_trace
            ? LLVMBuildCall2(g->builder, g->fn_exc_trace_ty, g->fn_exc_trace, 0, 0, "async.exc.trace")
            : LLVMConstNull(g->i8ptr);
        LLVMValueRef fail_args[3] = { task_raw, errv, tracev };
        LLVMBuildCall2(g->builder, g->fn_async_task_fail_ty, g->fn_async_task_fail, fail_args, 3, "");
        LLVMBuildRetVoid(g->builder);
    }

    LLVMPositionBuilderAtEnd(g->builder, ok_bb);
    {
        LLVMValueRef tagv = LLVMConstInt(g->i64, 0, 0);
        LLVMValueRef payloadv = LLVMConstInt(g->i64, 0, 0);
        if (logical_ret.kind != TY_VOID)
        {
            CGValue rv;
            rv.value = retv;
            rv.type = logical_ret;
            LLVMValueRef anyv = box_any_value(g, rv);
            tagv = LLVMBuildExtractValue(g->builder, anyv, 0, "async.ret.tag");
            payloadv = LLVMBuildExtractValue(g->builder, anyv, 1, "async.ret.payload");
        }
        LLVMValueRef complete_args[3] = { task_raw, tagv, payloadv };
        LLVMBuildCall2(g->builder, g->fn_async_task_complete_ty, g->fn_async_task_complete, complete_args, 3, "");
        LLVMBuildRetVoid(g->builder);
    }
}

static void codegen_async_func(Codegen *g, FuncSymbol *fs)
{
    Func *f = fs ? fs->func : 0;
    LLVMValueRef fn = fs ? fs->fn : 0;
    Type logical_ret = cg_func_logical_ret_type(f);
    const char *qname = f ? (f->qname ? f->qname : f->name) : 0;
    if (!g || !fs || !f || !fn || !f->body) return;

    codegen_func_body(g,
                      fs->async_body_fn,
                      &f->params,
                      logical_ret,
                      f->body,
                      build_trace_label(qname, f->src, f->line, f->col));
    codegen_async_runner(g, fs);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry");
    LLVMPositionBuilderAtEnd(g->builder, entry);

    maybe_call_global_init(g);

    LLVMValueRef env_mem = LLVMBuildCall2(
        g->builder,
        g->fn_gc_alloc_ty,
        g->fn_gc_alloc,
        (LLVMValueRef[]){ LLVMBuildZExtOrBitCast(g->builder, LLVMSizeOf(fs->async_env_ty), g->i64, "async.env.sz") },
        1,
        "async.env.mem");
    LLVMValueRef env_ptr = LLVMBuildBitCast(g->builder, env_mem, LLVMPointerType(fs->async_env_ty, 0), "async.env.ptr");
    for (int i = 0; i < f->params.count; i++)
    {
        LLVMValueRef field_ptr = LLVMBuildStructGEP2(g->builder, fs->async_env_ty, env_ptr, (unsigned)i, "async.env.store.ptr");
        LLVMBuildStore(g->builder, LLVMGetParam(fn, (unsigned)i), field_ptr);
    }

    LLVMValueRef spawn_args[2] = {
        LLVMBuildBitCast(g->builder, fs->async_runner_fn, g->i8ptr, "async.runner.raw"),
        LLVMBuildBitCast(g->builder, env_ptr, g->i8ptr, "async.env.raw")
    };
    LLVMValueRef taskv = LLVMBuildCall2(g->builder, g->fn_async_task_spawn_ty, g->fn_async_task_spawn, spawn_args, 2, "async.task");
    LLVMBuildRet(g->builder, taskv);
}

static void codegen_func(Codegen *g, Func *f)
{
    const char *qname = f->qname ? f->qname : f->name;
    FuncSymbol *fs = functable_find(&g->funcs, qname);
    if (!fs) return;
    if (!f->body) return;
    if (f->is_async)
    {
        codegen_async_func(g, fs);
        return;
    }
    codegen_func_body(g,
                      fs->fn,
                      &f->params,
                      f->ret_type,
                      f->body,
                      build_trace_label(qname, f->src, f->line, f->col));
}

static const char *mangle_method_name(ClassSymbol *cs, Method *m, int idx)
{
    const char *mname = m->is_constructor ? "ctor" : m->name;
    char buf[256];
    size_t off = 0;
    kn_append(buf, sizeof(buf), &off, cg_class_qname(cs->decl));
    kn_append(buf, sizeof(buf), &off, "$");
    kn_append(buf, sizeof(buf), &off, mname);
    kn_append(buf, sizeof(buf), &off, "$");
    append_int(buf, sizeof(buf), &off, idx);
    char *out = (char *)kn_malloc(off + 1);
    if (!out) kn_die("out of memory");
    kn_memcpy(out, buf, off + 1);
    return out;
}

static LLVMValueRef declare_method(Codegen *g, ClassSymbol *cs, Method *m, int idx, LLVMTypeRef *out_ty)
{
    int argc = m->params.count + (m->is_static ? 0 : 1);
    LLVMTypeRef *args = 0;
    if (argc > 0)
    {
        args = (LLVMTypeRef *)kn_malloc(sizeof(LLVMTypeRef) * (size_t)argc);
        int off = 0;
        if (!m->is_static)
            args[off++] = g->i8ptr;
        for (int i = 0; i < m->params.count; i++)
            args[off++] = llvm_type(g, m->params.items[i].type);
    }
    LLVMTypeRef ret = llvm_type(g, m->ret_type);
    LLVMTypeRef fn_ty = LLVMFunctionType(ret, args, (unsigned)argc, 0);
    const char *name = mangle_method_name(cs, m, idx);
    LLVMValueRef fn = LLVMAddFunction(g->mod, name, fn_ty);
    if (out_ty) *out_ty = fn_ty;
    return fn;
}

static bool method_sig_equal(const Method *a, const Method *b)
{
    if (!a || !b) return false;
    if (a->is_static != b->is_static) return false;
    if (kn_strcmp(a->name, b->name) != 0) return false;
    if (!type_equal(a->ret_type, b->ret_type)) return false;
    if (a->params.count != b->params.count) return false;
    for (int i = 0; i < a->params.count; i++)
        if (!type_equal(a->params.items[i].type, b->params.items[i].type)) return false;
    return true;
}

static LLVMTypeRef method_fn_type(Codegen *g, const Method *m, int is_static)
{
    int argc = m->params.count + (is_static ? 0 : 1);
    LLVMTypeRef *args = 0;
    if (argc > 0)
    {
        args = (LLVMTypeRef *)kn_malloc(sizeof(LLVMTypeRef) * (size_t)argc);
        int off = 0;
        if (!is_static)
            args[off++] = g->i8ptr;
        for (int i = 0; i < m->params.count; i++)
            args[off++] = llvm_type(g, m->params.items[i].type);
    }
    LLVMTypeRef ret = llvm_type(g, m->ret_type);
    return LLVMFunctionType(ret, args, (unsigned)argc, 0);
}

static bool vsig_equal(const VSlotSig *s, const Method *m)
{
    if (!s || !m) return false;
    if (kn_strcmp(s->name, m->name) != 0) return false;
    if (!type_equal(s->ret_type, m->ret_type)) return false;
    if (s->params->count != m->params.count) return false;
    for (int i = 0; i < m->params.count; i++)
        if (!type_equal(s->params->items[i].type, m->params.items[i].type))
            return false;
    return true;
}

static int vslot_find_method(VSlotTable *t, const Method *m)
{
    for (int i = 0; i < t->count; i++)
    {
        if (vsig_equal(&t->items[i], m))
            return i;
    }
    return -1;
}

static void build_global_vslots(Codegen *g, ClassList *classes, InterfaceList *interfaces)
{
    if (!g) return;

    if (classes)
    {
        for (int i = 0; i < classes->count; i++)
        {
            ClassDecl *c = &classes->items[i];
            for (int m = 0; m < c->methods.count; m++)
            {
                Method *md = &c->methods.items[m];
                if (md->is_constructor || md->is_static) continue;
                if (!method_is_virtual(md)) continue;
                if (vslot_find_method(&g->vslots, md) < 0)
                {
                    VSlotSig s;
                    s.name = md->name;
                    s.ret_type = md->ret_type;
                    s.params = &md->params;
                    vslottable_add(&g->vslots, s);
                }
            }
        }
    }

    if (interfaces)
    {
        for (int i = 0; i < interfaces->count; i++)
        {
            InterfaceDecl *in = &interfaces->items[i];
            for (int m = 0; m < in->methods.count; m++)
            {
                Method *md = &in->methods.items[m];
                if (vslot_find_method(&g->vslots, md) < 0)
                {
                    VSlotSig s;
                    s.name = md->name;
                    s.ret_type = md->ret_type;
                    s.params = &md->params;
                    vslottable_add(&g->vslots, s);
                }
            }
        }
    }

    for (int i = 0; i < g->classes.count; i++)
    {
        ClassSymbol *cs = &g->classes.items[i];
        int mcount = cs->decl->methods.count;
        if (mcount > 0)
        {
            cs->method_vslots = (int *)kn_malloc(sizeof(int) * (size_t)mcount);
            for (int m = 0; m < mcount; m++)
                cs->method_vslots[m] = -1;
            for (int m = 0; m < mcount; m++)
            {
                Method *md = &cs->decl->methods.items[m];
                if (md->is_constructor || md->is_static) continue;
                if (!method_is_virtual(md)) continue;
                int slot = vslot_find_method(&g->vslots, md);
                if (slot >= 0) cs->method_vslots[m] = slot;
            }
        }
        cs->vtable_count = g->vslots.count;
    }
}

static Method *find_method_by_sig(ClassDecl *c, const VSlotSig *sig, int *out_index)
{
    if (!c || !sig) return 0;
    for (int i = 0; i < c->methods.count; i++)
    {
        Method *m = &c->methods.items[i];
        if (m->is_constructor || m->is_static) continue;
        if (kn_strcmp(m->name, sig->name) != 0) continue;
        if (!type_equal(m->ret_type, sig->ret_type)) continue;
        if (m->params.count != sig->params->count) continue;
        bool ok = true;
        for (int p = 0; p < m->params.count; p++)
        {
            if (!type_equal(m->params.items[p].type, sig->params->items[p].type))
            {
                ok = false;
                break;
            }
        }
        if (!ok) continue;
        if (out_index) *out_index = i;
        return m;
    }
    return 0;
}

static Method *find_method_hierarchy_by_sig(Codegen *g, ClassSymbol *cs, const VSlotSig *sig, ClassSymbol **owner_out, int *index_out)
{
    ClassSymbol *cur = cs;
    while (cur)
    {
        int idx = -1;
        Method *m = find_method_by_sig(cur->decl, sig, &idx);
        if (m)
        {
            if (owner_out) *owner_out = cur;
            if (index_out) *index_out = idx;
            return m;
        }
        if (!cur->decl->base) break;
        cur = classtable_find(&g->classes, cur->decl->base);
    }
    return 0;
}

static const char *mangle_vtable_name(ClassSymbol *cs)
{
    char buf[256];
    size_t off = 0;
    kn_append(buf, sizeof(buf), &off, cg_class_qname(cs->decl));
    kn_append(buf, sizeof(buf), &off, "$vtable");
    char *out = (char *)kn_malloc(off + 1);
    if (!out) kn_die("out of memory");
    kn_memcpy(out, buf, off + 1);
    return out;
}

static void emit_class_vtable(Codegen *g, ClassSymbol *cs)
{
    if (!cs) return;
    unsigned slot_count = (unsigned)(g->vslots.count > 0 ? g->vslots.count : 1);
    LLVMTypeRef arr_ty = LLVMArrayType(g->i8ptr, slot_count);
    const char *name = mangle_vtable_name(cs);
    LLVMValueRef gv = LLVMAddGlobal(g->mod, arr_ty, name);
    LLVMSetLinkage(gv, LLVMInternalLinkage);
    LLVMSetGlobalConstant(gv, 1);

    LLVMValueRef *vals = (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)slot_count);
    for (unsigned i = 0; i < slot_count; i++)
    {
        if ((int)i < g->vslots.count)
        {
            VSlotSig *sig = &g->vslots.items[i];
            ClassSymbol *owner = 0;
            int midx = -1;
            Method *m = find_method_hierarchy_by_sig(g, cs, sig, &owner, &midx);
            if (m && owner)
                vals[i] = LLVMConstBitCast(owner->methods[midx], g->i8ptr);
            else
                vals[i] = LLVMConstNull(g->i8ptr);
        }
        else
        {
            vals[i] = LLVMConstNull(g->i8ptr);
        }
    }
    LLVMValueRef init = LLVMConstArray(g->i8ptr, vals, slot_count);
    LLVMSetInitializer(gv, init);
    cs->vtable_global = gv;
    cs->vtable_count = (int)slot_count;
}

static void codegen_method(Codegen *g, ClassSymbol *cs, Method *m, int idx)
{
    LLVMValueRef fn = cs->methods[idx];
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry");
    LLVMPositionBuilderAtEnd(g->builder, entry);

    g->current_class = cs->decl;
    g->current_this = 0;
    g->current_ret_type = m->ret_type;
    {
        char buf[256];
        size_t off = 0;
        const char *mname = m->is_constructor ? "Constructor" : m->name;
        kn_append(buf, sizeof(buf), &off, cg_class_qname(cs->decl));
        kn_append(buf, sizeof(buf), &off, ".");
        kn_append(buf, sizeof(buf), &off, mname);
        buf[off] = 0;
        g->current_func_name = build_trace_label(buf, m->src ? m->src : cs->decl->src, m->line, m->col);
    }
    g->gc_frame = 0;

    if (g->fn_gc_push)
        g->gc_frame = LLVMBuildCall2(g->builder, g->fn_gc_push_ty, g->fn_gc_push, 0, 0, "gc.frame");

    Scope sc; sc.vars = 0; sc.parent = g->scope; g->scope = &sc;

    int param_base = 0;
    if (!m->is_static)
    {
        LLVMValueRef this_param = LLVMGetParam(fn, 0);
        g->current_this = LLVMBuildBitCast(g->builder, this_param, cs->ptr_ty, "this");
        LLVMValueRef this_alloca = build_entry_alloca(g, fn, cs->ptr_ty, "this.gc");
        LLVMBuildStore(g->builder, g->current_this, this_alloca);
        Type this_ty = type_class(cg_class_qname(cs->decl));
        gc_add_root(g, this_alloca, this_ty);
        param_base = 1;
    }

    for (int i = 0; i < m->params.count; i++)
    {
        Param *p = &m->params.items[i];
        LLVMValueRef param = LLVMGetParam(fn, (unsigned)(param_base + i));
        LLVMValueRef alloca = build_entry_alloca(g, fn, llvm_type(g, p->type), p->name);
        LLVMBuildStore(g->builder, param, alloca);
        scope_add(g->scope, p->name, p->type, alloca);
        gc_add_root(g, alloca, p->type);
    }

    maybe_call_global_init(g);

    if (m->body)
        gen_stmt(g, m->body);

    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(g->builder)))
    {
        if (m->ret_type.kind == TY_VOID)
        {
            if (g->gc_frame)
                LLVMBuildCall2(g->builder, g->fn_gc_pop_ty, g->fn_gc_pop, &g->gc_frame, 1, "");
            LLVMBuildRetVoid(g->builder);
        }
        else
        {
            if (g->gc_frame)
                LLVMBuildCall2(g->builder, g->fn_gc_pop_ty, g->fn_gc_pop, &g->gc_frame, 1, "");
            LLVMBuildRet(g->builder, LLVMConstInt(llvm_type(g, m->ret_type), 0, 0));
        }
    }

    g->scope = sc.parent;
    g->gc_frame = 0;
    g->current_func_name = 0;
}

static LLVMValueRef declare_func(Codegen *g, Func *f, LLVMTypeRef *out_ty)
{
    LLVMTypeRef ret = llvm_type(g, cg_func_public_ret_type(f));
    LLVMTypeRef *args = 0;
    if (f->params.count > 0)
    {
        args = (LLVMTypeRef *)kn_malloc(sizeof(LLVMTypeRef) * (size_t)f->params.count);
        for (int i = 0; i < f->params.count; i++)
            args[i] = llvm_type(g, f->params.items[i].type);
    }
    LLVMTypeRef fn_ty = LLVMFunctionType(ret, args, (unsigned)f->params.count, 0);
    const char *sym = (f->is_extern && f->extern_symbol && f->extern_symbol[0])
                          ? f->extern_symbol
                          : ((f->is_extern && f->name) ? f->name : (f->qname ? f->qname : f->name));
    LLVMValueRef fn = LLVMAddFunction(g->mod, sym, fn_ty);
    if (f->is_extern && f->extern_abi == KN_EXTERN_ABI_SYSTEM && g->target_is_windows && g->target_is_x86)
        LLVMSetFunctionCallConv(fn, LLVMX86StdcallCallConv);
    if (out_ty) *out_ty = fn_ty;
    return fn;
}

int kn_codegen(const char *out_path, MetaList *metas, FuncList *funcs, ImportList *imports, ClassList *classes,
               InterfaceList *interfaces, StructList *structs, EnumList *enums, StmtList *globals, const KnCodegenOptions *opts)
{
    if (!out_path || !metas || !funcs || !imports || !classes || !interfaces || !structs || !enums || !globals)
        return -1;

    // Keep LLVM link-time surface small: only initialize the backends we support.
    // Targets: Windows x64/x86, Linux x64 (X86) and Linux arm64 (AArch64).
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86Target();
    LLVMInitializeX86TargetMC();
    LLVMInitializeX86AsmPrinter();
    LLVMInitializeX86AsmParser();

    LLVMInitializeAArch64TargetInfo();
    LLVMInitializeAArch64Target();
    LLVMInitializeAArch64TargetMC();
    LLVMInitializeAArch64AsmPrinter();
    LLVMInitializeAArch64AsmParser();

    Codegen g;
    kn_memset(&g, 0, sizeof(g));
    g.ctx = LLVMContextCreate();
    g.mod = LLVMModuleCreateWithNameInContext("kinal", g.ctx);
    g.builder = LLVMCreateBuilderInContext(g.ctx);
    g.i1 = LLVMInt1TypeInContext(g.ctx);
    g.i8 = LLVMInt8TypeInContext(g.ctx);
    g.i16 = LLVMInt16TypeInContext(g.ctx);
    g.i32 = LLVMInt32TypeInContext(g.ctx);
    g.i64 = LLVMInt64TypeInContext(g.ctx);
    g.f16 = LLVMHalfTypeInContext(g.ctx);
    g.f32 = LLVMFloatTypeInContext(g.ctx);
    g.f64 = LLVMDoubleTypeInContext(g.ctx);
    g.f128 = LLVMFP128TypeInContext(g.ctx);
    g.voidt = LLVMVoidTypeInContext(g.ctx);
    g.i8ptr = LLVMPointerType(g.i8, 0);
    g.any_ty = LLVMStructCreateNamed(g.ctx, "KnAny");
    {
        LLVMTypeRef any_fields[2] = { g.i64, g.i64 };
        LLVMStructSetBody(g.any_ty, any_fields, 2, 0);
    }
    {
        LLVMTypeRef args[3] = { g.i8ptr, LLVMPointerType(g.any_ty, 0), g.i64 };
        g.callable_wrapper_ty = LLVMFunctionType(g.any_ty, args, 3, 0);
    }
    g.interfaces = interfaces;
    g.metas = metas;
    g.func_list = funcs;
    g.structs_list = structs;
    g.enums = enums;
    Scope global_scope;
    global_scope.vars = 0;
    global_scope.parent = 0;
    g.global_scope = &global_scope;
    g.scope = &global_scope;
    (void)imports;

    if (structs)
    {
        // create named struct types
        for (int i = 0; i < structs->count; i++)
        {
            StructDecl *st = &structs->items[i];
            StructSymbol ss;
            kn_memset(&ss, 0, sizeof(ss));
            ss.decl = st;
            ss.struct_ty = LLVMStructCreateNamed(g.ctx, cg_struct_qname(st));
            ss.ptr_ty = LLVMPointerType(ss.struct_ty, 0);
            structtable_add(&g.structs, ss);
        }

        // set struct bodies
        for (int i = 0; i < g.structs.count; i++)
        {
            StructSymbol *ss = &g.structs.items[i];
            StructDecl *st = ss->decl;
            int fcount = st->fields.count;
            if (fcount > 0)
            {
                LLVMTypeRef *fields = (LLVMTypeRef *)kn_malloc(sizeof(LLVMTypeRef) * (size_t)fcount);
                for (int f = 0; f < fcount; f++)
                    fields[f] = llvm_type(&g, st->fields.items[f].type);
                LLVMStructSetBody(ss->struct_ty, fields, (unsigned)fcount, st->is_packed ? 1 : 0);
            }
            else
            {
                LLVMStructSetBody(ss->struct_ty, 0, 0, st->is_packed ? 1 : 0);
            }
        }
    }

    if (classes)
    {
        // create named struct types
        for (int i = 0; i < classes->count; i++)
        {
            ClassDecl *c = &classes->items[i];
            ClassSymbol cs;
            kn_memset(&cs, 0, sizeof(cs));
            cs.decl = c;
            cs.struct_ty = LLVMStructCreateNamed(g.ctx, cg_class_qname(c));
            cs.ptr_ty = LLVMPointerType(cs.struct_ty, 0);
            classtable_add(&g.classes, cs);
        }

        // set struct bodies
        for (int i = 0; i < g.classes.count; i++)
        {
            ClassSymbol *cs = &g.classes.items[i];
            ClassDecl *c = cs->decl;
            ClassSymbol *base = c->base ? classtable_find(&g.classes, c->base) : 0;
            int field_count = class_instance_field_count(c) + 1;
            LLVMTypeRef *fields = (LLVMTypeRef *)kn_malloc(sizeof(LLVMTypeRef) * (size_t)field_count);
            if (base)
                fields[0] = base->struct_ty;
            else
                fields[0] = LLVMPointerType(g.i8ptr, 0); // vtable pointer in root
            int out_field = 1;
            for (int f = 0; f < c->fields.count; f++)
            {
                if (c->fields.items[f].is_static) continue;
                fields[out_field++] = llvm_type(&g, c->fields.items[f].type);
            }
            LLVMStructSetBody(cs->struct_ty, fields, (unsigned)field_count, 0);
        }

        // declare methods
        for (int i = 0; i < g.classes.count; i++)
        {
            ClassSymbol *cs = &g.classes.items[i];
            int mcount = cs->decl->methods.count;
            if (mcount > 0)
            {
                cs->methods = (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)mcount);
                cs->method_tys = (LLVMTypeRef *)kn_malloc(sizeof(LLVMTypeRef) * (size_t)mcount);
                for (int m = 0; m < mcount; m++)
                {
                    LLVMTypeRef mt = 0;
                    cs->methods[m] = declare_method(&g, cs, &cs->decl->methods.items[m], m, &mt);
                    cs->method_tys[m] = mt;
                }
            }
        }

        // register static methods as callable function pointers
        for (int i = 0; i < g.classes.count; i++)
        {
            ClassSymbol *cs = &g.classes.items[i];
            for (int m = 0; m < cs->decl->methods.count; m++)
            {
                Method *met = &cs->decl->methods.items[m];
                if (!met->is_static || met->is_constructor) continue;
                Func *synth = (Func *)kn_malloc(sizeof(Func));
                kn_memset(synth, 0, sizeof(Func));
                synth->name = met->name;
                synth->ret_type = met->ret_type;
                synth->params = met->params;
                synth->type_params = 0;
                synth->type_param_count = 0;
                synth->type_param_cap = 0;
                synth->body = 0;
                synth->safety = met->safety;
                synth->attrs = met->attrs;
                synth->src = met->src;
                synth->is_extern = 0;
                synth->extern_abi = 0;
                synth->is_static = 1;
                synth->is_async = met->is_async;
                synth->async_result_type = met->async_result_type;
                synth->is_generic_instance = 0;
                synth->generic_template_qname = 0;
                synth->generic_args = 0;
                synth->generic_arg_count = 0;
                synth->unit = cs->decl->unit;
                synth->qname = mangle_method_name(cs, met, m);
                synth->line = met->line;
                synth->col = met->col;
                FuncSymbol fs;
                kn_memset(&fs, 0, sizeof(fs));
                fs.qname = synth->qname;
                fs.func = synth;
                fs.fn = cs->methods[m];
                fs.fn_ty = cs->method_tys[m];
                functable_add(&g.funcs, fs);
            }
        }

        // build and emit vtables
        build_global_vslots(&g, classes, interfaces);
        for (int i = 0; i < g.classes.count; i++)
            emit_class_vtable(&g, &g.classes.items[i]);
    }

    g.error_class = classtable_find(&g.classes, "IO.Error");
    g.error_ctor_index = g.error_class ? find_error_ctor_index(g.error_class) : -1;
    g.error_title_index = g.error_class ? find_class_field_index(g.error_class->decl, "Title") : -1;
    g.error_msg_index = g.error_class ? find_class_field_index(g.error_class->decl, "Message") : -1;
    g.error_inner_index = g.error_class ? find_class_field_index(g.error_class->decl, "Inner") : -1;
    g.error_trace_index = g.error_class ? find_class_field_index(g.error_class->decl, "Trace") : -1;
    g.datetime_class = classtable_find(&g.classes, "IO.Time.DateTime");
    g.datetime_ticks_index = g.datetime_class ? find_class_field_index(g.datetime_class->decl, "Ticks") : -1;
    g.datetime_year_index = g.datetime_class ? find_class_field_index(g.datetime_class->decl, "Year") : -1;
    g.datetime_month_index = g.datetime_class ? find_class_field_index(g.datetime_class->decl, "Month") : -1;
    g.datetime_day_index = g.datetime_class ? find_class_field_index(g.datetime_class->decl, "Day") : -1;
    g.datetime_hour_index = g.datetime_class ? find_class_field_index(g.datetime_class->decl, "Hour") : -1;
    g.datetime_minute_index = g.datetime_class ? find_class_field_index(g.datetime_class->decl, "Minute") : -1;
    g.datetime_second_index = g.datetime_class ? find_class_field_index(g.datetime_class->decl, "Second") : -1;
    g.datetime_millisecond_index = g.datetime_class ? find_class_field_index(g.datetime_class->decl, "Millisecond") : -1;
    g.function_class = classtable_find(&g.classes, "IO.Type.Object.Function");
    g.block_class = classtable_find(&g.classes, "IO.Type.Object.Block");
    g.function_invoke_index = (g.function_class && g.function_class->decl) ? find_class_field_index(g.function_class->decl, "Invoke") : -1;
    g.function_env_index = (g.function_class && g.function_class->decl) ? find_class_field_index(g.function_class->decl, "Env") : -1;
    g.block_invoke_index = (g.block_class && g.block_class->decl) ? find_class_field_index(g.block_class->decl, "Invoke") : -1;
    g.block_env_index = (g.block_class && g.block_class->decl) ? find_class_field_index(g.block_class->decl, "Env") : -1;
    if (g.block_class)
    {
        ClassSymbol *block_base = (g.block_class->decl && g.block_class->decl->base)
            ? classtable_find(&g.classes, g.block_class->decl->base)
            : 0;
        g.pseudo_block_ty = LLVMStructCreateNamed(g.ctx, "__kn_pseudo_block");
        g.pseudo_block_ptr_ty = LLVMPointerType(g.pseudo_block_ty, 0);
        LLVMTypeRef fields[4];
        fields[0] = block_base ? block_base->struct_ty : LLVMPointerType(g.i8ptr, 0);
        fields[1] = g.i8ptr;
        fields[2] = g.i8ptr;
        fields[3] = g.i64;
        LLVMStructSetBody(g.pseudo_block_ty, fields, 4, 0);
    }
    declare_global_vars(&g, globals);

    char *triple = 0;
    if (opts && opts->target_triple && opts->target_triple[0])
    {
        size_t tlen = kn_strlen(opts->target_triple);
        triple = (char *)kn_malloc(tlen + 1);
        if (!triple)
            return -1;
        kn_memcpy(triple, opts->target_triple, tlen);
        triple[tlen] = 0;
    }
    else
    {
        triple = LLVMGetDefaultTargetTriple();
    }
    g.target_is_windows = triple_is_windows(triple);
    g.target_is_x86 = triple_is_x86(triple);
    g.emit_entry = (!opts || opts->emit_entry != 0) ? 1 : 0;
    g.env_kind = (!opts) ? KN_ENV_HOSTED : opts->env_kind;
    g.runtime_mode = (!opts) ? KN_RUNTIME_GC : opts->runtime_mode;
    g.panic_mode = (!opts) ? KN_PANIC_TRAP : opts->panic_mode;
    g.entry_name = (opts && opts->entry_name && opts->entry_name[0])
                       ? opts->entry_name
                       : ((g.env_kind == KN_ENV_FREESTANDING) ? "KMain" : "Main");
    LLVMSetTarget(g.mod, triple);

    LLVMTargetRef target;
    char *err = 0;
    if (LLVMGetTargetFromTriple(triple, &target, &err))
    {
        kn_write_str("LLVMGetTargetFromTriple failed: ");
        kn_write_str(err ? err : "unknown");
        kn_write_str("\n");
        return -1;
    }

    const char *cpu = "generic";
    if (str_contains_ci(triple, "x86_64") || str_contains_ci(triple, "amd64"))
        cpu = "x86-64";
    else if (triple_is_x86(triple))
        cpu = "pentium4";
    else if (str_contains_ci(triple, "aarch64") || str_contains_ci(triple, "arm64"))
        cpu = "generic";

    LLVMRelocMode reloc_mode = (opts && opts->emit_pic) ? LLVMRelocPIC : LLVMRelocDefault;
    LLVMTargetMachineRef tm = LLVMCreateTargetMachine(target, triple, cpu, "", LLVMCodeGenLevelDefault, reloc_mode, LLVMCodeModelDefault);
    if (!tm)
    {
        kn_write_str("LLVMCreateTargetMachine failed\n");
        return -1;
    }
    LLVMTargetDataRef dl = LLVMCreateTargetDataLayout(tm);
    g.target_data = dl;
    LLVMSetModuleDataLayout(g.mod, dl);
    // declare user functions
    for (int i = 0; i < funcs->count; i++)
    {
        Func *f = &funcs->items[i];
        if (f->type_param_count > 0 && !f->is_generic_instance)
            continue;
        FuncSymbol s;
        kn_memset(&s, 0, sizeof(s));
        LLVMTypeRef fn_ty = 0;
        s.qname = f->qname ? f->qname : f->name;
        s.func = f;
        if (f->is_delegate)
        {
            // Build the LLVM function type from the delegate signature
            LLVMTypeRef ret = llvm_type(&g, f->ret_type);
            LLVMTypeRef *args = 0;
            if (f->params.count > 0)
            {
                args = (LLVMTypeRef *)kn_malloc(sizeof(LLVMTypeRef) * (size_t)f->params.count);
                for (int j = 0; j < f->params.count; j++)
                    args[j] = llvm_type(&g, f->params.items[j].type);
            }
            fn_ty = LLVMFunctionType(ret, args, (unsigned)f->params.count, 0);
            s.fn = 0;
            s.fn_ty = fn_ty;
            // Create a global byte* variable to hold the function pointer (initially null)
            s.delegate_global = LLVMAddGlobal(g.mod, g.i8ptr, s.qname);
            LLVMSetInitializer(s.delegate_global, LLVMConstNull(g.i8ptr));
            LLVMSetLinkage(s.delegate_global, LLVMInternalLinkage);
        }
        else
        {
            s.fn = declare_func(&g, f, &fn_ty);
            s.fn_ty = fn_ty;
            s.delegate_global = 0;
            if (f->is_async)
            {
                Type logical_ret = cg_func_logical_ret_type(f);
                LLVMTypeRef *body_args = 0;
                if (f->params.count > 0)
                {
                    body_args = (LLVMTypeRef *)kn_malloc(sizeof(LLVMTypeRef) * (size_t)f->params.count);
                    for (int j = 0; j < f->params.count; j++)
                        body_args[j] = llvm_type(&g, f->params.items[j].type);
                }
                s.async_body_fn_ty = LLVMFunctionType(llvm_type(&g, logical_ret), body_args, (unsigned)f->params.count, 0);
                s.async_body_fn = LLVMAddFunction(g.mod, mangle_helper_name("__kn_async_body", s.qname, -1), s.async_body_fn_ty);
                LLVMSetLinkage(s.async_body_fn, LLVMInternalLinkage);

                s.async_env_ty = LLVMStructCreateNamed(g.ctx, mangle_helper_name("__kn_async_env", s.qname, -1));
                if (f->params.count > 0)
                {
                    LLVMTypeRef *env_fields = (LLVMTypeRef *)kn_malloc(sizeof(LLVMTypeRef) * (size_t)f->params.count);
                    for (int j = 0; j < f->params.count; j++)
                        env_fields[j] = llvm_type(&g, f->params.items[j].type);
                    LLVMStructSetBody(s.async_env_ty, env_fields, (unsigned)f->params.count, 0);
                }
                else
                {
                    LLVMStructSetBody(s.async_env_ty, 0, 0, 0);
                }

                {
                    LLVMTypeRef runner_args[2] = { g.i8ptr, g.i8ptr };
                    s.async_runner_fn_ty = LLVMFunctionType(g.voidt, runner_args, 2, 0);
                }
                s.async_runner_fn = LLVMAddFunction(g.mod, mangle_helper_name("__kn_async_runner", s.qname, -1), s.async_runner_fn_ty);
                LLVMSetLinkage(s.async_runner_fn, LLVMInternalLinkage);
            }
        }
        functable_add(&g.funcs, s);
        if (kn_strcmp(f->name, g.entry_name) == 0)
        {
            g.fn_main = s.fn;
            g.fn_main_ty = fn_ty;
            g.fn_main_is_async = f->is_async;
            g.fn_main_ret_type = cg_func_logical_ret_type(f);
        }
    }

    // runtime helpers
    build_runtime(&g);
    codegen_global_init(&g, globals);

    // codegen user functions
    for (int i = 0; i < funcs->count; i++)
    {
        Func *f = &funcs->items[i];
        if (f->type_param_count > 0 && !f->is_generic_instance)
            continue;
        codegen_func(&g, f);
    }

    // codegen methods
    for (int i = 0; i < g.classes.count; i++)
    {
        ClassSymbol *cs = &g.classes.items[i];
        for (int m = 0; m < cs->decl->methods.count; m++)
            codegen_method(&g, cs, &cs->decl->methods.items[m], m);
    }

    emit_meta_module_export(&g);
    emit_freestanding_entry_wrapper(&g);

    prune_unused_runtime_symbols(g.mod);

    char *verify_err = 0;
    if (LLVMVerifyModule(g.mod, LLVMReturnStatusAction, &verify_err))
    {
        kn_write_str("LLVMVerifyModule failed\n");
        if (verify_err) kn_write_str(verify_err);
        return -1;
    }

    char *emit_err = 0;
    int emit_kind = opts ? opts->emit_kind : KN_CODEGEN_OBJECT;
    if (emit_kind == KN_CODEGEN_LLVM_IR)
    {
        if (LLVMPrintModuleToFile(g.mod, out_path, &emit_err))
        {
            kn_write_str("Emit LLVM IR failed: ");
            kn_write_str(emit_err ? emit_err : "unknown");
            kn_write_str("\n");
            return -1;
        }
    }
    else if (emit_kind == KN_CODEGEN_LLVM_BC)
    {
        if (LLVMWriteBitcodeToFile(g.mod, out_path) != 0)
        {
            kn_write_str("Emit LLVM bitcode failed\n");
            return -1;
        }
    }
    else
    {
        LLVMCodeGenFileType file_type = (emit_kind == KN_CODEGEN_ASSEMBLY) ? LLVMAssemblyFile : LLVMObjectFile;
        if (LLVMTargetMachineEmitToFile(tm, g.mod, (char *)out_path, file_type, &emit_err))
        {
            kn_write_str(emit_kind == KN_CODEGEN_ASSEMBLY ? "Emit assembly failed: " : "Emit object failed: ");
            kn_write_str(emit_err ? emit_err : "unknown");
            kn_write_str("\n");
            return -1;
        }
    }

    if (opts && opts->target_triple && opts->target_triple[0])
        kn_free(triple);
    else
        LLVMDisposeMessage(triple);
    LLVMDisposeBuilder(g.builder);
    LLVMDisposeModule(g.mod);
    LLVMContextDispose(g.ctx);
    LLVMDisposeTargetMachine(tm);
    return 0;
}
