// Internal codegen fragment: expression lowering

static CGValue gen_expr(Codegen *g, Expr *e);
static LLVMValueRef build_array_value(Codegen *g, Type arr_type, Expr *init);
static LLVMValueRef build_dict_value(Codegen *g, Expr *init);
static LLVMValueRef build_heap_array_value(Codegen *g, Type arr_type);
static LLVMValueRef build_array_append_value(Codegen *g, CGValue arr, CGValue item);
static void init_class_array_fields(Codegen *g, ClassSymbol *cs, LLVMValueRef obj);
static CGValue gen_builtin_call(Codegen *g, KnBuiltinKind kind, ExprList *args, Expr *recv);
static LLVMValueRef gen_lvalue(Codegen *g, Expr *e, Type *out_type);
static LLVMValueRef gen_member_ptr(Codegen *g, Expr *e, Type *out_type);
static LLVMValueRef gen_index_ptr(Codegen *g, Expr *e, Type *out_type);
static CGValue gen_new(Codegen *g, Expr *e);
static LLVMValueRef build_entry_alloca(Codegen *g, LLVMValueRef fn, LLVMTypeRef ty, const char *name);
static LLVMValueRef gc_push_temp_frame(Codegen *g, const char *name);
static void gc_pop_temp_frame(Codegen *g, LLVMValueRef frame);
static CGValue gc_root_temp_value(Codegen *g, LLVMValueRef frame, CGValue v, const char *name);
static LLVMValueRef gen_rooted_call_arg(Codegen *g, LLVMValueRef frame, Expr *arg_expr, Type param_type);
static LLVMTypeRef method_fn_type(Codegen *g, const Method *m, int is_static);
static int vslot_find_method(VSlotTable *t, const Method *m);
static int class_instance_field_count(ClassDecl *c);
static int class_instance_field_layout_index(ClassDecl *c, int raw_index);
static LLVMValueRef ensure_static_field_global(Codegen *g, const char *class_qname, Field *f);
static void emit_return_default(Codegen *g);
static void trace_push_current_func(Codegen *g);
static void maybe_propagate_exception(Codegen *g);
static void emit_println_if_nonnull(Codegen *g, LLVMValueRef s, const char *tag);
static void emit_entry_exception_report(Codegen *g, LLVMValueRef errv);
static LLVMValueRef finalize_entry_return_code(Codegen *g, LLVMTypeRef t_i32, LLVMValueRef ok_rc);
static LLVMValueRef any_null_value(Codegen *g);
static LLVMValueRef run_async_task_any(Codegen *g, LLVMValueRef taskv, const char *tag_name);
static LLVMValueRef ensure_func_wrapper(Codegen *g, FuncSymbol *fs);
static ClosureInfo *ensure_closure_info(Codegen *g, Expr *e, int is_block);
static LLVMValueRef build_closure_env(Codegen *g, ClosureInfo *ci);
static LLVMValueRef build_callable_object(Codegen *g, const char *class_name, LLVMValueRef invoke_ptr, LLVMValueRef env_ptr);
static LLVMValueRef build_pseudo_block_object(Codegen *g, LLVMValueRef invoke_ptr, LLVMValueRef env_ptr, Safety safety);
static void call_block_entry(Codegen *g, CGValue blkobj, LLVMValueRef mode, LLVMValueRef a, LLVMValueRef b);
static LLVMValueRef call_callable_wrapper(Codegen *g, CGValue fnobj, LLVMValueRef args_ptr, LLVMValueRef argc);
static LLVMValueRef load_callable_invoke_ptr(Codegen *g, CGValue fnobj);
static LLVMValueRef gen_delegate_bridge(Codegen *g, FuncSymbol *dfs);
static void gen_stmt(Codegen *g, Stmt *s);
static void declare_global_vars(Codegen *g, StmtList *globals);
static void codegen_global_init(Codegen *g, StmtList *globals);
static void maybe_call_global_init(Codegen *g);

static CGValue gen_binary(Codegen *g, Expr *e)
{
    TokenType op = (TokenType)e->v.binary.op;
    CGValue out;
    out.value = 0;
    if (op == TOK_ANDAND || op == TOK_OROR)
    {
        CGValue l = gen_expr(g, e->v.binary.left);
        LLVMValueRef lcond = cast_value(g, l, type_make(TY_BOOL));
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
        LLVMBasicBlockRef lhs_bb = LLVMGetInsertBlock(g->builder);
        LLVMBasicBlockRef rhs_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, op == TOK_ANDAND ? "land.rhs" : "lor.rhs");
        LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, op == TOK_ANDAND ? "land.merge" : "lor.merge");

        if (op == TOK_ANDAND)
            LLVMBuildCondBr(g->builder, lcond, rhs_bb, merge_bb);
        else
            LLVMBuildCondBr(g->builder, lcond, merge_bb, rhs_bb);

        LLVMPositionBuilderAtEnd(g->builder, rhs_bb);
        CGValue r = gen_expr(g, e->v.binary.right);
        LLVMValueRef rcond = cast_value(g, r, type_make(TY_BOOL));
        LLVMBasicBlockRef rhs_end_bb = LLVMGetInsertBlock(g->builder);
        LLVMBuildBr(g->builder, merge_bb);

        LLVMPositionBuilderAtEnd(g->builder, merge_bb);
        LLVMValueRef phi = LLVMBuildPhi(g->builder, g->i1, op == TOK_ANDAND ? "land" : "lor");
        LLVMValueRef incoming_vals[2];
        LLVMBasicBlockRef incoming_bbs[2];
        incoming_vals[0] = LLVMConstInt(g->i1, op == TOK_ANDAND ? 0 : 1, 0);
        incoming_bbs[0] = lhs_bb;
        incoming_vals[1] = rcond;
        incoming_bbs[1] = rhs_end_bb;
        LLVMAddIncoming(phi, incoming_vals, incoming_bbs, 2);
        out.value = phi;
        out.type = type_make(TY_BOOL);
        return out;
    }

    CGValue l = gen_expr(g, e->v.binary.left);
    CGValue r = gen_expr(g, e->v.binary.right);
    bool lhs_cstr = (l.type.kind == TY_PTR && l.type.elem == TY_CHAR && l.type.ptr_depth == 1);
    bool rhs_cstr = (r.type.kind == TY_PTR && r.type.elem == TY_CHAR && r.type.ptr_depth == 1);
    bool lhs_concat_text = (l.type.kind == TY_STRING) || lhs_cstr;
    bool rhs_concat_text = (r.type.kind == TY_STRING) || rhs_cstr;
    bool lhs_stringify = l.type.kind != TY_UNKNOWN && l.type.kind != TY_VOID;
    bool rhs_stringify = r.type.kind != TY_UNKNOWN && r.type.kind != TY_VOID;
    if (op == TOK_PLUS &&
        ((lhs_concat_text && rhs_stringify) || (rhs_concat_text && lhs_stringify)) &&
        !(lhs_cstr && rhs_cstr))
    {
        LLVMValueRef lv = cast_value(g, l, type_make(TY_STRING));
        LLVMValueRef rv = cast_value(g, r, type_make(TY_STRING));
        LLVMValueRef argv[2] = { lv, rv };
        out.value = LLVMBuildCall2(g->builder, g->fn_str_concat_ty, g->fn_str_concat, argv, 2, "strcat");
        out.type = type_make(TY_STRING);
        return out;
    }

    if ((op == TOK_PLUS || op == TOK_MINUS) && (l.type.kind == TY_PTR || r.type.kind == TY_PTR))
    {
        CGValue ptrv = l;
        CGValue idxv = r;
        if (l.type.kind != TY_PTR)
        {
            ptrv = r;
            idxv = l;
        }
        Type elem = type_from_elem(ptrv.type);
        LLVMTypeRef elem_ty = llvm_type(g, elem);
        LLVMValueRef idx = cast_value(g, idxv, type_make(TY_I64));
        if (op == TOK_MINUS && ptrv.value == l.value)
            idx = LLVMBuildNeg(g->builder, idx, "pneg");
        out.value = LLVMBuildInBoundsGEP2(g->builder, elem_ty, ptrv.value, &idx, 1, "ptradd");
        out.type = ptrv.type;
        return out;
    }

    switch (op)
    {
    case TOK_PLUS:
    case TOK_MINUS:
    case TOK_STAR:
    case TOK_SLASH:
    case TOK_PERCENT:
    {
        Type res = e->type;
        LLVMValueRef lv = cast_value(g, l, res);
        LLVMValueRef rv = cast_value(g, r, res);
        if (is_float_kind(res.kind))
        {
            switch (op)
            {
            case TOK_PLUS: out.value = LLVMBuildFAdd(g->builder, lv, rv, "fadd"); break;
            case TOK_MINUS: out.value = LLVMBuildFSub(g->builder, lv, rv, "fsub"); break;
            case TOK_STAR: out.value = LLVMBuildFMul(g->builder, lv, rv, "fmul"); break;
            case TOK_SLASH: out.value = LLVMBuildFDiv(g->builder, lv, rv, "fdiv"); break;
            case TOK_PERCENT: out.value = LLVMBuildFRem(g->builder, lv, rv, "frem"); break;
            default: break;
            }
        }
        else
        {
            bool u = is_unsigned_kind(res.kind);
            switch (op)
            {
            case TOK_PLUS: out.value = LLVMBuildAdd(g->builder, lv, rv, "add"); break;
            case TOK_MINUS: out.value = LLVMBuildSub(g->builder, lv, rv, "sub"); break;
            case TOK_STAR: out.value = LLVMBuildMul(g->builder, lv, rv, "mul"); break;
            case TOK_SLASH: out.value = u ? LLVMBuildUDiv(g->builder, lv, rv, "udiv") : LLVMBuildSDiv(g->builder, lv, rv, "sdiv"); break;
            case TOK_PERCENT: out.value = u ? LLVMBuildURem(g->builder, lv, rv, "urem") : LLVMBuildSRem(g->builder, lv, rv, "srem"); break;
            default: break;
            }
        }
        out.type = res;
        return out;
    }
    case TOK_EQ:
    case TOK_NE:
    case TOK_LT:
    case TOK_LE:
    case TOK_GT:
    case TOK_GE:
    {
        if (l.type.kind == TY_STRING && r.type.kind == TY_STRING && (op == TOK_EQ || op == TOK_NE))
        {
            LLVMValueRef argv[2] = { l.value, r.value };
            LLVMValueRef eq = LLVMBuildCall2(g->builder, g->fn_str_eq_ty, g->fn_str_eq, argv, 2, "streq");
            if (op == TOK_NE)
                eq = LLVMBuildNot(g->builder, eq, "streq.not");
            out.value = eq;
            out.type = type_make(TY_BOOL);
            return out;
        }
        if ((op == TOK_EQ || op == TOK_NE) &&
            (l.type.kind == TY_PTR || l.type.kind == TY_CLASS || l.type.kind == TY_STRING || l.type.kind == TY_NULL) &&
            (r.type.kind == TY_PTR || r.type.kind == TY_CLASS || r.type.kind == TY_STRING || r.type.kind == TY_NULL))
        {
            LLVMValueRef lv = cast_value(g, l, type_ptr(TY_BYTE));
            LLVMValueRef rv = cast_value(g, r, type_ptr(TY_BYTE));
            LLVMIntPredicate pred = (op == TOK_NE) ? LLVMIntNE : LLVMIntEQ;
            out.value = LLVMBuildICmp(g->builder, pred, lv, rv, "pcmp");
            out.type = type_make(TY_BOOL);
            return out;
        }
        if (l.type.kind == TY_PTR && r.type.kind == TY_PTR)
        {
            LLVMIntPredicate pred = LLVMIntEQ;
            if (op == TOK_EQ) pred = LLVMIntEQ;
            else if (op == TOK_NE) pred = LLVMIntNE;
            else if (op == TOK_LT) pred = LLVMIntULT;
            else if (op == TOK_LE) pred = LLVMIntULE;
            else if (op == TOK_GT) pred = LLVMIntUGT;
            else if (op == TOK_GE) pred = LLVMIntUGE;
            out.value = LLVMBuildICmp(g->builder, pred, l.value, r.value, "pcmp");
            out.type = type_make(TY_BOOL);
            return out;
        }

        Type ct;
        if (is_float_kind(l.type.kind) || is_float_kind(r.type.kind))
        {
            if (is_float_kind(l.type.kind) && is_float_kind(r.type.kind))
            {
                int lb = float_bits(l.type.kind);
                int rb = float_bits(r.type.kind);
                ct = lb >= rb ? l.type : r.type;
            }
            else if (is_float_kind(l.type.kind))
                ct = l.type;
            else
                ct = r.type;
            LLVMValueRef lv = cast_value(g, l, ct);
            LLVMValueRef rv = cast_value(g, r, ct);
            LLVMRealPredicate pred = LLVMRealOEQ;
            if (op == TOK_EQ) pred = LLVMRealOEQ;
            else if (op == TOK_NE) pred = LLVMRealONE;
            else if (op == TOK_LT) pred = LLVMRealOLT;
            else if (op == TOK_LE) pred = LLVMRealOLE;
            else if (op == TOK_GT) pred = LLVMRealOGT;
            else if (op == TOK_GE) pred = LLVMRealOGE;
            out.value = LLVMBuildFCmp(g->builder, pred, lv, rv, "fcmp");
            out.type = type_make(TY_BOOL);
            return out;
        }
        else
        {
            int lb = int_bits(l.type.kind);
            int rb = int_bits(r.type.kind);
            int bits = lb > rb ? lb : rb;
            bool u = is_unsigned_kind(l.type.kind) || is_unsigned_kind(r.type.kind);
            if (bits <= 8) ct = type_make(u ? TY_U8 : TY_I8);
            else if (bits <= 16) ct = type_make(u ? TY_U16 : TY_I16);
            else if (bits <= 32) ct = type_make(u ? TY_U32 : TY_I32);
            else ct = type_make(u ? TY_U64 : TY_I64);
            LLVMValueRef lv = cast_value(g, l, ct);
            LLVMValueRef rv = cast_value(g, r, ct);
            LLVMIntPredicate pred = LLVMIntEQ;
            if (op == TOK_EQ) pred = LLVMIntEQ;
            else if (op == TOK_NE) pred = LLVMIntNE;
            else if (op == TOK_LT) pred = u ? LLVMIntULT : LLVMIntSLT;
            else if (op == TOK_LE) pred = u ? LLVMIntULE : LLVMIntSLE;
            else if (op == TOK_GT) pred = u ? LLVMIntUGT : LLVMIntSGT;
            else if (op == TOK_GE) pred = u ? LLVMIntUGE : LLVMIntSGE;
            out.value = LLVMBuildICmp(g->builder, pred, lv, rv, "icmp");
            out.type = type_make(TY_BOOL);
            return out;
        }
    }
    case TOK_AMP:
    case TOK_BITOR:
    case TOK_XOR:
    case TOK_SHL:
    case TOK_SHR:
    {
        int lb = int_bits(l.type.kind);
        int rb = int_bits(r.type.kind);
        int bits = lb > rb ? lb : rb;
        bool u = is_unsigned_kind(l.type.kind) || is_unsigned_kind(r.type.kind);
        Type ct;
        if (bits <= 8) ct = type_make(u ? TY_U8 : TY_I8);
        else if (bits <= 16) ct = type_make(u ? TY_U16 : TY_I16);
        else if (bits <= 32) ct = type_make(u ? TY_U32 : TY_I32);
        else ct = type_make(u ? TY_U64 : TY_INT);

        LLVMValueRef lv = cast_value(g, l, ct);
        LLVMValueRef rv = cast_value(g, r, ct);
        if (op == TOK_AMP)
            out.value = LLVMBuildAnd(g->builder, lv, rv, "band");
        else if (op == TOK_BITOR)
            out.value = LLVMBuildOr(g->builder, lv, rv, "bor");
        else if (op == TOK_XOR)
            out.value = LLVMBuildXor(g->builder, lv, rv, "bxor");
        else if (op == TOK_SHL)
            out.value = LLVMBuildShl(g->builder, lv, rv, "bshl");
        else if (op == TOK_SHR)
            out.value = u ? LLVMBuildLShr(g->builder, lv, rv, "bshr") : LLVMBuildAShr(g->builder, lv, rv, "bshr");
        out.type = ct;
        return out;
    }
    default: break;
    }
    return out;
}

static LLVMValueRef gen_switch_match(Codegen *g, CGValue lhs, CGValue rhs)
{
    if (lhs.type.kind == TY_STRING && rhs.type.kind == TY_STRING)
    {
        LLVMValueRef argv[2] = { lhs.value, rhs.value };
        return LLVMBuildCall2(g->builder, g->fn_str_eq_ty, g->fn_str_eq, argv, 2, "sw.streq");
    }

    if ((lhs.type.kind == TY_PTR || lhs.type.kind == TY_CLASS || lhs.type.kind == TY_STRING || lhs.type.kind == TY_NULL) &&
        (rhs.type.kind == TY_PTR || rhs.type.kind == TY_CLASS || rhs.type.kind == TY_STRING || rhs.type.kind == TY_NULL))
    {
        LLVMValueRef lv = cast_value(g, lhs, type_ptr(TY_BYTE));
        LLVMValueRef rv = cast_value(g, rhs, type_ptr(TY_BYTE));
        return LLVMBuildICmp(g->builder, LLVMIntEQ, lv, rv, "sw.pcmp");
    }

    if (lhs.type.kind == TY_ENUM && rhs.type.kind == TY_ENUM)
        return LLVMBuildICmp(g->builder, LLVMIntEQ, lhs.value, rhs.value, "sw.ecmp");

    if (is_float_kind(lhs.type.kind) || is_float_kind(rhs.type.kind))
    {
        Type ft;
        if (is_float_kind(lhs.type.kind) && is_float_kind(rhs.type.kind))
        {
            int lb = float_bits(lhs.type.kind);
            int rb = float_bits(rhs.type.kind);
            ft = lb >= rb ? lhs.type : rhs.type;
        }
        else if (is_float_kind(lhs.type.kind))
            ft = lhs.type;
        else
            ft = rhs.type;
        LLVMValueRef lv = cast_value(g, lhs, ft);
        LLVMValueRef rv = cast_value(g, rhs, ft);
        return LLVMBuildFCmp(g->builder, LLVMRealOEQ, lv, rv, "sw.fcmp");
    }

    if (lhs.type.kind == TY_BOOL && rhs.type.kind == TY_BOOL)
        return LLVMBuildICmp(g->builder, LLVMIntEQ, lhs.value, rhs.value, "sw.bcmp");

    Type ct;
    int lb = int_bits(lhs.type.kind);
    int rb = int_bits(rhs.type.kind);
    int bits = lb > rb ? lb : rb;
    bool u = is_unsigned_kind(lhs.type.kind) || is_unsigned_kind(rhs.type.kind);
    if (bits <= 8) ct = type_make(u ? TY_U8 : TY_I8);
    else if (bits <= 16) ct = type_make(u ? TY_U16 : TY_I16);
    else if (bits <= 32) ct = type_make(u ? TY_U32 : TY_I32);
    else ct = type_make(u ? TY_U64 : TY_I64);

    LLVMValueRef lv = cast_value(g, lhs, ct);
    LLVMValueRef rv = cast_value(g, rhs, ct);
    return LLVMBuildICmp(g->builder, LLVMIntEQ, lv, rv, "sw.icmp");
}

static bool method_is_virtual(const Method *m)
{
    if (!m) return false;
    return m->is_virtual || m->is_override || m->is_abstract;
}

static ClassSymbol *class_root(Codegen *g, ClassSymbol *cs)
{
    if (!g || !cs) return cs;
    ClassSymbol *cur = cs;
    while (cur && cur->decl && cur->decl->base)
    {
        ClassSymbol *base = classtable_find(&g->classes, cur->decl->base);
        if (!base) break;
        cur = base;
    }
    return cur;
}

static LLVMValueRef load_vtable_ptr(Codegen *g, LLVMValueRef recv_ptr, ClassSymbol *recv_sym)
{
    ClassSymbol *root = class_root(g, recv_sym);
    if (!root) return LLVMConstNull(LLVMPointerType(g->i8ptr, 0));
    LLVMValueRef root_ptr = LLVMBuildBitCast(g->builder, recv_ptr, root->ptr_ty, "root");
    LLVMValueRef vptr_ptr = LLVMBuildStructGEP2(g->builder, root->struct_ty, root_ptr, 0, "vptr");
    LLVMTypeRef vptr_ty = LLVMPointerType(g->i8ptr, 0);
    return LLVMBuildLoad2(g->builder, vptr_ty, vptr_ptr, "vtable");
}

static LLVMValueRef load_vtable_ptr_any(Codegen *g, LLVMValueRef recv_ptr)
{
    LLVMTypeRef vptr_ptr_ty = LLVMPointerType(LLVMPointerType(g->i8ptr, 0), 0);
    LLVMValueRef vptr_ptr = LLVMBuildBitCast(g->builder, recv_ptr, vptr_ptr_ty, "vptrptr");
    LLVMTypeRef vptr_ty = LLVMPointerType(g->i8ptr, 0);
    return LLVMBuildLoad2(g->builder, vptr_ty, vptr_ptr, "vtable");
}

static LLVMValueRef const_bool(Codegen *g, int value)
{
    return LLVMConstInt(g->i1, value ? 1 : 0, 0);
}

static bool class_symbol_is_derived_from(Codegen *g, ClassSymbol *cs, const char *base_name)
{
    if (!g || !cs || !base_name) return false;
    int guard = 0;
    for (ClassSymbol *cur = cs; cur && guard++ < 256; )
    {
        const char *cur_name = cg_class_qname(cur->decl);
        if (cur_name && kn_strcmp(cur_name, base_name) == 0)
            return true;
        if (!cur->decl || !cur->decl->base)
            break;
        cur = classtable_find(&g->classes, cur->decl->base);
    }
    return false;
}

static bool class_symbol_implements_interface(Codegen *g, ClassSymbol *cs, const char *iface_name)
{
    if (!g || !cs || !cs->decl || !iface_name) return false;
    for (int i = 0; i < cs->decl->interface_count; i++)
        if (kn_strcmp(cs->decl->interfaces[i], iface_name) == 0)
            return true;
    if (cs->decl->base)
    {
        ClassSymbol *base = classtable_find(&g->classes, cs->decl->base);
        if (base && class_symbol_implements_interface(g, base, iface_name))
            return true;
    }
    return false;
}

static bool class_symbol_matches_is_target(Codegen *g, ClassSymbol *cs, Type target)
{
    if (!g || !cs || target.kind != TY_CLASS || !target.name) return false;
    if (classtable_find(&g->classes, target.name))
        return class_symbol_is_derived_from(g, cs, target.name);
    if (interface_find(g, target.name))
        return class_symbol_implements_interface(g, cs, target.name);
    return false;
}

static LLVMValueRef class_vtable_const_ptr(Codegen *g, ClassSymbol *cs)
{
    if (!g || !cs || !cs->vtable_global)
        return LLVMConstNull(LLVMPointerType(g->i8ptr, 0));
    LLVMTypeRef vtbl_ty = LLVMGetElementType(LLVMTypeOf(cs->vtable_global));
    LLVMValueRef zero = LLVMConstInt(g->i32, 0, 0);
    LLVMValueRef idxs[2] = { zero, zero };
    return LLVMConstInBoundsGEP2(vtbl_ty, cs->vtable_global, idxs, 2);
}

static LLVMValueRef gen_is_any(Codegen *g, CGValue value, Type target)
{
    LLVMValueRef anyv = cast_value(g, value, type_make(TY_ANY));
    LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "is.any.tag");
    if (target.kind == TY_ANY)
        return const_bool(g, 1);
    if (target.kind == TY_BOOL)
        return LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_BOOL, 0), "is.any.bool");
    if (target.kind == TY_CHAR)
        return LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_CHAR, 0), "is.any.char");
    if (is_float_kind(target.kind))
        return LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_F64, 0), "is.any.float");
    if (target.kind == TY_STRING)
        return LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_STRING, 0), "is.any.string");
    if (is_integer_kind(target.kind) || target.kind == TY_ENUM)
        return LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_I64, 0), "is.any.int");
    return const_bool(g, 0);
}

static LLVMValueRef gen_is_object(Codegen *g, CGValue value, Type target)
{
    LLVMValueRef obj_i8 = cast_value(g, value, type_ptr(TY_BYTE));
    LLVMValueRef is_null = LLVMBuildICmp(g->builder, LLVMIntEQ, obj_i8, LLVMConstNull(g->i8ptr), "is.obj.null");
    int candidate_count = 0;

    for (int i = 0; i < g->classes.count; i++)
    {
        ClassSymbol *cand = &g->classes.items[i];
        if (!class_symbol_matches_is_target(g, cand, target))
            continue;
        candidate_count++;
    }

    if (candidate_count == 0)
        return const_bool(g, 0);

    LLVMBasicBlockRef entry_bb = LLVMGetInsertBlock(g->builder);
    LLVMValueRef fn = LLVMGetBasicBlockParent(entry_bb);
    LLVMBasicBlockRef nonnull_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "is.obj.nonnull");
    LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "is.obj.done");
    LLVMBuildCondBr(g->builder, is_null, done_bb, nonnull_bb);

    LLVMPositionBuilderAtEnd(g->builder, nonnull_bb);
    ClassSymbol *src_sym = value.type.name ? classtable_find(&g->classes, value.type.name) : 0;
    LLVMValueRef vtbl = src_sym ? load_vtable_ptr(g, value.value, src_sym) : load_vtable_ptr_any(g, obj_i8);
    LLVMValueRef matched = const_bool(g, 0);
    int built_count = 0;
    for (int i = 0; i < g->classes.count; i++)
    {
        ClassSymbol *cand = &g->classes.items[i];
        if (!class_symbol_matches_is_target(g, cand, target))
            continue;
        LLVMValueRef eq = LLVMBuildICmp(g->builder, LLVMIntEQ, vtbl, class_vtable_const_ptr(g, cand), "is.obj.vtbl");
        matched = built_count == 0 ? eq : LLVMBuildOr(g->builder, matched, eq, "is.obj.or");
        built_count++;
    }
    LLVMBuildBr(g->builder, done_bb);

    LLVMBasicBlockRef matched_bb = LLVMGetInsertBlock(g->builder);
    LLVMPositionBuilderAtEnd(g->builder, done_bb);
    LLVMValueRef result = LLVMBuildPhi(g->builder, g->i1, "is.obj");
    LLVMValueRef incoming_vals[2] = { const_bool(g, 0), matched };
    LLVMBasicBlockRef incoming_bbs[2] = { entry_bb, matched_bb };
    LLVMAddIncoming(result, incoming_vals, incoming_bbs, 2);
    return result;
}

static LLVMValueRef gen_is_value(Codegen *g, CGValue value, Type target)
{
    Type src = value.type;
    if (src.kind == TY_ANY)
        return gen_is_any(g, value, target);
    if (src.kind == TY_NULL)
        return const_bool(g, 0);
    if (src.kind == TY_CLASS && target.kind == TY_CLASS)
        return gen_is_object(g, value, target);
    if (src.kind == TY_STRING && target.kind == TY_STRING)
        return LLVMBuildICmp(g->builder, LLVMIntNE, value.value, LLVMConstNull(g->i8ptr), "is.string");
    if (src.kind == TY_PTR && target.kind == TY_PTR && type_equal(src, target))
    {
        LLVMValueRef pv = cast_value(g, value, type_ptr(TY_BYTE));
        return LLVMBuildICmp(g->builder, LLVMIntNE, pv, LLVMConstNull(g->i8ptr), "is.ptr");
    }
    if (type_equal(src, target))
        return const_bool(g, 1);
    return const_bool(g, 0);
}

static bool is_lvalue_expr_codegen(const Expr *e)
{
    if (!e) return false;
    if (e->kind == EXPR_VAR || e->kind == EXPR_MEMBER || e->kind == EXPR_INDEX)
        return true;
    if (e->kind == EXPR_UNARY && e->v.unary.op == TOK_STAR)
        return true;
    return false;
}

static LLVMValueRef gen_member_ptr(Codegen *g, Expr *e, Type *out_type)
{
    Expr *recv = e->v.member.recv;
    if (e->v.member.is_static)
    {
        ClassSymbol *owner = classtable_find(&g->classes, e->v.member.field_owner);
        if (!owner || !owner->decl) kn_die("unknown static field owner");
        if (e->v.member.field_index < 0 || e->v.member.field_index >= owner->decl->fields.count)
            kn_die("invalid static field index");
        Field *f = &owner->decl->fields.items[e->v.member.field_index];
        if (out_type) *out_type = f->type;
        return ensure_static_field_global(g, e->v.member.field_owner, f);
    }
    if (out_type) *out_type = e->type;
    StructSymbol *ss = structtable_find(&g->structs, e->v.member.field_owner);
    if (ss)
    {
        LLVMValueRef base_ptr = 0;
        if (is_lvalue_expr_codegen(recv))
        {
            Type tmp = type_make(TY_UNKNOWN);
            base_ptr = gen_lvalue(g, recv, &tmp);
        }
        else
        {
            CGValue rv = gen_expr(g, recv);
            LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
            LLVMValueRef tmp = build_entry_alloca(g, fn, ss->struct_ty, "stmp");
            LLVMBuildStore(g->builder, rv.value, tmp);
            base_ptr = tmp;
        }
        unsigned field_idx = (unsigned)e->v.member.field_index;
        return LLVMBuildStructGEP2(g->builder, ss->struct_ty, base_ptr, field_idx, "sfield");
    }

    CGValue rv = gen_expr(g, recv);
    ClassSymbol *owner = classtable_find(&g->classes, e->v.member.field_owner);
    if (!owner) kn_die("unknown field owner");
    LLVMValueRef base_ptr = rv.value;
    if (recv->type.kind == TY_CLASS && e->v.member.field_owner &&
        kn_strcmp(recv->type.name, e->v.member.field_owner) != 0)
    {
        base_ptr = LLVMBuildBitCast(g->builder, rv.value, owner->ptr_ty, "field.base");
    }
    int layout_index = class_instance_field_layout_index(owner->decl, e->v.member.field_index);
    if (layout_index < 0) kn_die("invalid instance field layout");
    unsigned field_idx = (unsigned)(1 + layout_index);
    return LLVMBuildStructGEP2(g->builder, owner->struct_ty, base_ptr, field_idx, "field");
}

static LLVMValueRef gen_lvalue(Codegen *g, Expr *e, Type *out_type)
{
    if (e->kind == EXPR_VAR)
    {
        VarSymbol *v = scope_find(g->scope, e->v.var.name);
        if (!v)
        {
            // Check for delegate function
            FuncSymbol *fs = functable_find(&g->funcs, e->v.var.name);
            if (fs && fs->func && fs->func->is_delegate && fs->delegate_global)
            {
                if (out_type) *out_type = type_class("IO.Type.Object.Function");
                return fs->delegate_global;
            }
            kn_write_str("unknown variable: ");
            kn_write_str(e->v.var.name ? e->v.var.name : "<null>");
            kn_write_str("\n");
            kn_die("unknown variable");
        }
        if (out_type) *out_type = v->type;
        return v->value;
    }
    if (e->kind == EXPR_MEMBER)
        return gen_member_ptr(g, e, out_type);
    if (e->kind == EXPR_INDEX)
        return gen_index_ptr(g, e, out_type);
    if (e->kind == EXPR_UNARY && e->v.unary.op == TOK_STAR)
    {
        CGValue pv = gen_expr(g, e->v.unary.expr);
        if (out_type) *out_type = e->type;
        return pv.value;
    }
    kn_die("invalid lvalue");
    return 0;
}

static LLVMValueRef gen_index_ptr(Codegen *g, Expr *e, Type *out_type)
{
    CGValue recv = gen_expr(g, e->v.index.recv);
    CGValue idx = gen_expr(g, e->v.index.index);
    Type elem = type_make(TY_UNKNOWN);
    LLVMValueRef base_ptr = 0;
    if (recv.type.kind == TY_ARRAY)
    {
        elem = type_make(recv.type.elem);
        elem.name = recv.type.name;
        base_ptr = LLVMBuildExtractValue(g->builder, recv.value, 0, "arr.data");
    }
    else if (recv.type.kind == TY_STRING)
    {
        elem = type_make(TY_CHAR);
        base_ptr = recv.value;
    }
    else if (recv.type.kind == TY_PTR)
    {
        elem = type_from_elem(recv.type);
        base_ptr = recv.value;
    }
    else
    {
        kn_die("index on non-array");
    }
    if (out_type) *out_type = elem;
    LLVMValueRef idxv = cast_value(g, idx, type_make(TY_I64));
    LLVMTypeRef elem_ty = llvm_type(g, elem);
    return LLVMBuildInBoundsGEP2(g->builder, elem_ty, base_ptr, &idxv, 1, "idx");
}

static CGValue gen_new(Codegen *g, Expr *e)
{
    CGValue out;
    out.value = 0;
    out.type = type_make(TY_UNKNOWN);
    ClassSymbol *cs = classtable_find(&g->classes, e->v.new_expr.class_name);
    if (!cs) kn_die("unknown class");
    LLVMValueRef temp_frame = gc_push_temp_frame(g, "gc.new.frame");

    LLVMValueRef sizev = LLVMSizeOf(cs->struct_ty);
    if (LLVMGetTypeKind(LLVMTypeOf(sizev)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(sizev)) != 64)
        sizev = LLVMBuildZExtOrBitCast(g->builder, sizev, g->i64, "objsz");
    LLVMValueRef mem = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc,
                                      (LLVMValueRef[]){ sizev }, 1, "obj");
    LLVMValueRef obj = LLVMBuildBitCast(g->builder, mem, cs->ptr_ty, "obj");
    out.type = type_class(cg_class_qname(cs->decl));
    out.value = obj;
    out = gc_root_temp_value(g, temp_frame, out, "gc.new.obj");
    obj = out.value;

    // set vtable
    ClassSymbol *root = class_root(g, cs);
    LLVMValueRef root_ptr = LLVMBuildBitCast(g->builder, obj, root->ptr_ty, "root");
    LLVMValueRef vptr_ptr = LLVMBuildStructGEP2(g->builder, root->struct_ty, root_ptr, 0, "vptr");
    LLVMValueRef vtbl_ptr = LLVMConstNull(LLVMPointerType(g->i8ptr, 0));
    if (cs->vtable_global)
    {
        LLVMTypeRef vtbl_ty = LLVMGetElementType(LLVMTypeOf(cs->vtable_global));
        LLVMValueRef zero = LLVMConstInt(g->i32, 0, 0);
        LLVMValueRef idxs[2] = { zero, zero };
        vtbl_ptr = LLVMConstInBoundsGEP2(vtbl_ty, cs->vtable_global, idxs, 2);
    }
    LLVMBuildStore(g->builder, vtbl_ptr, vptr_ptr);

    // Array descriptors stored in class fields require owned backing storage.
    init_class_array_fields(g, cs, obj);

    // call constructor if resolved
    if (e->v.new_expr.ctor_index >= 0)
    {
        int idx = e->v.new_expr.ctor_index;
        Method *ctor = &cs->decl->methods.items[idx];
        LLVMValueRef fn = cs->methods[idx];
        LLVMTypeRef fn_ty = cs->method_tys[idx];
        int argc = e->v.new_expr.args.count + 1;
        LLVMValueRef *argv = (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)argc);
        LLVMValueRef this_arg = LLVMBuildBitCast(g->builder, obj, g->i8ptr, "this");
        argv[0] = this_arg;
        for (int i = 0; i < e->v.new_expr.args.count; i++)
        {
            Type pt = ctor->params.items[i].type;
            argv[i + 1] = gen_rooted_call_arg(g, temp_frame, e->v.new_expr.args.items[i], pt);
        }
        (void)ctor;
        LLVMBuildCall2(g->builder, fn_ty, fn, argv, (unsigned)argc, "");
    }

    gc_pop_temp_frame(g, temp_frame);
    if (e->v.new_expr.ctor_index >= 0)
        maybe_propagate_exception(g);
    out.value = obj;
    return out;
}

