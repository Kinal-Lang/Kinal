#include "kn/sema.h"
#include "kn/diag.h"
#include "kn/source.h"
#include "kn/std.h"
#include "kn/lexer.h"
#include "kn/util.h"
#include "kn/version.h"

typedef struct VarSym VarSym;
struct VarSym
{
    const char *name;
    Type type;
    Type async_result_type;
    int is_const;
    Expr *origin;
    int used;
    int is_param;
    int line;
    int col;
    const KnSource *src;
    VarSym *next;
};

typedef struct Scope Scope;
struct Scope
{
    VarSym *vars;
    Scope *parent;
};

typedef struct
{
    const char *qname;
    Func *func;
} FuncSym;

typedef struct
{
    FuncSym *items;
    int count;
    int cap;
} FuncTable;

typedef struct
{
    const char *key;
    Func *func;
} GenericInst;

typedef struct
{
    GenericInst *items;
    int count;
    int cap;
} GenericInstTable;

typedef struct
{
    const KnSource *src;
    MetaList *metas;
    ImportList *imports;
    ClassList *classes;
    InterfaceList *interfaces;
    StructList *structs;
    EnumList *enums;
    FuncList *func_list;
    FuncTable funcs;
    GenericInstTable generic_insts;
    Scope *scope;
    Func *current;
    Method *current_method;
    ClassDecl *current_class;
    int current_is_static;
    const char *current_unit;
    int loop_depth;
    int try_depth;
    int in_block_literal;
    BlockRecordList *block_records;
    Safety block_safety;
    int target_os;
    int target_arch;
    int target_env;
    int target_pointer_bits;
    const char *target_path_separator;
    const char *target_new_line;
    const char *target_exe_suffix;
    const char *target_object_suffix;
    const char *target_dynamic_library_suffix;
    const char *target_static_library_suffix;
    int host_os;
    int host_arch;
    int host_env;
    int host_pointer_bits;
    const char *host_path_separator;
    const char *host_new_line;
    const char *host_exe_suffix;
    const char *host_object_suffix;
    const char *host_dynamic_library_suffix;
    const char *host_static_library_suffix;
    int runtime_backend;
} Sema;

static bool type_equal(Type a, Type b);
static bool params_equal(const ParamList *a, const ParamList *b);
static bool type_supported(Type t);
static bool method_is_virtual(const Method *m);
static bool type_assignable(Sema *s, Type dst, Type src);
static VarSym *scope_find(Scope *s, const char *name);
static bool eval_const_expr(Expr *e, int64_t *out);
static bool builtin_is_vararg(KnBuiltinKind kind);
static ClassDecl *find_class(Sema *s, const char *name);
static InterfaceDecl *find_interface(Sema *s, const char *name);
static StructDecl *find_struct(Sema *s, const char *name);
static EnumDecl *find_enum(Sema *s, const char *name);
static bool builtin_conv_target(KnBuiltinKind kind, Type *out);
static bool is_error_type(Type t);
static bool is_async_task_type(Type t);
static bool is_function_object_type(Sema *s, Type t);
static bool is_block_object_type(Type t);
static int block_record_ip(BlockRecordList *records, const char *name);
static bool class_is_derived_from(Sema *s, ClassDecl *c, ClassDecl *base);
static Type sema_expr(Sema *s, Expr *e);
static Type sema_array_literal(Sema *s, Expr *e, Type expected_elem, bool has_expected);
static int bind_default_literal_type(Sema *s, Expr *e, Type expected, const char *detail);
static Type sema_expr_with_expected(Sema *s, Expr *e, Type expected, const char *detail);
static const char *type_mismatch_detail(Type expected, Type got);
static void check_call_safety(Sema *s, Safety callee, int line, int col, int len);
static void check_ptr_safety(Sema *s, int line, int col, int len, const char *detail);
static void check_int_to_ptr_safety(Sema *s, Type dst, Type src, Expr *expr);
static void check_byte_literal_range(Sema *s, Type dst, Expr *src);
static bool func_is_generic_template(const Func *f);
static bool func_is_generic_instance(const Func *f);
static void retype_prepared_args(Sema *s, ExprList *args, ParamList *typed_args,
                                 const Param *params, int param_count, const char *default_detail);
static bool stmt_contains_unsafe_block(Stmt *st);
static void warn_unused_scope(Sema *s, Scope *sc);

