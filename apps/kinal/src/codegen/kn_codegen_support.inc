// Internal codegen fragment: symbol tables, types, and low-level helpers


typedef struct
{
    LLVMValueRef value;
    Type type;
} CGValue;

static bool cg_is_async_task_type(Type t)
{
    return t.kind == TY_CLASS && t.name && kn_strcmp(t.name, "IO.Async.Task") == 0;
}

static Type cg_async_task_type(void)
{
    return type_class("IO.Async.Task");
}

static Type cg_func_public_ret_type(const Func *f)
{
    if (!f) return type_make(TY_UNKNOWN);
    return f->is_async ? cg_async_task_type() : f->ret_type;
}

static Type cg_func_logical_ret_type(const Func *f)
{
    if (!f) return type_make(TY_UNKNOWN);
    if (f->is_async && f->async_result_type.kind != TY_UNKNOWN)
        return f->async_result_type;
    return f->ret_type;
}

static Type type_from_elem(Type t)
{
    if (t.kind == TY_PTR && t.ptr_depth > 1)
    {
        Type e = type_ptr(t.elem);
        e.ptr_depth = t.ptr_depth - 1;
        e.name = t.name;
        return e;
    }
    Type e = type_make(t.elem);
    e.name = t.name;
    return e;
}

static EnumDecl *enum_find(Codegen *g, const char *name)
{
    if (!g || !g->enums || !name) return 0;
    for (int i = 0; i < g->enums->count; i++)
        if (kn_strcmp(g->enums->items[i].name, name) == 0 ||
            (g->enums->items[i].qname && kn_strcmp(g->enums->items[i].qname, name) == 0))
            return &g->enums->items[i];
    return 0;
}

static InterfaceDecl *interface_find(Codegen *g, const char *name)
{
    if (!g || !g->interfaces || !name) return 0;
    for (int i = 0; i < g->interfaces->count; i++)
        if (kn_strcmp(g->interfaces->items[i].name, name) == 0 ||
            (g->interfaces->items[i].qname && kn_strcmp(g->interfaces->items[i].qname, name) == 0))
            return &g->interfaces->items[i];
    return 0;
}

static LLVMTypeRef llvm_type(Codegen *g, Type t);

static LLVMTypeRef llvm_array_type(Codegen *g, Type elem)
{
    ArrayType *at = arraytable_find(&g->arrays, elem);
    if (at) return at->struct_ty;

    LLVMTypeRef elem_ty = llvm_type(g, elem);
    LLVMTypeRef fields[2] = { LLVMPointerType(elem_ty, 0), g->i64 };
    LLVMTypeRef st = LLVMStructTypeInContext(g->ctx, fields, 2, 0);
    ArrayType a;
    a.elem = elem;
    a.struct_ty = st;
    arraytable_add(&g->arrays, a);
    return st;
}

static LLVMTypeRef llvm_type(Codegen *g, Type t)
{
    switch (t.kind)
    {
    case TY_VOID: return g->voidt;
    case TY_BOOL: return g->i1;
    case TY_BYTE:
    case TY_CHAR:
    case TY_I8:
    case TY_U8: return g->i8;
    case TY_I16:
    case TY_U16: return g->i16;
    case TY_I32:
    case TY_U32: return g->i32;
    case TY_INT:
    case TY_I64:
    case TY_U64:
    case TY_ISIZE:
    case TY_USIZE: return g->i64;
    case TY_F16: return g->f16;
    case TY_F32: return g->f32;
    case TY_FLOAT:
    case TY_F64: return g->f64;
    case TY_F128: return g->f128;
    case TY_STRING: return g->i8ptr;
    case TY_ANY: return g->any_ty ? g->any_ty : g->i8ptr;
    case TY_NULL: return g->i8ptr;
    case TY_ENUM:
    {
        EnumDecl *en = enum_find(g, t.name);
        if (en)
            return llvm_type(g, en->underlying);
        return g->i64;
    }
    case TY_STRUCT:
    {
        StructSymbol *ss = structtable_find(&g->structs, t.name);
        if (ss) return ss->struct_ty;
        return g->i8;
    }
    case TY_ARRAY:
    {
        Type elem = type_make(t.elem);
        elem.name = t.name;
        return llvm_array_type(g, elem);
    }
    case TY_PTR:
    {
        Type elem = type_from_elem(t);
        return LLVMPointerType(llvm_type(g, elem), 0);
    }
    case TY_CLASS:
    {
        if (cg_is_async_task_type(t))
            return g->i8ptr;
        ClassSymbol *cs = classtable_find(&g->classes, t.name);
        if (cs) return cs->ptr_ty;
        return g->i8ptr;
    }
    default:
        return g->i64;
    }
}