static int find_error_ctor_index(ClassSymbol *cs)
{
    if (!cs || !cs->decl) return -1;
    for (int i = 0; i < cs->decl->methods.count; i++)
    {
        Method *m = &cs->decl->methods.items[i];
        if (!m->is_constructor) continue;
        if (m->params.count != 2) continue;
        if (m->params.items[0].type.kind != TY_STRING) continue;
        if (m->params.items[1].type.kind != TY_STRING) continue;
        return i;
    }
    return -1;
}

static int class_instance_field_count(ClassDecl *c)
{
    if (!c) return 0;
    int count = 0;
    for (int i = 0; i < c->fields.count; i++)
    {
        if (c->fields.items[i].is_static) continue;
        count++;
    }
    return count;
}

static int class_instance_field_layout_index(ClassDecl *c, int raw_index)
{
    if (!c || raw_index < 0 || raw_index >= c->fields.count) return -1;
    if (c->fields.items[raw_index].is_static) return -1;
    int layout = 0;
    for (int i = 0; i < raw_index; i++)
    {
        if (c->fields.items[i].is_static) continue;
        layout++;
    }
    return layout;
}

static int find_class_field_index(ClassDecl *c, const char *name)
{
    if (!c || !name) return -1;
    int layout = 0;
    for (int i = 0; i < c->fields.count; i++)
    {
        if (c->fields.items[i].is_static) continue;
        if (kn_strcmp(c->fields.items[i].name, name) == 0)
            return layout;
        layout++;
    }
    return -1;
}

static const char *mangle_static_field_name(const char *class_qname, const char *field_name)
{
    char buf[512];
    size_t off = 0;
    kn_append(buf, sizeof(buf), &off, "__kn_sf$");
    if (class_qname) kn_append(buf, sizeof(buf), &off, class_qname);
    kn_append(buf, sizeof(buf), &off, "$");
    if (field_name) kn_append(buf, sizeof(buf), &off, field_name);
    char *out = (char *)kn_malloc(off + 1);
    if (!out) kn_die("out of memory");
    kn_memcpy(out, buf, off + 1);
    return out;
}

static LLVMValueRef ensure_static_field_global(Codegen *g, const char *class_qname, Field *f)
{
    const char *gname = mangle_static_field_name(class_qname, f ? f->name : 0);
    LLVMValueRef gv = LLVMGetNamedGlobal(g->mod, gname);
    if (gv) return gv;
    if (!f) kn_die("invalid static field");
    LLVMTypeRef ty = llvm_type(g, f->type);
    gv = LLVMAddGlobal(g->mod, ty, gname);
    LLVMSetLinkage(gv, LLVMInternalLinkage);
    LLVMSetInitializer(gv, LLVMConstNull(ty));
    return gv;
}

static LLVMValueRef build_error_object(Codegen *g, LLVMValueRef title, LLVMValueRef message)
{
    if (!g || !g->error_class || g->error_ctor_index < 0)
        return LLVMConstNull(g->i8ptr);

    ClassSymbol *cs = g->error_class;
    LLVMValueRef sizev = LLVMSizeOf(cs->struct_ty);
    if (LLVMGetTypeKind(LLVMTypeOf(sizev)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(sizev)) != 64)
        sizev = LLVMBuildZExtOrBitCast(g->builder, sizev, g->i64, "objsz");
    LLVMValueRef mem = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc,
                                      (LLVMValueRef[]){ sizev }, 1, "obj");
    LLVMValueRef obj = LLVMBuildBitCast(g->builder, mem, cs->ptr_ty, "obj");

    ClassSymbol *root = class_root(g, cs);
    LLVMValueRef root_ptr = LLVMBuildBitCast(g->builder, obj, root->ptr_ty, "root");
    LLVMValueRef vptr_ptr = LLVMBuildStructGEP2(g->builder, root->struct_ty, root_ptr, 0, "vptr");
    LLVMValueRef vtbl_ptr = LLVMConstNull(LLVMPointerType(g->i8ptr, 0));
    if (cs->vtable_global)
    {
        LLVMTypeRef vtbl_ty = LLVMGetElementType(LLVMTypeOf(cs->vtable_global));
        LLVMValueRef zero = LLVMConstInt(g->i32, 0, 0);
        LLVMValueRef idxs[2] = { zero, zero };
        vtbl_ptr = LLVMConstInBoundsGEP2(vtbl_ty, cs->vtable_global, idxs, 2);
    }
    LLVMBuildStore(g->builder, vtbl_ptr, vptr_ptr);

    Method *ctor = &cs->decl->methods.items[g->error_ctor_index];
    LLVMValueRef fn = cs->methods[g->error_ctor_index];
    LLVMTypeRef fn_ty = cs->method_tys[g->error_ctor_index];
    LLVMValueRef argv[3];
    argv[0] = LLVMBuildBitCast(g->builder, obj, g->i8ptr, "this");
    argv[1] = title;
    argv[2] = message;
    (void)ctor;
    LLVMBuildCall2(g->builder, fn_ty, fn, argv, 3, "");
    maybe_propagate_exception(g);

    return LLVMBuildBitCast(g->builder, obj, g->i8ptr, "err.i8");
}

static LLVMValueRef run_async_task_any(Codegen *g, LLVMValueRef taskv, const char *tag_name)
{
    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
    LLVMValueRef tag_slot = build_entry_alloca(g, fn, g->i64, tag_name ? tag_name : "async.tag");
    LLVMValueRef payload_slot = build_entry_alloca(g, fn, g->i64, "async.payload");
    LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), tag_slot);
    LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), payload_slot);

    LLVMValueRef run_args[3] = {
        LLVMBuildBitCast(g->builder, taskv, g->i8ptr, "async.task.raw"),
        tag_slot,
        payload_slot
    };
    LLVMBuildCall2(g->builder, g->fn_async_task_run_ty, g->fn_async_task_run, run_args, 3, "");

    LLVMValueRef tagv = LLVMBuildLoad2(g->builder, g->i64, tag_slot, "async.tag.v");
    LLVMValueRef payloadv = LLVMBuildLoad2(g->builder, g->i64, payload_slot, "async.payload.v");
    LLVMValueRef anyv = LLVMGetUndef(g->any_ty);
    anyv = LLVMBuildInsertValue(g->builder, anyv, tagv, 0, "async.any.tag");
    anyv = LLVMBuildInsertValue(g->builder, anyv, payloadv, 1, "async.any.payload");
    return anyv;
}

static void update_error_trace_field(Codegen *g, LLVMValueRef err_i8)
{
    if (!g || !g->error_class || g->error_trace_index < 0 || !g->fn_exc_trace) return;
    LLVMValueRef isnull = LLVMBuildICmp(g->builder, LLVMIntEQ, err_i8, LLVMConstNull(g->i8ptr), "err.trace.null");
    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
    LLVMBasicBlockRef do_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "err.trace.set");
    LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "err.trace.end");
    LLVMBuildCondBr(g->builder, isnull, end_bb, do_bb);

    LLVMPositionBuilderAtEnd(g->builder, do_bb);
    LLVMValueRef obj = LLVMBuildBitCast(g->builder, err_i8, g->error_class->ptr_ty, "err.trace.obj");
    unsigned fidx = (unsigned)(1 + g->error_trace_index);
    LLVMValueRef trace_ptr = LLVMBuildStructGEP2(g->builder, g->error_class->struct_ty, obj, fidx, "err.trace.ptr");
    LLVMValueRef trace = LLVMBuildCall2(g->builder, g->fn_exc_trace_ty, g->fn_exc_trace, 0, 0, "err.trace");
    LLVMBuildStore(g->builder, trace, trace_ptr);
    LLVMBuildBr(g->builder, end_bb);

    LLVMPositionBuilderAtEnd(g->builder, end_bb);
}

static void link_error_inner_from_last(Codegen *g, LLVMValueRef err_i8)
{
    if (!g || !g->error_class || g->error_inner_index < 0 || !g->fn_exc_last) return;
    LLVMValueRef isnull = LLVMBuildICmp(g->builder, LLVMIntEQ, err_i8, LLVMConstNull(g->i8ptr), "err.inner.null");
    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
    LLVMBasicBlockRef do_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "err.inner.do");
    LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "err.inner.end");
    LLVMBuildCondBr(g->builder, isnull, end_bb, do_bb);

    LLVMPositionBuilderAtEnd(g->builder, do_bb);
    LLVMValueRef obj = LLVMBuildBitCast(g->builder, err_i8, g->error_class->ptr_ty, "err.inner.obj");
    unsigned fidx = (unsigned)(1 + g->error_inner_index);
    LLVMValueRef inner_ptr = LLVMBuildStructGEP2(g->builder, g->error_class->struct_ty, obj, fidx, "err.inner.ptr");
    LLVMValueRef inner_cur = LLVMBuildLoad2(g->builder, g->error_class->ptr_ty, inner_ptr, "err.inner.cur");
    LLVMValueRef inner_is_null = LLVMBuildICmp(g->builder, LLVMIntEQ, inner_cur, LLVMConstNull(g->error_class->ptr_ty), "err.inner.empty");

    LLVMBasicBlockRef set_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "err.inner.set");
    LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "err.inner.done");
    LLVMBuildCondBr(g->builder, inner_is_null, set_bb, done_bb);

    LLVMPositionBuilderAtEnd(g->builder, set_bb);
    LLVMValueRef prev = LLVMBuildCall2(g->builder, g->fn_exc_last_ty, g->fn_exc_last, 0, 0, "exc.prev");
    LLVMValueRef prev_not_null = LLVMBuildICmp(g->builder, LLVMIntNE, prev, LLVMConstNull(g->i8ptr), "exc.prev.nn");
    LLVMValueRef not_self = LLVMBuildICmp(g->builder, LLVMIntNE, prev, err_i8, "exc.prev.self");
    LLVMValueRef can_link = LLVMBuildAnd(g->builder, prev_not_null, not_self, "exc.prev.link");
    LLVMValueRef prev_obj = LLVMBuildBitCast(g->builder, prev, g->error_class->ptr_ty, "exc.prev.obj");
    LLVMValueRef inner_new = LLVMBuildSelect(g->builder, can_link, prev_obj, LLVMConstNull(g->error_class->ptr_ty), "err.inner.new");
    LLVMBuildStore(g->builder, inner_new, inner_ptr);
    LLVMBuildBr(g->builder, done_bb);

    LLVMPositionBuilderAtEnd(g->builder, done_bb);
    LLVMBuildBr(g->builder, end_bb);

    LLVMPositionBuilderAtEnd(g->builder, end_bb);
}

static LLVMValueRef datetime_field_ptr(Codegen *g, LLVMValueRef obj, int field_index, const char *tag)
{
    if (!g || !g->datetime_class || field_index < 0)
        return 0;
    unsigned idx = (unsigned)(1 + field_index);
    return LLVMBuildStructGEP2(g->builder, g->datetime_class->struct_ty, obj, idx, tag);
}

static void datetime_store_i64(Codegen *g, LLVMValueRef obj, int field_index, LLVMValueRef value, const char *tag)
{
    LLVMValueRef ptr = datetime_field_ptr(g, obj, field_index, tag);
    if (!ptr) return;
    LLVMBuildStore(g->builder, value, ptr);
}

static LLVMValueRef build_datetime_now(Codegen *g)
{
    if (!g || !g->datetime_class)
        return LLVMConstNull(g->i8ptr);

    ClassSymbol *cs = g->datetime_class;
    LLVMValueRef sizev = LLVMSizeOf(cs->struct_ty);
    if (LLVMGetTypeKind(LLVMTypeOf(sizev)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(sizev)) != 64)
        sizev = LLVMBuildZExtOrBitCast(g->builder, sizev, g->i64, "dtsz");
    LLVMValueRef mem = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc,
                                      (LLVMValueRef[]){ sizev }, 1, "dt.obj");
    LLVMValueRef obj = LLVMBuildBitCast(g->builder, mem, cs->ptr_ty, "dt.ptr");

    ClassSymbol *root = class_root(g, cs);
    LLVMValueRef root_ptr = LLVMBuildBitCast(g->builder, obj, root->ptr_ty, "dt.root");
    LLVMValueRef vptr_ptr = LLVMBuildStructGEP2(g->builder, root->struct_ty, root_ptr, 0, "dt.vptr");
    LLVMValueRef vtbl_ptr = LLVMConstNull(LLVMPointerType(g->i8ptr, 0));
    if (cs->vtable_global)
    {
        LLVMTypeRef vtbl_ty = LLVMGetElementType(LLVMTypeOf(cs->vtable_global));
        LLVMValueRef zero = LLVMConstInt(g->i32, 0, 0);
        LLVMValueRef idxs[2] = { zero, zero };
        vtbl_ptr = LLVMConstInBoundsGEP2(vtbl_ty, cs->vtable_global, idxs, 2);
    }
    LLVMBuildStore(g->builder, vtbl_ptr, vptr_ptr);

    LLVMValueRef y_ptr = LLVMBuildAlloca(g->builder, g->i32, "dt.y");
    LLVMValueRef mo_ptr = LLVMBuildAlloca(g->builder, g->i32, "dt.mo");
    LLVMValueRef d_ptr = LLVMBuildAlloca(g->builder, g->i32, "dt.d");
    LLVMValueRef h_ptr = LLVMBuildAlloca(g->builder, g->i32, "dt.h");
    LLVMValueRef mi_ptr = LLVMBuildAlloca(g->builder, g->i32, "dt.mi");
    LLVMValueRef s_ptr = LLVMBuildAlloca(g->builder, g->i32, "dt.s");
    LLVMValueRef ms_ptr = LLVMBuildAlloca(g->builder, g->i32, "dt.ms");
    LLVMValueRef t_ptr = LLVMBuildAlloca(g->builder, g->i64, "dt.t");

    LLVMValueRef argv[8] = { y_ptr, mo_ptr, d_ptr, h_ptr, mi_ptr, s_ptr, ms_ptr, t_ptr };
    LLVMBuildCall2(g->builder, g->fn_time_parts_ty, g->fn_time_parts, argv, 8, "");

    LLVMValueRef tick_v = LLVMBuildLoad2(g->builder, g->i64, t_ptr, "dt.tick");
    LLVMValueRef year_v = LLVMBuildSExt(g->builder, LLVMBuildLoad2(g->builder, g->i32, y_ptr, "dt.yv"), g->i64, "dt.y64");
    LLVMValueRef month_v = LLVMBuildSExt(g->builder, LLVMBuildLoad2(g->builder, g->i32, mo_ptr, "dt.mov"), g->i64, "dt.mo64");
    LLVMValueRef day_v = LLVMBuildSExt(g->builder, LLVMBuildLoad2(g->builder, g->i32, d_ptr, "dt.dv"), g->i64, "dt.d64");
    LLVMValueRef hour_v = LLVMBuildSExt(g->builder, LLVMBuildLoad2(g->builder, g->i32, h_ptr, "dt.hv"), g->i64, "dt.h64");
    LLVMValueRef minute_v = LLVMBuildSExt(g->builder, LLVMBuildLoad2(g->builder, g->i32, mi_ptr, "dt.miv"), g->i64, "dt.mi64");
    LLVMValueRef second_v = LLVMBuildSExt(g->builder, LLVMBuildLoad2(g->builder, g->i32, s_ptr, "dt.sv"), g->i64, "dt.s64");
    LLVMValueRef milli_v = LLVMBuildSExt(g->builder, LLVMBuildLoad2(g->builder, g->i32, ms_ptr, "dt.msv"), g->i64, "dt.ms64");

    datetime_store_i64(g, obj, g->datetime_ticks_index, tick_v, "dt.ticks");
    datetime_store_i64(g, obj, g->datetime_year_index, year_v, "dt.year");
    datetime_store_i64(g, obj, g->datetime_month_index, month_v, "dt.month");
    datetime_store_i64(g, obj, g->datetime_day_index, day_v, "dt.day");
    datetime_store_i64(g, obj, g->datetime_hour_index, hour_v, "dt.hour");
    datetime_store_i64(g, obj, g->datetime_minute_index, minute_v, "dt.minute");
    datetime_store_i64(g, obj, g->datetime_second_index, second_v, "dt.second");
    datetime_store_i64(g, obj, g->datetime_millisecond_index, milli_v, "dt.millisecond");

    return obj;
}