static void sema_error(Sema *s, int line, int col, int len, const char *title, const char *detail)
{
    if (s && (!s->src || !s->src->path || !s->src->path[0]) && kn_diag_error_count() > 0)
        return;
    kn_diag_report(s->src, KN_STAGE_SEMA, line, col, len, title, detail, 0);
}

static void sema_warn(Sema *s, int warn_level, int line, int col, int len, const char *title, const char *detail)
{
    if (!s || !s->src)
        return;
    kn_diag_warn(s->src, KN_STAGE_SEMA, warn_level, line, col, len, title, detail);
}

static void sema_u32_to_text(uint32_t v, char out[16])
{
    char tmp[16];
    int n = 0;
    if (v == 0)
    {
        out[0] = '0';
        out[1] = 0;
        return;
    }
    while (v > 0 && n < (int)sizeof(tmp))
    {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    int off = 0;
    for (int i = n - 1; i >= 0; i--)
        out[off++] = tmp[i];
    out[off] = 0;
}

static bool qname_has_dot(const char *name)
{
    if (!name) return false;
    for (const char *p = name; *p; p++)
        if (*p == '.') return true;
    return false;
}

static const char *join_qualified_name(const char *unit, const char *name)
{
    if (!name) return 0;
    if (!unit || !unit[0] || qname_has_dot(name))
        return name;
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

static const char *prepend_unit_name(const char *unit, const char *name)
{
    if (!name) return 0;
    if (!unit || !unit[0])
        return name;
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

static bool import_has_alias_for_module(Sema *s, const char *module)
{
    if (!s || !s->imports || !module) return false;
    for (int i = 0; i < s->imports->count; i++)
    {
        ImportMap *m = &s->imports->items[i];
        if (m->unit && m->unit[0] && s->current_unit && s->current_unit[0] &&
            kn_strcmp(m->unit, s->current_unit) != 0)
            continue;
        if (m->kind != IMPORT_ALIAS || !m->module) continue;
        if (kn_strcmp(m->module, module) == 0)
            return true;
    }
    return false;
}

static bool import_has_open_module(Sema *s, const char *module)
{
    if (!s || !s->imports || !module) return false;
    for (int i = 0; i < s->imports->count; i++)
    {
        ImportMap *m = &s->imports->items[i];
        if (m->unit && m->unit[0] && s->current_unit && s->current_unit[0] &&
            kn_strcmp(m->unit, s->current_unit) != 0)
            continue;
        if (m->kind != IMPORT_OPEN || !m->module) continue;
        if (kn_strcmp(m->module, module) == 0)
            return true;
    }
    return false;
}

static bool module_accessible_by_namespace(Sema *s, const char *module)
{
    if (!module || !module[0]) return false;
    if (s && s->current_unit && kn_strcmp(s->current_unit, module) == 0)
        return true;
    if (import_has_alias_for_module(s, module))
        return false;
    return import_has_open_module(s, module);
}

static bool qname_uses_hidden_module_alias(Sema *s, const char *name)
{
    if (!s || !s->imports || !name || !qname_has_dot(name)) return false;
    for (int i = 0; i < s->imports->count; i++)
    {
        ImportMap *m = &s->imports->items[i];
        if (m->unit && m->unit[0] && s->current_unit && s->current_unit[0] &&
            kn_strcmp(m->unit, s->current_unit) != 0)
            continue;
        if (m->kind != IMPORT_ALIAS || !m->module || !m->module[0]) continue;
        size_t ulen = kn_strlen(m->module);
        if (kn_strncmp(name, m->module, ulen) != 0) continue;
        if (name[ulen] != '.') continue;
        return true;
    }
    return false;
}

static bool qname_matches_decl(const char *name, const char *decl_name, const char *decl_unit, const char *decl_qname)
{
    if (!name || !decl_name) return false;
    if (decl_qname && kn_strcmp(name, decl_qname) == 0)
        return true;
    if (kn_strcmp(name, decl_name) == 0)
        return true;
    if (!decl_unit || !decl_unit[0]) return false;
    size_t ulen = kn_strlen(decl_unit);
    size_t nlen = kn_strlen(decl_name);
    size_t qlen = kn_strlen(name);
    return qlen == ulen + 1 + nlen &&
           kn_strncmp(name, decl_unit, ulen) == 0 &&
           name[ulen] == '.' &&
           kn_strcmp(name + ulen + 1, decl_name) == 0;
}

static const char *expand_builtin_core_alias(const char *name)
{
    if (!name) return name;
    if (kn_strcmp(name, "dict") == 0)
        return "IO.Collection.dict";
    if (kn_strcmp(name, "list") == 0)
        return "IO.Collection.list";
    if (kn_strcmp(name, "set") == 0)
        return "IO.Collection.set";
    {
        const char *obj_prefix = "Object.";
        size_t obj_len = kn_strlen(obj_prefix);
        if (kn_strncmp(name, obj_prefix, obj_len) == 0)
            return join_qualified_name("IO.Type.Object", name + obj_len);
    }
    return name;
}

static bool builtin_type_qname_globally_visible(const char *qname)
{
    if (!qname) return false;
    if (kn_strcmp(qname, "IO.Error") == 0)
        return true;
    if (kn_strncmp(qname, "IO.Type.Object.", 15) == 0)
        return true;
    if (kn_strncmp(qname, "IO.Collection.", 14) == 0)
        return true;
    return false;
}

static int sema_is_freestanding_core(void)
{
    return kn_std_get_profile() == KN_STD_PROFILE_FREESTANDING_CORE;
}

static const char *expand_alias_qualified_name(Sema *s, const char *name)
{
    if (!s || !s->imports || !name) return name;
    const char *dot = 0;
    for (const char *p = name; *p; p++)
    {
        if (*p == '.')
        {
            dot = p;
            break;
        }
    }
    if (!dot || dot == name || !dot[1]) return name;

    size_t alias_len = (size_t)(dot - name);
    for (int i = 0; i < s->imports->count; i++)
    {
        ImportMap *m = &s->imports->items[i];
        if (m->unit && m->unit[0] && s->current_unit && s->current_unit[0] &&
            kn_strcmp(m->unit, s->current_unit) != 0)
            continue;
        if (m->kind != IMPORT_ALIAS || !m->local || !m->module) continue;
        if (kn_strlen(m->local) != alias_len) continue;
        if (kn_strncmp(m->local, name, alias_len) != 0) continue;
        return join_qualified_name(m->module, dot + 1);
    }
    return name;
}

static int named_decl_match_score(Sema *s, const char *raw, const char *decl_name,
                                  const char *decl_unit, const char *decl_qname)
{
    if (!raw || !decl_name) return 0;
    const char *name = expand_builtin_core_alias(raw);
    if (decl_qname && builtin_type_qname_globally_visible(name) && kn_strcmp(name, decl_qname) == 0)
        return qname_has_dot(raw) ? 100 : 85;
    if (decl_qname && kn_strcmp(raw, decl_qname) == 0)
        return 100;
    if (qname_matches_decl(name, decl_name, decl_unit, decl_qname))
        return qname_has_dot(name) ? 95 : 60;

    if (qname_has_dot(name))
    {
        const char *expanded = expand_alias_qualified_name(s, name);
        if (expanded != name && qname_matches_decl(expanded, decl_name, decl_unit, decl_qname))
            return 90;
        if (s && s->current_unit && s->current_unit[0])
        {
            const char *local_q = prepend_unit_name(s->current_unit, name);
            if (qname_matches_decl(local_q, decl_name, decl_unit, decl_qname))
                return 88;
        }
        return 0;
    }

    if (s && s->current_unit)
    {
        const char *local_q = join_qualified_name(s->current_unit, name);
        if (qname_matches_decl(local_q, decl_name, decl_unit, decl_qname))
            return 80;
    }

    if (s && s->imports)
    {
        for (int i = 0; i < s->imports->count; i++)
        {
            ImportMap *m = &s->imports->items[i];
            if (m->unit && m->unit[0] && s->current_unit && s->current_unit[0] &&
                kn_strcmp(m->unit, s->current_unit) != 0)
                continue;
            if (m->kind != IMPORT_OPEN || !m->module) continue;
            const char *open_q = join_qualified_name(m->module, name);
            if (qname_matches_decl(open_q, decl_name, decl_unit, decl_qname))
                return 70;
        }
    }

    return 0;
}

static int visible_decl_match_score(Sema *s, const char *raw, const char *decl_name,
                                    const char *decl_unit, const char *decl_qname)
{
    if (!raw || !decl_name) return 0;
    const char *name = expand_builtin_core_alias(raw);
    if (builtin_type_qname_globally_visible(name) &&
        qname_matches_decl(name, decl_name, decl_unit, decl_qname))
        return qname_has_dot(raw) ? 100 : 85;

    if (qname_has_dot(name))
    {
        const char *expanded = expand_alias_qualified_name(s, name);
        if (import_has_open_module(s, name) && qname_matches_decl(name, decl_name, decl_unit, decl_qname))
            return 95;
        if (expanded != name && qname_matches_decl(expanded, decl_name, decl_unit, decl_qname))
        {
            if (import_has_open_module(s, expanded))
                return 95;
            return 100;
        }
        if (s && s->current_unit && s->current_unit[0])
        {
            const char *local_q = prepend_unit_name(s->current_unit, name);
            if (qname_matches_decl(local_q, decl_name, decl_unit, decl_qname))
                return 98;
        }
        if (decl_unit && module_accessible_by_namespace(s, decl_unit) &&
            qname_matches_decl(name, decl_name, decl_unit, decl_qname))
            return 95;
        return 0;
    }

    if ((!decl_unit || !decl_unit[0]) &&
        (!s || !s->current_unit || !s->current_unit[0]) &&
        kn_strcmp(name, decl_name) == 0)
        return 80;

    if (s && s->current_unit)
    {
        const char *local_q = join_qualified_name(s->current_unit, name);
        if (qname_matches_decl(local_q, decl_name, decl_unit, decl_qname))
            return 80;
    }

    if (s && s->imports)
    {
        for (int i = 0; i < s->imports->count; i++)
        {
            ImportMap *m = &s->imports->items[i];
            if (m->unit && m->unit[0] && s->current_unit && s->current_unit[0] &&
                kn_strcmp(m->unit, s->current_unit) != 0)
                continue;
            if (m->kind != IMPORT_OPEN || !m->module) continue;
            const char *open_q = join_qualified_name(m->module, name);
            if (qname_matches_decl(open_q, decl_name, decl_unit, decl_qname))
                return 70;
        }
    }

    return 0;
}

static const char *type_decl_lookup_name(const char *name, const char *qname, const char *owner_type_qname)
{
    if (owner_type_qname && owner_type_qname[0])
        return qname ? qname : name;
    return name;
}

static ClassDecl *find_visible_class(Sema *s, const char *name)
{
    ClassDecl *best = 0;
    int best_score = 0;
    if (!s || !name) return 0;
    for (int i = 0; i < s->classes->count; i++)
    {
        ClassDecl *c = &s->classes->items[i];
        int score = visible_decl_match_score(s, name,
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

static InterfaceDecl *find_visible_interface(Sema *s, const char *name)
{
    InterfaceDecl *best = 0;
    int best_score = 0;
    if (!s || !name) return 0;
    for (int i = 0; i < s->interfaces->count; i++)
    {
        InterfaceDecl *it = &s->interfaces->items[i];
        int score = visible_decl_match_score(s, name,
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

static StructDecl *find_visible_struct(Sema *s, const char *name)
{
    StructDecl *best = 0;
    int best_score = 0;
    if (!s || !name) return 0;
    for (int i = 0; i < s->structs->count; i++)
    {
        StructDecl *st = &s->structs->items[i];
        int score = visible_decl_match_score(s, name,
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

static EnumDecl *find_visible_enum(Sema *s, const char *name)
{
    EnumDecl *best = 0;
    int best_score = 0;
    if (!s || !name) return 0;
    for (int i = 0; i < s->enums->count; i++)
    {
        EnumDecl *en = &s->enums->items[i];
        int score = visible_decl_match_score(s, name,
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

static const char *class_qname(const ClassDecl *c)
{
    return c ? (c->qname ? c->qname : c->name) : 0;
}

static const char *interface_qname(const InterfaceDecl *i)
{
    return i ? (i->qname ? i->qname : i->name) : 0;
}

static const char *struct_qname(const StructDecl *st)
{
    return st ? (st->qname ? st->qname : st->name) : 0;
}

static const char *enum_qname(const EnumDecl *e)
{
    return e ? (e->qname ? e->qname : e->name) : 0;
}

static void sema_error_arg_count_named(Sema *s, int line, int col, int len, const char *name, int expected)
{
    char detail[256];
    char nbuf[16];
    size_t off = 0;
    detail[0] = 0;

    if (!name || !name[0]) name = "Function";
    if (expected < 0) expected = 0;

    sema_u32_to_text((uint32_t)expected, nbuf);
    kn_append(detail, sizeof(detail), &off, name);
    kn_append(detail, sizeof(detail), &off, "() takes ");
    kn_append(detail, sizeof(detail), &off, nbuf);
    kn_append(detail, sizeof(detail), &off, " arguments");

    sema_error(s, line, col, len, "Argument Count", detail);
}

static void sema_error_type_must_be_named(Sema *s, int line, int col, int len,
                                          const char *name, const char *subject, const char *expected)
{
    char detail[256];
    size_t off = 0;
    detail[0] = 0;

    if (!name || !name[0]) name = "Value";
    if (!subject || !subject[0]) subject = "value";
    if (!expected || !expected[0]) expected = "valid type";

    kn_append(detail, sizeof(detail), &off, name);
    kn_append(detail, sizeof(detail), &off, "() ");
    kn_append(detail, sizeof(detail), &off, subject);
    kn_append(detail, sizeof(detail), &off, " must be ");
    kn_append(detail, sizeof(detail), &off, expected);

    sema_error(s, line, col, len, "Type Mismatch", detail);
}

static void sema_error_named_arg_unknown(Sema *s, int line, int col, int len, const char *arg_name)
{
    char detail[256];
    size_t off = 0;
    detail[0] = 0;
    if (!arg_name || !arg_name[0]) arg_name = "parameter";
    kn_append(detail, sizeof(detail), &off, "Unknown named argument '");
    kn_append(detail, sizeof(detail), &off, arg_name);
    kn_append(detail, sizeof(detail), &off, "'");
    sema_error(s, line, col, len, "Unknown Parameter", detail);
}

static void sema_error_named_arg_duplicate(Sema *s, int line, int col, int len, const char *arg_name)
{
    char detail[256];
    size_t off = 0;
    detail[0] = 0;
    if (!arg_name || !arg_name[0]) arg_name = "parameter";
    kn_append(detail, sizeof(detail), &off, "Duplicate named argument '");
    kn_append(detail, sizeof(detail), &off, arg_name);
    kn_append(detail, sizeof(detail), &off, "'");
    sema_error(s, line, col, len, "Duplicate Named Argument", detail);
}

static int call_has_named_args(const NameList *names)
{
    if (!names || names->count <= 0) return 0;
    for (int i = 0; i < names->count; i++)
    {
        const char *n = names->items[i];
        if (n && n[0]) return 1;
    }
    return 0;
}

static int find_param_name_index(const char **param_names, int param_count, const char *name)
{
    if (!param_names || !name || !name[0] || param_count <= 0) return -1;
    for (int i = 0; i < param_count; i++)
    {
        const char *pn = param_names[i];
        if (!pn || !pn[0]) continue;
        if (kn_strcmp(pn, name) == 0)
            return i;
    }
    return -1;
}

static const char **alloc_param_names_from_paramlist(const ParamList *params)
{
    if (!params || params->count <= 0) return 0;
    const char **out = (const char **)kn_malloc(sizeof(char *) * (size_t)params->count);
    if (!out) return 0;
    for (int i = 0; i < params->count; i++)
        out[i] = params->items[i].name;
    return out;
}

static const char **alloc_param_names_from_params(const Param *params, int count)
{
    if (!params || count <= 0) return 0;
    const char **out = (const char **)kn_malloc(sizeof(char *) * (size_t)count);
    if (!out) return 0;
    for (int i = 0; i < count; i++)
        out[i] = params[i].name;
    return out;
}

static int remap_named_args(Sema *s, int call_line, int call_col, ExprList *args, NameList *arg_names,
                            ParamList *typed_args, int param_count, const char **param_names)
{
    if (!args || !arg_names || args->count != arg_names->count)
        return 1;
    if (typed_args && typed_args->count != args->count)
        return 1;
    if (!call_has_named_args(arg_names))
        return 1;
    if (!param_names || param_count <= 0)
    {
        sema_error(s, call_line, call_col, 1, "Invalid Named Argument", "Named arguments are not supported for this callable");
        return 0;
    }

    int ok = 1;
    int argc = args->count;
    int *mapping = (int *)kn_malloc(sizeof(int) * (size_t)argc);
    int *used = (int *)kn_malloc(sizeof(int) * (size_t)(param_count > 0 ? param_count : 1));
    Expr **new_items = (Expr **)kn_malloc(sizeof(Expr *) * (size_t)argc);
    const char **new_names = (const char **)kn_malloc(sizeof(const char *) * (size_t)argc);
    Param *new_typed = typed_args ? (Param *)kn_malloc(sizeof(Param) * (size_t)argc) : 0;
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
    for (int i = 0; i < param_count; i++) used[i] = 0;

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
                sema_error_named_arg_unknown(s, args->items[i]->line, args->items[i]->col, 1, an);
                ok = 0;
                continue;
            }
            if (used[idx])
            {
                sema_error_named_arg_duplicate(s, args->items[i]->line, args->items[i]->col, 1, an);
                ok = 0;
                continue;
            }
            used[idx] = 1;
            mapping[i] = idx;
            continue;
        }

        if (saw_named)
        {
            sema_error(s, args->items[i]->line, args->items[i]->col, 1,
                       "Invalid Named Argument", "Positional argument cannot follow named argument");
            ok = 0;
        }
        while (next_pos < param_count && used[next_pos]) next_pos++;
        if (next_pos < param_count)
        {
            mapping[i] = next_pos;
            used[next_pos] = 1;
            next_pos++;
        }
    }

    int out = 0;
    for (int p = 0; p < param_count; p++)
    {
        for (int i = 0; i < argc; i++)
        {
            if (mapping[i] == p)
            {
                new_items[out] = args->items[i];
                new_names[out] = arg_names->items[i];
                if (new_typed) new_typed[out] = typed_args->items[i];
                out++;
                break;
            }
        }
    }
    for (int i = 0; i < argc; i++)
    {
        int idx = mapping[i];
        if (idx < 0 || idx >= param_count)
        {
            new_items[out] = args->items[i];
            new_names[out] = arg_names->items[i];
            if (new_typed) new_typed[out] = typed_args->items[i];
            out++;
        }
    }

    for (int i = 0; i < argc; i++)
    {
        args->items[i] = new_items[i];
        arg_names->items[i] = new_names[i];
        if (new_typed) typed_args->items[i] = new_typed[i];
    }

    kn_free(mapping);
    kn_free(used);
    kn_free(new_items);
    kn_free(new_names);
    if (new_typed) kn_free(new_typed);
    return ok;
}

static void sema_warn_unused_var(Sema *s, VarSym *v)
{
    if (!s || !v || !v->name || !v->name[0] || v->is_param || v->used)
        return;
    if (v->name[0] == '_')
        return;

    char detail[256];
    size_t off = 0;
    detail[0] = 0;
    kn_append(detail, sizeof(detail), &off, "Variable '");
    kn_append(detail, sizeof(detail), &off, v->name);
    kn_append(detail, sizeof(detail), &off, "' is never used");
    kn_diag_warn(v->src ? v->src : s->src, KN_STAGE_SEMA, 2, v->line, v->col, (int)kn_strlen(v->name), "Unused Variable", detail);
}

static void sema_warn_unreachable(Sema *s, int line, int col, int len, const char *reason)
{
    char detail[256];
    size_t off = 0;
    detail[0] = 0;
    kn_append(detail, sizeof(detail), &off, "Unreachable code");
    if (reason && reason[0])
    {
        kn_append(detail, sizeof(detail), &off, ": ");
        kn_append(detail, sizeof(detail), &off, reason);
    }
    sema_warn(s, 2, line, col, len, "Unreachable Code", detail);
}

static const char *base_type_name(TypeKind k)
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

static bool is_integer_kind(TypeKind k);
static bool is_unsigned_kind(TypeKind k);
static int int_bits(TypeKind k);

static bool func_is_generic_template(const Func *f)
{
    return f && f->type_param_count > 0 && !f->is_generic_instance;
}

static bool func_is_generic_instance(const Func *f)
{
    return f && f->is_generic_instance;
}

static Method *find_method_sig(ClassDecl *c, const Method *sig, int *out_index)
{
    if (!c || !sig) return 0;
    for (int i = 0; i < c->methods.count; i++)
    {
        Method *m = &c->methods.items[i];
        if (m->is_constructor) continue;
        if (m->is_static != sig->is_static) continue;
        if (kn_strcmp(m->name, sig->name) != 0) continue;
        if (!type_equal(m->ret_type, sig->ret_type)) continue;
        if (!params_equal(&m->params, &sig->params)) continue;
        if (out_index) *out_index = i;
        return m;
    }
    return 0;
}

static Method *find_method_sig_hierarchy(Sema *s, ClassDecl *c, const Method *sig, ClassDecl **owner_out, int *out_index)
{
    ClassDecl *cur = c;
    while (cur)
    {
        int idx = -1;
        Method *m = find_method_sig(cur, sig, &idx);
        if (m)
        {
            if (owner_out) *owner_out = cur;
            if (out_index) *out_index = idx;
            return m;
        }
        if (!cur->base) break;
        cur = find_class(s, cur->base);
    }
    return 0;
}

static Method *find_virtual_in_base(Sema *s, ClassDecl *c, const Method *sig, ClassDecl **owner_out, int *out_index)
{
    if (!c || !c->base) return 0;
    ClassDecl *cur = find_class(s, c->base);
    while (cur)
    {
        int idx = -1;
        Method *m = find_method_sig(cur, sig, &idx);
        if (m && method_is_virtual(m))
        {
            if (owner_out) *owner_out = cur;
            if (out_index) *out_index = idx;
            return m;
        }
        if (!cur->base) break;
        cur = find_class(s, cur->base);
    }
    return 0;
}

static void class_add_interface(ClassDecl *c, const char *iface)
{
    if (!c || !iface) return;
    int n = c->interface_count;
    const char **arr = (const char **)kn_malloc(sizeof(char *) * (size_t)(n + 1));
    if (c->interfaces)
    {
        kn_memcpy(arr, c->interfaces, sizeof(char *) * (size_t)n);
        kn_free(c->interfaces);
    }
    arr[n] = iface;
    c->interfaces = arr;
    c->interface_count = n + 1;
}

static void resolve_named_type(Sema *s, Type *t)
{
    if (!t || !t->name) return;
    if (t->kind == TY_CLASS)
    {
        StructDecl *st = find_visible_struct(s, t->name);
        if (st)
        {
            t->kind = TY_STRUCT;
            t->name = struct_qname(st);
        }
        else
        {
            EnumDecl *en = find_visible_enum(s, t->name);
            if (en)
            {
                t->kind = TY_ENUM;
                t->name = enum_qname(en);
            }
            else
            {
                ClassDecl *c = find_visible_class(s, t->name);
                if (c)
                    t->name = class_qname(c);
                else
                {
                    InterfaceDecl *it = find_visible_interface(s, t->name);
                    if (it)
                        t->name = interface_qname(it);
                }
            }
        }
    }
    if ((t->kind == TY_PTR || t->kind == TY_ARRAY) && t->elem == TY_CLASS)
    {
        StructDecl *st = find_visible_struct(s, t->name);
        if (st)
        {
            t->elem = TY_STRUCT;
            t->name = struct_qname(st);
        }
        else
        {
            EnumDecl *en = find_visible_enum(s, t->name);
            if (en)
            {
                t->elem = TY_ENUM;
                t->name = enum_qname(en);
            }
            else
            {
                ClassDecl *c = find_visible_class(s, t->name);
                if (c)
                    t->name = class_qname(c);
                else
                {
                    InterfaceDecl *it = find_visible_interface(s, t->name);
                    if (it)
                        t->name = interface_qname(it);
                }
            }
        }
    }
    if ((t->kind == TY_PTR || t->kind == TY_ARRAY) && t->elem == TY_STRUCT)
    {
        StructDecl *st = find_struct(s, t->name);
        if (st)
            t->name = struct_qname(st);
    }
    if ((t->kind == TY_PTR || t->kind == TY_ARRAY) && t->elem == TY_ENUM)
    {
        EnumDecl *en = find_enum(s, t->name);
        if (en)
            t->name = enum_qname(en);
    }
}


#include "sema/kn_sema_validate.inc"
#include "sema/kn_sema_expr.inc"
#include "sema/kn_sema_stmt.inc"