static LLVMValueRef build_zeroed_global_initializer(Codegen *g, Type ty)
{
    LLVMTypeRef llvm_ty = llvm_type(g, ty);
    if (ty.kind == TY_ARRAY && ty.array_len > 0)
    {
        Type elem = type_make(ty.elem);
        elem.name = ty.name;
        LLVMTypeRef elem_ty = llvm_type(g, elem);
        LLVMTypeRef storage_ty = LLVMArrayType(elem_ty, (unsigned)ty.array_len);
        LLVMValueRef storage = LLVMAddGlobal(g->mod, storage_ty, "");
        LLVMSetLinkage(storage, LLVMPrivateLinkage);
        LLVMSetInitializer(storage, LLVMConstNull(storage_ty));

        LLVMValueRef idxs[2] = { LLVMConstInt(g->i32, 0, 0), LLVMConstInt(g->i32, 0, 0) };
        LLVMValueRef fields[2];
        fields[0] = LLVMConstInBoundsGEP2(storage_ty, storage, idxs, 2);
        fields[1] = LLVMConstInt(g->i64, (uint64_t)ty.array_len, 0);
        return LLVMConstNamedStruct(llvm_ty, fields, 2);
    }

    return LLVMConstNull(llvm_ty);
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

static bool is_gc_root_type(Type t)
{
    switch (t.kind)
    {
    case TY_STRING:
    case TY_CLASS:
    case TY_ARRAY:
    case TY_ANY:
    case TY_PTR:
    case TY_STRUCT:
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

static LLVMTypeRef llvm_float_type(Codegen *g, TypeKind k)
{
    switch (k)
    {
    case TY_F16: return g->f16;
    case TY_F32: return g->f32;
    case TY_FLOAT:
    case TY_F64: return g->f64;
    case TY_F128: return g->f128;
    default: return g->f64;
    }
}

static const char *cg_base_type_name(TypeKind k)
{
    switch (k)
    {
    case TY_VOID: return "void";
    case TY_BOOL: return "bool";
    case TY_BYTE: return "byte";
    case TY_CHAR: return "char";
    case TY_INT: return "int";
    case TY_FLOAT: return "float";
    case TY_F16: return "f16";
    case TY_F32: return "f32";
    case TY_F64: return "f64";
    case TY_F128: return "f128";
    case TY_I8: return "i8";
    case TY_I16: return "i16";
    case TY_I32: return "i32";
    case TY_I64: return "i64";
    case TY_U8: return "u8";
    case TY_U16: return "u16";
    case TY_U32: return "u32";
    case TY_U64: return "u64";
    case TY_ISIZE: return "isize";
    case TY_USIZE: return "usize";
    case TY_STRING: return "string";
    case TY_ANY: return "any";
    case TY_NULL: return "null";
    default: return "unknown";
    }
}

static const char *cg_type_name(Type t)
{
    char buf[128];
    size_t off = 0;
    if ((t.kind == TY_CLASS || t.kind == TY_STRUCT || t.kind == TY_ENUM) && t.name)
    {
        kn_append(buf, sizeof(buf), &off, t.name);
    }
    else if (t.kind == TY_PTR)
    {
        if ((t.elem == TY_CLASS || t.elem == TY_STRUCT || t.elem == TY_ENUM) && t.name)
            kn_append(buf, sizeof(buf), &off, t.name);
        else
            kn_append(buf, sizeof(buf), &off, cg_base_type_name(t.elem));
        int depth = t.ptr_depth > 0 ? t.ptr_depth : 1;
        for (int i = 0; i < depth; i++)
            kn_append(buf, sizeof(buf), &off, "*");
    }
    else if (t.kind == TY_ARRAY)
    {
        if ((t.elem == TY_CLASS || t.elem == TY_STRUCT || t.elem == TY_ENUM) && t.name)
            kn_append(buf, sizeof(buf), &off, t.name);
        else
            kn_append(buf, sizeof(buf), &off, cg_base_type_name(t.elem));
        kn_append(buf, sizeof(buf), &off, "[]");
    }
    else
    {
        kn_append(buf, sizeof(buf), &off, cg_base_type_name(t.kind));
    }
    char *out = (char *)kn_malloc(off + 1);
    if (!out) return "unknown";
    kn_memcpy(out, buf, off + 1);
    return out;
}

static bool str_contains_ci(const char *s, const char *needle)
{
    if (!s || !needle || !needle[0]) return false;
    size_t nlen = kn_strlen(needle);
    for (size_t i = 0; s[i]; i++)
    {
        size_t j = 0;
        while (j < nlen)
        {
            char a = s[i + j];
            char b = needle[j];
            if (!a) break;
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
            j++;
        }
        if (j == nlen) return true;
    }
    return false;
}

static bool str_eq_n(const char *s, size_t slen, const char *lit)
{
    if (!s || !lit) return false;
    size_t n = kn_strlen(lit);
    if (slen != n) return false;
    return kn_strncmp(s, lit, n) == 0;
}

static bool str_starts_with_n(const char *s, size_t slen, const char *prefix)
{
    if (!s || !prefix) return false;
    size_t n = kn_strlen(prefix);
    if (slen < n) return false;
    return kn_strncmp(s, prefix, n) == 0;
}

static bool is_entry_symbol_name(const char *name, size_t len)
{
    return str_eq_n(name, len, "main") ||
           str_eq_n(name, len, "mainCRTStartup") ||
           str_eq_n(name, len, "__kn_entry");
}

static void prune_unused_runtime_symbols(LLVMModuleRef mod)
{
    if (!mod) return;

    // Runtime helpers emitted in-module use "__kn_" prefix. Mark defined ones internal
    // so LLVM can safely drop dead helpers without touching user-defined symbols.
    for (LLVMValueRef fn = LLVMGetFirstFunction(mod); fn; fn = LLVMGetNextFunction(fn))
    {
        size_t nlen = 0;
        const char *name = LLVMGetValueName2(fn, &nlen);
        if (!name) continue;
        if (!str_starts_with_n(name, nlen, "__kn_")) continue;
        if (is_entry_symbol_name(name, nlen)) continue;
        if (str_eq_n(name, nlen, "__kn_meta_get_module")) continue;
        if (LLVMCountBasicBlocks(fn) > 0)
            LLVMSetLinkage(fn, LLVMInternalLinkage);
    }

    int changed = 0;
    do
    {
        changed = 0;
        LLVMValueRef fn = LLVMGetFirstFunction(mod);
        while (fn)
        {
            LLVMValueRef next = LLVMGetNextFunction(fn);
            size_t nlen = 0;
            const char *name = LLVMGetValueName2(fn, &nlen);
            int has_uses = LLVMGetFirstUse(fn) != 0;
            int has_body = LLVMCountBasicBlocks(fn) > 0;
            int should_delete = 0;

            if (name && !is_entry_symbol_name(name, nlen) && !has_uses)
            {
                if (str_eq_n(name, nlen, "__kn_meta_get_module"))
                {
                    fn = next;
                    continue;
                }
                if (!has_body)
                {
                    // Remove dead declarations to keep emitted .ll compact.
                    should_delete = 1;
                }
                else if (str_starts_with_n(name, nlen, "__kn_"))
                {
                    // Remove dead runtime helper implementations; keep user funcs.
                    should_delete = 1;
                }
            }

            if (should_delete)
            {
                LLVMDeleteFunction(fn);
                changed = 1;
            }
            fn = next;
        }
    } while (changed);
}

static bool triple_is_windows(const char *triple)
{
    return str_contains_ci(triple, "windows");
}

static bool triple_is_x86(const char *triple)
{
    if (!triple) return false;
    if (str_contains_ci(triple, "x86_64")) return false;
    if (str_contains_ci(triple, "amd64")) return false;
    return str_contains_ci(triple, "i386") || str_contains_ci(triple, "i486") ||
           str_contains_ci(triple, "i586") || str_contains_ci(triple, "i686") ||
           str_contains_ci(triple, "x86");
}

static LLVMTypeRef llvm_int_type(Codegen *g, TypeKind k)
{
    switch (k)
    {
    case TY_BYTE:
    case TY_CHAR:
    case TY_I8:
    case TY_U8: return g->i8;
    case TY_I16:
    case TY_U16: return g->i16;
    case TY_I32:
    case TY_U32: return g->i32;
    case TY_INT:
    case TY_I64:
    case TY_U64:
    case TY_ISIZE:
    case TY_USIZE: return g->i64;
    default: return g->i64;
    }
}

enum
{
    KN_ANY_TAG_NONE = 0,
    KN_ANY_TAG_I64  = 1,
    KN_ANY_TAG_F64  = 2,
    KN_ANY_TAG_BOOL = 3,
    KN_ANY_TAG_CHAR = 4,
    KN_ANY_TAG_STRING = 5,
    KN_ANY_TAG_PTR  = 6,
    KN_ANY_TAG_DICT = 7,
    KN_ANY_TAG_LIST = 8,
    KN_ANY_TAG_SET  = 9
};

static LLVMValueRef box_any_value(Codegen *g, CGValue v);
static LLVMValueRef any_to_type(Codegen *g, LLVMValueRef anyv, Type dst);
static LLVMValueRef build_string_const(Codegen *g, const char *s, int len);
static LLVMValueRef any_null_value(Codegen *g);

static LLVMValueRef cast_value(Codegen *g, CGValue v, Type dst)
{
    if (v.type.kind == TY_NULL)
    {
        if (dst.kind == TY_ANY)
            return any_null_value(g);
        return LLVMConstNull(llvm_type(g, dst));
    }
    if (dst.kind == TY_ANY)
        return box_any_value(g, v);
    if (v.type.kind == TY_ANY)
        return any_to_type(g, v.value, dst);
    if (type_equal(v.type, dst))
        return v.value;

    LLVMTypeRef dst_ty = llvm_type(g, dst);
    LLVMTypeRef src_ty = LLVMTypeOf(v.value);

    if (is_integer_kind(dst.kind) && is_integer_kind(v.type.kind))
    {
        int sb = int_bits(v.type.kind);
        int db = int_bits(dst.kind);
        if (sb < db)
        {
            if (is_unsigned_kind(v.type.kind))
                return LLVMBuildZExt(g->builder, v.value, dst_ty, "zext");
            return LLVMBuildSExt(g->builder, v.value, dst_ty, "sext");
        }
        if (sb > db)
            return LLVMBuildTrunc(g->builder, v.value, dst_ty, "trunc");
        return LLVMBuildBitCast(g->builder, v.value, dst_ty, "icast");
    }
    if (is_integer_kind(dst.kind) && v.type.kind == TY_ENUM && LLVMGetTypeKind(src_ty) == LLVMIntegerTypeKind)
    {
        unsigned sb = LLVMGetIntTypeWidth(src_ty);
        int db = int_bits(dst.kind);
        if ((int)sb < db)
            return LLVMBuildSExt(g->builder, v.value, dst_ty, "eext");
        if ((int)sb > db)
            return LLVMBuildTrunc(g->builder, v.value, dst_ty, "etrunc");
        return LLVMBuildBitCast(g->builder, v.value, dst_ty, "ecast");
    }

    if (is_integer_kind(dst.kind) && v.type.kind == TY_BOOL)
        return LLVMBuildZExt(g->builder, v.value, dst_ty, "b2i");

    if (is_float_kind(dst.kind) && is_integer_kind(v.type.kind))
    {
        if (is_unsigned_kind(v.type.kind))
            return LLVMBuildUIToFP(g->builder, v.value, dst_ty, "uitofp");
        return LLVMBuildSIToFP(g->builder, v.value, dst_ty, "sitofp");
    }
    if (is_float_kind(dst.kind) && v.type.kind == TY_BOOL)
        return LLVMBuildUIToFP(g->builder, v.value, dst_ty, "btof");

    if (is_float_kind(dst.kind) && is_float_kind(v.type.kind))
    {
        int sb = float_bits(v.type.kind);
        int db = float_bits(dst.kind);
        if (sb < db)
            return LLVMBuildFPExt(g->builder, v.value, dst_ty, "fpext");
        if (sb > db)
            return LLVMBuildFPTrunc(g->builder, v.value, dst_ty, "fptrunc");
        return v.value;
    }

    if (is_integer_kind(dst.kind) && is_float_kind(v.type.kind))
    {
        if (is_unsigned_kind(dst.kind))
            return LLVMBuildFPToUI(g->builder, v.value, dst_ty, "fptoui");
        return LLVMBuildFPToSI(g->builder, v.value, dst_ty, "fptosi");
    }

    if (dst.kind == TY_BOOL && is_integer_kind(v.type.kind))
    {
        LLVMValueRef zero = LLVMConstInt(llvm_int_type(g, v.type.kind), 0, 0);
        return LLVMBuildICmp(g->builder, LLVMIntNE, v.value, zero, "i2b");
    }
    if (dst.kind == TY_BOOL && is_float_kind(v.type.kind))
    {
        LLVMValueRef zero = LLVMConstReal(llvm_float_type(g, v.type.kind), 0.0);
        return LLVMBuildFCmp(g->builder, LLVMRealONE, v.value, zero, "f2b");
    }

    if (dst.kind == TY_STRING)
    {
        if (v.type.kind == TY_STRING) return v.value;
        if (v.type.kind == TY_PTR && v.type.elem == TY_CHAR && v.type.ptr_depth == 1)
            return LLVMBuildBitCast(g->builder, v.value, g->i8ptr, "cstr2s");
        if (g->env_kind == KN_ENV_FREESTANDING && g->runtime_mode == KN_RUNTIME_NONE)
        {
            if (v.type.kind == TY_NULL)
                return build_string_const(g, "null", 4);
            if (v.type.kind == TY_BOOL)
            {
                LLVMValueRef tstr = build_string_const(g, "true", 4);
                LLVMValueRef fstr = build_string_const(g, "false", 5);
                return LLVMBuildSelect(g->builder, v.value, tstr, fstr, "b2s");
            }
            if (is_float_kind(v.type.kind))
            {
                CGValue fv = v;
                if (v.type.kind != TY_FLOAT && v.type.kind != TY_F64)
                {
                    fv.value = LLVMBuildFPExt(g->builder, v.value, g->f64, "f2s.ext");
                    fv.type = type_make(TY_F64);
                }
                return LLVMBuildCall2(g->builder, g->fn_f64_to_string_ty, g->fn_f64_to_string, &fv.value, 1, "f2s");
            }
            if (v.type.kind == TY_CHAR)
                return LLVMBuildCall2(g->builder, g->fn_char_to_string_ty, g->fn_char_to_string, &v.value, 1, "c2s");
            if (is_integer_kind(v.type.kind) || v.type.kind == TY_ENUM)
            {
                LLVMValueRef iv = cast_value(g, v, type_make(TY_I64));
                return LLVMBuildCall2(g->builder, g->fn_i64_to_string_ty, g->fn_i64_to_string, &iv, 1, "i2s");
            }
            if (v.type.kind == TY_PTR || v.type.kind == TY_CLASS)
            {
                LLVMValueRef pv = LLVMBuildBitCast(g->builder, v.value, g->i8ptr, "p2s.cast");
                return LLVMBuildCall2(g->builder, g->fn_ptr_to_string_ty, g->fn_ptr_to_string, &pv, 1, "p2s");
            }
        }
        // Route all non-string -> string casts through `any` conversion.
        // This avoids duplicated conversion paths and keeps semantics identical
        // to Console.Print/PrintLine vararg conversion.
        LLVMValueRef anyv = box_any_value(g, v);
        return LLVMBuildCall2(g->builder, g->fn_any_to_string_ty, g->fn_any_to_string, &anyv, 1, "v2s");
    }

    if (v.type.kind == TY_STRING)
    {
        if (is_integer_kind(dst.kind) || dst.kind == TY_ENUM)
        {
            LLVMValueRef sv = v.value;
            LLVMValueRef iv = LLVMBuildCall2(g->builder, g->fn_string_to_i64_ty, g->fn_string_to_i64, &sv, 1, "s2i");
            CGValue tmp; tmp.value = iv; tmp.type = type_make(TY_I64);
            return cast_value(g, tmp, dst);
        }
        if (is_float_kind(dst.kind))
        {
            LLVMValueRef sv = v.value;
            LLVMValueRef f64v = LLVMBuildCall2(g->builder, g->fn_string_to_f64_ty, g->fn_string_to_f64, &sv, 1, "s2f");
            if (dst.kind == TY_FLOAT || dst.kind == TY_F64)
                return f64v;
            CGValue tmp; tmp.value = f64v; tmp.type = type_make(TY_F64);
            return cast_value(g, tmp, dst);
        }
        if (dst.kind == TY_BOOL)
        {
            LLVMValueRef sv = v.value;
            return LLVMBuildCall2(g->builder, g->fn_string_to_bool_ty, g->fn_string_to_bool, &sv, 1, "s2b");
        }
        if (dst.kind == TY_CHAR)
        {
            LLVMValueRef sv = v.value;
            return LLVMBuildCall2(g->builder, g->fn_string_to_char_ty, g->fn_string_to_char, &sv, 1, "s2c");
        }
    }

    if (LLVMGetTypeKind(src_ty) == LLVMPointerTypeKind && LLVMGetTypeKind(dst_ty) == LLVMPointerTypeKind)
        return LLVMBuildBitCast(g->builder, v.value, dst_ty, "pcast");
    if (is_integer_kind(dst.kind) && LLVMGetTypeKind(src_ty) == LLVMPointerTypeKind)
        return LLVMBuildPtrToInt(g->builder, v.value, dst_ty, "p2i");
    if (dst.kind == TY_PTR && is_integer_kind(v.type.kind))
        return LLVMBuildIntToPtr(g->builder, v.value, dst_ty, "i2p");

    return v.value;
}

static LLVMValueRef box_any_value(Codegen *g, CGValue v)
{
    if (v.type.kind == TY_ANY)
        return v.value;

    LLVMValueRef tag = LLVMConstInt(g->i64, KN_ANY_TAG_NONE, 0);
    LLVMValueRef payload = LLVMConstInt(g->i64, 0, 0);

    if (is_float_kind(v.type.kind))
    {
        tag = LLVMConstInt(g->i64, KN_ANY_TAG_F64, 0);
        LLVMValueRef fv = v.value;
        if (v.type.kind != TY_FLOAT && v.type.kind != TY_F64)
            fv = (float_bits(v.type.kind) < 64)
                ? LLVMBuildFPExt(g->builder, v.value, g->f64, "any.fpext")
                : LLVMBuildFPTrunc(g->builder, v.value, g->f64, "any.fptrunc");
        payload = LLVMBuildBitCast(g->builder, fv, g->i64, "any.f64");
    }
    else if (v.type.kind == TY_NULL)
    {
        tag = LLVMConstInt(g->i64, KN_ANY_TAG_NONE, 0);
        payload = LLVMConstInt(g->i64, 0, 0);
    }
    else if (v.type.kind == TY_BOOL)
    {
        tag = LLVMConstInt(g->i64, KN_ANY_TAG_BOOL, 0);
        payload = LLVMBuildZExt(g->builder, v.value, g->i64, "any.bool");
    }
    else if (v.type.kind == TY_CHAR)
    {
        tag = LLVMConstInt(g->i64, KN_ANY_TAG_CHAR, 0);
        payload = LLVMBuildZExt(g->builder, v.value, g->i64, "any.char");
    }
    else if (is_integer_kind(v.type.kind) || v.type.kind == TY_ENUM)
    {
        tag = LLVMConstInt(g->i64, KN_ANY_TAG_I64, 0);
        payload = cast_value(g, v, type_make(TY_I64));
    }
    else if (v.type.kind == TY_STRING)
    {
        LLVMValueRef nullp = LLVMConstNull(llvm_type(g, v.type));
        LLVMValueRef is_null = LLVMBuildICmp(g->builder, LLVMIntEQ, v.value, nullp, "any.str.isnull");
        LLVMValueRef raw_payload = LLVMBuildPtrToInt(g->builder, v.value, g->i64, "any.str");
        tag = LLVMBuildSelect(g->builder, is_null,
                              LLVMConstInt(g->i64, KN_ANY_TAG_NONE, 0),
                              LLVMConstInt(g->i64, KN_ANY_TAG_STRING, 0),
                              "any.str.tag");
        payload = LLVMBuildSelect(g->builder, is_null,
                                  LLVMConstInt(g->i64, 0, 0),
                                  raw_payload,
                                  "any.str.payload");
    }
    else if (v.type.kind == TY_PTR || v.type.kind == TY_CLASS)
    {
        LLVMValueRef pv = LLVMBuildBitCast(g->builder, v.value, g->i8ptr, "any.ptr.cast");
        LLVMValueRef is_null = LLVMBuildICmp(g->builder, LLVMIntEQ, pv, LLVMConstNull(g->i8ptr), "any.ptr.isnull");
        LLVMValueRef raw_payload = LLVMBuildPtrToInt(g->builder, pv, g->i64, "any.ptr");
        LLVMValueRef tag_non_null = LLVMConstInt(g->i64, KN_ANY_TAG_PTR, 0);
        if (v.type.kind == TY_CLASS && v.type.name)
        {
            if (kn_strcmp(v.type.name, "IO.Collection.dict") == 0)
                tag_non_null = LLVMConstInt(g->i64, KN_ANY_TAG_DICT, 0);
            else if (kn_strcmp(v.type.name, "IO.Collection.list") == 0)
                tag_non_null = LLVMConstInt(g->i64, KN_ANY_TAG_LIST, 0);
            else if (kn_strcmp(v.type.name, "IO.Collection.set") == 0)
                tag_non_null = LLVMConstInt(g->i64, KN_ANY_TAG_SET, 0);
        }
        tag = LLVMBuildSelect(g->builder, is_null,
                              LLVMConstInt(g->i64, KN_ANY_TAG_NONE, 0),
                              tag_non_null,
                              "any.ptr.tag");
        payload = LLVMBuildSelect(g->builder, is_null,
                                  LLVMConstInt(g->i64, 0, 0),
                                  raw_payload,
                                  "any.ptr.payload");
    }
    else
    {
        // Box struct/array/other value types as heap-allocated blob.
        LLVMTypeRef vt = llvm_type(g, v.type);
        LLVMValueRef sizev = LLVMSizeOf(vt);
        if (LLVMGetTypeKind(LLVMTypeOf(sizev)) != LLVMIntegerTypeKind || LLVMGetIntTypeWidth(LLVMTypeOf(sizev)) != 64)
            sizev = LLVMBuildZExtOrBitCast(g->builder, sizev, g->i64, "aboxsz");
        LLVMValueRef mem = LLVMBuildCall2(g->builder, g->fn_gc_alloc_ty, g->fn_gc_alloc,
                                          (LLVMValueRef[]){ sizev }, 1, "abox");
        LLVMValueRef typed = LLVMBuildBitCast(g->builder, mem, LLVMPointerType(vt, 0), "abox.ptr");
        LLVMBuildStore(g->builder, v.value, typed);
        tag = LLVMConstInt(g->i64, KN_ANY_TAG_PTR, 0);
        payload = LLVMBuildPtrToInt(g->builder, mem, g->i64, "aboxp");
    }

    LLVMValueRef res = LLVMGetUndef(g->any_ty);
    res = LLVMBuildInsertValue(g->builder, res, tag, 0, "any.tag");
    res = LLVMBuildInsertValue(g->builder, res, payload, 1, "any.payload");
    return res;
}

static LLVMValueRef any_to_type(Codegen *g, LLVMValueRef anyv, Type dst)
{
    if (dst.kind == TY_STRING)
        return LLVMBuildCall2(g->builder, g->fn_any_to_string_ty, g->fn_any_to_string, &anyv, 1, "any2s");
    if (is_float_kind(dst.kind))
    {
        LLVMValueRef f64v = LLVMBuildCall2(g->builder, g->fn_any_to_f64_ty, g->fn_any_to_f64, &anyv, 1, "any2f");
        if (dst.kind == TY_FLOAT || dst.kind == TY_F64)
            return f64v;
        CGValue tmp; tmp.value = f64v; tmp.type = type_make(TY_F64);
        return cast_value(g, tmp, dst);
    }
    if (dst.kind == TY_BOOL)
        return LLVMBuildCall2(g->builder, g->fn_any_to_bool_ty, g->fn_any_to_bool, &anyv, 1, "any2b");
    if (dst.kind == TY_CHAR)
        return LLVMBuildCall2(g->builder, g->fn_any_to_char_ty, g->fn_any_to_char, &anyv, 1, "any2c");
    if (dst.kind == TY_PTR || dst.kind == TY_CLASS)
    {
        LLVMValueRef pv = LLVMBuildCall2(g->builder, g->fn_any_to_ptr_ty, g->fn_any_to_ptr, &anyv, 1, "any2p");
        return LLVMBuildBitCast(g->builder, pv, llvm_type(g, dst), "any2p.cast");
    }
    if (is_integer_kind(dst.kind) || dst.kind == TY_ENUM)
    {
        LLVMValueRef iv = LLVMBuildCall2(g->builder, g->fn_any_to_i64_ty, g->fn_any_to_i64, &anyv, 1, "any2i");
        CGValue tmp; tmp.value = iv; tmp.type = type_make(TY_I64);
        return cast_value(g, tmp, dst);
    }
    return LLVMConstNull(llvm_type(g, dst));
}

static LLVMValueRef any_null_value(Codegen *g)
{
    LLVMValueRef res = LLVMGetUndef(g->any_ty);
    res = LLVMBuildInsertValue(g->builder, res, LLVMConstInt(g->i64, KN_ANY_TAG_NONE, 0), 0, "any.null.tag");
    res = LLVMBuildInsertValue(g->builder, res, LLVMConstInt(g->i64, 0, 0), 1, "any.null.payload");
    return res;
}

static VarSymbol *scope_find(Scope *s, const char *name)
{
    for (Scope *c = s; c; c = c->parent)
    {
        VarSymbol *v = c->vars;
        while (v)
        {
            if (kn_strcmp(v->name, name) == 0) return v;
            v = v->next;
        }
    }
    return 0;
}

static void scope_add(Scope *s, const char *name, Type type, LLVMValueRef val)
{
    VarSymbol *v = (VarSymbol *)kn_malloc(sizeof(VarSymbol));
    if (!v) kn_die("out of memory");
    v->name = name;
    v->type = type;
    v->value = val;
    v->next = s->vars;
    s->vars = v;
}

static LLVMValueRef build_string_const(Codegen *g, const char *s, int len)
{
    LLVMValueRef str = LLVMConstStringInContext(g->ctx, s, (unsigned)len, 0);
    LLVMTypeRef arr_ty = LLVMArrayType(g->i8, (unsigned)(len + 1));
    LLVMValueRef gvar = LLVMAddGlobal(g->mod, arr_ty, "");
    LLVMSetLinkage(gvar, LLVMPrivateLinkage);
    LLVMSetGlobalConstant(gvar, 1);
    LLVMSetInitializer(gvar, str);
    LLVMValueRef zero = LLVMConstInt(g->i32, 0, 0);
    LLVMValueRef idxs[2] = { zero, zero };
    return LLVMConstInBoundsGEP2(arr_ty, gvar, idxs, 2);
}

static void append_int(char *buf, size_t cap, size_t *off, int v)
{
    char tmp[32];
    int n = 0;
    if (v == 0)
        tmp[n++] = '0';
    else
    {
        int x = v;
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

static const char *build_trace_label(const char *name, const KnSource *src, int line, int col)
{
    char buf[1024];
    size_t off = 0;
    size_t name_len = name ? kn_strlen(name) : 0;
    int add_call_suffix = 1;

    if (!name || !name[0])
        name = "<unknown>";

    if (name[0] == '<')
        add_call_suffix = 0;
    else if (name_len > 0 && name[name_len - 1] == ')')
        add_call_suffix = 0;

    kn_append(buf, sizeof(buf), &off, name);
    if (add_call_suffix)
        kn_append(buf, sizeof(buf), &off, "()");

    if (src && src->path && src->path[0])
    {
        kn_append(buf, sizeof(buf), &off, "  ");
        kn_append(buf, sizeof(buf), &off, src->path);
        if (line > 0)
        {
            kn_append(buf, sizeof(buf), &off, ":");
            append_int(buf, sizeof(buf), &off, line);
            if (col > 0)
            {
                kn_append(buf, sizeof(buf), &off, ":");
                append_int(buf, sizeof(buf), &off, col);
            }
        }
    }

    {
        char *out = (char *)kn_malloc(off + 1);
        if (!out)
            return name;
        kn_memcpy(out, buf, off + 1);
        return out;
    }
}

static LLVMTypeRef fn_type(LLVMValueRef fn)
{
    LLVMTypeRef ty = LLVMTypeOf(fn);
    if (LLVMGetTypeKind(ty) == LLVMPointerTypeKind)
        ty = LLVMGetElementType(ty);
    return ty;
}