static CGValue gen_expr(Codegen *g, Expr *e)
{
    CGValue out;
    out.value = 0;
    out.type = type_make(TY_UNKNOWN);

    switch (e->kind)
    {
    case EXPR_INT:
        out.value = LLVMConstInt(g->i64, (uint64_t)e->v.int_val, 1);
        out.type = type_make(TY_INT);
        return out;
    case EXPR_CHAR:
        out.value = LLVMConstInt(g->i8, (uint64_t)(unsigned char)e->v.int_val, 0);
        out.type = type_make(TY_CHAR);
        return out;
    case EXPR_FLOAT:
        out.value = LLVMConstReal(g->f64, e->v.float_val);
        out.type = type_make(TY_FLOAT);
        return out;
    case EXPR_BOOL:
        out.value = LLVMConstInt(g->i1, (uint64_t)(e->v.int_val != 0), 0);
        out.type = type_make(TY_BOOL);
        return out;
    case EXPR_DEFAULT:
        out.value = LLVMConstNull(llvm_type(g, e->type));
        out.type = e->type;
        return out;
    case EXPR_NULL:
        out.value = LLVMConstNull(g->i8ptr);
        out.type = type_make(TY_NULL);
        return out;
    case EXPR_STRING:
        out.value = build_string_const(g, e->v.str_val.ptr, e->v.str_val.len);
        out.type = type_make(TY_STRING);
        return out;
    case EXPR_ARRAY:
        out.value = build_array_value(g, e->type, e);
        out.type = e->type;
        return out;
    case EXPR_DICT:
        out.value = build_dict_value(g, e);
        out.type = e->type;
        return out;
    case EXPR_VAR:
    {
        VarSymbol *v = scope_find(g->scope, e->v.var.name);
        if (!v)
        {
            kn_write_str("unknown variable: ");
            kn_write_str(e->v.var.name ? e->v.var.name : "<null>");
            kn_write_str("\n");
            kn_die("unknown variable");
        }
        out.type = v->type;
        out.value = LLVMBuildLoad2(g->builder, llvm_type(g, v->type), v->value, "load");
        return out;
    }
    case EXPR_FUNC_REF:
    {
        FuncSymbol *fs = functable_find(&g->funcs, e->v.func_ref.qname);
        if (!fs)
        {
            if (e->v.func_ref.builtin_id != KN_BUILTIN_NONE)
            {
                LLVMValueRef obj = build_callable_object(g, "IO.Type.Object.Function",
                                                         LLVMConstNull(g->i8ptr), LLVMConstNull(g->i8ptr));
                out.value = obj;
                out.type = type_class("IO.Type.Object.Function");
                return out;
            }
            kn_die("unknown function reference");
        }
        LLVMValueRef wrap = ensure_func_wrapper(g, fs);
        LLVMValueRef inv = LLVMBuildBitCast(g->builder, wrap, g->i8ptr, "fnref.inv");
        LLVMValueRef obj = build_callable_object(g, "IO.Type.Object.Function", inv, LLVMConstNull(g->i8ptr));
        out.value = obj;
        out.type = type_class("IO.Type.Object.Function");
        return out;
    }
    case EXPR_ANON_FUNC:
    {
        ClosureInfo *ci = ensure_closure_info(g, e, 0);
        if (!ci || !ci->wrapper_fn) kn_die("anonymous function wrapper not generated");
        LLVMValueRef env = build_closure_env(g, ci);
        LLVMValueRef inv = LLVMBuildBitCast(g->builder, ci->wrapper_fn, g->i8ptr, "lam.inv");
        LLVMValueRef obj = build_callable_object(g, "IO.Type.Object.Function", inv, env);
        out.value = obj;
        out.type = type_class("IO.Type.Object.Function");
        return out;
    }
    case EXPR_BLOCK_LITERAL:
    {
        ClosureInfo *ci = ensure_closure_info(g, e, 1);
        if (!ci || !ci->body_fn) kn_die("block body not generated");
        LLVMValueRef env = build_closure_env(g, ci);
        LLVMValueRef inv = LLVMBuildBitCast(g->builder, ci->body_fn, g->i8ptr, "blk.inv");
        LLVMValueRef obj = build_pseudo_block_object(g, inv, env, e->v.block_lit.safety);
        out.value = obj;
        out.type = type_class("IO.Type.Object.Block");
        return out;
    }
    case EXPR_THIS:
    {
        if (!g->current_class || !g->current_this) kn_die("this outside method");
        out.value = g->current_this;
        out.type = type_class(cg_class_qname(g->current_class));
        return out;
    }
    case EXPR_BASE:
    {
        if (!g->current_class || !g->current_class->base) kn_die("base outside class");
        ClassSymbol *bs = classtable_find(&g->classes, g->current_class->base);
        if (!bs) kn_die("unknown base class");
        LLVMValueRef base_ptr = LLVMBuildBitCast(g->builder, g->current_this, bs->ptr_ty, "base");
        out.value = base_ptr;
        out.type = type_class(cg_class_qname(bs->decl));
        return out;
    }
    case EXPR_MEMBER:
    {
        if (e->v.member.is_enum_const)
        {
            Type et = type_enum(e->v.member.enum_type ? e->v.member.enum_type : "");
            LLVMTypeRef ety = llvm_type(g, et);
            out.value = LLVMConstInt(ety, (uint64_t)e->v.member.enum_value, 1);
            out.type = et;
            return out;
        }
        Type ft = type_make(TY_UNKNOWN);
        LLVMValueRef ptr = gen_member_ptr(g, e, &ft);
        out.type = ft;
        out.value = LLVMBuildLoad2(g->builder, llvm_type(g, ft), ptr, "fld");
        return out;
    }
    case EXPR_INDEX:
    {
        Type et = type_make(TY_UNKNOWN);
        LLVMValueRef ptr = gen_index_ptr(g, e, &et);
        out.type = et;
        out.value = LLVMBuildLoad2(g->builder, llvm_type(g, et), ptr, "idxval");
        return out;
    }
    case EXPR_CAST:
    {
        CGValue val = gen_expr(g, e->v.cast.expr);
        out.type = e->v.cast.type;
        // Special case: [Object.Function](ptr) — wrap raw pointer as callable object.
        if (e->v.cast.type.kind == TY_CLASS && e->v.cast.type.name &&
            kn_strcmp(e->v.cast.type.name, "IO.Type.Object.Function") == 0 &&
            val.type.kind == TY_PTR)
        {
            LLVMValueRef invoke_ptr = LLVMBuildBitCast(g->builder, val.value, g->i8ptr, "fcast.inv");
            out.value = build_callable_object(g, "IO.Type.Object.Function", invoke_ptr, 0);
            return out;
        }
        out.value = cast_value(g, val, e->v.cast.type);
        return out;
    }
    case EXPR_NEW:
        return gen_new(g, e);
    case EXPR_ASSIGN:
    {
        // Check for delegate assignment
        if (e->v.assign.target && e->v.assign.target->kind == EXPR_VAR)
        {
            FuncSymbol *dfs = functable_find(&g->funcs, e->v.assign.target->v.var.name);
            if (dfs && dfs->func && dfs->func->is_delegate && dfs->delegate_global)
            {
                LLVMValueRef store_val = 0;
                if (dfs->func->delegate_abi == KN_DELEGATE_ABI_C)
                {
                    // By C: need raw function pointer with matching native calling convention
                    Expr *rhs = e->v.assign.value;
                    // If RHS is a function reference, get the raw function address directly
                    if (rhs->kind == EXPR_FUNC_REF && rhs->v.func_ref.qname)
                    {
                        FuncSymbol *rfs = functable_find(&g->funcs, rhs->v.func_ref.qname);
                        if (rfs && rfs->fn)
                            store_val = LLVMBuildBitCast(g->builder, rfs->fn, g->i8ptr, "dlg.c.fn");
                    }
                    if (!store_val)
                    {
                        CGValue val = gen_expr(g, rhs);
                        if (val.type.kind == TY_PTR)
                        {
                            // Raw pointer (void*) — use directly as function pointer
                            store_val = LLVMBuildBitCast(g->builder, val.value, g->i8ptr, "dlg.c.raw");
                        }
                        else
                        {
                            // Object.Function — extract the Invoke pointer
                            CGValue fnobj = val;
                            if (fnobj.type.kind != TY_CLASS)
                                fnobj.type = type_class("IO.Type.Object.Function");
                            store_val = load_callable_invoke_ptr(g, fnobj);
                        }
                    }
                }
                else
                {
                    // By Kinal: wrap into Object.Function with bridge thunk
                    CGValue val = gen_expr(g, e->v.assign.value);
                    LLVMValueRef raw_ptr = 0;
                    if (val.type.kind == TY_CLASS)
                    {
                        // Object.Function — extract the raw Invoke ptr
                        CGValue fnobj = val;
                        if (!fnobj.type.name || kn_strcmp(fnobj.type.name, "IO.Type.Object.Function") != 0)
                            fnobj.type = type_class("IO.Type.Object.Function");
                        raw_ptr = load_callable_invoke_ptr(g, fnobj);
                    }
                    else
                    {
                        raw_ptr = LLVMBuildBitCast(g->builder, val.value, g->i8ptr, "dlg.kn.raw");
                    }
                    // Generate bridge thunk: any(byte* env, any* args, i64 argc)
                    // where env = raw C function pointer
                    LLVMValueRef bridge = gen_delegate_bridge(g, dfs);
                    // Build Object.Function with Invoke=bridge, Env=raw_ptr
                    store_val = LLVMBuildBitCast(g->builder,
                        build_callable_object(g, "IO.Type.Object.Function", bridge, raw_ptr),
                        g->i8ptr, "dlg.kn.obj");
                }
                LLVMBuildStore(g->builder, store_val, dfs->delegate_global);
                out.type = type_class("IO.Type.Object.Function");
                out.value = store_val;
                return out;
            }
        }
        Type tgt_type = type_make(TY_UNKNOWN);
        LLVMValueRef ptr = gen_lvalue(g, e->v.assign.target, &tgt_type);
        CGValue val = gen_expr(g, e->v.assign.value);
        LLVMValueRef cv = cast_value(g, val, tgt_type);
        LLVMBuildStore(g->builder, cv, ptr);
        out.type = tgt_type;
        out.value = cv;
        return out;
    }
    case EXPR_INVOKE:
    {
        LLVMValueRef temp_frame = gc_push_temp_frame(g, "gc.invoke.frame");
        CGValue callee = gen_expr(g, e->v.invoke.callee);
        callee = gc_root_temp_value(g, temp_frame, callee, "gc.invoke.callee");
        int argc = e->v.invoke.args.count;
        LLVMValueRef arr_ptr = argc > 0 ? LLVMBuildAlloca(g->builder, LLVMArrayType(g->any_ty, (unsigned)argc), "fn.args.arr") : 0;
        LLVMValueRef args_ptr = argc > 0
            ? LLVMBuildInBoundsGEP2(g->builder, LLVMArrayType(g->any_ty, (unsigned)argc), arr_ptr,
                                    (LLVMValueRef[]){ LLVMConstInt(g->i32, 0, 0), LLVMConstInt(g->i32, 0, 0) }, 2, "fn.args")
            : LLVMConstNull(LLVMPointerType(g->any_ty, 0));
        for (int i = 0; i < argc; i++)
        {
            CGValue av = gen_expr(g, e->v.invoke.args.items[i]);
            av = gc_root_temp_value(g, temp_frame, av, "gc.invoke.arg.raw");
            LLVMValueRef anyv = cast_value(g, av, type_make(TY_ANY));
            CGValue boxed;
            boxed.type = type_make(TY_ANY);
            boxed.value = anyv;
            boxed = gc_root_temp_value(g, temp_frame, boxed, "gc.invoke.arg.any");
            anyv = boxed.value;
            LLVMValueRef ptr = LLVMBuildInBoundsGEP2(g->builder, g->any_ty, args_ptr,
                                                     (LLVMValueRef[]){ LLVMConstInt(g->i64, (uint64_t)i, 0) }, 1, "fn.arg.ptr");
            LLVMBuildStore(g->builder, anyv, ptr);
        }
        LLVMValueRef anyv = call_callable_wrapper(g, callee, args_ptr, LLVMConstInt(g->i64, (uint64_t)argc, 0));
        gc_pop_temp_frame(g, temp_frame);
        if (e->type.kind != TY_ANY && e->type.kind != TY_UNKNOWN)
        {
            CGValue tmp;
            tmp.value = anyv;
            tmp.type = type_make(TY_ANY);
            out.value = cast_value(g, tmp, e->type);
            out.type = e->type;
        }
        else
        {
            out.value = anyv;
            out.type = type_make(TY_ANY);
        }
        maybe_propagate_exception(g);
        return out;
    }
    case EXPR_AWAIT:
    {
        CGValue taskv = gen_expr(g, e->v.await_expr.expr);
        LLVMValueRef anyv = run_async_task_any(g, taskv.value, "await.tag");
        maybe_propagate_exception(g);
        if (e->type.kind == TY_VOID)
        {
            out.value = LLVMGetUndef(g->voidt);
            out.type = type_make(TY_VOID);
            return out;
        }
        if (e->type.kind == TY_ANY || e->type.kind == TY_UNKNOWN)
        {
            out.value = anyv;
            out.type = type_make(TY_ANY);
            return out;
        }
        out.value = any_to_type(g, anyv, e->type);
        out.type = e->type;
        return out;
    }
    case EXPR_CALL:
    {
        if (e->v.call.builtin_id != KN_BUILTIN_NONE)
        {
            KnBuiltinKind bk = (KnBuiltinKind)e->v.call.builtin_id;
            if (bk >= KN_BUILTIN_IO_TYPE_BOOL && bk <= KN_BUILTIN_IO_TYPE_ANY && e->v.call.args.count == 1)
            {
                Type dst = type_make(TY_UNKNOWN);
                switch (bk)
                {
                case KN_BUILTIN_IO_TYPE_BOOL: dst = type_make(TY_BOOL); break;
                case KN_BUILTIN_IO_TYPE_BYTE: dst = type_make(TY_BYTE); break;
                case KN_BUILTIN_IO_TYPE_CHAR: dst = type_make(TY_CHAR); break;
                case KN_BUILTIN_IO_TYPE_INT: dst = type_make(TY_INT); break;
                case KN_BUILTIN_IO_TYPE_FLOAT: dst = type_make(TY_FLOAT); break;
                case KN_BUILTIN_IO_TYPE_F16: dst = type_make(TY_F16); break;
                case KN_BUILTIN_IO_TYPE_F32: dst = type_make(TY_F32); break;
                case KN_BUILTIN_IO_TYPE_F64: dst = type_make(TY_F64); break;
                case KN_BUILTIN_IO_TYPE_F128: dst = type_make(TY_F128); break;
                case KN_BUILTIN_IO_TYPE_I8: dst = type_make(TY_I8); break;
                case KN_BUILTIN_IO_TYPE_I16: dst = type_make(TY_I16); break;
                case KN_BUILTIN_IO_TYPE_I32: dst = type_make(TY_I32); break;
                case KN_BUILTIN_IO_TYPE_I64: dst = type_make(TY_I64); break;
                case KN_BUILTIN_IO_TYPE_U8: dst = type_make(TY_U8); break;
                case KN_BUILTIN_IO_TYPE_U16: dst = type_make(TY_U16); break;
                case KN_BUILTIN_IO_TYPE_U32: dst = type_make(TY_U32); break;
                case KN_BUILTIN_IO_TYPE_U64: dst = type_make(TY_U64); break;
                case KN_BUILTIN_IO_TYPE_ISIZE: dst = type_make(TY_ISIZE); break;
                case KN_BUILTIN_IO_TYPE_USIZE: dst = type_make(TY_USIZE); break;
                case KN_BUILTIN_IO_TYPE_STRING: dst = type_make(TY_STRING); break;
                case KN_BUILTIN_IO_TYPE_ANY: dst = type_make(TY_ANY); break;
                default: dst = type_make(TY_UNKNOWN); break;
                }
                CGValue srcv = gen_expr(g, e->v.call.args.items[0]);
                out.value = cast_value(g, srcv, dst);
                out.type = dst;
                return out;
            }
            ExprList builtin_args = e->v.call.args;
            return gen_builtin_call(g, bk, &builtin_args, 0);
        }
        FuncSymbol *fs = functable_find(&g->funcs, e->v.call.name);
        if (!fs) kn_die("unknown function");
        Type public_ret = cg_func_public_ret_type(fs->func);
        int argc = e->v.call.args.count;
        LLVMValueRef *args = (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)argc);
        LLVMValueRef temp_frame = gc_push_temp_frame(g, "gc.call.frame");
        for (int i = 0; i < argc; i++)
        {
            Type pt = fs->func->params.items[i].type;
            args[i] = gen_rooted_call_arg(g, temp_frame, e->v.call.args.items[i], pt);
        }
        const char *cname = public_ret.kind == TY_VOID ? "" : "call";
        if (fs->func->is_delegate && fs->delegate_global)
        {
            // Delegate call: load function pointer from global, bitcast, and call
            if (fs->func->delegate_abi == KN_DELEGATE_ABI_C)
            {
                // By C: direct native call through raw function pointer
                LLVMValueRef raw = LLVMBuildLoad2(g->builder, g->i8ptr, fs->delegate_global, "dlg.raw");
                LLVMValueRef fn_ptr = LLVMBuildBitCast(g->builder, raw, LLVMPointerType(fs->fn_ty, 0), "dlg.fn");
                out.value = LLVMBuildCall2(g->builder, fs->fn_ty, fn_ptr, args, (unsigned)argc, cname);
            }
            else
            {
                // By Kinal: load Object.Function pointer, call through callable wrapper
                LLVMValueRef obj_ptr = LLVMBuildLoad2(g->builder, g->i8ptr, fs->delegate_global, "dlg.obj");
                // Build any[] args array for callable wrapper
                LLVMValueRef arr_ptr = argc > 0 ? LLVMBuildAlloca(g->builder, LLVMArrayType(g->any_ty, (unsigned)argc), "dlg.args.arr") : 0;
                LLVMValueRef any_args_ptr = argc > 0
                    ? LLVMBuildInBoundsGEP2(g->builder, LLVMArrayType(g->any_ty, (unsigned)argc), arr_ptr,
                                            (LLVMValueRef[]){ LLVMConstInt(g->i32, 0, 0), LLVMConstInt(g->i32, 0, 0) }, 2, "dlg.args")
                    : LLVMConstNull(LLVMPointerType(g->any_ty, 0));
                for (int i = 0; i < argc; i++)
                {
                    CGValue av;
                    av.value = args[i];
                    av.type = fs->func->params.items[i].type;
                    LLVMValueRef anyv = box_any_value(g, av);
                    LLVMValueRef ptr = LLVMBuildInBoundsGEP2(g->builder, g->any_ty, any_args_ptr,
                                                             (LLVMValueRef[]){ LLVMConstInt(g->i64, (uint64_t)i, 0) }, 1, "dlg.arg.ptr");
                    LLVMBuildStore(g->builder, anyv, ptr);
                }
                CGValue fnobj;
                fnobj.value = obj_ptr;
                fnobj.type = type_class("IO.Type.Object.Function");
                LLVMValueRef anyv = call_callable_wrapper(g, fnobj, any_args_ptr, LLVMConstInt(g->i64, (uint64_t)argc, 0));
                if (fs->func->ret_type.kind != TY_VOID && fs->func->ret_type.kind != TY_ANY)
                {
                    CGValue tmp;
                    tmp.value = anyv;
                    tmp.type = type_make(TY_ANY);
                    out.value = cast_value(g, tmp, fs->func->ret_type);
                }
                else if (fs->func->ret_type.kind == TY_VOID)
                {
                    out.value = LLVMGetUndef(LLVMVoidTypeInContext(g->ctx));
                }
                else
                {
                    out.value = anyv;
                }
            }
        }
        else
        {
            out.value = LLVMBuildCall2(g->builder, fs->fn_ty, fs->fn, args, (unsigned)argc, cname);
        }
        gc_pop_temp_frame(g, temp_frame);
        if (fs->func->is_extern && fs->func->extern_abi == KN_EXTERN_ABI_SYSTEM && g->target_is_windows && g->target_is_x86)
            LLVMSetInstructionCallConv(out.value, LLVMX86StdcallCallConv);
        out.type = public_ret;
        maybe_propagate_exception(g);
        return out;
    }
    case EXPR_MEMBER_CALL:
    {
        if (e->v.member_call.builtin_id != KN_BUILTIN_NONE)
        {
            KnBuiltinKind bk = (KnBuiltinKind)e->v.member_call.builtin_id;
            if (bk >= KN_BUILTIN_IO_TYPE_BOOL && bk <= KN_BUILTIN_IO_TYPE_ANY && e->v.member_call.args.count == 1)
            {
                Type dst = type_make(TY_UNKNOWN);
                switch (bk)
                {
                case KN_BUILTIN_IO_TYPE_BOOL: dst = type_make(TY_BOOL); break;
                case KN_BUILTIN_IO_TYPE_BYTE: dst = type_make(TY_BYTE); break;
                case KN_BUILTIN_IO_TYPE_CHAR: dst = type_make(TY_CHAR); break;
                case KN_BUILTIN_IO_TYPE_INT: dst = type_make(TY_INT); break;
                case KN_BUILTIN_IO_TYPE_FLOAT: dst = type_make(TY_FLOAT); break;
                case KN_BUILTIN_IO_TYPE_F16: dst = type_make(TY_F16); break;
                case KN_BUILTIN_IO_TYPE_F32: dst = type_make(TY_F32); break;
                case KN_BUILTIN_IO_TYPE_F64: dst = type_make(TY_F64); break;
                case KN_BUILTIN_IO_TYPE_F128: dst = type_make(TY_F128); break;
                case KN_BUILTIN_IO_TYPE_I8: dst = type_make(TY_I8); break;
                case KN_BUILTIN_IO_TYPE_I16: dst = type_make(TY_I16); break;
                case KN_BUILTIN_IO_TYPE_I32: dst = type_make(TY_I32); break;
                case KN_BUILTIN_IO_TYPE_I64: dst = type_make(TY_I64); break;
                case KN_BUILTIN_IO_TYPE_U8: dst = type_make(TY_U8); break;
                case KN_BUILTIN_IO_TYPE_U16: dst = type_make(TY_U16); break;
                case KN_BUILTIN_IO_TYPE_U32: dst = type_make(TY_U32); break;
                case KN_BUILTIN_IO_TYPE_U64: dst = type_make(TY_U64); break;
                case KN_BUILTIN_IO_TYPE_ISIZE: dst = type_make(TY_ISIZE); break;
                case KN_BUILTIN_IO_TYPE_USIZE: dst = type_make(TY_USIZE); break;
                case KN_BUILTIN_IO_TYPE_STRING: dst = type_make(TY_STRING); break;
                case KN_BUILTIN_IO_TYPE_ANY: dst = type_make(TY_ANY); break;
                default: dst = type_make(TY_UNKNOWN); break;
                }
                CGValue srcv = gen_expr(g, e->v.member_call.args.items[0]);
                out.value = cast_value(g, srcv, dst);
                out.type = dst;
                return out;
            }
            if (bk == KN_BUILTIN_ARRAY_LENGTH)
            {
                CGValue arr = gen_expr(g, e->v.member_call.recv);
                out.value = LLVMBuildExtractValue(g->builder, arr.value, 1, "len");
                out.type = type_make(TY_INT);
                return out;
            }
            if (bk == KN_BUILTIN_ARRAY_INDEX && e->v.member_call.args.count == 1)
            {
                CGValue arr = gen_expr(g, e->v.member_call.recv);
                CGValue idx = gen_expr(g, e->v.member_call.args.items[0]);
                LLVMValueRef idxv = cast_value(g, idx, type_make(TY_I64));
                Type elem = type_make(arr.type.elem);
                elem.name = arr.type.name;
                LLVMTypeRef elem_ty = llvm_type(g, elem);
                LLVMValueRef data = LLVMBuildExtractValue(g->builder, arr.value, 0, "arr.data");
                LLVMValueRef ptr = LLVMBuildInBoundsGEP2(g->builder, elem_ty, data, &idxv, 1, "arr.idx");
                out.value = LLVMBuildLoad2(g->builder, elem_ty, ptr, "arr.val");
                out.type = elem;
                return out;
            }
            ExprList builtin_args = e->v.member_call.args;
            Expr *builtin_recv = e->v.member_call.is_static ? 0 : e->v.member_call.recv;
            return gen_builtin_call(g, bk, &builtin_args, builtin_recv);
        }
        if (e->v.member_call.is_qual_call && e->v.member_call.qual_name)
        {
            FuncSymbol *fs = functable_find(&g->funcs, e->v.member_call.qual_name);
            if (!fs) kn_die("unknown qualified function");
            Type public_ret = cg_func_public_ret_type(fs->func);
            int argc = e->v.member_call.args.count;
            LLVMValueRef *argv = (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)argc);
            LLVMValueRef temp_frame = gc_push_temp_frame(g, "gc.qcall.frame");
            for (int i = 0; i < argc; i++)
            {
                Type pt = fs->func->params.items[i].type;
                argv[i] = gen_rooted_call_arg(g, temp_frame, e->v.member_call.args.items[i], pt);
            }
            const char *cname = public_ret.kind == TY_VOID ? "" : "qcall";
            if (fs->func->is_delegate && fs->delegate_global)
            {
                if (fs->func->delegate_abi == KN_DELEGATE_ABI_C)
                {
                    LLVMValueRef raw = LLVMBuildLoad2(g->builder, g->i8ptr, fs->delegate_global, "qdlg.raw");
                    LLVMValueRef fn_ptr = LLVMBuildBitCast(g->builder, raw, LLVMPointerType(fs->fn_ty, 0), "qdlg.fn");
                    out.value = LLVMBuildCall2(g->builder, fs->fn_ty, fn_ptr, argv, (unsigned)argc, cname);
                }
                else
                {
                    LLVMValueRef obj_ptr = LLVMBuildLoad2(g->builder, g->i8ptr, fs->delegate_global, "qdlg.obj");
                    LLVMValueRef arr_ptr = argc > 0 ? LLVMBuildAlloca(g->builder, LLVMArrayType(g->any_ty, (unsigned)argc), "qdlg.args.arr") : 0;
                    LLVMValueRef any_args_ptr = argc > 0
                        ? LLVMBuildInBoundsGEP2(g->builder, LLVMArrayType(g->any_ty, (unsigned)argc), arr_ptr,
                                                (LLVMValueRef[]){ LLVMConstInt(g->i32, 0, 0), LLVMConstInt(g->i32, 0, 0) }, 2, "qdlg.args")
                        : LLVMConstNull(LLVMPointerType(g->any_ty, 0));
                    for (int i = 0; i < argc; i++)
                    {
                        CGValue av;
                        av.value = argv[i];
                        av.type = fs->func->params.items[i].type;
                        LLVMValueRef anyv = box_any_value(g, av);
                        LLVMValueRef ptr = LLVMBuildInBoundsGEP2(g->builder, g->any_ty, any_args_ptr,
                                                                 (LLVMValueRef[]){ LLVMConstInt(g->i64, (uint64_t)i, 0) }, 1, "qdlg.arg.ptr");
                        LLVMBuildStore(g->builder, anyv, ptr);
                    }
                    CGValue fnobj;
                    fnobj.value = obj_ptr;
                    fnobj.type = type_class("IO.Type.Object.Function");
                    LLVMValueRef anyv = call_callable_wrapper(g, fnobj, any_args_ptr, LLVMConstInt(g->i64, (uint64_t)argc, 0));
                    if (fs->func->ret_type.kind != TY_VOID && fs->func->ret_type.kind != TY_ANY)
                    {
                        CGValue tmp;
                        tmp.value = anyv;
                        tmp.type = type_make(TY_ANY);
                        out.value = cast_value(g, tmp, fs->func->ret_type);
                    }
                    else if (fs->func->ret_type.kind == TY_VOID)
                    {
                        out.value = LLVMGetUndef(LLVMVoidTypeInContext(g->ctx));
                    }
                    else
                    {
                        out.value = anyv;
                    }
                }
            }
            else
            {
                out.value = LLVMBuildCall2(g->builder, fs->fn_ty, fs->fn, argv, (unsigned)argc, cname);
            }
            gc_pop_temp_frame(g, temp_frame);
            if (fs->func->is_extern && fs->func->extern_abi == KN_EXTERN_ABI_SYSTEM && g->target_is_windows && g->target_is_x86)
                LLVMSetInstructionCallConv(out.value, LLVMX86StdcallCallConv);
            out.type = public_ret;
            maybe_propagate_exception(g);
            return out;
        }
        ClassSymbol *owner = classtable_find(&g->classes, e->v.member_call.method_owner);
        if (owner)
        {
            Method *m = &owner->decl->methods.items[e->v.member_call.method_index];
            LLVMValueRef fn = owner->methods[e->v.member_call.method_index];
            LLVMTypeRef fn_ty = owner->method_tys[e->v.member_call.method_index];

            int argc = e->v.member_call.args.count + (m->is_static ? 0 : 1);
            LLVMValueRef *argv = (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)argc);
            int argi = 0;
            LLVMValueRef temp_frame = gc_push_temp_frame(g, "gc.mcall.frame");
            if (!m->is_static)
            {
                CGValue recv = gen_expr(g, e->v.member_call.recv);
                recv = gc_root_temp_value(g, temp_frame, recv, "gc.mcall.recv");
                LLVMValueRef this_arg = LLVMBuildBitCast(g->builder, recv.value, g->i8ptr, "this");
                argv[argi++] = this_arg;

                bool use_vtable = method_is_virtual(m) && e->v.member_call.recv && e->v.member_call.recv->kind != EXPR_BASE;
                if (use_vtable && owner->method_vslots)
                {
                    int slot = owner->method_vslots[e->v.member_call.method_index];
                    if (slot >= 0)
                    {
                        ClassSymbol *recv_sym = classtable_find(&g->classes, recv.type.name);
                        LLVMValueRef vtbl = recv_sym ? load_vtable_ptr(g, recv.value, recv_sym) : load_vtable_ptr_any(g, recv.value);
                        LLVMValueRef idxv = LLVMConstInt(g->i32, (uint64_t)slot, 0);
                        LLVMValueRef slot_ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8ptr, vtbl, &idxv, 1, "vslot");
                        LLVMValueRef fn_i8 = LLVMBuildLoad2(g->builder, g->i8ptr, slot_ptr, "vfn");
                        LLVMValueRef fn_ptr = LLVMBuildBitCast(g->builder, fn_i8, LLVMPointerType(fn_ty, 0), "vfnc");
                        for (int i = 0; i < e->v.member_call.args.count; i++)
                        {
                            Type pt = m->params.items[i].type;
                            argv[argi++] = gen_rooted_call_arg(g, temp_frame, e->v.member_call.args.items[i], pt);
                        }
                        const char *vcname = m->ret_type.kind == TY_VOID ? "" : "vcall";
                        out.value = LLVMBuildCall2(g->builder, fn_ty, fn_ptr, argv, (unsigned)argc, vcname);
                        gc_pop_temp_frame(g, temp_frame);
                        out.type = m->ret_type;
                        maybe_propagate_exception(g);
                        return out;
                    }
                }
            }

            for (int i = 0; i < e->v.member_call.args.count; i++)
            {
                Type pt = m->params.items[i].type;
                argv[argi++] = gen_rooted_call_arg(g, temp_frame, e->v.member_call.args.items[i], pt);
            }
            const char *mcname = m->ret_type.kind == TY_VOID ? "" : "call";
            out.value = LLVMBuildCall2(g->builder, fn_ty, fn, argv, (unsigned)argc, mcname);
            gc_pop_temp_frame(g, temp_frame);
            out.type = m->ret_type;
            maybe_propagate_exception(g);
            return out;
        }

        InterfaceDecl *iface = interface_find(g, e->v.member_call.method_owner);
        if (!iface) kn_die("unknown method owner");
        Method *im = &iface->methods.items[e->v.member_call.method_index];
        LLVMTypeRef fn_ty = method_fn_type(g, im, 0);
        int argc = e->v.member_call.args.count + 1;
        LLVMValueRef *argv = (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)argc);
        LLVMValueRef temp_frame = gc_push_temp_frame(g, "gc.icall.frame");
        CGValue recv = gen_expr(g, e->v.member_call.recv);
        recv = gc_root_temp_value(g, temp_frame, recv, "gc.icall.recv");
        LLVMValueRef this_arg = LLVMBuildBitCast(g->builder, recv.value, g->i8ptr, "this");
        argv[0] = this_arg;
        for (int i = 0; i < e->v.member_call.args.count; i++)
        {
            Type pt = im->params.items[i].type;
            argv[i + 1] = gen_rooted_call_arg(g, temp_frame, e->v.member_call.args.items[i], pt);
        }
        int slot = vslot_find_method(&g->vslots, im);
        if (slot < 0) kn_die("missing interface slot");
        ClassSymbol *recv_sym = classtable_find(&g->classes, recv.type.name);
        LLVMValueRef vtbl = recv_sym ? load_vtable_ptr(g, recv.value, recv_sym) : load_vtable_ptr_any(g, recv.value);
        LLVMValueRef idxv = LLVMConstInt(g->i32, (uint64_t)slot, 0);
        LLVMValueRef slot_ptr = LLVMBuildInBoundsGEP2(g->builder, g->i8ptr, vtbl, &idxv, 1, "ifslot");
        LLVMValueRef fn_i8 = LLVMBuildLoad2(g->builder, g->i8ptr, slot_ptr, "iffn");
        LLVMValueRef fn_ptr = LLVMBuildBitCast(g->builder, fn_i8, LLVMPointerType(fn_ty, 0), "iffnc");
        const char *icname = im->ret_type.kind == TY_VOID ? "" : "icall";
        out.value = LLVMBuildCall2(g->builder, fn_ty, fn_ptr, argv, (unsigned)argc, icname);
        gc_pop_temp_frame(g, temp_frame);
        out.type = im->ret_type;
        maybe_propagate_exception(g);
        return out;
    }
    case EXPR_BINARY:
        return gen_binary(g, e);
    case EXPR_IF:
    {
        CGValue c = gen_expr(g, e->v.if_expr.cond);
        LLVMValueRef cond = cast_value(g, c, type_make(TY_BOOL));
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
        LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "ifexpr.then");
        LLVMBasicBlockRef else_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "ifexpr.else");
        LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "ifexpr.merge");
        LLVMBuildCondBr(g->builder, cond, then_bb, else_bb);

        LLVMPositionBuilderAtEnd(g->builder, then_bb);
        CGValue tv = gen_expr(g, e->v.if_expr.then_expr);
        LLVMValueRef tvc = cast_value(g, tv, e->type);
        LLVMBuildBr(g->builder, merge_bb);
        LLVMBasicBlockRef then_end = LLVMGetInsertBlock(g->builder);

        LLVMPositionBuilderAtEnd(g->builder, else_bb);
        CGValue ev = gen_expr(g, e->v.if_expr.else_expr);
        LLVMValueRef evc = cast_value(g, ev, e->type);
        LLVMBuildBr(g->builder, merge_bb);
        LLVMBasicBlockRef else_end = LLVMGetInsertBlock(g->builder);

        LLVMPositionBuilderAtEnd(g->builder, merge_bb);
        LLVMValueRef phi = LLVMBuildPhi(g->builder, llvm_type(g, e->type), "ifexpr");
        LLVMValueRef vals[2] = { tvc, evc };
        LLVMBasicBlockRef bbs[2] = { then_end, else_end };
        LLVMAddIncoming(phi, vals, bbs, 2);
        out.value = phi;
        out.type = e->type;
        return out;
    }
    case EXPR_SWITCH:
    {
        int case_count = e->v.switch_expr.cases.count;
        CGValue sv = gen_expr(g, e->v.switch_expr.value);
        LLVMValueRef temp_frame = gc_push_temp_frame(g, "gc.swexpr.frame");
        sv = gc_root_temp_value(g, temp_frame, sv, "gc.swexpr.value");
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
        LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "swexpr.merge");
        LLVMBasicBlockRef fallback_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "swexpr.fallback");
        LLVMBasicBlockRef *case_bbs = (LLVMBasicBlockRef *)kn_malloc(sizeof(LLVMBasicBlockRef) * (size_t)(case_count > 0 ? case_count : 1));
        LLVMValueRef *incoming_vals = (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)(case_count + 1));
        LLVMBasicBlockRef *incoming_bbs = (LLVMBasicBlockRef *)kn_malloc(sizeof(LLVMBasicBlockRef) * (size_t)(case_count + 1));
        int incoming_count = 0;

        for (int i = 0; i < case_count; i++)
            case_bbs[i] = LLVMAppendBasicBlockInContext(g->ctx, fn, e->v.switch_expr.cases.items[i].is_default ? "swexpr.default" : "swexpr.case");

        LLVMBasicBlockRef default_bb = fallback_bb;
        LLVMBasicBlockRef cur_check = LLVMGetInsertBlock(g->builder);
        for (int i = 0; i < case_count; i++)
        {
            ExprSwitchCase *c = &e->v.switch_expr.cases.items[i];
            if (c->is_default)
            {
                default_bb = case_bbs[i];
                continue;
            }
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "swexpr.next");
            LLVMPositionBuilderAtEnd(g->builder, cur_check);
            CGValue mv = gen_expr(g, c->match);
            LLVMValueRef cond = gen_switch_match(g, sv, mv);
            LLVMBuildCondBr(g->builder, cond, case_bbs[i], next_bb);
            cur_check = next_bb;
        }

        LLVMPositionBuilderAtEnd(g->builder, cur_check);
        LLVMBuildBr(g->builder, default_bb);

        for (int i = 0; i < case_count; i++)
        {
            LLVMPositionBuilderAtEnd(g->builder, case_bbs[i]);
            gc_pop_temp_frame(g, temp_frame);
            CGValue bv = gen_expr(g, e->v.switch_expr.cases.items[i].body);
            LLVMValueRef cv = cast_value(g, bv, e->type);
            LLVMBuildBr(g->builder, merge_bb);
            incoming_vals[incoming_count] = cv;
            incoming_bbs[incoming_count] = LLVMGetInsertBlock(g->builder);
            incoming_count++;
        }

        LLVMPositionBuilderAtEnd(g->builder, fallback_bb);
        gc_pop_temp_frame(g, temp_frame);
        LLVMValueRef fallback = LLVMConstNull(llvm_type(g, e->type));
        LLVMBuildBr(g->builder, merge_bb);
        incoming_vals[incoming_count] = fallback;
        incoming_bbs[incoming_count] = LLVMGetInsertBlock(g->builder);
        incoming_count++;

        LLVMPositionBuilderAtEnd(g->builder, merge_bb);
        LLVMValueRef phi = LLVMBuildPhi(g->builder, llvm_type(g, e->type), "swexpr");
        LLVMAddIncoming(phi, incoming_vals, incoming_bbs, (unsigned)incoming_count);
        out.value = phi;
        out.type = e->type;
        return out;
    }
    case EXPR_IS:
    {
        CGValue value = gen_expr(g, e->v.is_expr.expr);
        Type target = e->v.is_expr.target;
        out.value = gen_is_value(g, value, target);
        out.type = type_make(TY_BOOL);
        return out;
    }
    case EXPR_UNARY:
    {
        if (e->v.unary.op == TOK_AMP &&
            e->v.unary.expr &&
            e->v.unary.expr->kind == EXPR_FUNC_REF)
        {
            if (e->v.unary.expr->v.func_ref.builtin_id != KN_BUILTIN_NONE)
                kn_die("cannot take address of builtin function");
            FuncSymbol *fs = functable_find(&g->funcs, e->v.unary.expr->v.func_ref.qname);
            if (!fs || !fs->fn)
                kn_die("unknown function for address-of");
            out.value = LLVMBuildBitCast(g->builder, fs->fn, llvm_type(g, e->type), "fn.addr");
            out.type = e->type;
            return out;
        }
        if (e->v.unary.op == TOK_PLUSPLUS || e->v.unary.op == TOK_MINUSMINUS)
        {
            Type lt = type_make(TY_UNKNOWN);
            LLVMValueRef ptr = gen_lvalue(g, e->v.unary.expr, &lt);
            LLVMValueRef oldv = LLVMBuildLoad2(g->builder, llvm_type(g, lt), ptr, "incdec.old");

            LLVMValueRef one_i64 = LLVMConstInt(g->i64, 1, 0);
            LLVMValueRef newv = 0;
            if (lt.kind == TY_PTR)
            {
                Type elem = type_from_elem(lt);
                LLVMTypeRef elem_ty = llvm_type(g, elem);
                LLVMValueRef idx = one_i64;
                if (e->v.unary.op == TOK_MINUSMINUS)
                    idx = LLVMBuildNeg(g->builder, idx, "incdec.pneg");
                newv = LLVMBuildInBoundsGEP2(g->builder, elem_ty, oldv, &idx, 1, "incdec.ptr");
            }
            else if (is_float_kind(lt.kind))
            {
                LLVMValueRef one = LLVMConstReal(llvm_float_type(g, lt.kind), 1.0);
                newv = (e->v.unary.op == TOK_PLUSPLUS)
                    ? LLVMBuildFAdd(g->builder, oldv, one, "incdec.fadd")
                    : LLVMBuildFSub(g->builder, oldv, one, "incdec.fsub");
            }
            else
            {
                LLVMValueRef one = cast_value(g, (CGValue){ one_i64, type_make(TY_I64) }, lt);
                newv = (e->v.unary.op == TOK_PLUSPLUS)
                    ? LLVMBuildAdd(g->builder, oldv, one, "incdec.add")
                    : LLVMBuildSub(g->builder, oldv, one, "incdec.sub");
            }

            LLVMBuildStore(g->builder, newv, ptr);
            out.value = e->v.unary.is_postfix ? oldv : newv;
            out.type = lt;
            return out;
        }
        CGValue v = gen_expr(g, e->v.unary.expr);
        if (e->v.unary.op == TOK_MINUS)
        {
            if (is_float_kind(v.type.kind))
                out.value = LLVMBuildFNeg(g->builder, v.value, "fneg");
            else
                out.value = LLVMBuildNeg(g->builder, v.value, "neg");
            out.type = v.type;
            return out;
        }
        if (e->v.unary.op == TOK_NOT)
        {
            LLVMValueRef one = LLVMConstInt(g->i1, 1, 0);
            out.value = LLVMBuildXor(g->builder, v.value, one, "not");
            out.type = v.type;
            return out;
        }
        if (e->v.unary.op == TOK_TILDE)
        {
            out.value = LLVMBuildNot(g->builder, v.value, "bnot");
            out.type = v.type;
            return out;
        }
        if (e->v.unary.op == TOK_STAR)
        {
            Type elem = type_from_elem(v.type);
            out.value = LLVMBuildLoad2(g->builder, llvm_type(g, elem), v.value, "deref");
            out.type = elem;
            return out;
        }
        if (e->v.unary.op == TOK_AMP)
        {
            Type tmp = type_make(TY_UNKNOWN);
            out.value = gen_lvalue(g, e->v.unary.expr, &tmp);
            out.type = e->type;
            return out;
        }
        kn_die("bad unary op");
        return out;
    }
    default: break;
    }

    kn_die("unknown expr");
    return out;
}

static LLVMValueRef build_entry_alloca(Codegen *g, LLVMValueRef fn, LLVMTypeRef ty, const char *name)
{
    LLVMBuilderRef b = LLVMCreateBuilderInContext(g->ctx);
    LLVMBasicBlockRef entry = LLVMGetEntryBasicBlock(fn);
    LLVMValueRef first = LLVMGetFirstInstruction(entry);
    if (first)
        LLVMPositionBuilderBefore(b, first);
    else
        LLVMPositionBuilderAtEnd(b, entry);
    LLVMValueRef alloca = LLVMBuildAlloca(b, ty, name);
    LLVMDisposeBuilder(b);
    return alloca;
}

static void gc_add_root(Codegen *g, LLVMValueRef slot, Type t)
{
    if (!g || !g->gc_frame) return;
    if (!is_gc_root_type(t)) return;
    LLVMValueRef sizev = LLVMSizeOf(llvm_type(g, t));
    if (LLVMGetTypeKind(LLVMTypeOf(sizev)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(sizev)) != 64)
        sizev = LLVMBuildZExtOrBitCast(g->builder, sizev, g->i64, "gc.sz");
    LLVMValueRef addr = LLVMBuildBitCast(g->builder, slot, g->i8ptr, "gc.addr");
    LLVMValueRef argv[3] = { g->gc_frame, addr, sizev };
    LLVMBuildCall2(g->builder, g->fn_gc_add_root_ty, g->fn_gc_add_root, argv, 3, "");
}

static void gc_add_root_to_frame(Codegen *g, LLVMValueRef frame, LLVMValueRef slot, Type t)
{
    if (!g || !frame) return;
    if (!is_gc_root_type(t)) return;
    LLVMValueRef sizev = LLVMSizeOf(llvm_type(g, t));
    if (LLVMGetTypeKind(LLVMTypeOf(sizev)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(sizev)) != 64)
        sizev = LLVMBuildZExtOrBitCast(g->builder, sizev, g->i64, "gc.sz");
    LLVMValueRef addr = LLVMBuildBitCast(g->builder, slot, g->i8ptr, "gc.addr");
    LLVMValueRef argv[3] = { frame, addr, sizev };
    LLVMBuildCall2(g->builder, g->fn_gc_add_root_ty, g->fn_gc_add_root, argv, 3, "");
}

static LLVMValueRef gc_push_temp_frame(Codegen *g, const char *name)
{
    if (!g || !g->fn_gc_push)
        return 0;
    return LLVMBuildCall2(g->builder, g->fn_gc_push_ty, g->fn_gc_push, 0, 0, name ? name : "gc.temp");
}

static void gc_pop_temp_frame(Codegen *g, LLVMValueRef frame)
{
    if (!g || !frame || !g->fn_gc_pop)
        return;
    LLVMValueRef argv[1] = { frame };
    LLVMBuildCall2(g->builder, g->fn_gc_pop_ty, g->fn_gc_pop, argv, 1, "");
}

static CGValue gc_root_temp_value(Codegen *g, LLVMValueRef frame, CGValue v, const char *name)
{
    if (!g || !frame || !is_gc_root_type(v.type))
        return v;
    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
    if (!fn)
        return v;
    LLVMTypeRef ty = llvm_type(g, v.type);
    LLVMValueRef slot = build_entry_alloca(g, fn, ty, name ? name : "gc.temp.slot");
    LLVMBuildStore(g->builder, LLVMConstNull(ty), slot);
    gc_add_root_to_frame(g, frame, slot, v.type);
    LLVMBuildStore(g->builder, v.value, slot);
    v.value = LLVMBuildLoad2(g->builder, ty, slot, name ? name : "gc.temp");
    return v;
}

static LLVMValueRef gen_rooted_call_arg(Codegen *g, LLVMValueRef frame, Expr *arg_expr, Type param_type)
{
    CGValue av = gen_expr(g, arg_expr);
    av = gc_root_temp_value(g, frame, av, "gc.arg.raw");
    CGValue cv;
    cv.type = param_type;
    cv.value = cast_value(g, av, param_type);
    cv = gc_root_temp_value(g, frame, cv, "gc.arg.cast");
    return cv.value;
}

static LLVMValueRef build_array_value(Codegen *g, Type arr_type, Expr *init)
{
    int64_t init_count = 0;
    if (init && init->kind == EXPR_ARRAY)
        init_count = init->v.array.items.count;

    int64_t alloc_len = arr_type.array_len >= 0 ? arr_type.array_len : init_count;
    if (alloc_len < 0) alloc_len = 0;

    Type elem_type = type_make(arr_type.elem);
    elem_type.name = arr_type.name;
    LLVMTypeRef elem_ty = llvm_type(g, elem_type);
    LLVMValueRef data_ptr = LLVMConstNull(LLVMPointerType(elem_ty, 0));

    if (alloc_len > 0)
    {
        LLVMTypeRef arr_ty = LLVMArrayType(elem_ty, (unsigned)alloc_len);
        uint64_t arr_bytes = g->target_data ? LLVMABISizeOfType(g->target_data, arr_ty) : 0;
        int use_heap = (g->env_kind == KN_ENV_HOSTED && g->fn_gc_alloc && arr_bytes > (uint64_t)(64 * 1024));

        if (use_heap)
        {
            LLVMValueRef len_val = LLVMConstInt(g->i64, (uint64_t)alloc_len, 0);
            LLVMValueRef elem_size = LLVMSizeOf(elem_ty);
            if (LLVMGetTypeKind(LLVMTypeOf(elem_size)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(elem_size)) != 64)
                elem_size = LLVMBuildZExtOrBitCast(g->builder, elem_size, g->i64, "arr.elem_sz");
            LLVMValueRef bytes = LLVMBuildMul(g->builder, len_val, elem_size, "arr.bytes");
            LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc, (LLVMValueRef[]){ bytes }, 1, "arr.mem");
            data_ptr = LLVMBuildBitCast(g->builder, raw, LLVMPointerType(elem_ty, 0), "arr.data");
        }
        else
        {
            LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
            LLVMValueRef alloca = build_entry_alloca(g, fn, arr_ty, "arrtmp");
            LLVMValueRef zero = LLVMConstInt(g->i32, 0, 0);
            unsigned zero_align = g->target_data ? LLVMABIAlignmentOfType(g->target_data, arr_ty) : 1;

            LLVMBuildMemSet(g->builder, alloca, LLVMConstInt(g->i8, 0, 0), LLVMConstInt(g->i64, arr_bytes, 0), zero_align);

            LLVMValueRef idxs0[2] = { zero, zero };
            data_ptr = LLVMBuildInBoundsGEP2(g->builder, arr_ty, alloca, idxs0, 2, "arr.data");
        }

        // init provided elements
        if (init && init->kind == EXPR_ARRAY)
        {
            for (int64_t i = 0; i < init_count; i++)
            {
                CGValue v = gen_expr(g, init->v.array.items.items[(int)i]);
                LLVMValueRef cv = cast_value(g, v, elem_type);
                LLVMValueRef idxv = LLVMConstInt(g->i64, (uint64_t)i, 0);
                LLVMValueRef ptr = LLVMBuildInBoundsGEP2(g->builder, elem_ty, data_ptr, &idxv, 1, "elem");
                LLVMBuildStore(g->builder, cv, ptr);
            }
        }
    }

    LLVMTypeRef arr_struct = llvm_array_type(g, elem_type);
    LLVMValueRef out = LLVMGetUndef(arr_struct);
    out = LLVMBuildInsertValue(g->builder, out, data_ptr, 0, "");
    LLVMValueRef len_val = LLVMConstInt(g->i64, (uint64_t)alloc_len, 0);
    out = LLVMBuildInsertValue(g->builder, out, len_val, 1, "");
    return out;
}

static LLVMValueRef build_dict_value(Codegen *g, Expr *init)
{
    LLVMValueRef temp_frame = gc_push_temp_frame(g, "gc.dict.frame");
    LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_dict_new_ty, g->fn_dict_new, 0, 0, "dict.new");
    CGValue objv;
    objv.type = type_class("IO.Collection.dict");
    objv.value = LLVMBuildBitCast(g->builder, raw, llvm_type(g, objv.type), "dict.obj");
    objv = gc_root_temp_value(g, temp_frame, objv, "gc.dict.obj");
    LLVMValueRef dp = LLVMBuildBitCast(g->builder, objv.value, g->i8ptr, "dict.ptr");

    if (init && init->kind == EXPR_DICT)
    {
        for (int i = 0; i < init->v.dict.keys.count; i++)
        {
            LLVMValueRef key = gen_rooted_call_arg(g, temp_frame, init->v.dict.keys.items[i], type_make(TY_STRING));
            CGValue value = gen_expr(g, init->v.dict.values.items[i]);
            value = gc_root_temp_value(g, temp_frame, value, "gc.dict.value.raw");
            CGValue anyv;
            anyv.type = type_make(TY_ANY);
            anyv.value = cast_value(g, value, anyv.type);
            anyv = gc_root_temp_value(g, temp_frame, anyv, "gc.dict.value.any");
            LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv.value, 0, "dict.lit.tag");
            LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv.value, 1, "dict.lit.payload");
            LLVMBuildCall2(g->builder, g->fn_dict_set_ty, g->fn_dict_set,
                           (LLVMValueRef[]){ dp, key, tag, payload }, 4, "");
        }
    }

    gc_pop_temp_frame(g, temp_frame);
    return objv.value;
}

static LLVMValueRef build_heap_array_value(Codegen *g, Type arr_type)
{
    int64_t alloc_len = arr_type.array_len > 0 ? arr_type.array_len : 0;

    Type elem_type = type_make(arr_type.elem);
    elem_type.name = arr_type.name;
    LLVMTypeRef elem_ty = llvm_type(g, elem_type);
    LLVMValueRef data_ptr = LLVMConstNull(LLVMPointerType(elem_ty, 0));

    if (alloc_len > 0)
    {
        LLVMValueRef elem_size = LLVMSizeOf(elem_ty);
        if (LLVMGetTypeKind(LLVMTypeOf(elem_size)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(elem_size)) != 64)
            elem_size = LLVMBuildZExtOrBitCast(g->builder, elem_size, g->i64, "arr.elem_sz");
        LLVMValueRef len_val = LLVMConstInt(g->i64, (uint64_t)alloc_len, 0);
        LLVMValueRef bytes = LLVMBuildMul(g->builder, len_val, elem_size, "arr.bytes");
        LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc, (LLVMValueRef[]){ bytes }, 1, "arr.mem");
        data_ptr = LLVMBuildBitCast(g->builder, raw, LLVMPointerType(elem_ty, 0), "arr.data");
    }

    LLVMTypeRef arr_struct = llvm_array_type(g, elem_type);
    LLVMValueRef out = LLVMGetUndef(arr_struct);
    out = LLVMBuildInsertValue(g->builder, out, data_ptr, 0, "");
    LLVMValueRef out_len = LLVMConstInt(g->i64, (uint64_t)alloc_len, 0);
    out = LLVMBuildInsertValue(g->builder, out, out_len, 1, "");
    return out;
}

static LLVMValueRef build_array_append_value(Codegen *g, CGValue arr, CGValue item)
{
    Type elem = type_make(arr.type.elem);
    elem.name = arr.type.name;
    LLVMTypeRef elem_ty = llvm_type(g, elem);
    LLVMTypeRef arr_struct = llvm_array_type(g, elem);

    LLVMValueRef old_data = LLVMBuildExtractValue(g->builder, arr.value, 0, "arr.add.old.data");
    LLVMValueRef old_len = LLVMBuildExtractValue(g->builder, arr.value, 1, "arr.add.old.len");
    LLVMValueRef one = LLVMConstInt(g->i64, 1, 0);
    LLVMValueRef new_len = LLVMBuildAdd(g->builder, old_len, one, "arr.add.new.len");

    LLVMValueRef elem_size = LLVMSizeOf(elem_ty);
    if (LLVMGetTypeKind(LLVMTypeOf(elem_size)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(elem_size)) != 64)
        elem_size = LLVMBuildZExtOrBitCast(g->builder, elem_size, g->i64, "arr.add.elem.sz");
    LLVMValueRef bytes = LLVMBuildMul(g->builder, new_len, elem_size, "arr.add.bytes");
    LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc, (LLVMValueRef[]){ bytes }, 1, "arr.add.mem");
    LLVMValueRef new_data = LLVMBuildBitCast(g->builder, raw, LLVMPointerType(elem_ty, 0), "arr.add.data");

    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
    LLVMValueRef idx = build_entry_alloca(g, fn, g->i64, "arr.add.i");
    LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), idx);

    LLVMBasicBlockRef copy_cond = LLVMAppendBasicBlockInContext(g->ctx, fn, "arr.add.copy.cond");
    LLVMBasicBlockRef copy_body = LLVMAppendBasicBlockInContext(g->ctx, fn, "arr.add.copy.body");
    LLVMBasicBlockRef copy_done = LLVMAppendBasicBlockInContext(g->ctx, fn, "arr.add.copy.done");
    LLVMBuildBr(g->builder, copy_cond);

    LLVMPositionBuilderAtEnd(g->builder, copy_cond);
    LLVMValueRef iv = LLVMBuildLoad2(g->builder, g->i64, idx, "arr.add.iv");
    LLVMValueRef has_more = LLVMBuildICmp(g->builder, LLVMIntULT, iv, old_len, "arr.add.more");
    LLVMBuildCondBr(g->builder, has_more, copy_body, copy_done);

    LLVMPositionBuilderAtEnd(g->builder, copy_body);
    LLVMValueRef src_ptr = LLVMBuildInBoundsGEP2(g->builder, elem_ty, old_data, &iv, 1, "arr.add.src");
    LLVMValueRef dst_ptr = LLVMBuildInBoundsGEP2(g->builder, elem_ty, new_data, &iv, 1, "arr.add.dst");
    LLVMBuildStore(g->builder, LLVMBuildLoad2(g->builder, elem_ty, src_ptr, "arr.add.val"), dst_ptr);
    LLVMBuildStore(g->builder, LLVMBuildAdd(g->builder, iv, one, "arr.add.next"), idx);
    LLVMBuildBr(g->builder, copy_cond);

    LLVMPositionBuilderAtEnd(g->builder, copy_done);
    LLVMValueRef tail_ptr = LLVMBuildInBoundsGEP2(g->builder, elem_ty, new_data, &old_len, 1, "arr.add.tail");
    LLVMBuildStore(g->builder, cast_value(g, item, elem), tail_ptr);

    LLVMValueRef out = LLVMGetUndef(arr_struct);
    out = LLVMBuildInsertValue(g->builder, out, new_data, 0, "arr.add.out.data");
    out = LLVMBuildInsertValue(g->builder, out, new_len, 1, "arr.add.out.len");
    return out;
}

static void init_class_array_fields(Codegen *g, ClassSymbol *cs, LLVMValueRef obj)
{
    if (!g || !cs || !cs->decl) return;

    if (cs->decl->base)
    {
        ClassSymbol *base = classtable_find(&g->classes, cs->decl->base);
        if (base)
            init_class_array_fields(g, base, obj);
    }

    LLVMValueRef self_obj = LLVMBuildBitCast(g->builder, obj, cs->ptr_ty, "arr.obj");
    for (int i = 0; i < cs->decl->fields.count; i++)
    {
        Field *f = &cs->decl->fields.items[i];
        if (f->is_static) continue;
        if (f->type.kind != TY_ARRAY) continue;
        int layout_index = class_instance_field_layout_index(cs->decl, i);
        if (layout_index < 0) continue;

        LLVMValueRef field_ptr = LLVMBuildStructGEP2(
            g->builder,
            cs->struct_ty,
            self_obj,
            (unsigned)(1 + layout_index),
            "arr.field"
        );
        LLVMValueRef arr_val = build_heap_array_value(g, f->type);
        LLVMBuildStore(g->builder, arr_val, field_ptr);
    }
}

static LLVMValueRef dict_as_i8ptr(Codegen *g, CGValue v)
{
    LLVMValueRef dv = cast_value(g, v, type_class("IO.Collection.dict"));
    return LLVMBuildBitCast(g->builder, dv, g->i8ptr, "dict.ptr");
}

static LLVMValueRef list_as_i8ptr(Codegen *g, CGValue v)
{
    LLVMValueRef lv = cast_value(g, v, type_class("IO.Collection.list"));
    return LLVMBuildBitCast(g->builder, lv, g->i8ptr, "list.ptr");
}

static LLVMValueRef set_as_i8ptr(Codegen *g, CGValue v)
{
    LLVMValueRef sv = cast_value(g, v, type_class("IO.Collection.set"));
    return LLVMBuildBitCast(g->builder, sv, g->i8ptr, "set.ptr");
}

static CGValue gen_builtin_call(Codegen *g, KnBuiltinKind kind, ExprList *args, Expr *recv)
{
    CGValue out;
    out.value = 0;
    out.type = type_make(TY_VOID);

    switch (kind)
    {
    case KN_BUILTIN_IO_CONSOLE_PRINTLINE:
    {
        if (args->count == 0)
        {
            LLVMValueRef empty = build_string_const(g, "", 0);
            LLVMBuildCall2(g->builder, g->fn_println_ty, g->fn_println, &empty, 1, "");
            out.type = type_make(TY_VOID);
            return out;
        }
        for (int i = 0; i < args->count; i++)
        {
            if (i > 0)
            {
                LLVMValueRef sp = build_string_const(g, " ", 1);
                LLVMBuildCall2(g->builder, g->fn_print_ty, g->fn_print, &sp, 1, "");
            }
            CGValue arg = gen_expr(g, args->items[i]);
            LLVMValueRef anyv = cast_value(g, arg, type_make(TY_ANY));
            LLVMValueRef s = LLVMBuildCall2(g->builder, g->fn_any_to_string_ty, g->fn_any_to_string, &anyv, 1, "any.str");
            if (i + 1 == args->count)
                LLVMBuildCall2(g->builder, g->fn_println_ty, g->fn_println, &s, 1, "");
            else
                LLVMBuildCall2(g->builder, g->fn_print_ty, g->fn_print, &s, 1, "");
        }
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_CONSOLE_PRINT:
    {
        for (int i = 0; i < args->count; i++)
        {
            if (i > 0)
            {
                LLVMValueRef sp = build_string_const(g, " ", 1);
                LLVMBuildCall2(g->builder, g->fn_print_ty, g->fn_print, &sp, 1, "");
            }
            CGValue arg = gen_expr(g, args->items[i]);
            LLVMValueRef anyv = cast_value(g, arg, type_make(TY_ANY));
            LLVMValueRef s = LLVMBuildCall2(g->builder, g->fn_any_to_string_ty, g->fn_any_to_string, &anyv, 1, "any.str");
            LLVMBuildCall2(g->builder, g->fn_print_ty, g->fn_print, &s, 1, "");
        }
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_CONSOLE_WRITE:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef anyv = cast_value(g, arg, type_make(TY_ANY));
        LLVMValueRef s = LLVMBuildCall2(g->builder, g->fn_any_to_string_ty, g->fn_any_to_string, &anyv, 1, "any.str");
        LLVMValueRef argv[1] = { s };
        LLVMBuildCall2(g->builder, g->fn_print_ty, g->fn_print, argv, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_CONSOLE_WRITELINE:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef anyv = cast_value(g, arg, type_make(TY_ANY));
        LLVMValueRef s = LLVMBuildCall2(g->builder, g->fn_any_to_string_ty, g->fn_any_to_string, &anyv, 1, "any.str");
        LLVMValueRef argv[1] = { s };
        LLVMBuildCall2(g->builder, g->fn_println_ty, g->fn_println, argv, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_CONSOLE_PRINTINT:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef v = cast_value(g, arg, type_make(TY_INT));
        LLVMValueRef argv[1] = { v };
        LLVMBuildCall2(g->builder, g->fn_print_i64_ty, g->fn_print_i64, argv, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_CONSOLE_PRINTBOOL:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef argv[1] = { arg.value };
        LLVMBuildCall2(g->builder, g->fn_print_bool_ty, g->fn_print_bool, argv, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_CONSOLE_PRINTCHAR:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef v = cast_value(g, arg, type_make(TY_CHAR));
        LLVMValueRef argv[1] = { v };
        LLVMBuildCall2(g->builder, g->fn_print_char_ty, g->fn_print_char, argv, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_CONSOLE_PRINTLINEINT:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef v = cast_value(g, arg, type_make(TY_INT));
        LLVMBuildCall2(g->builder, g->fn_print_i64_ty, g->fn_print_i64, (LLVMValueRef[]){ v }, 1, "");
        LLVMValueRef nl = LLVMConstInt(g->i8, '\n', 0);
        LLVMBuildCall2(g->builder, g->fn_print_char_ty, g->fn_print_char, &nl, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_CONSOLE_PRINTLINEBOOL:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMBuildCall2(g->builder, g->fn_print_bool_ty, g->fn_print_bool, (LLVMValueRef[]){ arg.value }, 1, "");
        LLVMValueRef nl = LLVMConstInt(g->i8, '\n', 0);
        LLVMBuildCall2(g->builder, g->fn_print_char_ty, g->fn_print_char, &nl, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_CONSOLE_PRINTLINECHAR:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef v = cast_value(g, arg, type_make(TY_CHAR));
        LLVMBuildCall2(g->builder, g->fn_print_char_ty, g->fn_print_char, (LLVMValueRef[]){ v }, 1, "");
        LLVMValueRef nl = LLVMConstInt(g->i8, '\n', 0);
        LLVMBuildCall2(g->builder, g->fn_print_char_ty, g->fn_print_char, &nl, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_CONSOLE_WRITEINT:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef v = cast_value(g, arg, type_make(TY_INT));
        LLVMBuildCall2(g->builder, g->fn_print_i64_ty, g->fn_print_i64, (LLVMValueRef[]){ v }, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_CONSOLE_WRITEBOOL:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMBuildCall2(g->builder, g->fn_print_bool_ty, g->fn_print_bool, (LLVMValueRef[]){ arg.value }, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_CONSOLE_WRITECHAR:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef v = cast_value(g, arg, type_make(TY_CHAR));
        LLVMBuildCall2(g->builder, g->fn_print_char_ty, g->fn_print_char, (LLVMValueRef[]){ v }, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_CONSOLE_READLINE:
    {
        out.value = LLVMBuildCall2(g->builder, g->fn_readline_ty, g->fn_readline, 0, 0, "readline");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_TIME_NOW:
    {
        out.value = build_datetime_now(g);
        out.type = type_class("IO.Time.DateTime");
        return out;
    }
    case KN_BUILTIN_IO_TIME_GETTICK:
    {
        out.value = LLVMBuildCall2(g->builder, g->fn_time_tick_ty, g->fn_time_tick, 0, 0, "tick");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_TIME_SLEEP:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef ms = LLVMBuildTruncOrBitCast(g->builder, arg.value, g->i32, "ms");
        LLVMValueRef argv[1] = { ms };
        LLVMBuildCall2(g->builder, g->fn_time_sleep_ty, g->fn_time_sleep, argv, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_TIME_FORMAT:
    {
        CGValue dt = gen_expr(g, args->items[0]);
        CGValue pat = gen_expr(g, args->items[1]);
        LLVMValueRef tickv = LLVMConstInt(g->i64, 0, 0);

        if (g->datetime_class && g->datetime_ticks_index >= 0)
        {
            LLVMValueRef dt_obj = cast_value(g, dt, type_class("IO.Time.DateTime"));
            unsigned fidx = (unsigned)(1 + g->datetime_ticks_index);
            LLVMValueRef tick_ptr = LLVMBuildStructGEP2(g->builder, g->datetime_class->struct_ty, dt_obj, fidx, "dt.tick.ptr");
            tickv = LLVMBuildLoad2(g->builder, g->i64, tick_ptr, "dt.tick");
        }

        LLVMValueRef fmt = cast_value(g, pat, type_make(TY_STRING));
        LLVMValueRef argv[2] = { tickv, fmt };
        out.value = LLVMBuildCall2(g->builder, g->fn_time_format_ty, g->fn_time_format, argv, 2, "timefmt");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_TIME_NANOTICK:
    {
        out.value = LLVMBuildCall2(g->builder, g->fn_time_nano_ty, g->fn_time_nano, 0, 0, "nanotick");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_MEMORY_COPY:
    {
        CGValue dst = gen_expr(g, args->items[0]);
        CGValue src = gen_expr(g, args->items[1]);
        CGValue len = gen_expr(g, args->items[2]);
        LLVMValueRef dstp = cast_value(g, dst, type_ptr(TY_BYTE));
        LLVMValueRef srcp = cast_value(g, src, type_ptr(TY_BYTE));
        LLVMValueRef n = cast_value(g, len, type_make(TY_USIZE));
        LLVMBuildMemCpy(g->builder, dstp, 1, srcp, 1, n);
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_MEMORY_SET:
    {
        CGValue dst = gen_expr(g, args->items[0]);
        CGValue bytev = gen_expr(g, args->items[1]);
        CGValue len = gen_expr(g, args->items[2]);
        LLVMValueRef dstp = cast_value(g, dst, type_ptr(TY_BYTE));
        LLVMValueRef val = cast_value(g, bytev, type_make(TY_BYTE));
        LLVMValueRef n = cast_value(g, len, type_make(TY_USIZE));
        LLVMBuildMemSet(g->builder, dstp, val, n, 1);
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_MEMORY_ZERO:
    {
        CGValue dst = gen_expr(g, args->items[0]);
        CGValue len = gen_expr(g, args->items[1]);
        LLVMValueRef dstp = cast_value(g, dst, type_ptr(TY_BYTE));
        LLVMValueRef n = cast_value(g, len, type_make(TY_USIZE));
        LLVMBuildMemSet(g->builder, dstp, LLVMConstInt(g->i8, 0, 0), n, 1);
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_MEMORY_COMPARE:
    {
        CGValue a = gen_expr(g, args->items[0]);
        CGValue b = gen_expr(g, args->items[1]);
        CGValue len = gen_expr(g, args->items[2]);
        LLVMValueRef ap = cast_value(g, a, type_ptr(TY_BYTE));
        LLVMValueRef bp = cast_value(g, b, type_ptr(TY_BYTE));
        LLVMValueRef n = cast_value(g, len, type_make(TY_USIZE));
        LLVMValueRef rv = LLVMBuildCall2(g->builder, g->fn_mem_compare_ty, g->fn_mem_compare, (LLVMValueRef[]){ ap, bp, n }, 3, "memcmp");
        out.value = LLVMBuildSExt(g->builder, rv, g->i64, "memcmp.int");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_VOLATILE_READ8:
    case KN_BUILTIN_IO_VOLATILE_READ16:
    case KN_BUILTIN_IO_VOLATILE_READ32:
    case KN_BUILTIN_IO_VOLATILE_READ64:
    {
        CGValue p = gen_expr(g, args->items[0]);
        Type ptr_ty = type_ptr(TY_U8);
        Type out_ty = type_make(TY_U8);
        switch (kind)
        {
        case KN_BUILTIN_IO_VOLATILE_READ8: ptr_ty = type_ptr(TY_U8); out_ty = type_make(TY_U8); break;
        case KN_BUILTIN_IO_VOLATILE_READ16: ptr_ty = type_ptr(TY_U16); out_ty = type_make(TY_U16); break;
        case KN_BUILTIN_IO_VOLATILE_READ32: ptr_ty = type_ptr(TY_U32); out_ty = type_make(TY_U32); break;
        case KN_BUILTIN_IO_VOLATILE_READ64: ptr_ty = type_ptr(TY_U64); out_ty = type_make(TY_U64); break;
        default: break;
        }
        LLVMValueRef pv = cast_value(g, p, ptr_ty);
        LLVMValueRef lv = LLVMBuildLoad2(g->builder, llvm_type(g, out_ty), pv, "volatile.read");
        LLVMSetVolatile(lv, 1);
        out.value = lv;
        out.type = out_ty;
        return out;
    }
    case KN_BUILTIN_IO_VOLATILE_WRITE8:
    case KN_BUILTIN_IO_VOLATILE_WRITE16:
    case KN_BUILTIN_IO_VOLATILE_WRITE32:
    case KN_BUILTIN_IO_VOLATILE_WRITE64:
    {
        CGValue p = gen_expr(g, args->items[0]);
        CGValue v = gen_expr(g, args->items[1]);
        Type ptr_ty = type_ptr(TY_U8);
        Type val_ty = type_make(TY_U8);
        switch (kind)
        {
        case KN_BUILTIN_IO_VOLATILE_WRITE8: ptr_ty = type_ptr(TY_U8); val_ty = type_make(TY_U8); break;
        case KN_BUILTIN_IO_VOLATILE_WRITE16: ptr_ty = type_ptr(TY_U16); val_ty = type_make(TY_U16); break;
        case KN_BUILTIN_IO_VOLATILE_WRITE32: ptr_ty = type_ptr(TY_U32); val_ty = type_make(TY_U32); break;
        case KN_BUILTIN_IO_VOLATILE_WRITE64: ptr_ty = type_ptr(TY_U64); val_ty = type_make(TY_U64); break;
        default: break;
        }
        LLVMValueRef pv = cast_value(g, p, ptr_ty);
        LLVMValueRef vv = cast_value(g, v, val_ty);
        LLVMValueRef st = LLVMBuildStore(g->builder, vv, pv);
        LLVMSetVolatile(st, 1);
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_PANIC_ABORT:
    {
        LLVMValueRef msg = LLVMConstNull(g->i8ptr);
        LLVMBuildCall2(g->builder, g->fn_panic_ty, g->fn_panic, &msg, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_PANIC_HALT:
    {
        LLVMValueRef msg = LLVMConstNull(g->i8ptr);
        LLVMBuildCall2(g->builder, g->fn_panic_ty, g->fn_panic, &msg, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_STRING_LENGTH:
    {
        CGValue arg;
        if (recv)
        {
            // Member call: string.Length()
            arg = gen_expr(g, recv);
        }
        else
        {
            // Module call: IO.Type.string.Length(string)
            arg = gen_expr(g, args->items[0]);
        }
        LLVMValueRef argv[1] = { arg.value };
        out.value = LLVMBuildCall2(g->builder, g->fn_strlen_ty, g->fn_strlen, argv, 1, "strlen");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_STRING_INDEX:
    {
        // string.Index(idx)
        CGValue str_val = gen_expr(g, recv);
        CGValue idx_val = gen_expr(g, args->items[0]);
        LLVMValueRef idxv = cast_value(g, idx_val, type_make(TY_I64));
        LLVMTypeRef char_ty = llvm_type(g, type_make(TY_CHAR));
        LLVMValueRef ptr = LLVMBuildInBoundsGEP2(g->builder, char_ty, str_val.value, &idxv, 1, "str.idx");
        out.value = LLVMBuildLoad2(g->builder, char_ty, ptr, "str.char");
        out.type = type_make(TY_CHAR);
        return out;
    }
    case KN_BUILTIN_IO_STRING_CONCAT:
    {
        CGValue a = gen_expr(g, args->items[0]);
        CGValue b = gen_expr(g, args->items[1]);
        LLVMValueRef argv[2] = { a.value, b.value };
        out.value = LLVMBuildCall2(g->builder, g->fn_str_concat_ty, g->fn_str_concat, argv, 2, "strcat");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_STRING_EQUALS:
    {
        CGValue a = gen_expr(g, args->items[0]);
        CGValue b = gen_expr(g, args->items[1]);
        LLVMValueRef argv[2] = { a.value, b.value };
        out.value = LLVMBuildCall2(g->builder, g->fn_str_eq_ty, g->fn_str_eq, argv, 2, "streq");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_STRING_TOCHARS:
    {
        CGValue sv = (args->count == 0 && recv) ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef s = cast_value(g, sv, type_make(TY_STRING));
        LLVMValueRef len_ptr = LLVMBuildAlloca(g->builder, g->i64, "chars.len.ptr");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), len_ptr);
        LLVMValueRef argv[2] = { s, len_ptr };
        LLVMValueRef data = LLVMBuildCall2(g->builder, g->fn_string_to_chars_ty, g->fn_string_to_chars, argv, 2, "str.tochars");
        LLVMValueRef len = LLVMBuildLoad2(g->builder, g->i64, len_ptr, "chars.len");

        Type elem = type_make(TY_CHAR);
        LLVMTypeRef arr_ty = llvm_array_type(g, elem);
        LLVMValueRef arrv = LLVMGetUndef(arr_ty);
        arrv = LLVMBuildInsertValue(g->builder, arrv, data, 0, "chars.arr.data");
        arrv = LLVMBuildInsertValue(g->builder, arrv, len, 1, "chars.arr.len");
        out.value = arrv;
        out.type = type_array(TY_CHAR, -1);
        return out;
    }
    case KN_BUILTIN_IO_DICT_NEW:
    {
        LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_dict_new_ty, g->fn_dict_new, 0, 0, "dict.new");
        LLVMValueRef obj = LLVMBuildBitCast(g->builder, raw, llvm_type(g, type_class("IO.Collection.dict")), "dict.obj");
        out.value = obj;
        out.type = type_class("IO.Collection.dict");
        return out;
    }
    case KN_BUILTIN_IO_DICT_SET:
    {
        int inst = (recv && args->count == 2) ? 1 : 0;
        CGValue dv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int key_idx = inst ? 0 : 1;
        int val_idx = inst ? 1 : 2;
        CGValue keyv = gen_expr(g, args->items[key_idx]);
        CGValue valv = gen_expr(g, args->items[val_idx]);

        LLVMValueRef dp = dict_as_i8ptr(g, dv);
        LLVMValueRef key = cast_value(g, keyv, type_make(TY_STRING));
        LLVMValueRef anyv = cast_value(g, valv, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "dict.tag");
        LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv, 1, "dict.payload");
        LLVMValueRef argv[4] = { dp, key, tag, payload };
        LLVMBuildCall2(g->builder, g->fn_dict_set_ty, g->fn_dict_set, argv, 4, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_DICT_GET:
    {
        int inst = (recv && args->count == 1) ? 1 : 0;
        CGValue dv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int key_idx = inst ? 0 : 1;
        CGValue keyv = gen_expr(g, args->items[key_idx]);

        LLVMValueRef dp = dict_as_i8ptr(g, dv);
        LLVMValueRef key = cast_value(g, keyv, type_make(TY_STRING));
        LLVMValueRef tag_ptr = LLVMBuildAlloca(g->builder, g->i64, "dict.get.tag.ptr");
        LLVMValueRef payload_ptr = LLVMBuildAlloca(g->builder, g->i64, "dict.get.payload.ptr");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), tag_ptr);
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), payload_ptr);
        LLVMValueRef argv[4] = { dp, key, tag_ptr, payload_ptr };
        LLVMBuildCall2(g->builder, g->fn_dict_get_ty, g->fn_dict_get, argv, 4, "");
        LLVMValueRef tag = LLVMBuildLoad2(g->builder, g->i64, tag_ptr, "dict.get.tag");
        LLVMValueRef payload = LLVMBuildLoad2(g->builder, g->i64, payload_ptr, "dict.get.payload");
        LLVMValueRef anyres = LLVMGetUndef(g->any_ty);
        anyres = LLVMBuildInsertValue(g->builder, anyres, tag, 0, "dict.get.any.tag");
        anyres = LLVMBuildInsertValue(g->builder, anyres, payload, 1, "dict.get.any.payload");
        LLVMValueRef anytmp = LLVMBuildAlloca(g->builder, g->any_ty, "dict.get.any.tmp");
        LLVMBuildStore(g->builder, anyres, anytmp);
        out.value = LLVMBuildLoad2(g->builder, g->any_ty, anytmp, "dict.get.any");
        out.type = type_make(TY_ANY);
        return out;
    }
    case KN_BUILTIN_IO_DICT_TRY_FETCH:
    {
        int inst = (recv && args->count == 2) ? 1 : 0;
        CGValue dv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int key_idx = inst ? 0 : 1;
        int fb_idx = inst ? 1 : 2;
        CGValue keyv = gen_expr(g, args->items[key_idx]);
        CGValue fbv = gen_expr(g, args->items[fb_idx]);

        LLVMValueRef dp = dict_as_i8ptr(g, dv);
        LLVMValueRef key = cast_value(g, keyv, type_make(TY_STRING));
        LLVMValueRef has = LLVMBuildCall2(g->builder, g->fn_dict_contains_ty, g->fn_dict_contains, (LLVMValueRef[]){ dp, key }, 2, "dict.try.has");

        LLVMValueRef tag_ptr = LLVMBuildAlloca(g->builder, g->i64, "dict.try.tag.ptr");
        LLVMValueRef payload_ptr = LLVMBuildAlloca(g->builder, g->i64, "dict.try.payload.ptr");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), tag_ptr);
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), payload_ptr);
        LLVMBuildCall2(g->builder, g->fn_dict_get_ty, g->fn_dict_get, (LLVMValueRef[]){ dp, key, tag_ptr, payload_ptr }, 4, "");
        LLVMValueRef tag = LLVMBuildLoad2(g->builder, g->i64, tag_ptr, "dict.try.tag");
        LLVMValueRef payload = LLVMBuildLoad2(g->builder, g->i64, payload_ptr, "dict.try.payload");

        LLVMValueRef fba = cast_value(g, fbv, type_make(TY_ANY));
        LLVMValueRef fb_tag = LLVMBuildExtractValue(g->builder, fba, 0, "dict.try.fb.tag");
        LLVMValueRef fb_payload = LLVMBuildExtractValue(g->builder, fba, 1, "dict.try.fb.payload");

        LLVMValueRef sel_tag = LLVMBuildSelect(g->builder, has, tag, fb_tag, "dict.try.sel.tag");
        LLVMValueRef sel_payload = LLVMBuildSelect(g->builder, has, payload, fb_payload, "dict.try.sel.payload");
        LLVMValueRef anyres = LLVMGetUndef(g->any_ty);
        anyres = LLVMBuildInsertValue(g->builder, anyres, sel_tag, 0, "dict.try.any.tag");
        anyres = LLVMBuildInsertValue(g->builder, anyres, sel_payload, 1, "dict.try.any.payload");
        out.value = anyres;
        out.type = type_make(TY_ANY);
        return out;
    }
    case KN_BUILTIN_IO_DICT_CONTAINS:
    {
        int inst = (recv && args->count == 1) ? 1 : 0;
        CGValue dv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int key_idx = inst ? 0 : 1;
        CGValue keyv = gen_expr(g, args->items[key_idx]);

        LLVMValueRef dp = dict_as_i8ptr(g, dv);
        LLVMValueRef key = cast_value(g, keyv, type_make(TY_STRING));
        LLVMValueRef argv[2] = { dp, key };
        out.value = LLVMBuildCall2(g->builder, g->fn_dict_contains_ty, g->fn_dict_contains, argv, 2, "dict.contains");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_DICT_COUNT:
    {
        int inst = (recv && args->count == 0) ? 1 : 0;
        CGValue dv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef dp = dict_as_i8ptr(g, dv);
        out.value = LLVMBuildCall2(g->builder, g->fn_dict_count_ty, g->fn_dict_count, &dp, 1, "dict.count");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_DICT_REMOVE:
    {
        int inst = (recv && args->count == 1) ? 1 : 0;
        CGValue dv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int key_idx = inst ? 0 : 1;
        CGValue keyv = gen_expr(g, args->items[key_idx]);
        LLVMValueRef dp = dict_as_i8ptr(g, dv);
        LLVMValueRef key = cast_value(g, keyv, type_make(TY_STRING));
        LLVMValueRef argv[2] = { dp, key };
        out.value = LLVMBuildCall2(g->builder, g->fn_dict_remove_ty, g->fn_dict_remove, argv, 2, "dict.remove");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_DICT_IS_EMPTY:
    {
        int inst = (recv && args->count == 0) ? 1 : 0;
        CGValue dv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef dp = dict_as_i8ptr(g, dv);
        out.value = LLVMBuildCall2(g->builder, g->fn_dict_is_empty_ty, g->fn_dict_is_empty, &dp, 1, "dict.empty");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_DICT_KEYS:
    {
        int inst = (recv && args->count == 0) ? 1 : 0;
        CGValue dv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef dp = dict_as_i8ptr(g, dv);
        LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_dict_keys_ty, g->fn_dict_keys, &dp, 1, "dict.keys");
        out.type = type_class("IO.Collection.list");
        out.value = LLVMBuildBitCast(g->builder, raw, llvm_type(g, out.type), "dict.keys.obj");
        return out;
    }
    case KN_BUILTIN_IO_DICT_VALUES:
    {
        int inst = (recv && args->count == 0) ? 1 : 0;
        CGValue dv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef dp = dict_as_i8ptr(g, dv);
        LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_dict_values_ty, g->fn_dict_values, &dp, 1, "dict.values");
        out.type = type_class("IO.Collection.list");
        out.value = LLVMBuildBitCast(g->builder, raw, llvm_type(g, out.type), "dict.values.obj");
        return out;
    }
    case KN_BUILTIN_IO_LIST_NEW:
    {
        LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_list_new_ty, g->fn_list_new, 0, 0, "list.new");
        LLVMValueRef obj = LLVMBuildBitCast(g->builder, raw, llvm_type(g, type_class("IO.Collection.list")), "list.obj");
        out.value = obj;
        out.type = type_class("IO.Collection.list");
        return out;
    }
    case KN_BUILTIN_IO_LIST_ADD:
    {
        int inst = (recv && args->count == 1) ? 1 : 0;
        CGValue lv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int val_idx = inst ? 0 : 1;
        CGValue vv = gen_expr(g, args->items[val_idx]);
        LLVMValueRef lp = list_as_i8ptr(g, lv);
        LLVMValueRef anyv = cast_value(g, vv, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "list.add.tag");
        LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv, 1, "list.add.payload");
        LLVMValueRef argv[3] = { lp, tag, payload };
        LLVMBuildCall2(g->builder, g->fn_list_add_ty, g->fn_list_add, argv, 3, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_LIST_GET:
    {
        int inst = (recv && args->count == 1) ? 1 : 0;
        CGValue lv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int idx_i = inst ? 0 : 1;
        CGValue iv = gen_expr(g, args->items[idx_i]);
        LLVMValueRef lp = list_as_i8ptr(g, lv);
        LLVMValueRef idxv = cast_value(g, iv, type_make(TY_I64));
        LLVMValueRef tag_ptr = LLVMBuildAlloca(g->builder, g->i64, "list.get.tag.ptr");
        LLVMValueRef payload_ptr = LLVMBuildAlloca(g->builder, g->i64, "list.get.payload.ptr");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), tag_ptr);
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), payload_ptr);
        LLVMValueRef argv[4] = { lp, idxv, tag_ptr, payload_ptr };
        LLVMBuildCall2(g->builder, g->fn_list_get_ty, g->fn_list_get, argv, 4, "");
        LLVMValueRef tag = LLVMBuildLoad2(g->builder, g->i64, tag_ptr, "list.get.tag");
        LLVMValueRef payload = LLVMBuildLoad2(g->builder, g->i64, payload_ptr, "list.get.payload");
        LLVMValueRef anyres = LLVMGetUndef(g->any_ty);
        anyres = LLVMBuildInsertValue(g->builder, anyres, tag, 0, "list.get.any.tag");
        anyres = LLVMBuildInsertValue(g->builder, anyres, payload, 1, "list.get.any.payload");
        out.value = anyres;
        out.type = type_make(TY_ANY);
        return out;
    }
    case KN_BUILTIN_IO_LIST_SET:
    {
        int inst = (recv && args->count == 2) ? 1 : 0;
        CGValue lv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int idx_i = inst ? 0 : 1;
        int val_i = inst ? 1 : 2;
        CGValue iv = gen_expr(g, args->items[idx_i]);
        CGValue vv = gen_expr(g, args->items[val_i]);
        LLVMValueRef lp = list_as_i8ptr(g, lv);
        LLVMValueRef idxv = cast_value(g, iv, type_make(TY_I64));
        LLVMValueRef anyv = cast_value(g, vv, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "list.set.tag");
        LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv, 1, "list.set.payload");
        LLVMValueRef argv[4] = { lp, idxv, tag, payload };
        LLVMBuildCall2(g->builder, g->fn_list_set_ty, g->fn_list_set, argv, 4, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_LIST_INSERT:
    {
        int inst = (recv && args->count == 2) ? 1 : 0;
        CGValue lv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int idx_i = inst ? 0 : 1;
        int val_i = inst ? 1 : 2;
        CGValue iv = gen_expr(g, args->items[idx_i]);
        CGValue vv = gen_expr(g, args->items[val_i]);
        LLVMValueRef lp = list_as_i8ptr(g, lv);
        LLVMValueRef idxv = cast_value(g, iv, type_make(TY_I64));
        LLVMValueRef anyv = cast_value(g, vv, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "list.ins.tag");
        LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv, 1, "list.ins.payload");
        LLVMValueRef argv[4] = { lp, idxv, tag, payload };
        LLVMBuildCall2(g->builder, g->fn_list_insert_ty, g->fn_list_insert, argv, 4, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_LIST_TRY_FETCH:
    {
        int inst = (recv && args->count == 2) ? 1 : 0;
        CGValue lv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int idx_i = inst ? 0 : 1;
        int def_i = inst ? 1 : 2;
        CGValue iv = gen_expr(g, args->items[idx_i]);
        CGValue dv = gen_expr(g, args->items[def_i]);
        LLVMValueRef lp = list_as_i8ptr(g, lv);
        LLVMValueRef idxv = cast_value(g, iv, type_make(TY_I64));
        LLVMValueRef def_any = cast_value(g, dv, type_make(TY_ANY));
        LLVMValueRef cnt = LLVMBuildCall2(g->builder, g->fn_list_count_ty, g->fn_list_count, &lp, 1, "list.try.cnt");
        LLVMValueRef in_range = LLVMBuildICmp(g->builder, LLVMIntULT, idxv, cnt, "list.try.range");
        LLVMValueRef tag_ptr = LLVMBuildAlloca(g->builder, g->i64, "list.try.tag.ptr");
        LLVMValueRef payload_ptr = LLVMBuildAlloca(g->builder, g->i64, "list.try.payload.ptr");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), tag_ptr);
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), payload_ptr);
        LLVMBuildCall2(g->builder, g->fn_list_get_ty, g->fn_list_get, (LLVMValueRef[]){ lp, idxv, tag_ptr, payload_ptr }, 4, "");
        LLVMValueRef got_tag = LLVMBuildLoad2(g->builder, g->i64, tag_ptr, "list.try.tag");
        LLVMValueRef got_payload = LLVMBuildLoad2(g->builder, g->i64, payload_ptr, "list.try.payload");
        LLVMValueRef def_tag = LLVMBuildExtractValue(g->builder, def_any, 0, "list.try.def.tag");
        LLVMValueRef def_payload = LLVMBuildExtractValue(g->builder, def_any, 1, "list.try.def.payload");
        LLVMValueRef res_tag = LLVMBuildSelect(g->builder, in_range, got_tag, def_tag, "list.try.res.tag");
        LLVMValueRef res_payload = LLVMBuildSelect(g->builder, in_range, got_payload, def_payload, "list.try.res.payload");
        LLVMValueRef anyres = LLVMGetUndef(g->any_ty);
        anyres = LLVMBuildInsertValue(g->builder, anyres, res_tag, 0, "list.try.any.tag");
        anyres = LLVMBuildInsertValue(g->builder, anyres, res_payload, 1, "list.try.any.payload");
        out.value = anyres;
        out.type = type_make(TY_ANY);
        return out;
    }
    case KN_BUILTIN_IO_LIST_COUNT:
    {
        int inst = (recv && args->count == 0) ? 1 : 0;
        CGValue lv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef lp = list_as_i8ptr(g, lv);
        out.value = LLVMBuildCall2(g->builder, g->fn_list_count_ty, g->fn_list_count, &lp, 1, "list.count");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_LIST_CLEAR:
    {
        int inst = (recv && args->count == 0) ? 1 : 0;
        CGValue lv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef lp = list_as_i8ptr(g, lv);
        LLVMBuildCall2(g->builder, g->fn_list_clear_ty, g->fn_list_clear, &lp, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_LIST_REMOVE_AT:
    {
        int inst = (recv && args->count == 1) ? 1 : 0;
        CGValue lv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int idx_i = inst ? 0 : 1;
        CGValue iv = gen_expr(g, args->items[idx_i]);
        LLVMValueRef lp = list_as_i8ptr(g, lv);
        LLVMValueRef idxv = cast_value(g, iv, type_make(TY_I64));
        LLVMValueRef tag_ptr = LLVMBuildAlloca(g->builder, g->i64, "list.rm.tag.ptr");
        LLVMValueRef payload_ptr = LLVMBuildAlloca(g->builder, g->i64, "list.rm.payload.ptr");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), tag_ptr);
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), payload_ptr);
        LLVMValueRef argv[4] = { lp, idxv, tag_ptr, payload_ptr };
        LLVMBuildCall2(g->builder, g->fn_list_remove_at_ty, g->fn_list_remove_at, argv, 4, "");
        LLVMValueRef tag = LLVMBuildLoad2(g->builder, g->i64, tag_ptr, "list.rm.tag");
        LLVMValueRef payload = LLVMBuildLoad2(g->builder, g->i64, payload_ptr, "list.rm.payload");
        LLVMValueRef anyres = LLVMGetUndef(g->any_ty);
        anyres = LLVMBuildInsertValue(g->builder, anyres, tag, 0, "list.rm.any.tag");
        anyres = LLVMBuildInsertValue(g->builder, anyres, payload, 1, "list.rm.any.payload");
        out.value = anyres;
        out.type = type_make(TY_ANY);
        return out;
    }
    case KN_BUILTIN_IO_LIST_CONTAINS:
    {
        int inst = (recv && args->count == 1) ? 1 : 0;
        CGValue lv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int val_i = inst ? 0 : 1;
        CGValue vv = gen_expr(g, args->items[val_i]);
        LLVMValueRef lp = list_as_i8ptr(g, lv);
        LLVMValueRef anyv = cast_value(g, vv, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "list.contains.tag");
        LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv, 1, "list.contains.payload");
        LLVMValueRef argv[3] = { lp, tag, payload };
        out.value = LLVMBuildCall2(g->builder, g->fn_list_contains_ty, g->fn_list_contains, argv, 3, "list.contains");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_LIST_POP:
    {
        int inst = (recv && args->count == 0) ? 1 : 0;
        CGValue lv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef lp = list_as_i8ptr(g, lv);
        LLVMValueRef tag_ptr = LLVMBuildAlloca(g->builder, g->i64, "list.pop.tag.ptr");
        LLVMValueRef payload_ptr = LLVMBuildAlloca(g->builder, g->i64, "list.pop.payload.ptr");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), tag_ptr);
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), payload_ptr);
        LLVMValueRef argv[3] = { lp, tag_ptr, payload_ptr };
        LLVMBuildCall2(g->builder, g->fn_list_pop_ty, g->fn_list_pop, argv, 3, "");
        LLVMValueRef tag = LLVMBuildLoad2(g->builder, g->i64, tag_ptr, "list.pop.tag");
        LLVMValueRef payload = LLVMBuildLoad2(g->builder, g->i64, payload_ptr, "list.pop.payload");
        LLVMValueRef anyres = LLVMGetUndef(g->any_ty);
        anyres = LLVMBuildInsertValue(g->builder, anyres, tag, 0, "list.pop.any.tag");
        anyres = LLVMBuildInsertValue(g->builder, anyres, payload, 1, "list.pop.any.payload");
        out.value = anyres;
        out.type = type_make(TY_ANY);
        return out;
    }
    case KN_BUILTIN_IO_LIST_PEEK:
    {
        int inst = (recv && args->count == 0) ? 1 : 0;
        CGValue lv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef lp = list_as_i8ptr(g, lv);
        LLVMValueRef cnt = LLVMBuildCall2(g->builder, g->fn_list_count_ty, g->fn_list_count, &lp, 1, "list.peek.cnt");
        LLVMValueRef idx = LLVMBuildSub(g->builder, cnt, LLVMConstInt(g->i64, 1, 0), "list.peek.idx");
        LLVMValueRef tag_ptr = LLVMBuildAlloca(g->builder, g->i64, "list.peek.tag.ptr");
        LLVMValueRef payload_ptr = LLVMBuildAlloca(g->builder, g->i64, "list.peek.payload.ptr");
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), tag_ptr);
        LLVMBuildStore(g->builder, LLVMConstInt(g->i64, 0, 0), payload_ptr);
        LLVMBuildCall2(g->builder, g->fn_list_get_ty, g->fn_list_get, (LLVMValueRef[]){ lp, idx, tag_ptr, payload_ptr }, 4, "");
        LLVMValueRef tag = LLVMBuildLoad2(g->builder, g->i64, tag_ptr, "list.peek.tag");
        LLVMValueRef payload = LLVMBuildLoad2(g->builder, g->i64, payload_ptr, "list.peek.payload");
        LLVMValueRef anyres = LLVMGetUndef(g->any_ty);
        anyres = LLVMBuildInsertValue(g->builder, anyres, tag, 0, "list.peek.any.tag");
        anyres = LLVMBuildInsertValue(g->builder, anyres, payload, 1, "list.peek.any.payload");
        out.value = anyres;
        out.type = type_make(TY_ANY);
        return out;
    }
    case KN_BUILTIN_IO_LIST_INDEX_OF:
    {
        int inst = (recv && args->count == 1) ? 1 : 0;
        CGValue lv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int val_i = inst ? 0 : 1;
        CGValue vv = gen_expr(g, args->items[val_i]);
        LLVMValueRef lp = list_as_i8ptr(g, lv);
        LLVMValueRef anyv = cast_value(g, vv, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "list.index.tag");
        LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv, 1, "list.index.payload");
        LLVMValueRef argv[3] = { lp, tag, payload };
        out.value = LLVMBuildCall2(g->builder, g->fn_list_index_of_ty, g->fn_list_index_of, argv, 3, "list.indexof");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_LIST_IS_EMPTY:
    {
        int inst = (recv && args->count == 0) ? 1 : 0;
        CGValue lv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef lp = list_as_i8ptr(g, lv);
        out.value = LLVMBuildCall2(g->builder, g->fn_list_is_empty_ty, g->fn_list_is_empty, &lp, 1, "list.empty");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_SET_NEW:
    {
        LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_set_new_ty, g->fn_set_new, 0, 0, "set.new");
        LLVMValueRef obj = LLVMBuildBitCast(g->builder, raw, llvm_type(g, type_class("IO.Collection.set")), "set.obj");
        out.value = obj;
        out.type = type_class("IO.Collection.set");
        return out;
    }
    case KN_BUILTIN_IO_SET_ADD:
    {
        int inst = (recv && args->count == 1) ? 1 : 0;
        CGValue sv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int key_i = inst ? 0 : 1;
        CGValue kv = gen_expr(g, args->items[key_i]);
        LLVMValueRef sp = set_as_i8ptr(g, sv);
        LLVMValueRef key = cast_value(g, kv, type_make(TY_STRING));
        LLVMValueRef argv[2] = { sp, key };
        LLVMBuildCall2(g->builder, g->fn_set_add_ty, g->fn_set_add, argv, 2, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_SET_CONTAINS:
    {
        int inst = (recv && args->count == 1) ? 1 : 0;
        CGValue sv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int key_i = inst ? 0 : 1;
        CGValue kv = gen_expr(g, args->items[key_i]);
        LLVMValueRef sp = set_as_i8ptr(g, sv);
        LLVMValueRef key = cast_value(g, kv, type_make(TY_STRING));
        LLVMValueRef argv[2] = { sp, key };
        out.value = LLVMBuildCall2(g->builder, g->fn_set_contains_ty, g->fn_set_contains, argv, 2, "set.contains");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_SET_COUNT:
    {
        int inst = (recv && args->count == 0) ? 1 : 0;
        CGValue sv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef sp = set_as_i8ptr(g, sv);
        out.value = LLVMBuildCall2(g->builder, g->fn_set_count_ty, g->fn_set_count, &sp, 1, "set.count");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_SET_REMOVE:
    {
        int inst = (recv && args->count == 1) ? 1 : 0;
        CGValue sv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        int key_i = inst ? 0 : 1;
        CGValue kv = gen_expr(g, args->items[key_i]);
        LLVMValueRef sp = set_as_i8ptr(g, sv);
        LLVMValueRef key = cast_value(g, kv, type_make(TY_STRING));
        LLVMValueRef argv[2] = { sp, key };
        out.value = LLVMBuildCall2(g->builder, g->fn_set_remove_ty, g->fn_set_remove, argv, 2, "set.remove");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_SET_CLEAR:
    {
        int inst = (recv && args->count == 0) ? 1 : 0;
        CGValue sv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef sp = set_as_i8ptr(g, sv);
        LLVMBuildCall2(g->builder, g->fn_set_clear_ty, g->fn_set_clear, &sp, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_SET_IS_EMPTY:
    {
        int inst = (recv && args->count == 0) ? 1 : 0;
        CGValue sv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef sp = set_as_i8ptr(g, sv);
        out.value = LLVMBuildCall2(g->builder, g->fn_set_is_empty_ty, g->fn_set_is_empty, &sp, 1, "set.empty");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_SET_VALUES:
    {
        int inst = (recv && args->count == 0) ? 1 : 0;
        CGValue sv = inst ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef sp = set_as_i8ptr(g, sv);
        LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_set_values_ty, g->fn_set_values, &sp, 1, "set.values");
        out.type = type_class("IO.Collection.list");
        out.value = LLVMBuildBitCast(g->builder, raw, llvm_type(g, out.type), "set.values.obj");
        return out;
    }
    case KN_BUILTIN_IO_MATH_ABS:
    {
        CGValue a = gen_expr(g, args->items[0]);
        LLVMValueRef v = cast_value(g, a, type_make(TY_INT));
        LLVMValueRef zero = LLVMConstInt(g->i64, 0, 0);
        LLVMValueRef isneg = LLVMBuildICmp(g->builder, LLVMIntSLT, v, zero, "isneg");
        LLVMValueRef neg = LLVMBuildNeg(g->builder, v, "neg");
        out.value = LLVMBuildSelect(g->builder, isneg, neg, v, "abs");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_MATH_MIN:
    {
        CGValue a = gen_expr(g, args->items[0]);
        CGValue b = gen_expr(g, args->items[1]);
        LLVMValueRef av = cast_value(g, a, type_make(TY_INT));
        LLVMValueRef bv = cast_value(g, b, type_make(TY_INT));
        LLVMValueRef cmp = LLVMBuildICmp(g->builder, LLVMIntSLT, av, bv, "lt");
        out.value = LLVMBuildSelect(g->builder, cmp, av, bv, "min");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_MATH_MAX:
    {
        CGValue a = gen_expr(g, args->items[0]);
        CGValue b = gen_expr(g, args->items[1]);
        LLVMValueRef av = cast_value(g, a, type_make(TY_INT));
        LLVMValueRef bv = cast_value(g, b, type_make(TY_INT));
        LLVMValueRef cmp = LLVMBuildICmp(g->builder, LLVMIntSGT, av, bv, "gt");
        out.value = LLVMBuildSelect(g->builder, cmp, av, bv, "max");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_MATH_CLAMP:
    {
        CGValue v = gen_expr(g, args->items[0]);
        CGValue minv = gen_expr(g, args->items[1]);
        CGValue maxv = gen_expr(g, args->items[2]);
        LLVMValueRef vv = cast_value(g, v, type_make(TY_INT));
        LLVMValueRef vmin = cast_value(g, minv, type_make(TY_INT));
        LLVMValueRef vmax = cast_value(g, maxv, type_make(TY_INT));
        LLVMValueRef lt = LLVMBuildICmp(g->builder, LLVMIntSLT, vv, vmin, "lt");
        LLVMValueRef gt = LLVMBuildICmp(g->builder, LLVMIntSGT, vv, vmax, "gt");
        LLVMValueRef sel_min = LLVMBuildSelect(g->builder, lt, vmin, vv, "cmin");
        out.value = LLVMBuildSelect(g->builder, gt, vmax, sel_min, "cmax");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_TYPE_BOOL:
    case KN_BUILTIN_IO_TYPE_BYTE:
    case KN_BUILTIN_IO_TYPE_CHAR:
    case KN_BUILTIN_IO_TYPE_INT:
    case KN_BUILTIN_IO_TYPE_FLOAT:
    case KN_BUILTIN_IO_TYPE_F16:
    case KN_BUILTIN_IO_TYPE_F32:
    case KN_BUILTIN_IO_TYPE_F64:
    case KN_BUILTIN_IO_TYPE_F128:
    case KN_BUILTIN_IO_TYPE_I8:
    case KN_BUILTIN_IO_TYPE_I16:
    case KN_BUILTIN_IO_TYPE_I32:
    case KN_BUILTIN_IO_TYPE_I64:
    case KN_BUILTIN_IO_TYPE_U8:
    case KN_BUILTIN_IO_TYPE_U16:
    case KN_BUILTIN_IO_TYPE_U32:
    case KN_BUILTIN_IO_TYPE_U64:
    case KN_BUILTIN_IO_TYPE_ISIZE:
    case KN_BUILTIN_IO_TYPE_USIZE:
    case KN_BUILTIN_IO_TYPE_STRING:
    case KN_BUILTIN_IO_TYPE_ANY:
    {
        CGValue a = gen_expr(g, args->items[0]);
        Type dst = type_make(TY_UNKNOWN);
        switch (kind)
        {
        case KN_BUILTIN_IO_TYPE_BOOL: dst = type_make(TY_BOOL); break;
        case KN_BUILTIN_IO_TYPE_BYTE: dst = type_make(TY_BYTE); break;
        case KN_BUILTIN_IO_TYPE_CHAR: dst = type_make(TY_CHAR); break;
        case KN_BUILTIN_IO_TYPE_INT: dst = type_make(TY_INT); break;
        case KN_BUILTIN_IO_TYPE_FLOAT: dst = type_make(TY_FLOAT); break;
        case KN_BUILTIN_IO_TYPE_F16: dst = type_make(TY_F16); break;
        case KN_BUILTIN_IO_TYPE_F32: dst = type_make(TY_F32); break;
        case KN_BUILTIN_IO_TYPE_F64: dst = type_make(TY_F64); break;
        case KN_BUILTIN_IO_TYPE_F128: dst = type_make(TY_F128); break;
        case KN_BUILTIN_IO_TYPE_I8: dst = type_make(TY_I8); break;
        case KN_BUILTIN_IO_TYPE_I16: dst = type_make(TY_I16); break;
        case KN_BUILTIN_IO_TYPE_I32: dst = type_make(TY_I32); break;
        case KN_BUILTIN_IO_TYPE_I64: dst = type_make(TY_I64); break;
        case KN_BUILTIN_IO_TYPE_U8: dst = type_make(TY_U8); break;
        case KN_BUILTIN_IO_TYPE_U16: dst = type_make(TY_U16); break;
        case KN_BUILTIN_IO_TYPE_U32: dst = type_make(TY_U32); break;
        case KN_BUILTIN_IO_TYPE_U64: dst = type_make(TY_U64); break;
        case KN_BUILTIN_IO_TYPE_ISIZE: dst = type_make(TY_ISIZE); break;
        case KN_BUILTIN_IO_TYPE_USIZE: dst = type_make(TY_USIZE); break;
        case KN_BUILTIN_IO_TYPE_STRING: dst = type_make(TY_STRING); break;
        case KN_BUILTIN_IO_TYPE_ANY: dst = type_make(TY_ANY); break;
        default: dst = type_make(TY_UNKNOWN); break;
        }
        out.value = cast_value(g, a, dst);
        out.type = dst;
        return out;
    }
    case KN_BUILTIN_IO_SYSTEM_EXIT:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef v = cast_value(g, arg, type_make(TY_INT));
        LLVMBuildCall2(g->builder, g->fn_sys_exit_ty, g->fn_sys_exit, &v, 1, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_SYSTEM_COMMANDLINE:
    {
        out.value = LLVMBuildCall2(g->builder, g->fn_sys_cmdline_ty, g->fn_sys_cmdline, 0, 0, "cmdline");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_SYSTEM_FILE_EXISTS:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef path = cast_value(g, arg, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_sys_file_exists_ty, g->fn_sys_file_exists, &path, 1, "fexists");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_SYSTEM_EXEC:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef cmd = cast_value(g, arg, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_sys_exec_ty, g->fn_sys_exec, &cmd, 1, "sys.exec");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_SYSTEM_LOAD_LIBRARY:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef path = cast_value(g, arg, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_sys_load_library_ty, g->fn_sys_load_library, &path, 1, "sys.loadlib");
        out.type = type_ptr(TY_BYTE);
        return out;
    }
    case KN_BUILTIN_IO_SYSTEM_GET_SYMBOL:
    {
        CGValue libv = gen_expr(g, args->items[0]);
        CGValue namev = gen_expr(g, args->items[1]);
        LLVMValueRef lib = cast_value(g, libv, type_ptr(TY_BYTE));
        LLVMValueRef name = cast_value(g, namev, type_make(TY_STRING));
        LLVMValueRef argv[2] = { lib, name };
        out.value = LLVMBuildCall2(g->builder, g->fn_sys_get_symbol_ty, g->fn_sys_get_symbol, argv, 2, "sys.getsym");
        out.type = type_ptr(TY_BYTE);
        return out;
    }
    case KN_BUILTIN_IO_SYSTEM_CLOSE_LIBRARY:
    {
        CGValue libv = gen_expr(g, args->items[0]);
        LLVMValueRef lib = cast_value(g, libv, type_ptr(TY_BYTE));
        out.value = LLVMBuildCall2(g->builder, g->fn_sys_close_library_ty, g->fn_sys_close_library, &lib, 1, "sys.closelib");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_SYSTEM_LAST_ERROR:
    {
        out.value = LLVMBuildCall2(g->builder, g->fn_sys_last_error_ty, g->fn_sys_last_error, 0, 0, "sys.lasterr");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_ASYNC_SPAWN:
    {
        CGValue arg = gen_expr(g, args->items[0]);
        LLVMValueRef cmd = cast_value(g, arg, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_async_spawn_ty, g->fn_async_spawn, &cmd, 1, "async.spawn");
        out.type = type_ptr(TY_BYTE);
        return out;
    }
    case KN_BUILTIN_IO_ASYNC_IS_COMPLETED:
    {
        CGValue handlev = gen_expr(g, args->items[0]);
        LLVMValueRef handle = cast_value(g, handlev, type_ptr(TY_BYTE));
        out.value = LLVMBuildCall2(g->builder, g->fn_async_is_completed_ty, g->fn_async_is_completed, &handle, 1, "async.done");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ASYNC_WAIT:
    {
        CGValue handlev = gen_expr(g, args->items[0]);
        CGValue timeoutv = gen_expr(g, args->items[1]);
        LLVMValueRef handle = cast_value(g, handlev, type_ptr(TY_BYTE));
        LLVMValueRef timeout = cast_value(g, timeoutv, type_make(TY_INT));
        LLVMValueRef argv[2] = { handle, timeout };
        out.value = LLVMBuildCall2(g->builder, g->fn_async_wait_ty, g->fn_async_wait, argv, 2, "async.wait");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ASYNC_EXIT_CODE:
    {
        CGValue handlev = gen_expr(g, args->items[0]);
        LLVMValueRef handle = cast_value(g, handlev, type_ptr(TY_BYTE));
        out.value = LLVMBuildCall2(g->builder, g->fn_async_exit_code_ty, g->fn_async_exit_code, &handle, 1, "async.exit");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_ASYNC_CLOSE:
    {
        CGValue handlev = gen_expr(g, args->items[0]);
        LLVMValueRef handle = cast_value(g, handlev, type_ptr(TY_BYTE));
        out.value = LLVMBuildCall2(g->builder, g->fn_async_close_ty, g->fn_async_close, &handle, 1, "async.close");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ASYNC_RUN:
    {
        CGValue taskv = gen_expr(g, args->items[0]);
        Type result_type = args->items[0]->async_result_type.kind != TY_UNKNOWN
            ? args->items[0]->async_result_type
            : type_make(TY_ANY);
        LLVMValueRef anyv = run_async_task_any(g, taskv.value, "async.run.tag");
        if (result_type.kind == TY_VOID)
        {
            out.value = LLVMGetUndef(g->voidt);
            out.type = type_make(TY_VOID);
        }
        else if (result_type.kind == TY_ANY)
        {
            out.value = anyv;
            out.type = result_type;
        }
        else
        {
            out.value = any_to_type(g, anyv, result_type);
            out.type = result_type;
        }
        maybe_propagate_exception(g);
        return out;
    }
    case KN_BUILTIN_IO_ASYNC_SLEEP:
    {
        CGValue msv = gen_expr(g, args->items[0]);
        CGValue ms32;
        ms32.value = cast_value(g, msv, type_make(TY_I32));
        ms32.type = type_make(TY_I32);
        out.value = LLVMBuildCall2(g->builder, g->fn_async_task_sleep_ty, g->fn_async_task_sleep, &ms32.value, 1, "async.sleep.task");
        out.type = cg_async_task_type();
        return out;
    }
    case KN_BUILTIN_IO_ASYNC_YIELD:
    {
        out.value = LLVMBuildCall2(g->builder, g->fn_async_task_yield_ty, g->fn_async_task_yield, 0, 0, "async.yield.task");
        out.type = cg_async_task_type();
        return out;
    }
    case KN_BUILTIN_IO_META_QUERY:
    {
        CGValue namev = gen_expr(g, args->items[0]);
        LLVMValueRef name = cast_value(g, namev, type_make(TY_STRING));
        LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_meta_query_ty, g->fn_meta_query, &name, 1, "meta.query");
        out.value = LLVMBuildBitCast(g->builder, raw, llvm_type(g, type_class("IO.Collection.list")), "meta.query.obj");
        out.type = type_class("IO.Collection.list");
        return out;
    }
    case KN_BUILTIN_IO_META_OF:
    {
        CGValue ownerv = gen_expr(g, args->items[0]);
        LLVMValueRef owner = cast_value(g, ownerv, type_make(TY_STRING));
        LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_meta_of_ty, g->fn_meta_of, &owner, 1, "meta.of");
        out.value = LLVMBuildBitCast(g->builder, raw, llvm_type(g, type_class("IO.Collection.list")), "meta.of.obj");
        out.type = type_class("IO.Collection.list");
        return out;
    }
    case KN_BUILTIN_IO_META_HAS:
    {
        CGValue ownerv = gen_expr(g, args->items[0]);
        CGValue metav = gen_expr(g, args->items[1]);
        LLVMValueRef owner = cast_value(g, ownerv, type_make(TY_STRING));
        LLVMValueRef meta = cast_value(g, metav, type_make(TY_STRING));
        LLVMValueRef argv[2] = { owner, meta };
        out.value = LLVMBuildCall2(g->builder, g->fn_meta_has_ty, g->fn_meta_has, argv, 2, "meta.has");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_META_FIND:
    {
        CGValue ownerv = gen_expr(g, args->items[0]);
        CGValue metav = gen_expr(g, args->items[1]);
        LLVMValueRef owner = cast_value(g, ownerv, type_make(TY_STRING));
        LLVMValueRef meta = cast_value(g, metav, type_make(TY_STRING));
        LLVMValueRef argv[2] = { owner, meta };
        LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_meta_find_ty, g->fn_meta_find, argv, 2, "meta.find");
        out.value = LLVMBuildBitCast(g->builder, raw, llvm_type(g, type_class("IO.Collection.dict")), "meta.find.obj");
        out.type = type_class("IO.Collection.dict");
        return out;
    }
    case KN_BUILTIN_IO_PATH_COMBINE:
    {
        CGValue a = gen_expr(g, args->items[0]);
        CGValue b = gen_expr(g, args->items[1]);
        LLVMValueRef av = cast_value(g, a, type_make(TY_STRING));
        LLVMValueRef bv = cast_value(g, b, type_make(TY_STRING));
        LLVMValueRef argv[2] = { av, bv };
        out.value = LLVMBuildCall2(g->builder, g->fn_path_combine_ty, g->fn_path_combine, argv, 2, "path.combine");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_PATH_FILE_NAME:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_path_file_name_ty, g->fn_path_file_name, &pv, 1, "path.file");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_PATH_EXTENSION:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_path_extension_ty, g->fn_path_extension, &pv, 1, "path.ext");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_PATH_DIRECTORY:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_path_directory_ty, g->fn_path_directory, &pv, 1, "path.dir");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_PATH_NORMALIZE:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_path_normalize_ty, g->fn_path_normalize, &pv, 1, "path.norm");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_PATH_JOIN3:
    {
        CGValue a = gen_expr(g, args->items[0]);
        CGValue b = gen_expr(g, args->items[1]);
        CGValue c = gen_expr(g, args->items[2]);
        LLVMValueRef argv[3] = {
            cast_value(g, a, type_make(TY_STRING)),
            cast_value(g, b, type_make(TY_STRING)),
            cast_value(g, c, type_make(TY_STRING))
        };
        out.value = LLVMBuildCall2(g->builder, g->fn_path_join3_ty, g->fn_path_join3, argv, 3, "path.join3");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_PATH_JOIN4:
    {
        CGValue a = gen_expr(g, args->items[0]);
        CGValue b = gen_expr(g, args->items[1]);
        CGValue c = gen_expr(g, args->items[2]);
        CGValue d = gen_expr(g, args->items[3]);
        LLVMValueRef argv[4] = {
            cast_value(g, a, type_make(TY_STRING)),
            cast_value(g, b, type_make(TY_STRING)),
            cast_value(g, c, type_make(TY_STRING)),
            cast_value(g, d, type_make(TY_STRING))
        };
        out.value = LLVMBuildCall2(g->builder, g->fn_path_join4_ty, g->fn_path_join4, argv, 4, "path.join4");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_PATH_CWD:
    {
        out.value = LLVMBuildCall2(g->builder, g->fn_path_cwd_ty, g->fn_path_cwd, 0, 0, "path.cwd");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_PATH_CHANGE_EXTENSION:
    {
        CGValue p = gen_expr(g, args->items[0]);
        CGValue e2 = gen_expr(g, args->items[1]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        LLVMValueRef ev = cast_value(g, e2, type_make(TY_STRING));
        LLVMValueRef argv[2] = { pv, ev };
        out.value = LLVMBuildCall2(g->builder, g->fn_path_change_extension_ty, g->fn_path_change_extension, argv, 2, "path.changeext");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_PATH_HAS_EXTENSION:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        LLVMValueRef ext = LLVMBuildCall2(g->builder, g->fn_path_extension_ty, g->fn_path_extension, &pv, 1, "path.hasext.ext");
        LLVMValueRef len = LLVMBuildCall2(g->builder, g->fn_strlen_ty, g->fn_strlen, &ext, 1, "path.hasext.len");
        out.value = LLVMBuildICmp(g->builder, LLVMIntUGT, len, LLVMConstInt(g->i64, 0, 0), "path.hasext");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_PATH_IS_ABSOLUTE:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_path_is_absolute_ty, g->fn_path_is_absolute, &pv, 1, "path.abs");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_PATH_IS_RELATIVE:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        LLVMValueRef abs = LLVMBuildCall2(g->builder, g->fn_path_is_absolute_ty, g->fn_path_is_absolute, &pv, 1, "path.rel.abs");
        out.value = LLVMBuildNot(g->builder, abs, "path.rel");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_PATH_BASE_NAME:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_path_base_name_ty, g->fn_path_base_name, &pv, 1, "path.basename");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_PATH_ENSURE_TRAILING_SEPARATOR:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_path_ensure_trailing_separator_ty, g->fn_path_ensure_trailing_separator, &pv, 1, "path.sep");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_LENGTH:
    {
        CGValue s = gen_expr(g, args->items[0]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_strlen_ty, g->fn_strlen, &sv, 1, "text.len");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_IS_EMPTY:
    {
        CGValue s = gen_expr(g, args->items[0]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        LLVMValueRef len = LLVMBuildCall2(g->builder, g->fn_strlen_ty, g->fn_strlen, &sv, 1, "text.empty.len");
        out.value = LLVMBuildICmp(g->builder, LLVMIntEQ, len, LLVMConstInt(g->i64, 0, 0), "text.empty");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_COUNT:
    {
        CGValue s = gen_expr(g, args->items[0]);
        CGValue sub = gen_expr(g, args->items[1]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        LLVMValueRef subv = cast_value(g, sub, type_make(TY_STRING));
        LLVMValueRef argv[2] = { sv, subv };
        out.value = LLVMBuildCall2(g->builder, g->fn_text_count_ty, g->fn_text_count, argv, 2, "text.count");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_CONTAINS:
    {
        CGValue s = gen_expr(g, args->items[0]);
        CGValue sub = gen_expr(g, args->items[1]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        LLVMValueRef subv = cast_value(g, sub, type_make(TY_STRING));
        LLVMValueRef argv[2] = { sv, subv };
        out.value = LLVMBuildCall2(g->builder, g->fn_text_contains_ty, g->fn_text_contains, argv, 2, "text.contains");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_STARTS_WITH:
    {
        CGValue s = gen_expr(g, args->items[0]);
        CGValue pre = gen_expr(g, args->items[1]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        LLVMValueRef pv = cast_value(g, pre, type_make(TY_STRING));
        LLVMValueRef argv[2] = { sv, pv };
        out.value = LLVMBuildCall2(g->builder, g->fn_text_starts_with_ty, g->fn_text_starts_with, argv, 2, "text.starts");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_ENDS_WITH:
    {
        CGValue s = gen_expr(g, args->items[0]);
        CGValue suf = gen_expr(g, args->items[1]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        LLVMValueRef pv = cast_value(g, suf, type_make(TY_STRING));
        LLVMValueRef argv[2] = { sv, pv };
        out.value = LLVMBuildCall2(g->builder, g->fn_text_ends_with_ty, g->fn_text_ends_with, argv, 2, "text.ends");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_INDEX_OF:
    {
        CGValue s = gen_expr(g, args->items[0]);
        CGValue sub = gen_expr(g, args->items[1]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        LLVMValueRef subv = cast_value(g, sub, type_make(TY_STRING));
        LLVMValueRef argv[2] = { sv, subv };
        out.value = LLVMBuildCall2(g->builder, g->fn_text_index_of_ty, g->fn_text_index_of, argv, 2, "text.indexof");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_LAST_INDEX_OF:
    {
        CGValue s = gen_expr(g, args->items[0]);
        CGValue sub = gen_expr(g, args->items[1]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        LLVMValueRef subv = cast_value(g, sub, type_make(TY_STRING));
        LLVMValueRef argv[2] = { sv, subv };
        out.value = LLVMBuildCall2(g->builder, g->fn_text_last_index_of_ty, g->fn_text_last_index_of, argv, 2, "text.lastindexof");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_TRIM:
    {
        CGValue s = gen_expr(g, args->items[0]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_text_trim_ty, g->fn_text_trim, &sv, 1, "text.trim");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_REPLACE:
    {
        CGValue s = gen_expr(g, args->items[0]);
        CGValue from = gen_expr(g, args->items[1]);
        CGValue to = gen_expr(g, args->items[2]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        LLVMValueRef fv = cast_value(g, from, type_make(TY_STRING));
        LLVMValueRef tv = cast_value(g, to, type_make(TY_STRING));
        LLVMValueRef argv[3] = { sv, fv, tv };
        out.value = LLVMBuildCall2(g->builder, g->fn_text_replace_ty, g->fn_text_replace, argv, 3, "text.replace");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_TO_UPPER:
    {
        CGValue s = gen_expr(g, args->items[0]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_text_to_upper_ty, g->fn_text_to_upper, &sv, 1, "text.upper");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_TO_LOWER:
    {
        CGValue s = gen_expr(g, args->items[0]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_text_to_lower_ty, g->fn_text_to_lower, &sv, 1, "text.lower");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_SLICE:
    {
        CGValue s = gen_expr(g, args->items[0]);
        CGValue b = gen_expr(g, args->items[1]);
        CGValue l = gen_expr(g, args->items[2]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        LLVMValueRef bv = cast_value(g, b, type_make(TY_INT));
        LLVMValueRef lv = cast_value(g, l, type_make(TY_INT));
        LLVMValueRef argv[3] = { sv, bv, lv };
        out.value = LLVMBuildCall2(g->builder, g->fn_text_slice_ty, g->fn_text_slice, argv, 3, "text.slice");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_SPLIT:
    {
        CGValue s = gen_expr(g, args->items[0]);
        CGValue sep = gen_expr(g, args->items[1]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        LLVMValueRef sepv = cast_value(g, sep, type_make(TY_STRING));
        LLVMValueRef argv[2] = { sv, sepv };
        LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_text_split_ty, g->fn_text_split, argv, 2, "text.split");
        out.type = type_class("IO.Collection.list");
        out.value = LLVMBuildBitCast(g->builder, raw, llvm_type(g, out.type), "text.split.obj");
        return out;
    }
    case KN_BUILTIN_IO_TEXT_JOIN:
    {
        CGValue l = gen_expr(g, args->items[0]);
        CGValue sep = gen_expr(g, args->items[1]);
        LLVMValueRef lv = list_as_i8ptr(g, l);
        LLVMValueRef sepv = cast_value(g, sep, type_make(TY_STRING));
        LLVMValueRef argv[2] = { lv, sepv };
        out.value = LLVMBuildCall2(g->builder, g->fn_text_join_ty, g->fn_text_join, argv, 2, "text.join");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_TEXT_LINES:
    {
        CGValue s = gen_expr(g, args->items[0]);
        LLVMValueRef sv = cast_value(g, s, type_make(TY_STRING));
        LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_text_lines_ty, g->fn_text_lines, &sv, 1, "text.lines");
        out.type = type_class("IO.Collection.list");
        out.value = LLVMBuildBitCast(g->builder, raw, llvm_type(g, out.type), "text.lines.obj");
        return out;
    }
    case KN_BUILTIN_IO_FILE_CREATE:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_file_create_ty, g->fn_file_create, &pv, 1, "file.create");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_FILE_TOUCH:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_file_touch_ty, g->fn_file_touch, &pv, 1, "file.touch");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_FILE_READ_ALL_TEXT:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_file_read_all_text_ty, g->fn_file_read_all_text, &pv, 1, "file.read");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_FILE_READ_FIRST_LINE:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_file_read_first_line_ty, g->fn_file_read_first_line, &pv, 1, "file.firstline");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_FILE_WRITE_ALL_TEXT:
    {
        CGValue p = gen_expr(g, args->items[0]);
        CGValue t = gen_expr(g, args->items[1]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        LLVMValueRef tv = cast_value(g, t, type_make(TY_STRING));
        LLVMValueRef argv[2] = { pv, tv };
        out.value = LLVMBuildCall2(g->builder, g->fn_file_write_all_text_ty, g->fn_file_write_all_text, argv, 2, "file.write");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_FILE_APPEND_ALL_TEXT:
    {
        CGValue p = gen_expr(g, args->items[0]);
        CGValue t = gen_expr(g, args->items[1]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        LLVMValueRef tv = cast_value(g, t, type_make(TY_STRING));
        LLVMValueRef argv[2] = { pv, tv };
        out.value = LLVMBuildCall2(g->builder, g->fn_file_append_all_text_ty, g->fn_file_append_all_text, argv, 2, "file.append");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_FILE_APPEND_LINE:
    {
        CGValue p = gen_expr(g, args->items[0]);
        CGValue t = gen_expr(g, args->items[1]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        LLVMValueRef tv = cast_value(g, t, type_make(TY_STRING));
        LLVMValueRef argv[2] = { pv, tv };
        out.value = LLVMBuildCall2(g->builder, g->fn_file_append_line_ty, g->fn_file_append_line, argv, 2, "file.appendline");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_FILE_DELETE:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_file_delete_ty, g->fn_file_delete, &pv, 1, "file.delete");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_FILE_DELETE_IF_EXISTS:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_file_delete_if_exists_ty, g->fn_file_delete_if_exists, &pv, 1, "file.deleteif");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_FILE_SIZE:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_file_size_ty, g->fn_file_size, &pv, 1, "file.size");
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_FILE_IS_EMPTY:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_file_is_empty_ty, g->fn_file_is_empty, &pv, 1, "file.empty");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_FILE_COPY:
    {
        CGValue a = gen_expr(g, args->items[0]);
        CGValue b = gen_expr(g, args->items[1]);
        LLVMValueRef av = cast_value(g, a, type_make(TY_STRING));
        LLVMValueRef bv = cast_value(g, b, type_make(TY_STRING));
        LLVMValueRef argv[2] = { av, bv };
        out.value = LLVMBuildCall2(g->builder, g->fn_file_copy_ty, g->fn_file_copy, argv, 2, "file.copy");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_FILE_MOVE:
    {
        CGValue a = gen_expr(g, args->items[0]);
        CGValue b = gen_expr(g, args->items[1]);
        LLVMValueRef av = cast_value(g, a, type_make(TY_STRING));
        LLVMValueRef bv = cast_value(g, b, type_make(TY_STRING));
        LLVMValueRef argv[2] = { av, bv };
        out.value = LLVMBuildCall2(g->builder, g->fn_file_move_ty, g->fn_file_move, argv, 2, "file.move");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_FILE_READ_OR_DEFAULT:
    case KN_BUILTIN_IO_FILE_READ_OR:
    {
        CGValue p = gen_expr(g, args->items[0]);
        CGValue d = gen_expr(g, args->items[1]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        LLVMValueRef dv = cast_value(g, d, type_make(TY_STRING));
        LLVMValueRef exists = LLVMBuildCall2(g->builder, g->fn_sys_file_exists_ty, g->fn_sys_file_exists, &pv, 1, "file.readdef.exists");
        LLVMValueRef readv = LLVMBuildCall2(g->builder, g->fn_file_read_all_text_ty, g->fn_file_read_all_text, &pv, 1, "file.readdef.read");
        out.value = LLVMBuildSelect(g->builder, exists, readv, dv, "file.readdef");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_FILE_READ_IF_EXISTS:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        LLVMValueRef exists = LLVMBuildCall2(g->builder, g->fn_sys_file_exists_ty, g->fn_sys_file_exists, &pv, 1, "file.readif.exists");
        LLVMValueRef readv = LLVMBuildCall2(g->builder, g->fn_file_read_all_text_ty, g->fn_file_read_all_text, &pv, 1, "file.readif.read");
        out.value = LLVMBuildSelect(g->builder, exists, readv, LLVMConstNull(g->i8ptr), "file.readif");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_FILE_READ_LINES:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_file_read_lines_ty, g->fn_file_read_lines, &pv, 1, "file.readlines");
        out.type = type_class("IO.Collection.list");
        out.value = LLVMBuildBitCast(g->builder, raw, llvm_type(g, out.type), "file.readlines.obj");
        return out;
    }
    case KN_BUILTIN_IO_FILE_WRITE_LINES:
    {
        CGValue p = gen_expr(g, args->items[0]);
        CGValue l = gen_expr(g, args->items[1]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        LLVMValueRef lv = list_as_i8ptr(g, l);
        LLVMValueRef argv[2] = { pv, lv };
        out.value = LLVMBuildCall2(g->builder, g->fn_file_write_lines_ty, g->fn_file_write_lines, argv, 2, "file.writelines");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_FILE_REPLACE_TEXT:
    {
        CGValue p = gen_expr(g, args->items[0]);
        CGValue from = gen_expr(g, args->items[1]);
        CGValue to = gen_expr(g, args->items[2]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        LLVMValueRef fv = cast_value(g, from, type_make(TY_STRING));
        LLVMValueRef tv = cast_value(g, to, type_make(TY_STRING));
        LLVMValueRef argv[3] = { pv, fv, tv };
        out.value = LLVMBuildCall2(g->builder, g->fn_file_replace_text_ty, g->fn_file_replace_text, argv, 3, "file.replace");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_DIRECTORY_EXISTS:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_dir_exists_ty, g->fn_dir_exists, &pv, 1, "dir.exists");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_DIRECTORY_CREATE:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_dir_create_ty, g->fn_dir_create, &pv, 1, "dir.create");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_DIRECTORY_ENSURE:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_dir_ensure_ty, g->fn_dir_ensure, &pv, 1, "dir.ensure");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_DIRECTORY_DELETE:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_dir_delete_ty, g->fn_dir_delete, &pv, 1, "dir.delete");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_DIRECTORY_DELETE_IF_EXISTS:
    {
        CGValue p = gen_expr(g, args->items[0]);
        LLVMValueRef pv = cast_value(g, p, type_make(TY_STRING));
        out.value = LLVMBuildCall2(g->builder, g->fn_dir_delete_if_exists_ty, g->fn_dir_delete_if_exists, &pv, 1, "dir.deleteif");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ANY_ISNULL:
    {
        CGValue a = (recv && args->count == 0) ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        if (a.type.kind != TY_ANY && a.type.kind != TY_UNKNOWN)
        {
            if (a.type.kind == TY_NULL)
            {
                out.value = LLVMConstInt(llvm_type(g, type_make(TY_BOOL)), 1, 0);
                out.type = type_make(TY_BOOL);
                return out;
            }
            if (a.type.kind == TY_STRING || a.type.kind == TY_PTR || a.type.kind == TY_CLASS)
            {
                out.value = LLVMBuildICmp(g->builder, LLVMIntEQ, a.value,
                                          LLVMConstNull(llvm_type(g, a.type)),
                                          "ref.isnull");
                out.type = type_make(TY_BOOL);
                return out;
            }
            out.value = LLVMConstInt(llvm_type(g, type_make(TY_BOOL)), 0, 0);
            out.type = type_make(TY_BOOL);
            return out;
        }
        LLVMValueRef anyv = cast_value(g, a, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "any.tag");
        LLVMValueRef payload = LLVMBuildExtractValue(g->builder, anyv, 1, "any.payload");
        LLVMValueRef tag_is_none = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_NONE, 0), "any.none");
        LLVMValueRef payload_zero = LLVMBuildICmp(g->builder, LLVMIntEQ, payload, LLVMConstInt(g->i64, 0, 0), "any.zero");
        out.value = LLVMBuildAnd(g->builder, tag_is_none, payload_zero, "any.isnull");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ANY_TOSTRING:
    {
        CGValue a = (recv && args->count == 0) ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        out.value = cast_value(g, a, type_make(TY_STRING));
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_ANY_EQUALS:
    {
        CGValue a = (recv && args->count == 1) ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        CGValue b = (recv && args->count == 1) ? gen_expr(g, args->items[0]) : gen_expr(g, args->items[1]);
        LLVMValueRef av = cast_value(g, a, type_make(TY_ANY));
        LLVMValueRef bv = cast_value(g, b, type_make(TY_ANY));
        LLVMValueRef at = LLVMBuildExtractValue(g->builder, av, 0, "any.eq.at");
        LLVMValueRef bt = LLVMBuildExtractValue(g->builder, bv, 0, "any.eq.bt");
        LLVMValueRef ap = LLVMBuildExtractValue(g->builder, av, 1, "any.eq.ap");
        LLVMValueRef bp = LLVMBuildExtractValue(g->builder, bv, 1, "any.eq.bp");
        LLVMValueRef same_tag = LLVMBuildICmp(g->builder, LLVMIntEQ, at, bt, "any.eq.sametag");
        LLVMValueRef same_payload = LLVMBuildICmp(g->builder, LLVMIntEQ, ap, bp, "any.eq.samepayload");
        LLVMValueRef is_str = LLVMBuildICmp(g->builder, LLVMIntEQ, at, LLVMConstInt(g->i64, KN_ANY_TAG_STRING, 0), "any.eq.isstr");
        LLVMValueRef asp = LLVMBuildIntToPtr(g->builder, ap, g->i8ptr, "any.eq.as");
        LLVMValueRef bsp = LLVMBuildIntToPtr(g->builder, bp, g->i8ptr, "any.eq.bs");
        LLVMValueRef nullp = LLVMConstNull(g->i8ptr);
        LLVMValueRef asp_safe = LLVMBuildSelect(g->builder, is_str, asp, nullp, "any.eq.as.safe");
        LLVMValueRef bsp_safe = LLVMBuildSelect(g->builder, is_str, bsp, nullp, "any.eq.bs.safe");
        LLVMValueRef str_eq = LLVMBuildCall2(g->builder, g->fn_str_eq_ty, g->fn_str_eq, (LLVMValueRef[]){ asp_safe, bsp_safe }, 2, "any.eq.str");
        LLVMValueRef val_eq = LLVMBuildSelect(g->builder, is_str, str_eq, same_payload, "any.eq.val");
        out.value = LLVMBuildAnd(g->builder, same_tag, val_eq, "any.eq");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ANY_TAG:
    {
        CGValue a = (recv && args->count == 0) ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef anyv = cast_value(g, a, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "any.tag");
        out.value = tag;
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_IO_ANY_TYPENAME:
    {
        CGValue a = (recv && args->count == 0) ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        if (a.type.kind != TY_ANY && a.type.kind != TY_UNKNOWN)
        {
            const char *tn = cg_type_name(a.type);
            out.value = build_string_const(g, tn, (int)kn_strlen(tn));
            out.type = type_make(TY_STRING);
            return out;
        }
        LLVMValueRef anyv = cast_value(g, a, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "any.tag");
        LLVMValueRef is_none = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_NONE, 0), "");
        LLVMValueRef is_i64 = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_I64, 0), "");
        LLVMValueRef is_f64 = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_F64, 0), "");
        LLVMValueRef is_bool = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_BOOL, 0), "");
        LLVMValueRef is_char = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_CHAR, 0), "");
        LLVMValueRef is_str = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_STRING, 0), "");
        LLVMValueRef t_none = build_string_const(g, "null", 4);
        LLVMValueRef t_i64 = build_string_const(g, "int", 3);
        LLVMValueRef t_f64 = build_string_const(g, "float", 5);
        LLVMValueRef t_bool = build_string_const(g, "bool", 4);
        LLVMValueRef t_char = build_string_const(g, "char", 4);
        LLVMValueRef t_str = build_string_const(g, "string", 6);
        LLVMValueRef t_ptr = build_string_const(g, "pointer", 7);
        LLVMValueRef name = LLVMBuildSelect(g->builder, is_none, t_none, t_ptr, "");
        name = LLVMBuildSelect(g->builder, is_i64, t_i64, name, "");
        name = LLVMBuildSelect(g->builder, is_f64, t_f64, name, "");
        name = LLVMBuildSelect(g->builder, is_bool, t_bool, name, "");
        name = LLVMBuildSelect(g->builder, is_char, t_char, name, "");
        name = LLVMBuildSelect(g->builder, is_str, t_str, name, "");
        out.value = name;
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_ANY_ISNUMBER:
    {
        CGValue a = (recv && args->count == 0) ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef anyv = cast_value(g, a, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "any.tag");
        LLVMValueRef is_i64 = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_I64, 0), "");
        LLVMValueRef is_f64 = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_F64, 0), "");
        out.value = LLVMBuildOr(g->builder, is_i64, is_f64, "any.isnum");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ANY_ISPOINTER:
    {
        CGValue a = (recv && args->count == 0) ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef anyv = cast_value(g, a, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "any.tag");
        out.value = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_PTR, 0), "any.isptr");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ANY_ISINT:
    {
        CGValue a = (recv && args->count == 0) ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef anyv = cast_value(g, a, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "any.tag");
        out.value = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_I64, 0), "any.isint");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ANY_ISFLOAT:
    {
        CGValue a = (recv && args->count == 0) ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef anyv = cast_value(g, a, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "any.tag");
        out.value = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_F64, 0), "any.isfloat");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ANY_ISSTRING:
    {
        CGValue a = (recv && args->count == 0) ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef anyv = cast_value(g, a, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "any.tag");
        out.value = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_STRING, 0), "any.isstring");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ANY_ISBOOL:
    {
        CGValue a = (recv && args->count == 0) ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef anyv = cast_value(g, a, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "any.tag");
        out.value = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_BOOL, 0), "any.isbool");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ANY_ISCHAR:
    {
        CGValue a = (recv && args->count == 0) ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef anyv = cast_value(g, a, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "any.tag");
        out.value = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_CHAR, 0), "any.ischar");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ANY_ISOBJECT:
    {
        CGValue a = (recv && args->count == 0) ? gen_expr(g, recv) : gen_expr(g, args->items[0]);
        LLVMValueRef anyv = cast_value(g, a, type_make(TY_ANY));
        LLVMValueRef tag = LLVMBuildExtractValue(g->builder, anyv, 0, "any.tag");
        out.value = LLVMBuildICmp(g->builder, LLVMIntEQ, tag, LLVMConstInt(g->i64, KN_ANY_TAG_PTR, 0), "any.isobj");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ERROR_LASTTRACE:
    {
        out.value = LLVMBuildCall2(g->builder, g->fn_exc_trace_ty, g->fn_exc_trace, 0, 0, "exc.trace");
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_IO_ERROR_HAS:
    {
        out.value = LLVMBuildCall2(g->builder, g->fn_exc_has_ty, g->fn_exc_has, 0, 0, "exc.has");
        out.type = type_make(TY_BOOL);
        return out;
    }
    case KN_BUILTIN_IO_ERROR_CLEAR:
    {
        LLVMBuildCall2(g->builder, g->fn_exc_clear_ty, g->fn_exc_clear, 0, 0, "exc.clear");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_IO_ERROR_CURRENT:
    {
        LLVMValueRef cur = LLVMBuildCall2(g->builder, g->fn_exc_last_ty, g->fn_exc_last, 0, 0, "exc.current");
        if (g->error_class)
            out.value = LLVMBuildBitCast(g->builder, cur, g->error_class->ptr_ty, "err.current");
        else
            out.value = LLVMBuildBitCast(g->builder, cur, llvm_type(g, type_class("IO.Error")), "err.current");
        out.type = type_class("IO.Error");
        return out;
    }
    case KN_BUILTIN_IO_GC_COLLECT:
    {
        LLVMBuildCall2(g->builder, g->fn_gc_collect_ty, g->fn_gc_collect, 0, 0, "");
        out.type = type_make(TY_VOID);
        return out;
    }
    case KN_BUILTIN_ARRAY_LENGTH:
    {
        CGValue arr = gen_expr(g, recv);
        LLVMValueRef len = LLVMBuildExtractValue(g->builder, arr.value, 1, "len");
        out.value = len;
        out.type = type_make(TY_INT);
        return out;
    }
    case KN_BUILTIN_ARRAY_INDEX:
    {
        CGValue arr = gen_expr(g, recv);
        CGValue idx = gen_expr(g, args->items[0]);
        LLVMValueRef idxv = cast_value(g, idx, type_make(TY_I64));
        Type elem = type_make(arr.type.elem);
        elem.name = arr.type.name;
        LLVMTypeRef elem_ty = llvm_type(g, elem);
        LLVMValueRef data = LLVMBuildExtractValue(g->builder, arr.value, 0, "arr.data");
        LLVMValueRef ptr = LLVMBuildInBoundsGEP2(g->builder, elem_ty, data, &idxv, 1, "arr.idx");
        out.value = LLVMBuildLoad2(g->builder, elem_ty, ptr, "arr.val");
        out.type = elem;
        return out;
    }
    case KN_BUILTIN_ARRAY_ADD:
    {
        CGValue arr = gen_expr(g, recv);
        CGValue item = gen_expr(g, args->items[0]);
        out.value = build_array_append_value(g, arr, item);
        out.type = arr.type;
        if (out.type.kind == TY_ARRAY)
            out.type.array_len = out.type.array_len >= 0 ? out.type.array_len + 1 : -1;
        return out;
    }
    case KN_BUILTIN_TYPEOF:
    {
        CGValue rv = gen_expr(g, recv);
        const char *tn = cg_type_name(rv.type);
        out.value = build_string_const(g, tn, (int)kn_strlen(tn));
        out.type = type_make(TY_STRING);
        return out;
    }
    case KN_BUILTIN_BLOCK_RUN:
    case KN_BUILTIN_BLOCK_JUMP:
    case KN_BUILTIN_BLOCK_RUN_UNTIL:
    case KN_BUILTIN_BLOCK_JUMP_AND_RUN_UNTIL:
    {
        CGValue blk = gen_expr(g, recv);
        int mode = 0;
        if (kind == KN_BUILTIN_BLOCK_JUMP) mode = 1;
        else if (kind == KN_BUILTIN_BLOCK_RUN_UNTIL) mode = 2;
        else if (kind == KN_BUILTIN_BLOCK_JUMP_AND_RUN_UNTIL) mode = 3;

        LLVMValueRef a = LLVMConstInt(g->i64, 0, 0);
        LLVMValueRef b = LLVMConstInt(g->i64, 0, 0);
        if (args->count >= 1)
        {
            CGValue av = gen_expr(g, args->items[0]);
            a = cast_value(g, av, type_make(TY_I64));
        }
        if (args->count >= 2)
        {
            CGValue bv = gen_expr(g, args->items[1]);
            b = cast_value(g, bv, type_make(TY_I64));
        }
        call_block_entry(g, blk, LLVMConstInt(g->i64, (uint64_t)mode, 0), a, b);
        maybe_propagate_exception(g);
        out.type = type_make(TY_VOID);
        return out;
    }
    default:
        break;
    }
    return out;
}

static void gen_stmt(Codegen *g, Stmt *s);

static void emit_return_default(Codegen *g)
{
    if (g->gc_frame)
        LLVMBuildCall2(g->builder, g->fn_gc_pop_ty, g->fn_gc_pop, &g->gc_frame, 1, "");
    if (g->current_ret_type.kind == TY_VOID)
        LLVMBuildRetVoid(g->builder);
    else
        LLVMBuildRet(g->builder, LLVMConstNull(llvm_type(g, g->current_ret_type)));
}

static void trace_push_current_func(Codegen *g)
{
    if (!g || !g->fn_exc_push || !g->current_func_name || !g->current_func_name[0]) return;
    LLVMValueRef n = build_string_const(g, g->current_func_name, (int)kn_strlen(g->current_func_name));
    LLVMBuildCall2(g->builder, g->fn_exc_push_ty, g->fn_exc_push, &n, 1, "");
}

static void emit_print_const(Codegen *g, const char *text)
{
    if (!g || !g->fn_print || !text) return;
    LLVMValueRef s = build_string_const(g, text, (int)kn_strlen(text));
    LLVMBuildCall2(g->builder, g->fn_print_ty, g->fn_print, &s, 1, "");
}

static void emit_println_if_nonnull(Codegen *g, LLVMValueRef s, const char *tag)
{
    if (!g || !g->fn_println || !s) return;
    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
    if (!fn) return;

    LLVMValueRef isnull = LLVMBuildICmp(g->builder, LLVMIntEQ, s, LLVMConstNull(g->i8ptr), tag ? tag : "entry.print.null");
    LLVMBasicBlockRef print_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry.print");
    LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry.print.done");
    LLVMBuildCondBr(g->builder, isnull, done_bb, print_bb);

    LLVMPositionBuilderAtEnd(g->builder, print_bb);
    LLVMBuildCall2(g->builder, g->fn_println_ty, g->fn_println, &s, 1, "");
    LLVMBuildBr(g->builder, done_bb);

    LLVMPositionBuilderAtEnd(g->builder, done_bb);
}

static void emit_print_if_nonnull(Codegen *g, LLVMValueRef s, const char *tag)
{
    if (!g || !g->fn_print || !s) return;
    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
    if (!fn) return;

    LLVMValueRef isnull = LLVMBuildICmp(g->builder, LLVMIntEQ, s, LLVMConstNull(g->i8ptr), tag ? tag : "entry.print.null");
    LLVMBasicBlockRef print_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry.print.part");
    LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry.print.part.done");
    LLVMBuildCondBr(g->builder, isnull, done_bb, print_bb);

    LLVMPositionBuilderAtEnd(g->builder, print_bb);
    LLVMBuildCall2(g->builder, g->fn_print_ty, g->fn_print, &s, 1, "");
    LLVMBuildBr(g->builder, done_bb);

    LLVMPositionBuilderAtEnd(g->builder, done_bb);
}

static LLVMValueRef load_error_string_field(Codegen *g, LLVMValueRef obj, int field_index, const char *name)
{
    if (!g || !g->error_class || field_index < 0)
        return LLVMConstNull(g->i8ptr);
    {
        unsigned idx = (unsigned)(1 + field_index);
        LLVMValueRef ptr = LLVMBuildStructGEP2(g->builder, g->error_class->struct_ty, obj, idx, name ? name : "entry.err.str.ptr");
        return LLVMBuildLoad2(g->builder, g->i8ptr, ptr, name ? name : "entry.err.str");
    }
}

static LLVMValueRef load_error_inner_field(Codegen *g, LLVMValueRef obj, int field_index, const char *name)
{
    if (!g || !g->error_class || field_index < 0)
        return LLVMConstNull(g->error_class ? g->error_class->ptr_ty : g->i8ptr);
    {
        unsigned idx = (unsigned)(1 + field_index);
        LLVMValueRef ptr = LLVMBuildStructGEP2(g->builder, g->error_class->struct_ty, obj, idx, name ? name : "entry.err.inner.ptr");
        return LLVMBuildLoad2(g->builder, g->error_class->ptr_ty, ptr, name ? name : "entry.err.inner");
    }
}

static void emit_entry_exception_report(Codegen *g, LLVMValueRef errv)
{
    if (!g || !g->fn_println || !errv) return;

    if (g->error_class)
    {
        LLVMValueRef obj = LLVMBuildBitCast(g->builder, errv, g->error_class->ptr_ty, "entry.err.obj");
        LLVMValueRef title = load_error_string_field(g, obj, g->error_title_index, "entry.err.title");
        LLVMValueRef msg = load_error_string_field(g, obj, g->error_msg_index, "entry.err.msg");
        LLVMValueRef trace = load_error_string_field(g, obj, g->error_trace_index, "entry.err.trace");
        LLVMValueRef inner = load_error_inner_field(g, obj, g->error_inner_index, "entry.err.inner");

        emit_print_const(g, "\x1b[1;38;5;203mUnhandled ");
        emit_print_if_nonnull(g, title, "entry.err.title.null");
        emit_print_const(g, "\x1b[0m");
        emit_print_const(g, ": ");
        emit_print_if_nonnull(g, msg, "entry.err.msg.null");
        emit_print_const(g, "\n");

        {
            LLVMValueRef inner_slot = LLVMBuildAlloca(g->builder, g->error_class->ptr_ty, "entry.err.inner.slot");
            LLVMValueRef loop_init = inner;
            LLVMBuildStore(g->builder, loop_init, inner_slot);

            LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
            LLVMBasicBlockRef loop_cond = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry.err.inner.cond");
            LLVMBasicBlockRef loop_body = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry.err.inner.body");
            LLVMBasicBlockRef loop_done = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry.err.inner.done");

            emit_print_const(g, "\n");
            LLVMBuildBr(g->builder, loop_cond);

            LLVMPositionBuilderAtEnd(g->builder, loop_cond);
            {
                LLVMValueRef cur = LLVMBuildLoad2(g->builder, g->error_class->ptr_ty, inner_slot, "entry.err.inner.cur");
                LLVMValueRef is_null = LLVMBuildICmp(g->builder, LLVMIntEQ, cur, LLVMConstNull(g->error_class->ptr_ty), "entry.err.inner.isnull");
                LLVMBuildCondBr(g->builder, is_null, loop_done, loop_body);
            }

            LLVMPositionBuilderAtEnd(g->builder, loop_body);
            {
                LLVMValueRef cur = LLVMBuildLoad2(g->builder, g->error_class->ptr_ty, inner_slot, "entry.err.inner.body.cur");
                LLVMValueRef cur_title = load_error_string_field(g, cur, g->error_title_index, "entry.err.inner.title");
                LLVMValueRef cur_msg = load_error_string_field(g, cur, g->error_msg_index, "entry.err.inner.msg");
                LLVMValueRef next_inner = load_error_inner_field(g, cur, g->error_inner_index, "entry.err.inner.next");

                emit_print_const(g, "\x1b[38;5;179mCaused by\x1b[0m ");
                emit_print_if_nonnull(g, cur_title, "entry.err.inner.title.null");
                emit_print_const(g, ": ");
                emit_print_if_nonnull(g, cur_msg, "entry.err.inner.msg.null");
                emit_print_const(g, "\n");
                LLVMBuildStore(g->builder, next_inner, inner_slot);
                LLVMBuildBr(g->builder, loop_cond);
            }

            LLVMPositionBuilderAtEnd(g->builder, loop_done);
        }

        emit_print_const(g, "\n\x1b[38;5;81mStack Trace\x1b[0m\n");
        emit_println_if_nonnull(g, trace, "entry.err.trace.null");
        return;
    }

    emit_println_if_nonnull(g, errv, "entry.err.raw.null");
}

static LLVMValueRef finalize_entry_return_code(Codegen *g, LLVMTypeRef t_i32, LLVMValueRef ok_rc)
{
    if (!g || !g->fn_exc_has) return ok_rc;

    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
    if (!fn) return ok_rc;

    LLVMBasicBlockRef noexc_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry.noexc");
    LLVMBasicBlockRef exc_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry.exc");
    LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry.exc.done");

    LLVMValueRef has = LLVMBuildCall2(g->builder, g->fn_exc_has_ty, g->fn_exc_has, 0, 0, "entry.exc.has");
    LLVMValueRef cond = LLVMBuildICmp(g->builder, LLVMIntNE, has, LLVMConstInt(g->i1, 0, 0), "entry.exc.cond");
    LLVMBuildCondBr(g->builder, cond, exc_bb, noexc_bb);

    LLVMPositionBuilderAtEnd(g->builder, noexc_bb);
    LLVMBuildBr(g->builder, done_bb);
    LLVMBasicBlockRef noexc_end = LLVMGetInsertBlock(g->builder);

    LLVMPositionBuilderAtEnd(g->builder, exc_bb);
    LLVMValueRef errv = LLVMConstNull(g->i8ptr);
    if (g->fn_exc_get)
        errv = LLVMBuildCall2(g->builder, g->fn_exc_get_ty, g->fn_exc_get, 0, 0, "entry.exc.get");
    else if (g->fn_exc_last)
        errv = LLVMBuildCall2(g->builder, g->fn_exc_last_ty, g->fn_exc_last, 0, 0, "entry.exc.last");
    emit_entry_exception_report(g, errv);
    if (g->fn_exc_clear)
        LLVMBuildCall2(g->builder, g->fn_exc_clear_ty, g->fn_exc_clear, 0, 0, "");
    LLVMBuildBr(g->builder, done_bb);
    LLVMBasicBlockRef exc_end = LLVMGetInsertBlock(g->builder);

    LLVMPositionBuilderAtEnd(g->builder, done_bb);
    LLVMValueRef phi = LLVMBuildPhi(g->builder, t_i32, "entry.rc");
    LLVMAddIncoming(phi,
        (LLVMValueRef[]){ ok_rc, LLVMConstInt(t_i32, 1, 0) },
        (LLVMBasicBlockRef[]){ noexc_end, exc_end },
        2);
    return phi;
}

static void maybe_propagate_exception(Codegen *g)
{
    if (!g || !g->fn_exc_has) return;
    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
    LLVMBasicBlockRef err_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "exc.err");
    LLVMBasicBlockRef ok_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "exc.ok");
    LLVMValueRef has = LLVMBuildCall2(g->builder, g->fn_exc_has_ty, g->fn_exc_has, 0, 0, "exc.has");
    LLVMValueRef cond = LLVMBuildICmp(g->builder, LLVMIntNE, has, LLVMConstInt(g->i1, 0, 0), "exc.cond");
    LLVMBuildCondBr(g->builder, cond, err_bb, ok_bb);

    LLVMPositionBuilderAtEnd(g->builder, err_bb);
    trace_push_current_func(g);
    LLVMValueRef errv = g->fn_exc_get ? LLVMBuildCall2(g->builder, g->fn_exc_get_ty, g->fn_exc_get, 0, 0, "exc.get") : LLVMConstNull(g->i8ptr);
    update_error_trace_field(g, errv);
    if (g->try_depth > 0)
    {
        int idx = g->try_depth - 1;
        LLVMBuildStore(g->builder, errv, g->try_stack[idx].exc_slot);
        LLVMBuildBr(g->builder, g->try_stack[idx].catch_bb);
    }
    else
    {
        emit_return_default(g);
    }

    LLVMPositionBuilderAtEnd(g->builder, ok_bb);
}

static const char *mangle_helper_name(const char *prefix, const char *name, int id)
{
    char buf[512];
    size_t off = 0;
    kn_append(buf, sizeof(buf), &off, prefix);
    kn_append(buf, sizeof(buf), &off, "_");
    if (name && name[0])
    {
        for (const char *p = name; *p && off + 2 < sizeof(buf); p++)
        {
            char c = *p;
            int ok = (c >= 'a' && c <= 'z') ||
                     (c >= 'A' && c <= 'Z') ||
                     (c >= '0' && c <= '9');
            buf[off++] = ok ? c : '_';
        }
        buf[off] = 0;
    }
    if (id >= 0)
    {
        kn_append(buf, sizeof(buf), &off, "_");
        append_int(buf, sizeof(buf), &off, id);
    }
    char *out = (char *)kn_malloc(off + 1);
    if (!out) return prefix;
    kn_memcpy(out, buf, off + 1);
    return out;
}

// Generate a bridge thunk for a By Kinal delegate.
// The bridge has signature: any(byte* env, any* args, i64 argc)
// where env = raw C function pointer. It unboxes args, calls via env, boxes result.
static LLVMValueRef gen_delegate_bridge(Codegen *g, FuncSymbol *dfs)
{
    if (dfs->delegate_bridge)
        return dfs->delegate_bridge;

    Func *f = dfs->func;
    int argc = f->params.count;

    // Save current builder state
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(g->builder);

    // Create bridge function: any(byte* env, any* args, i64 argc)
    char bridge_name[256];
    size_t off = 0;
    kn_append(bridge_name, sizeof(bridge_name), &off, "__dlg_bridge_");
    if (dfs->qname)
        kn_append(bridge_name, sizeof(bridge_name), &off, dfs->qname);
    // Sanitize name
    for (char *p = bridge_name; *p; p++)
        if (*p == '.') *p = '_';

    LLVMValueRef bridge_fn = LLVMAddFunction(g->mod, bridge_name, g->callable_wrapper_ty);
    LLVMSetLinkage(bridge_fn, LLVMInternalLinkage);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, bridge_fn, "entry");
    LLVMPositionBuilderAtEnd(g->builder, entry);

    LLVMValueRef env_param = LLVMGetParam(bridge_fn, 0);   // byte* env = raw C fn ptr
    LLVMValueRef args_param = LLVMGetParam(bridge_fn, 1);   // any* args
    (void)LLVMGetParam(bridge_fn, 2);                       // i64 argc (unused, we know the count)

    // Bitcast env to the actual C function type
    LLVMValueRef c_fn = LLVMBuildBitCast(g->builder, env_param, LLVMPointerType(dfs->fn_ty, 0), "bridge.fn");

    // Unbox each arg from the any* array
    LLVMValueRef *call_args = 0;
    if (argc > 0)
        call_args = (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)argc);
    for (int i = 0; i < argc; i++)
    {
        LLVMValueRef idx = LLVMConstInt(g->i64, (uint64_t)i, 0);
        LLVMValueRef arg_ptr = LLVMBuildInBoundsGEP2(g->builder, g->any_ty, args_param, &idx, 1, "bridge.arg.ptr");
        LLVMValueRef anyv = LLVMBuildLoad2(g->builder, g->any_ty, arg_ptr, "bridge.arg.any");
        call_args[i] = any_to_type(g, anyv, f->params.items[i].type);
    }

    // Call the C function
    if (f->ret_type.kind == TY_VOID)
    {
        LLVMBuildCall2(g->builder, dfs->fn_ty, c_fn, call_args, (unsigned)argc, "");
        LLVMBuildRet(g->builder, any_null_value(g));
    }
    else
    {
        LLVMValueRef result = LLVMBuildCall2(g->builder, dfs->fn_ty, c_fn, call_args, (unsigned)argc, "bridge.call");
        CGValue rv;
        rv.value = result;
        rv.type = f->ret_type;
        LLVMBuildRet(g->builder, box_any_value(g, rv));
    }

    // Restore builder position
    if (saved_bb)
        LLVMPositionBuilderAtEnd(g->builder, saved_bb);

    dfs->delegate_bridge = bridge_fn;
    return bridge_fn;
}

static LLVMValueRef load_callable_invoke_ptr(Codegen *g, CGValue fnobj)
{
    if (!g)
        return LLVMConstNull(g->i8ptr);
    if (fnobj.type.kind == TY_CLASS && fnobj.type.name &&
        kn_strcmp(fnobj.type.name, "IO.Type.Object.Block") == 0)
    {
        if (!g->pseudo_block_ty || !g->pseudo_block_ptr_ty)
            return LLVMConstNull(g->i8ptr);
        LLVMValueRef bobj = LLVMBuildBitCast(g->builder, fnobj.value, g->pseudo_block_ptr_ty, "blk.obj");
        LLVMValueRef ptr = LLVMBuildStructGEP2(g->builder, g->pseudo_block_ty, bobj, 1, "blk.invoke.ptr");
        return LLVMBuildLoad2(g->builder, g->i8ptr, ptr, "blk.invoke");
    }
    ClassSymbol *carrier = g->function_class;
    int invoke_index = g->function_invoke_index;
    if (!carrier || invoke_index < 0)
        return LLVMConstNull(g->i8ptr);
    LLVMValueRef fobj = LLVMBuildBitCast(g->builder, fnobj.value, carrier->ptr_ty, "fn.obj");
    unsigned idx = (unsigned)(1 + invoke_index);
    LLVMValueRef ptr = LLVMBuildStructGEP2(g->builder, carrier->struct_ty, fobj, idx, "fn.invoke.ptr");
    return LLVMBuildLoad2(g->builder, g->i8ptr, ptr, "fn.invoke");
}

static LLVMValueRef load_callable_env_ptr(Codegen *g, CGValue fnobj)
{
    if (!g)
        return LLVMConstNull(g->i8ptr);
    if (fnobj.type.kind == TY_CLASS && fnobj.type.name &&
        kn_strcmp(fnobj.type.name, "IO.Type.Object.Block") == 0)
    {
        if (!g->pseudo_block_ty || !g->pseudo_block_ptr_ty)
            return LLVMConstNull(g->i8ptr);
        LLVMValueRef bobj = LLVMBuildBitCast(g->builder, fnobj.value, g->pseudo_block_ptr_ty, "blk.obj.env");
        LLVMValueRef ptr = LLVMBuildStructGEP2(g->builder, g->pseudo_block_ty, bobj, 2, "blk.env.ptr");
        return LLVMBuildLoad2(g->builder, g->i8ptr, ptr, "blk.env");
    }
    ClassSymbol *carrier = g->function_class;
    int env_index = g->function_env_index;
    if (!carrier || env_index < 0)
        return LLVMConstNull(g->i8ptr);
    LLVMValueRef fobj = LLVMBuildBitCast(g->builder, fnobj.value, carrier->ptr_ty, "fn.obj.env");
    unsigned idx = (unsigned)(1 + env_index);
    LLVMValueRef ptr = LLVMBuildStructGEP2(g->builder, carrier->struct_ty, fobj, idx, "fn.env.ptr");
    return LLVMBuildLoad2(g->builder, g->i8ptr, ptr, "fn.env");
}

static void call_block_entry(Codegen *g, CGValue blkobj, LLVMValueRef mode, LLVMValueRef a, LLVMValueRef b)
{
    if (!g) return;
    LLVMValueRef invoke_ptr = load_callable_invoke_ptr(g, blkobj);
    LLVMValueRef env_ptr = load_callable_env_ptr(g, blkobj);
    LLVMValueRef is_null = LLVMBuildICmp(g->builder, LLVMIntEQ, invoke_ptr, LLVMConstNull(g->i8ptr), "blk.invoke.null");
    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
    LLVMBasicBlockRef null_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "blk.null");
    LLVMBasicBlockRef call_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "blk.call");
    LLVMBasicBlockRef cont_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "blk.cont");
    LLVMBuildCondBr(g->builder, is_null, null_bb, call_bb);

    LLVMPositionBuilderAtEnd(g->builder, null_bb);
    LLVMBuildBr(g->builder, cont_bb);

    LLVMPositionBuilderAtEnd(g->builder, call_bb);
    LLVMTypeRef args[4] = { g->i8ptr, g->i64, g->i64, g->i64 };
    LLVMTypeRef body_ty = LLVMFunctionType(g->voidt, args, 4, 0);
    LLVMValueRef body_ptr = LLVMBuildBitCast(g->builder, invoke_ptr, LLVMPointerType(body_ty, 0), "blk.body");
    LLVMBuildCall2(g->builder, body_ty, body_ptr, (LLVMValueRef[]){ env_ptr, mode, a, b }, 4, "");
    LLVMBuildBr(g->builder, cont_bb);

    LLVMPositionBuilderAtEnd(g->builder, cont_bb);
}

static LLVMValueRef call_callable_wrapper(Codegen *g, CGValue fnobj, LLVMValueRef args_ptr, LLVMValueRef argc)
{
    LLVMValueRef invoke_ptr = load_callable_invoke_ptr(g, fnobj);
    LLVMValueRef env_ptr = load_callable_env_ptr(g, fnobj);
    LLVMValueRef is_null = LLVMBuildICmp(g->builder, LLVMIntEQ, invoke_ptr, LLVMConstNull(g->i8ptr), "fn.invoke.null");
    LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
    LLVMBasicBlockRef null_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "fn.null");
    LLVMBasicBlockRef call_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "fn.call");
    LLVMBasicBlockRef cont_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "fn.cont");
    LLVMBuildCondBr(g->builder, is_null, null_bb, call_bb);

    LLVMPositionBuilderAtEnd(g->builder, null_bb);
    LLVMValueRef nullv = any_null_value(g);
    LLVMBuildBr(g->builder, cont_bb);

    LLVMPositionBuilderAtEnd(g->builder, call_bb);
    LLVMValueRef fn_ptr = LLVMBuildBitCast(g->builder, invoke_ptr, LLVMPointerType(g->callable_wrapper_ty, 0), "fn.wrap");
    LLVMValueRef cv = LLVMBuildCall2(g->builder, g->callable_wrapper_ty, fn_ptr,
                                     (LLVMValueRef[]){ env_ptr, args_ptr, argc }, 3, "fn.invoke.call");
    LLVMBuildBr(g->builder, cont_bb);

    LLVMPositionBuilderAtEnd(g->builder, cont_bb);
    LLVMValueRef phi = LLVMBuildPhi(g->builder, g->any_ty, "fn.invoke.res");
    LLVMValueRef vals[2] = { nullv, cv };
    LLVMBasicBlockRef bbs[2] = { null_bb, call_bb };
    LLVMAddIncoming(phi, vals, bbs, 2);
    return phi;
}

static LLVMValueRef build_callable_object(Codegen *g, const char *class_name, LLVMValueRef invoke_ptr, LLVMValueRef env_ptr)
{
    if (class_name && kn_strcmp(class_name, "IO.Type.Object.Block") == 0)
        return build_pseudo_block_object(g, invoke_ptr, env_ptr, SAFETY_SAFE);

    ClassSymbol *cs = classtable_find(&g->classes, class_name);
    if (!cs) return LLVMConstNull(g->i8ptr);

    LLVMValueRef sizev = LLVMSizeOf(cs->struct_ty);
    if (LLVMGetTypeKind(LLVMTypeOf(sizev)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(sizev)) != 64)
        sizev = LLVMBuildZExtOrBitCast(g->builder, sizev, g->i64, "fobjsz");
    LLVMValueRef mem = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc,
                                      (LLVMValueRef[]){ sizev }, 1, "fobj.mem");
    LLVMValueRef obj = LLVMBuildBitCast(g->builder, mem, cs->ptr_ty, "fobj");

    ClassSymbol *root = class_root(g, cs);
    LLVMValueRef root_ptr = LLVMBuildBitCast(g->builder, obj, root->ptr_ty, "fobj.root");
    LLVMValueRef vptr_ptr = LLVMBuildStructGEP2(g->builder, root->struct_ty, root_ptr, 0, "fobj.vptr");
    LLVMValueRef vtbl_ptr = LLVMConstNull(LLVMPointerType(g->i8ptr, 0));
    if (cs->vtable_global)
    {
        LLVMTypeRef vtbl_ty = LLVMGetElementType(LLVMTypeOf(cs->vtable_global));
        LLVMValueRef zero = LLVMConstInt(g->i32, 0, 0);
        LLVMValueRef idxs[2] = { zero, zero };
        vtbl_ptr = LLVMConstInBoundsGEP2(vtbl_ty, cs->vtable_global, idxs, 2);
    }
    LLVMBuildStore(g->builder, vtbl_ptr, vptr_ptr);
    init_class_array_fields(g, cs, obj);

    ClassSymbol *carrier = 0;
    int invoke_index = -1;
    int env_index = -1;
    if (class_name && kn_strcmp(class_name, "IO.Type.Object.Block") == 0)
    {
        carrier = g->block_class;
        invoke_index = g->block_invoke_index;
        env_index = g->block_env_index;
    }
    else
    {
        carrier = g->function_class;
        invoke_index = g->function_invoke_index;
        env_index = g->function_env_index;
    }
    if (carrier && invoke_index >= 0 && env_index >= 0)
    {
        LLVMValueRef fptr = LLVMBuildBitCast(g->builder, obj, carrier->ptr_ty, "fobj.base");
        unsigned inv_idx = (unsigned)(1 + invoke_index);
        unsigned env_idx = (unsigned)(1 + env_index);
        LLVMValueRef inv_ptr = LLVMBuildStructGEP2(g->builder, carrier->struct_ty, fptr, inv_idx, "fobj.inv.ptr");
        LLVMValueRef envp_ptr = LLVMBuildStructGEP2(g->builder, carrier->struct_ty, fptr, env_idx, "fobj.env.ptr");
        LLVMBuildStore(g->builder, invoke_ptr ? invoke_ptr : LLVMConstNull(g->i8ptr), inv_ptr);
        LLVMBuildStore(g->builder, env_ptr ? env_ptr : LLVMConstNull(g->i8ptr), envp_ptr);
    }

    return obj;
}

static LLVMValueRef build_pseudo_block_object(Codegen *g, LLVMValueRef invoke_ptr, LLVMValueRef env_ptr, Safety safety)
{
    if (!g || !g->block_class || !g->pseudo_block_ty || !g->pseudo_block_ptr_ty)
        return LLVMConstNull(g ? g->i8ptr : LLVMPointerType(LLVMInt8Type(), 0));

    LLVMValueRef sizev = LLVMSizeOf(g->pseudo_block_ty);
    if (LLVMGetTypeKind(LLVMTypeOf(sizev)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(sizev)) != 64)
        sizev = LLVMBuildZExtOrBitCast(g->builder, sizev, g->i64, "blkobj.sz");
    LLVMValueRef mem = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc,
                                      (LLVMValueRef[]){ sizev }, 1, "blkobj.mem");
    LLVMValueRef obj = LLVMBuildBitCast(g->builder, mem, g->pseudo_block_ptr_ty, "blkobj");

    ClassSymbol *root = class_root(g, g->block_class);
    LLVMValueRef root_ptr = LLVMBuildBitCast(g->builder, obj, root->ptr_ty, "blkobj.root");
    LLVMValueRef vptr_ptr = LLVMBuildStructGEP2(g->builder, root->struct_ty, root_ptr, 0, "blkobj.vptr");
    LLVMValueRef vtbl_ptr = LLVMConstNull(LLVMPointerType(g->i8ptr, 0));
    if (g->block_class->vtable_global)
    {
        LLVMTypeRef vtbl_ty = LLVMGetElementType(LLVMTypeOf(g->block_class->vtable_global));
        LLVMValueRef zero = LLVMConstInt(g->i32, 0, 0);
        LLVMValueRef idxs[2] = { zero, zero };
        vtbl_ptr = LLVMConstInBoundsGEP2(vtbl_ty, g->block_class->vtable_global, idxs, 2);
    }
    LLVMBuildStore(g->builder, vtbl_ptr, vptr_ptr);

    LLVMValueRef inv_ptr = LLVMBuildStructGEP2(g->builder, g->pseudo_block_ty, obj, 1, "blkobj.inv.ptr");
    LLVMValueRef envp_ptr = LLVMBuildStructGEP2(g->builder, g->pseudo_block_ty, obj, 2, "blkobj.env.ptr");
    LLVMValueRef flags_ptr = LLVMBuildStructGEP2(g->builder, g->pseudo_block_ty, obj, 3, "blkobj.flags.ptr");
    LLVMBuildStore(g->builder, invoke_ptr ? invoke_ptr : LLVMConstNull(g->i8ptr), inv_ptr);
    LLVMBuildStore(g->builder, env_ptr ? env_ptr : LLVMConstNull(g->i8ptr), envp_ptr);
    LLVMBuildStore(g->builder, LLVMConstInt(g->i64, (uint64_t)safety, 0), flags_ptr);

    return LLVMBuildBitCast(g->builder, obj, g->block_class->ptr_ty, "blkobj.as.class");
}

static LLVMValueRef ensure_func_wrapper(Codegen *g, FuncSymbol *fs)
{
    if (!g || !fs) return 0;
    if (fs->wrapper_fn) return fs->wrapper_fn;

    const char *wname = mangle_helper_name("__kn_fnwrap", fs->qname, -1);
    LLVMValueRef wrapper = LLVMAddFunction(g->mod, wname, g->callable_wrapper_ty);
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(g->builder);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, wrapper, "entry");
    LLVMPositionBuilderAtEnd(g->builder, entry);

    LLVMValueRef args_ptr = LLVMGetParam(wrapper, 1);
    LLVMValueRef argc = LLVMGetParam(wrapper, 2);
    LLVMValueRef expect = LLVMConstInt(g->i64, (uint64_t)fs->func->params.count, 0);
    LLVMValueRef ok = LLVMBuildICmp(g->builder, LLVMIntUGE, argc, expect, "argc.ok");
    LLVMBasicBlockRef ok_bb = LLVMAppendBasicBlockInContext(g->ctx, wrapper, "ok");
    LLVMBasicBlockRef bad_bb = LLVMAppendBasicBlockInContext(g->ctx, wrapper, "bad");
    LLVMBuildCondBr(g->builder, ok, ok_bb, bad_bb);

    LLVMPositionBuilderAtEnd(g->builder, bad_bb);
    LLVMBuildRet(g->builder, any_null_value(g));

    LLVMPositionBuilderAtEnd(g->builder, ok_bb);
    int argc_count = fs->func->params.count;
    LLVMValueRef *argv = argc_count > 0 ? (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)argc_count) : 0;
    for (int i = 0; i < argc_count; i++)
    {
        LLVMValueRef idx = LLVMConstInt(g->i64, (uint64_t)i, 0);
        LLVMValueRef ptr = LLVMBuildInBoundsGEP2(g->builder, g->any_ty, args_ptr, &idx, 1, "arg.ptr");
        LLVMValueRef anyv = LLVMBuildLoad2(g->builder, g->any_ty, ptr, "arg.any");
        argv[i] = any_to_type(g, anyv, fs->func->params.items[i].type);
    }
    Type public_ret = cg_func_public_ret_type(fs->func);
    LLVMValueRef callv = LLVMBuildCall2(g->builder, fs->fn_ty, fs->fn, argv, (unsigned)argc_count,
                                        public_ret.kind == TY_VOID ? "" : "fn.call");
    if (fs->func->is_extern && fs->func->extern_abi == KN_EXTERN_ABI_SYSTEM && g->target_is_windows && g->target_is_x86)
        LLVMSetInstructionCallConv(callv, LLVMX86StdcallCallConv);
    if (public_ret.kind == TY_VOID)
    {
        LLVMBuildRet(g->builder, any_null_value(g));
    }
    else
    {
        CGValue rv;
        rv.value = callv;
        rv.type = public_ret;
        LLVMBuildRet(g->builder, box_any_value(g, rv));
    }

    if (saved_bb)
        LLVMPositionBuilderAtEnd(g->builder, saved_bb);

    fs->wrapper_fn = wrapper;
    fs->wrapper_fn_ty = g->callable_wrapper_ty;
    return wrapper;
}

static bool closure_capture_has(ClosureInfo *ci, const char *name)
{
    if (!ci || !name) return false;
    for (int i = 0; i < ci->capture_count; i++)
        if (kn_strcmp(ci->capture_names[i], name) == 0)
            return true;
    return false;
}

static void closure_capture_add(ClosureInfo *ci, const char *name, Type type)
{
    if (!ci || !name) return;
    if (closure_capture_has(ci, name)) return;
    if (ci->capture_count + 1 > ci->capture_cap)
    {
        int new_cap = ci->capture_cap ? ci->capture_cap * 2 : 8;
        const char **names = (const char **)kn_malloc(sizeof(char *) * (size_t)new_cap);
        Type *types = (Type *)kn_malloc(sizeof(Type) * (size_t)new_cap);
        if (ci->capture_names)
        {
            kn_memcpy((void *)names, ci->capture_names, sizeof(char *) * (size_t)ci->capture_count);
            kn_free((void *)ci->capture_names);
        }
        if (ci->capture_types)
        {
            kn_memcpy(types, ci->capture_types, sizeof(Type) * (size_t)ci->capture_count);
            kn_free(ci->capture_types);
        }
        ci->capture_names = names;
        ci->capture_types = types;
        ci->capture_cap = new_cap;
    }
    ci->capture_names[ci->capture_count] = name;
    ci->capture_types[ci->capture_count] = type;
    ci->capture_count++;
}

static void closure_collect_scope_captures(Codegen *g, ClosureInfo *ci)
{
    if (!g || !ci) return;
    for (Scope *sc = g->scope; sc; sc = sc->parent)
    {
        if (sc == g->global_scope)
            break;
        for (VarSymbol *v = sc->vars; v; v = v->next)
            closure_capture_add(ci, v->name, v->type);
    }
}

static LLVMValueRef build_closure_env(Codegen *g, ClosureInfo *ci)
{
    int total = 0;
    if (!g || !ci)
        return LLVMConstNull(g->i8ptr);
    total = ci->capture_count + (ci->capture_this ? 1 : 0);
    if (total <= 0)
        return LLVMConstNull(g->i8ptr);

    LLVMValueRef elem_sz = LLVMSizeOf(g->i8ptr);
    if (LLVMGetTypeKind(LLVMTypeOf(elem_sz)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(elem_sz)) != 64)
        elem_sz = LLVMBuildZExtOrBitCast(g->builder, elem_sz, g->i64, "env.esz");
    LLVMValueRef cnt = LLVMConstInt(g->i64, (uint64_t)total, 0);
    LLVMValueRef bytes = LLVMBuildMul(g->builder, cnt, elem_sz, "env.bytes");
    LLVMValueRef raw = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc, &bytes, 1, "env.mem");
    LLVMTypeRef arr_ty = LLVMPointerType(g->i8ptr, 0);
    LLVMValueRef arr = LLVMBuildBitCast(g->builder, raw, arr_ty, "env.arr");
    int base = 0;
    if (ci->capture_this)
    {
        LLVMValueRef slot = LLVMBuildInBoundsGEP2(g->builder, g->i8ptr, arr,
                                                  (LLVMValueRef[]){ LLVMConstInt(g->i64, 0, 0) }, 1, "env.this.slot");
        LLVMValueRef ptr = g->current_this ? LLVMBuildBitCast(g->builder, g->current_this, g->i8ptr, "env.this")
                                           : LLVMConstNull(g->i8ptr);
        LLVMBuildStore(g->builder, ptr, slot);
        base = 1;
    }
    for (int i = 0; i < ci->capture_count; i++)
    {
        VarSymbol *v = scope_find(g->scope, ci->capture_names[i]);
        LLVMValueRef slot = LLVMBuildInBoundsGEP2(g->builder, g->i8ptr, arr,
                                                  (LLVMValueRef[]){ LLVMConstInt(g->i64, (uint64_t)(i + base), 0) }, 1, "env.slot");
        LLVMValueRef ptr = v ? LLVMBuildBitCast(g->builder, v->value, g->i8ptr, "env.cap.ptr") : LLVMConstNull(g->i8ptr);
        LLVMBuildStore(g->builder, ptr, slot);
    }
    return raw;
}

static void bind_closure_captures(Codegen *g, ClosureInfo *ci, Scope *sc, LLVMValueRef env_raw)
{
    int base = 0;
    if (!g || !ci || !sc) return;
    LLVMTypeRef arr_ty = LLVMPointerType(g->i8ptr, 0);
    LLVMValueRef arr = LLVMBuildBitCast(g->builder, env_raw, arr_ty, "cap.arr");
    if (ci->capture_this && ci->lexical_class)
    {
        ClassSymbol *cs = classtable_find(&g->classes, cg_class_qname(ci->lexical_class));
        if (!cs) kn_die("unknown closure this class");
        LLVMValueRef slot = LLVMBuildInBoundsGEP2(g->builder, g->i8ptr, arr,
                                                  (LLVMValueRef[]){ LLVMConstInt(g->i64, 0, 0) }, 1, "cap.this.slot");
        LLVMValueRef raw = LLVMBuildLoad2(g->builder, g->i8ptr, slot, "cap.this.raw");
        g->current_this = LLVMBuildBitCast(g->builder, raw, cs->ptr_ty, "cap.this");
        base = 1;
    }
    for (int i = 0; i < ci->capture_count; i++)
    {
        LLVMValueRef slot = LLVMBuildInBoundsGEP2(g->builder, g->i8ptr, arr,
                                                  (LLVMValueRef[]){ LLVMConstInt(g->i64, (uint64_t)(i + base), 0) }, 1, "cap.slot");
        LLVMValueRef raw = LLVMBuildLoad2(g->builder, g->i8ptr, slot, "cap.raw");
        LLVMTypeRef ptr_ty = LLVMPointerType(llvm_type(g, ci->capture_types[i]), 0);
        LLVMValueRef typed = LLVMBuildBitCast(g->builder, raw, ptr_ty, "cap.typed");
        scope_add(sc, ci->capture_names[i], ci->capture_types[i], typed);
    }
}

static LLVMBasicBlockRef runtime_block_bb_for_ip(Codegen *g, int ip)
{
    if (!g || !g->runtime_block_records || !g->runtime_record_blocks) return 0;
    for (int i = 0; i < g->runtime_record_count; i++)
    {
        if (g->runtime_block_records->items[i].ip == ip)
            return g->runtime_record_blocks[i];
    }
    return 0;
}

static void emit_runtime_block_dispatch(Codegen *g, LLVMValueRef ip)
{
    if (!g) return;
    if (g->runtime_block_use_indirect && g->runtime_dispatch_table && g->runtime_dispatch_table_ty)
    {
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
        LLVMBasicBlockRef ok_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "blk.indirect.ok");
        LLVMBasicBlockRef bad_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "blk.indirect.bad");
        LLVMValueRef zero = LLVMConstInt(g->i64, 0, 0);
        LLVMValueRef max_ip = LLVMConstInt(g->i64, (uint64_t)g->runtime_record_count, 0);
        LLVMValueRef ge_zero = LLVMBuildICmp(g->builder, LLVMIntSGE, ip, zero, "blk.ip.ge0");
        LLVMValueRef le_max = LLVMBuildICmp(g->builder, LLVMIntULE, ip, max_ip, "blk.ip.lemax");
        LLVMValueRef ok = LLVMBuildAnd(g->builder, ge_zero, le_max, "blk.ip.ok");
        LLVMBuildCondBr(g->builder, ok, ok_bb, bad_bb);

        LLVMPositionBuilderAtEnd(g->builder, ok_bb);
        LLVMValueRef idxs[2] = { zero, ip };
        LLVMValueRef ptr = LLVMBuildInBoundsGEP2(g->builder, g->runtime_dispatch_table_ty,
                                                 g->runtime_dispatch_table, idxs, 2, "blk.dispatch.ptr");
        LLVMValueRef addr = LLVMBuildLoad2(g->builder, g->i8ptr, ptr, "blk.dispatch.addr");
        LLVMValueRef indbr = LLVMBuildIndirectBr(g->builder, addr, (unsigned)(1 + g->runtime_record_count));
        LLVMAddDestination(indbr, g->runtime_block_start);
        for (int i = 0; i < g->runtime_record_count; i++)
            LLVMAddDestination(indbr, g->runtime_record_blocks[i]);

        LLVMPositionBuilderAtEnd(g->builder, bad_bb);
        LLVMBuildBr(g->builder, g->runtime_block_done);
        return;
    }
    LLVMValueRef sw = LLVMBuildSwitch(g->builder, ip, g->runtime_block_done, (unsigned)(1 + g->runtime_record_count));
    LLVMAddCase(sw, LLVMConstInt(g->i64, 0, 0), g->runtime_block_start);
    for (int i = 0; i < g->runtime_record_count; i++)
    {
        LLVMAddCase(sw, LLVMConstInt(g->i64, (uint64_t)g->runtime_block_records->items[i].ip, 0), g->runtime_record_blocks[i]);
    }
}

static LLVMValueRef emit_lambda_body_function(Codegen *g, ClosureInfo *ci, Expr *e)
{
    const char *name = mangle_helper_name("__kn_lambda_body", "L", e->v.anon_func.id);
    int argc = 1 + e->v.anon_func.param_count;
    LLVMTypeRef *args = (LLVMTypeRef *)kn_malloc(sizeof(LLVMTypeRef) * (size_t)argc);
    args[0] = g->i8ptr;
    for (int i = 0; i < e->v.anon_func.param_count; i++)
        args[1 + i] = llvm_type(g, e->v.anon_func.params[i].type);
    LLVMTypeRef fn_ty = LLVMFunctionType(llvm_type(g, e->v.anon_func.ret_type), args, (unsigned)argc, 0);
    LLVMValueRef fn = LLVMAddFunction(g->mod, name, fn_ty);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry");
    LLVMPositionBuilderAtEnd(g->builder, entry);

    Scope *saved_scope = g->scope;
    Type saved_ret = g->current_ret_type;
    LLVMValueRef saved_gc = g->gc_frame;
    int saved_loop = g->loop_depth;
    int saved_try = g->try_depth;
    const char *saved_name = g->current_func_name;
    ClassDecl *saved_class = g->current_class;
    LLVMValueRef saved_this = g->current_this;

    g->scope = 0;
    g->current_ret_type = e->v.anon_func.ret_type;
    g->loop_depth = 0;
    g->try_depth = 0;
    g->current_class = ci->lexical_class;
    g->current_this = 0;
    {
        char trace_name[64];
        size_t off = 0;
        kn_append(trace_name, sizeof(trace_name), &off, "<lambda#");
        append_int(trace_name, sizeof(trace_name), &off, e->v.anon_func.id);
        kn_append(trace_name, sizeof(trace_name), &off, ">");
        g->current_func_name = build_trace_label(trace_name, 0, 0, 0);
    }
    g->gc_frame = 0;
    if (g->fn_gc_push)
        g->gc_frame = LLVMBuildCall2(g->builder, g->fn_gc_push_ty, g->fn_gc_push, 0, 0, "gc.frame");

    Scope sc;
    sc.vars = 0;
    sc.parent = g->global_scope;
    g->scope = &sc;

    bind_closure_captures(g, ci, &sc, LLVMGetParam(fn, 0));

    for (int i = 0; i < e->v.anon_func.param_count; i++)
    {
        Param *p = &e->v.anon_func.params[i];
        LLVMValueRef pv = LLVMGetParam(fn, (unsigned)(1 + i));
        LLVMValueRef alloca = build_entry_alloca(g, fn, llvm_type(g, p->type), p->name);
        LLVMBuildStore(g->builder, pv, alloca);
        scope_add(&sc, p->name, p->type, alloca);
        gc_add_root(g, alloca, p->type);
    }

    gen_stmt(g, e->v.anon_func.body);
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(g->builder)))
    {
        if (e->v.anon_func.ret_type.kind == TY_VOID)
        {
            if (g->gc_frame)
                LLVMBuildCall2(g->builder, g->fn_gc_pop_ty, g->fn_gc_pop, &g->gc_frame, 1, "");
            LLVMBuildRetVoid(g->builder);
        }
        else
        {
            if (g->gc_frame)
                LLVMBuildCall2(g->builder, g->fn_gc_pop_ty, g->fn_gc_pop, &g->gc_frame, 1, "");
            LLVMBuildRet(g->builder, LLVMConstNull(llvm_type(g, e->v.anon_func.ret_type)));
        }
    }

    g->scope = saved_scope;
    g->current_ret_type = saved_ret;
    g->gc_frame = saved_gc;
    g->loop_depth = saved_loop;
    g->try_depth = saved_try;
    g->current_func_name = saved_name;
    g->current_class = saved_class;
    g->current_this = saved_this;
    return fn;
}

static LLVMValueRef emit_block_body_function(Codegen *g, ClosureInfo *ci, Expr *e)
{
    const char *name = mangle_helper_name("__kn_block_body", "B", e->v.block_lit.id);
    LLVMTypeRef args[4] = { g->i8ptr, g->i64, g->i64, g->i64 };
    LLVMTypeRef fn_ty = LLVMFunctionType(g->voidt, args, 4, 0);
    LLVMValueRef fn = LLVMAddFunction(g->mod, name, fn_ty);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, fn, "entry");
    LLVMPositionBuilderAtEnd(g->builder, entry);

    Scope *saved_scope = g->scope;
    Type saved_ret = g->current_ret_type;
    LLVMValueRef saved_gc = g->gc_frame;
    int saved_loop = g->loop_depth;
    int saved_try = g->try_depth;
    const char *saved_name = g->current_func_name;
    int saved_in_rt = g->in_runtime_block;
    BlockRecordList *saved_records = g->runtime_block_records;
    LLVMBasicBlockRef *saved_bbs = g->runtime_record_blocks;
    int saved_bb_count = g->runtime_record_count;
    LLVMBasicBlockRef saved_start = g->runtime_block_start;
    LLVMBasicBlockRef saved_done = g->runtime_block_done;
    int saved_use_indirect = g->runtime_block_use_indirect;
    LLVMValueRef saved_dispatch_table = g->runtime_dispatch_table;
    LLVMTypeRef saved_dispatch_table_ty = g->runtime_dispatch_table_ty;
    LLVMValueRef saved_until_enabled = g->runtime_until_enabled;
    LLVMValueRef saved_until_ip = g->runtime_until_ip;
    ClassDecl *saved_class = g->current_class;
    LLVMValueRef saved_this = g->current_this;

    g->scope = 0;
    g->current_ret_type = type_make(TY_VOID);
    g->loop_depth = 0;
    g->try_depth = 0;
    {
        char trace_name[64];
        size_t off = 0;
        kn_append(trace_name, sizeof(trace_name), &off, "<block#");
        append_int(trace_name, sizeof(trace_name), &off, e->v.block_lit.id);
        kn_append(trace_name, sizeof(trace_name), &off, ">");
        g->current_func_name = build_trace_label(trace_name, 0, 0, 0);
    }
    g->current_class = ci->lexical_class;
    g->current_this = 0;
    g->gc_frame = 0;
    if (g->fn_gc_push)
        g->gc_frame = LLVMBuildCall2(g->builder, g->fn_gc_push_ty, g->fn_gc_push, 0, 0, "gc.frame");

    Scope sc;
    sc.vars = 0;
    sc.parent = g->global_scope;
    g->scope = &sc;
    bind_closure_captures(g, ci, &sc, LLVMGetParam(fn, 0));

    LLVMValueRef mode = LLVMGetParam(fn, 1);
    LLVMValueRef arg_a = LLVMGetParam(fn, 2);
    LLVMValueRef arg_b = LLVMGetParam(fn, 3);
    LLVMValueRef mode_is_jump = LLVMBuildOr(g->builder,
        LLVMBuildICmp(g->builder, LLVMIntEQ, mode, LLVMConstInt(g->i64, 1, 0), "m.jump"),
        LLVMBuildICmp(g->builder, LLVMIntEQ, mode, LLVMConstInt(g->i64, 3, 0), "m.jump2"),
        "m.anyjump");
    LLVMValueRef start_ip = LLVMBuildSelect(g->builder, mode_is_jump, arg_a, LLVMConstInt(g->i64, 0, 0), "blk.start.ip");
    LLVMValueRef until_enabled = LLVMBuildOr(g->builder,
        LLVMBuildICmp(g->builder, LLVMIntEQ, mode, LLVMConstInt(g->i64, 2, 0), "m.until"),
        LLVMBuildICmp(g->builder, LLVMIntEQ, mode, LLVMConstInt(g->i64, 3, 0), "m.until2"),
        "m.anyuntil");
    LLVMValueRef until_ip = LLVMBuildSelect(g->builder,
        LLVMBuildICmp(g->builder, LLVMIntEQ, mode, LLVMConstInt(g->i64, 3, 0), "m.until.b"),
        arg_b, arg_a, "blk.until.ip");

    g->in_runtime_block = 1;
    g->runtime_block_records = &e->v.block_lit.records;
    g->runtime_record_count = e->v.block_lit.records.count;
    g->runtime_record_blocks = e->v.block_lit.records.count > 0
        ? (LLVMBasicBlockRef *)kn_malloc(sizeof(LLVMBasicBlockRef) * (size_t)e->v.block_lit.records.count)
        : 0;
    g->runtime_block_start = LLVMAppendBasicBlockInContext(g->ctx, fn, "blk.start");
    g->runtime_block_done = LLVMAppendBasicBlockInContext(g->ctx, fn, "blk.done");
    g->runtime_block_use_indirect = (e->v.block_lit.safety == SAFETY_UNSAFE);
    g->runtime_dispatch_table = 0;
    g->runtime_dispatch_table_ty = 0;
    g->runtime_until_enabled = until_enabled;
    g->runtime_until_ip = until_ip;
    for (int i = 0; i < e->v.block_lit.records.count; i++)
    {
        const char *rname = mangle_helper_name("blk.rec", e->v.block_lit.records.items[i].name, e->v.block_lit.records.items[i].ip);
        g->runtime_record_blocks[i] = LLVMAppendBasicBlockInContext(g->ctx, fn, rname);
    }

    if (g->runtime_block_use_indirect)
    {
        unsigned slot_count = (unsigned)(1 + e->v.block_lit.records.count);
        g->runtime_dispatch_table_ty = LLVMArrayType(g->i8ptr, slot_count);
        g->runtime_dispatch_table = build_entry_alloca(g, fn, g->runtime_dispatch_table_ty, "blk.dispatch.table");

        LLVMValueRef zero64 = LLVMConstInt(g->i64, 0, 0);
        LLVMValueRef start_ptr = LLVMBuildInBoundsGEP2(g->builder, g->runtime_dispatch_table_ty, g->runtime_dispatch_table,
                                                       (LLVMValueRef[]){ zero64, zero64 }, 2, "blk.dispatch.start.ptr");
        LLVMValueRef start_addr = LLVMBlockAddress(fn, g->runtime_block_start);
        if (LLVMTypeOf(start_addr) != g->i8ptr)
            start_addr = LLVMConstBitCast(start_addr, g->i8ptr);
        LLVMBuildStore(g->builder, start_addr, start_ptr);
        for (int i = 0; i < e->v.block_lit.records.count; i++)
        {
            uint64_t ip_slot = (uint64_t)e->v.block_lit.records.items[i].ip;
            LLVMValueRef slot_ptr = LLVMBuildInBoundsGEP2(g->builder, g->runtime_dispatch_table_ty, g->runtime_dispatch_table,
                                                          (LLVMValueRef[]){ zero64, LLVMConstInt(g->i64, ip_slot, 0) }, 2,
                                                          "blk.dispatch.rec.ptr");
            LLVMValueRef rec_addr = LLVMBlockAddress(fn, g->runtime_record_blocks[i]);
            if (LLVMTypeOf(rec_addr) != g->i8ptr)
                rec_addr = LLVMConstBitCast(rec_addr, g->i8ptr);
            LLVMBuildStore(g->builder, rec_addr, slot_ptr);
        }
    }

    emit_runtime_block_dispatch(g, start_ip);

    LLVMPositionBuilderAtEnd(g->builder, g->runtime_block_start);
    LLVMValueRef stop_start = LLVMBuildAnd(g->builder, until_enabled,
        LLVMBuildICmp(g->builder, LLVMIntEQ, until_ip, LLVMConstInt(g->i64, 0, 0), "blk.stop.start"),
        "blk.stop.start.cond");
    LLVMBasicBlockRef start_cont = LLVMAppendBasicBlockInContext(g->ctx, fn, "blk.start.cont");
    LLVMBuildCondBr(g->builder, stop_start, g->runtime_block_done, start_cont);
    LLVMPositionBuilderAtEnd(g->builder, start_cont);
    gen_stmt(g, e->v.block_lit.body);
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(g->builder)))
        LLVMBuildBr(g->builder, g->runtime_block_done);

    LLVMPositionBuilderAtEnd(g->builder, g->runtime_block_done);
    if (g->gc_frame)
        LLVMBuildCall2(g->builder, g->fn_gc_pop_ty, g->fn_gc_pop, &g->gc_frame, 1, "");
    LLVMBuildRetVoid(g->builder);

    g->scope = saved_scope;
    g->current_ret_type = saved_ret;
    g->gc_frame = saved_gc;
    g->loop_depth = saved_loop;
    g->try_depth = saved_try;
    g->current_func_name = saved_name;
    g->in_runtime_block = saved_in_rt;
    g->runtime_block_records = saved_records;
    g->runtime_record_blocks = saved_bbs;
    g->runtime_record_count = saved_bb_count;
    g->runtime_block_start = saved_start;
    g->runtime_block_done = saved_done;
    g->runtime_block_use_indirect = saved_use_indirect;
    g->runtime_dispatch_table = saved_dispatch_table;
    g->runtime_dispatch_table_ty = saved_dispatch_table_ty;
    g->runtime_until_enabled = saved_until_enabled;
    g->runtime_until_ip = saved_until_ip;
    g->current_class = saved_class;
    g->current_this = saved_this;
    return fn;
}

static ClosureInfo *ensure_closure_info(Codegen *g, Expr *e, int is_block)
{
    int id = is_block ? e->v.block_lit.id : e->v.anon_func.id;
    ClosureInfo *old = closuretable_find(&g->closures, id, is_block);
    if (old) return old;

    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(g->builder);

    ClosureInfo ci;
    kn_memset(&ci, 0, sizeof(ci));
    ci.id = id;
    ci.is_block = is_block;
    ci.expr = e;
    ci.lexical_class = g->current_class;
    ci.capture_this = (g->current_class && g->current_this) ? 1 : 0;
    closure_collect_scope_captures(g, &ci);

    LLVMValueRef body_fn = is_block ? emit_block_body_function(g, &ci, e) : emit_lambda_body_function(g, &ci, e);
    ci.body_fn = body_fn;
    if (is_block)
    {
        ci.wrapper_fn = 0;
        ci.wrapper_fn_ty = 0;
        closuretable_add(&g->closures, ci);

        if (saved_bb)
            LLVMPositionBuilderAtEnd(g->builder, saved_bb);

        return closuretable_find(&g->closures, id, is_block);
    }

    const char *wname = is_block
        ? mangle_helper_name("__kn_block_wrap", "BW", id)
        : mangle_helper_name("__kn_lambda_wrap", "LW", id);
    LLVMValueRef wrapper = LLVMAddFunction(g->mod, wname, g->callable_wrapper_ty);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(g->ctx, wrapper, "entry");
    LLVMPositionBuilderAtEnd(g->builder, entry);

    LLVMValueRef env = LLVMGetParam(wrapper, 0);
    LLVMValueRef args_ptr = LLVMGetParam(wrapper, 1);
    LLVMValueRef argc = LLVMGetParam(wrapper, 2);

    if (is_block)
    {
        LLVMValueRef ok = LLVMBuildICmp(g->builder, LLVMIntEQ, argc, LLVMConstInt(g->i64, 3, 0), "blk.argc.ok");
        LLVMBasicBlockRef ok_bb = LLVMAppendBasicBlockInContext(g->ctx, wrapper, "ok");
        LLVMBasicBlockRef bad_bb = LLVMAppendBasicBlockInContext(g->ctx, wrapper, "bad");
        LLVMBuildCondBr(g->builder, ok, ok_bb, bad_bb);
        LLVMPositionBuilderAtEnd(g->builder, bad_bb);
        LLVMBuildRet(g->builder, any_null_value(g));

        LLVMPositionBuilderAtEnd(g->builder, ok_bb);
        Type i64t = type_make(TY_I64);
        LLVMValueRef a0 = LLVMBuildLoad2(g->builder, g->any_ty, LLVMBuildInBoundsGEP2(g->builder, g->any_ty, args_ptr, (LLVMValueRef[]){ LLVMConstInt(g->i64, 0, 0) }, 1, "b.a0p"), "b.a0");
        LLVMValueRef a1 = LLVMBuildLoad2(g->builder, g->any_ty, LLVMBuildInBoundsGEP2(g->builder, g->any_ty, args_ptr, (LLVMValueRef[]){ LLVMConstInt(g->i64, 1, 0) }, 1, "b.a1p"), "b.a1");
        LLVMValueRef a2 = LLVMBuildLoad2(g->builder, g->any_ty, LLVMBuildInBoundsGEP2(g->builder, g->any_ty, args_ptr, (LLVMValueRef[]){ LLVMConstInt(g->i64, 2, 0) }, 1, "b.a2p"), "b.a2");
        LLVMValueRef mode = any_to_type(g, a0, i64t);
        LLVMValueRef x = any_to_type(g, a1, i64t);
        LLVMValueRef y = any_to_type(g, a2, i64t);
        LLVMTypeRef body_args[4] = { g->i8ptr, g->i64, g->i64, g->i64 };
        LLVMTypeRef body_ty = LLVMFunctionType(g->voidt, body_args, 4, 0);
        LLVMBuildCall2(g->builder, body_ty, body_fn, (LLVMValueRef[]){ env, mode, x, y }, 4, "");
        LLVMBuildRet(g->builder, any_null_value(g));
    }
    else
    {
        int pcount = e->v.anon_func.param_count;
        LLVMValueRef ok = LLVMBuildICmp(g->builder, LLVMIntUGE, argc, LLVMConstInt(g->i64, (uint64_t)pcount, 0), "lam.argc.ok");
        LLVMBasicBlockRef ok_bb = LLVMAppendBasicBlockInContext(g->ctx, wrapper, "ok");
        LLVMBasicBlockRef bad_bb = LLVMAppendBasicBlockInContext(g->ctx, wrapper, "bad");
        LLVMBuildCondBr(g->builder, ok, ok_bb, bad_bb);
        LLVMPositionBuilderAtEnd(g->builder, bad_bb);
        LLVMBuildRet(g->builder, any_null_value(g));

        LLVMPositionBuilderAtEnd(g->builder, ok_bb);
        int argc2 = 1 + pcount;
        LLVMValueRef *argv = (LLVMValueRef *)kn_malloc(sizeof(LLVMValueRef) * (size_t)argc2);
        argv[0] = env;
        for (int i = 0; i < pcount; i++)
        {
            LLVMValueRef ptr = LLVMBuildInBoundsGEP2(g->builder, g->any_ty, args_ptr,
                                                     (LLVMValueRef[]){ LLVMConstInt(g->i64, (uint64_t)i, 0) }, 1, "l.arg.ptr");
            LLVMValueRef anyv = LLVMBuildLoad2(g->builder, g->any_ty, ptr, "l.arg.any");
            argv[1 + i] = any_to_type(g, anyv, e->v.anon_func.params[i].type);
        }
        LLVMTypeRef *body_args = (LLVMTypeRef *)kn_malloc(sizeof(LLVMTypeRef) * (size_t)argc2);
        body_args[0] = g->i8ptr;
        for (int i = 0; i < pcount; i++)
            body_args[1 + i] = llvm_type(g, e->v.anon_func.params[i].type);
        LLVMTypeRef body_ty = LLVMFunctionType(llvm_type(g, e->v.anon_func.ret_type), body_args, (unsigned)argc2, 0);
        LLVMValueRef rv = LLVMBuildCall2(g->builder, body_ty, body_fn, argv, (unsigned)argc2,
                                         e->v.anon_func.ret_type.kind == TY_VOID ? "" : "lam.call");
        if (e->v.anon_func.ret_type.kind == TY_VOID)
        {
            LLVMBuildRet(g->builder, any_null_value(g));
        }
        else
        {
            CGValue boxv;
            boxv.value = rv;
            boxv.type = e->v.anon_func.ret_type;
            LLVMBuildRet(g->builder, box_any_value(g, boxv));
        }
    }

    ci.wrapper_fn = wrapper;
    ci.wrapper_fn_ty = g->callable_wrapper_ty;
    closuretable_add(&g->closures, ci);

    if (saved_bb)
        LLVMPositionBuilderAtEnd(g->builder, saved_bb);

    return closuretable_find(&g->closures, id, is_block);
}

static LLVMValueRef gen_bool(Codegen *g, CGValue v)
{
    if (v.type.kind == TY_BOOL) return v.value;
    if (is_integer_kind(v.type.kind))
    {
        LLVMTypeRef it = llvm_int_type(g, v.type.kind);
        LLVMValueRef zero = LLVMConstInt(it, 0, 0);
        return LLVMBuildICmp(g->builder, LLVMIntNE, v.value, zero, "tobool");
    }
    if (is_float_kind(v.type.kind))
    {
        LLVMValueRef zero = LLVMConstReal(llvm_float_type(g, v.type.kind), 0.0);
        return LLVMBuildFCmp(g->builder, LLVMRealONE, v.value, zero, "toboolf");
    }
    return v.value;
}

static void gen_block(Codegen *g, Stmt *s)
{
    Scope sc; sc.vars = 0; sc.parent = g->scope; g->scope = &sc;
    for (int i = 0; i < s->v.block.stmts.count; i++)
    {
        gen_stmt(g, s->v.block.stmts.items[i]);
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(g->builder)))
            break;
    }
    g->scope = sc.parent;
}

static void gen_stmt(Codegen *g, Stmt *s)
{
    switch (s->kind)
    {
    case ST_BLOCK:
        gen_block(g, s);
        return;
    case ST_VAR:
    {
        Type ty = s->v.var.type;
        LLVMValueRef alloca = build_entry_alloca(g, LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder)), llvm_type(g, ty), s->v.var.name);
        if (ty.kind == TY_STRUCT)
        {
            StructSymbol *ss = structtable_find(&g->structs, ty.name);
            if (ss && ss->decl && ss->decl->align > 0)
                LLVMSetAlignment(alloca, (unsigned)ss->decl->align);
        }
        if (ty.kind == TY_ARRAY && (!s->v.var.init || s->v.var.init->kind == EXPR_ARRAY))
        {
            LLVMValueRef arr_val = build_array_value(g, ty, s->v.var.init);
            LLVMBuildStore(g->builder, arr_val, alloca);
        }
        else if (s->v.var.init)
        {
            CGValue initv = gen_expr(g, s->v.var.init);
            LLVMValueRef cv = cast_value(g, initv, ty);
            LLVMBuildStore(g->builder, cv, alloca);
        }
        scope_add(g->scope, s->v.var.name, ty, alloca);
        gc_add_root(g, alloca, ty);
        return;
    }
    case ST_ASSIGN:
    {
        VarSymbol *v = scope_find(g->scope, s->v.assign.name);
        if (!v)
        {
            kn_write_str("unknown variable: ");
            kn_write_str(s->v.assign.name ? s->v.assign.name : "<null>");
            kn_write_str("\n");
            kn_die("unknown variable");
        }
        CGValue val = gen_expr(g, s->v.assign.value);
        LLVMValueRef cv = cast_value(g, val, v->type);
        LLVMBuildStore(g->builder, cv, v->value);
        return;
    }
    case ST_EXPR:
    {
        gen_expr(g, s->v.expr.expr);
        return;
    }
    case ST_BLOCK_RECORD:
    {
        if (!g->in_runtime_block)
            return;
        LLVMBasicBlockRef rec_bb = runtime_block_bb_for_ip(g, s->v.record.ip);
        if (!rec_bb)
            return;
        LLVMBasicBlockRef cur_bb = LLVMGetInsertBlock(g->builder);
        if (cur_bb != rec_bb && !LLVMGetBasicBlockTerminator(cur_bb))
            LLVMBuildBr(g->builder, rec_bb);
        LLVMPositionBuilderAtEnd(g->builder, rec_bb);
        LLVMValueRef stop = LLVMBuildAnd(g->builder, g->runtime_until_enabled,
            LLVMBuildICmp(g->builder, LLVMIntEQ, g->runtime_until_ip, LLVMConstInt(g->i64, (uint64_t)s->v.record.ip, 0), "blk.stop.rec"),
            "blk.stop.rec.cond");
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
        LLVMBasicBlockRef cont_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "blk.rec.cont");
        LLVMBuildCondBr(g->builder, stop, g->runtime_block_done, cont_bb);
        LLVMPositionBuilderAtEnd(g->builder, cont_bb);
        return;
    }
    case ST_BLOCK_JUMP:
    {
        if (!g->in_runtime_block)
            return;
        CGValue target = gen_expr(g, s->v.jump.target);
        LLVMValueRef ip = cast_value(g, target, type_make(TY_I64));
        emit_runtime_block_dispatch(g, ip);
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
        LLVMBasicBlockRef cont_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "blk.jump.cont");
        LLVMPositionBuilderAtEnd(g->builder, cont_bb);
        return;
    }
    case ST_RETURN:
    {
        if (s->v.ret.expr)
        {
            CGValue rv = gen_expr(g, s->v.ret.expr);
            LLVMValueRef cv = cast_value(g, rv, g->current_ret_type);
            if (g->gc_frame)
                LLVMBuildCall2(g->builder, g->fn_gc_pop_ty, g->fn_gc_pop, &g->gc_frame, 1, "");
            LLVMBuildRet(g->builder, cv);
        }
        else
        {
            if (g->gc_frame)
                LLVMBuildCall2(g->builder, g->fn_gc_pop_ty, g->fn_gc_pop, &g->gc_frame, 1, "");
            LLVMBuildRetVoid(g->builder);
        }
        return;
    }
    case ST_IF:
    {
        if (s->v.ifs.is_const)
        {
            if (s->v.ifs.const_value)
            {
                if (s->v.ifs.pattern_name && s->v.ifs.cond && s->v.ifs.cond->kind == EXPR_IS)
                {
                    Scope sc; sc.vars = 0; sc.parent = g->scope; g->scope = &sc;
                    CGValue patv = gen_expr(g, s->v.ifs.cond->v.is_expr.expr);
                    Type pat_ty = s->v.ifs.cond->v.is_expr.target;
                    LLVMValueRef pat_slot = build_entry_alloca(g, LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder)),
                                                               llvm_type(g, pat_ty), s->v.ifs.pattern_name);
                    LLVMBuildStore(g->builder, cast_value(g, patv, pat_ty), pat_slot);
                    scope_add(g->scope, s->v.ifs.pattern_name, pat_ty, pat_slot);
                    gc_add_root(g, pat_slot, pat_ty);
                    gen_stmt(g, s->v.ifs.then_s);
                    g->scope = sc.parent;
                }
                else
                {
                    gen_stmt(g, s->v.ifs.then_s);
                }
            }
            else if (s->v.ifs.else_s)
                gen_stmt(g, s->v.ifs.else_s);
            return;
        }
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
        LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "then");
        LLVMBasicBlockRef else_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "else");
        LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "ifend");
        CGValue condv;
        condv.value = LLVMConstInt(g->i1, 0, 0);
        condv.type = type_make(TY_BOOL);
        CGValue patv;
        patv.value = 0;
        patv.type = type_make(TY_UNKNOWN);
        int has_pattern = s->v.ifs.pattern_name && s->v.ifs.cond && s->v.ifs.cond->kind == EXPR_IS;
        LLVMValueRef cond = LLVMConstInt(g->i1, 0, 0);
        if (has_pattern)
        {
            patv = gen_expr(g, s->v.ifs.cond->v.is_expr.expr);
            cond = gen_is_value(g, patv, s->v.ifs.cond->v.is_expr.target);
        }
        else
        {
            condv = gen_expr(g, s->v.ifs.cond);
            cond = gen_bool(g, condv);
        }
        LLVMBuildCondBr(g->builder, cond, then_bb, else_bb);
        LLVMPositionBuilderAtEnd(g->builder, then_bb);
        if (has_pattern)
        {
            Scope sc; sc.vars = 0; sc.parent = g->scope; g->scope = &sc;
            Type pat_ty = s->v.ifs.cond->v.is_expr.target;
            LLVMValueRef pat_slot = build_entry_alloca(g, fn, llvm_type(g, pat_ty), s->v.ifs.pattern_name);
            LLVMBuildStore(g->builder, cast_value(g, patv, pat_ty), pat_slot);
            scope_add(g->scope, s->v.ifs.pattern_name, pat_ty, pat_slot);
            gc_add_root(g, pat_slot, pat_ty);
            gen_stmt(g, s->v.ifs.then_s);
            g->scope = sc.parent;
        }
        else
        {
            gen_stmt(g, s->v.ifs.then_s);
        }
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(g->builder)))
            LLVMBuildBr(g->builder, merge_bb);
        LLVMPositionBuilderAtEnd(g->builder, else_bb);
        if (s->v.ifs.else_s)
            gen_stmt(g, s->v.ifs.else_s);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(g->builder)))
            LLVMBuildBr(g->builder, merge_bb);
        LLVMPositionBuilderAtEnd(g->builder, merge_bb);
        return;
    }
    case ST_SWITCH:
    {
        int case_count = s->v.switchs.cases.count;
        CGValue sv = gen_expr(g, s->v.switchs.value);
        LLVMValueRef temp_frame = gc_push_temp_frame(g, "gc.switch.frame");
        sv = gc_root_temp_value(g, temp_frame, sv, "gc.switch.value");
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
        LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "switch.end");
        LLVMBasicBlockRef fallback_bb = merge_bb;
        LLVMBasicBlockRef *case_bbs = (LLVMBasicBlockRef *)kn_malloc(sizeof(LLVMBasicBlockRef) * (size_t)(case_count > 0 ? case_count : 1));
        for (int i = 0; i < case_count; i++)
            case_bbs[i] = LLVMAppendBasicBlockInContext(g->ctx, fn, s->v.switchs.cases.items[i].is_default ? "switch.default" : "switch.case");

        LLVMBasicBlockRef cur_check = LLVMGetInsertBlock(g->builder);
        for (int i = 0; i < case_count; i++)
        {
            StmtSwitchCase *c = &s->v.switchs.cases.items[i];
            if (c->is_default)
            {
                fallback_bb = case_bbs[i];
                continue;
            }
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "switch.next");
            LLVMPositionBuilderAtEnd(g->builder, cur_check);
            CGValue mv = gen_expr(g, c->match);
            LLVMValueRef cond = gen_switch_match(g, sv, mv);
            LLVMBuildCondBr(g->builder, cond, case_bbs[i], next_bb);
            cur_check = next_bb;
        }

        LLVMPositionBuilderAtEnd(g->builder, cur_check);
        LLVMBuildBr(g->builder, fallback_bb);

        for (int i = 0; i < case_count; i++)
        {
            LLVMPositionBuilderAtEnd(g->builder, case_bbs[i]);
            gc_pop_temp_frame(g, temp_frame);
            gen_stmt(g, s->v.switchs.cases.items[i].body);
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(g->builder)))
                LLVMBuildBr(g->builder, merge_bb);
        }

        if (fallback_bb == merge_bb)
            gc_pop_temp_frame(g, temp_frame);
        LLVMPositionBuilderAtEnd(g->builder, merge_bb);
        return;
    }
    case ST_WHILE:
    {
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
        LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "while.cond");
        LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "while.body");
        LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "while.end");
        LLVMBuildBr(g->builder, cond_bb);
        LLVMPositionBuilderAtEnd(g->builder, cond_bb);
        CGValue condv = gen_expr(g, s->v.whiles.cond);
        LLVMValueRef cond = gen_bool(g, condv);
        LLVMBuildCondBr(g->builder, cond, body_bb, end_bb);
        g->break_blocks[g->loop_depth] = end_bb;
        g->continue_blocks[g->loop_depth] = cond_bb;
        g->loop_depth++;
        LLVMPositionBuilderAtEnd(g->builder, body_bb);
        gen_stmt(g, s->v.whiles.body);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(g->builder)))
            LLVMBuildBr(g->builder, cond_bb);
        g->loop_depth--;
        LLVMPositionBuilderAtEnd(g->builder, end_bb);
        return;
    }
    case ST_FOR:
    {
        if (s->v.fors.init) gen_stmt(g, s->v.fors.init);
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
        LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "for.cond");
        LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "for.body");
        LLVMBasicBlockRef post_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "for.post");
        LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "for.end");
        LLVMBuildBr(g->builder, cond_bb);
        LLVMPositionBuilderAtEnd(g->builder, cond_bb);
        if (s->v.fors.cond)
        {
            CGValue condv = gen_expr(g, s->v.fors.cond);
            LLVMValueRef cond = gen_bool(g, condv);
            LLVMBuildCondBr(g->builder, cond, body_bb, end_bb);
        }
        else
        {
            LLVMBuildBr(g->builder, body_bb);
        }
        g->break_blocks[g->loop_depth] = end_bb;
        g->continue_blocks[g->loop_depth] = post_bb;
        g->loop_depth++;
        LLVMPositionBuilderAtEnd(g->builder, body_bb);
        gen_stmt(g, s->v.fors.body);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(g->builder)))
            LLVMBuildBr(g->builder, post_bb);
        LLVMPositionBuilderAtEnd(g->builder, post_bb);
        if (s->v.fors.post) gen_expr(g, s->v.fors.post);
        LLVMBuildBr(g->builder, cond_bb);
        g->loop_depth--;
        LLVMPositionBuilderAtEnd(g->builder, end_bb);
        return;
    }
    case ST_TRY:
    {
        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(g->builder));
        LLVMBasicBlockRef catch_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "catch");
        LLVMBasicBlockRef after_bb = LLVMAppendBasicBlockInContext(g->ctx, fn, "try.end");
        LLVMValueRef exc_slot = build_entry_alloca(g, fn, g->i8ptr, "__kn_exc");
        LLVMBuildStore(g->builder, LLVMConstNull(g->i8ptr), exc_slot);

        if (g->try_depth < (int)(sizeof(g->try_stack) / sizeof(g->try_stack[0])))
        {
            g->try_stack[g->try_depth].catch_bb = catch_bb;
            g->try_stack[g->try_depth].after_bb = after_bb;
            g->try_stack[g->try_depth].exc_slot = exc_slot;
            g->try_depth++;
        }

        gen_stmt(g, s->v.trys.try_block);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(g->builder)))
            LLVMBuildBr(g->builder, after_bb);

        if (g->try_depth > 0)
            g->try_depth--;

        LLVMPositionBuilderAtEnd(g->builder, catch_bb);
        if (g->fn_exc_clear)
            LLVMBuildCall2(g->builder, g->fn_exc_clear_ty, g->fn_exc_clear, 0, 0, "");
        if (s->v.trys.has_param && s->v.trys.catch_name)
        {
            Scope sc; sc.vars = 0; sc.parent = g->scope; g->scope = &sc;
            LLVMValueRef excv = LLVMBuildLoad2(g->builder, g->i8ptr, exc_slot, "exc");
            Type ct = s->v.trys.catch_type;
            if (ct.kind == TY_CLASS && ct.name && kn_strcmp(ct.name, "IO.Error") == 0 && g->error_class)
            {
                LLVMValueRef obj = LLVMBuildBitCast(g->builder, excv, g->error_class->ptr_ty, "err");
                LLVMValueRef alloca = build_entry_alloca(g, fn, llvm_type(g, ct), s->v.trys.catch_name);
                LLVMBuildStore(g->builder, obj, alloca);
                scope_add(g->scope, s->v.trys.catch_name, ct, alloca);
                gc_add_root(g, alloca, ct);
            }
            else
            {
                LLVMValueRef msg = excv;
                if (g->error_class && g->error_msg_index >= 0)
                {
                    LLVMValueRef obj = LLVMBuildBitCast(g->builder, excv, g->error_class->ptr_ty, "err");
                    unsigned fidx = (unsigned)(1 + g->error_msg_index);
                    LLVMValueRef msg_ptr = LLVMBuildStructGEP2(g->builder, g->error_class->struct_ty, obj, fidx, "err.msg");
                    msg = LLVMBuildLoad2(g->builder, g->i8ptr, msg_ptr, "err.msg.val");
                }
                LLVMValueRef alloca = build_entry_alloca(g, fn, g->i8ptr, s->v.trys.catch_name);
                LLVMBuildStore(g->builder, msg, alloca);
                scope_add(g->scope, s->v.trys.catch_name, type_make(TY_STRING), alloca);
                gc_add_root(g, alloca, type_make(TY_STRING));
            }
            gen_stmt(g, s->v.trys.catch_block);
            g->scope = sc.parent;
        }
        else
        {
            gen_stmt(g, s->v.trys.catch_block);
        }
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(g->builder)))
            LLVMBuildBr(g->builder, after_bb);
        LLVMPositionBuilderAtEnd(g->builder, after_bb);
        return;
    }
    case ST_BREAK:
    {
        if (g->loop_depth <= 0) kn_die("break not in loop");
        LLVMBuildBr(g->builder, g->break_blocks[g->loop_depth - 1]);
        return;
    }
    case ST_CONTINUE:
    {
        if (g->loop_depth <= 0) kn_die("continue not in loop");
        LLVMBuildBr(g->builder, g->continue_blocks[g->loop_depth - 1]);
        return;
    }
    case ST_THROW:
    {
        LLVMValueRef errp = LLVMConstNull(g->i8ptr);
        if (s->v.throws.expr)
        {
            CGValue ev = gen_expr(g, s->v.throws.expr);
            if (ev.type.kind == TY_CLASS && ev.type.name && kn_strcmp(ev.type.name, "IO.Error") == 0)
            {
                errp = LLVMBuildBitCast(g->builder, ev.value, g->i8ptr, "errp");
            }
            else
            {
                LLVMValueRef msg = cast_value(g, ev, type_make(TY_STRING));
                LLVMValueRef title = build_string_const(g, "Error", 5);
                errp = build_error_object(g, title, msg);
            }
        }
        link_error_inner_from_last(g, errp);
        if (g->fn_exc_set)
            LLVMBuildCall2(g->builder, g->fn_exc_set_ty, g->fn_exc_set, &errp, 1, "");
        trace_push_current_func(g);
        update_error_trace_field(g, errp);
        if (g->try_depth > 0)
        {
            int idx = g->try_depth - 1;
            LLVMValueRef ev = errp;
            if (g->fn_exc_get)
                ev = LLVMBuildCall2(g->builder, g->fn_exc_get_ty, g->fn_exc_get, 0, 0, "exc.get");
            LLVMBuildStore(g->builder, ev, g->try_stack[idx].exc_slot);
            LLVMBuildBr(g->builder, g->try_stack[idx].catch_bb);
        }
        else
        {
            emit_return_default(g);
        }
        return;
    }
    default: break;
    }
}

