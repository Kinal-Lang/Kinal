#include "kn/builtins.h"

#include "kn/std.h"
#include "kn/util.h"

static char *dup_str(const char *s)
{
    size_t n = kn_strlen(s);
    char *out = (char *)kn_malloc(n + 1);
    if (!out) return 0;
    kn_memcpy(out, s, n);
    out[n] = 0;
    return out;
}

static int class_exists(ClassList *classes, const char *name)
{
    if (!classes || !name) return 0;
    for (int i = 0; i < classes->count; i++)
    {
        if (kn_strcmp(classes->items[i].name, name) == 0)
            return 1;
    }
    return 0;
}

static void inject_builtin_object_class(ClassList *classes)
{
    if (!classes || class_exists(classes, "IO.Type.Object.Class"))
        return;

    ClassDecl c;
    kn_memset(&c, 0, sizeof(c));
    c.name = dup_str("IO.Type.Object.Class");
    c.unit = dup_str("IO.Type.Object");

    classlist_push(classes, c);
}

static void inject_builtin_object_function(ClassList *classes)
{
    if (!classes || class_exists(classes, "IO.Type.Object.Function"))
        return;

    ClassDecl c;
    kn_memset(&c, 0, sizeof(c));
    c.name = dup_str("IO.Type.Object.Function");
    c.base = dup_str("IO.Type.Object.Class");
    c.unit = dup_str("IO.Type.Object");

    Field invoke;
    kn_memset(&invoke, 0, sizeof(invoke));
    invoke.name = dup_str("Invoke");
    invoke.type = type_ptr(TY_BYTE);
    invoke.access = ACC_PUBLIC;
    fieldlist_push(&c.fields, invoke);

    Field env;
    kn_memset(&env, 0, sizeof(env));
    env.name = dup_str("Env");
    env.type = type_ptr(TY_BYTE);
    env.access = ACC_PUBLIC;
    fieldlist_push(&c.fields, env);

    classlist_push(classes, c);
}

static void inject_builtin_object_block(ClassList *classes)
{
    if (!classes || class_exists(classes, "IO.Type.Object.Block"))
        return;

    ClassDecl c;
    kn_memset(&c, 0, sizeof(c));
    c.name = dup_str("IO.Type.Object.Block");
    c.base = dup_str("IO.Type.Object.Class");
    c.unit = dup_str("IO.Type.Object");

    Field invoke;
    kn_memset(&invoke, 0, sizeof(invoke));
    invoke.name = dup_str("Invoke");
    invoke.type = type_ptr(TY_BYTE);
    invoke.access = ACC_PUBLIC;
    fieldlist_push(&c.fields, invoke);

    Field env;
    kn_memset(&env, 0, sizeof(env));
    env.name = dup_str("Env");
    env.type = type_ptr(TY_BYTE);
    env.access = ACC_PUBLIC;
    fieldlist_push(&c.fields, env);

    classlist_push(classes, c);
}

static void inject_builtin_io_error(ClassList *classes)
{
    if (!classes || class_exists(classes, "IO.Error"))
        return;

    ClassDecl c;
    kn_memset(&c, 0, sizeof(c));
    c.name = dup_str("IO.Error");
    c.base = dup_str("IO.Type.Object.Class");
    c.unit = dup_str("IO");

    Field title;
    kn_memset(&title, 0, sizeof(title));
    title.name = dup_str("Title");
    title.type = type_make(TY_STRING);
    title.access = ACC_PUBLIC;
    fieldlist_push(&c.fields, title);

    Field message;
    kn_memset(&message, 0, sizeof(message));
    message.name = dup_str("Message");
    message.type = type_make(TY_STRING);
    message.access = ACC_PUBLIC;
    fieldlist_push(&c.fields, message);

    Field inner;
    kn_memset(&inner, 0, sizeof(inner));
    inner.name = dup_str("Inner");
    inner.type = type_class("IO.Error");
    inner.access = ACC_PUBLIC;
    fieldlist_push(&c.fields, inner);

    Field trace;
    kn_memset(&trace, 0, sizeof(trace));
    trace.name = dup_str("Trace");
    trace.type = type_make(TY_STRING);
    trace.access = ACC_PUBLIC;
    fieldlist_push(&c.fields, trace);

    Param p1; kn_memset(&p1, 0, sizeof(p1)); p1.name = dup_str("title"); p1.type = type_make(TY_STRING); p1.src = 0;
    Param p2; kn_memset(&p2, 0, sizeof(p2)); p2.name = dup_str("message"); p2.type = type_make(TY_STRING); p2.src = 0;
    ParamList params = {0};
    paramlist_push(&params, p1);
    paramlist_push(&params, p2);

    Method ctor;
    kn_memset(&ctor, 0, sizeof(ctor));
    ctor.name = "Constructor";
    ctor.ret_type = type_make(TY_VOID);
    ctor.params = params;
    ctor.safety = SAFETY_SAFE;
    ctor.access = ACC_PUBLIC;
    ctor.is_constructor = 1;

    Stmt *body = new_stmt(ST_BLOCK, 1, 1);
    // This.Title = title;
    Expr *lhs1 = new_expr(EXPR_MEMBER, 1, 1);
    lhs1->v.member.recv = new_expr(EXPR_THIS, 1, 1);
    lhs1->v.member.name = "Title";
    lhs1->v.member.is_static = 0;
    lhs1->v.member.static_type = 0;
    lhs1->v.member.field_index = -1;
    lhs1->v.member.field_owner = 0;
    lhs1->v.member.is_enum_const = 0;
    Expr *rhs1 = new_expr(EXPR_VAR, 1, 1);
    rhs1->v.var.name = "title";
    Expr *as1 = new_expr(EXPR_ASSIGN, 1, 1);
    as1->v.assign.target = lhs1;
    as1->v.assign.value = rhs1;
    Stmt *st1 = new_stmt(ST_EXPR, 1, 1);
    st1->v.expr.expr = as1;
    stmtlist_push(&body->v.block.stmts, st1);

    // This.Message = message;
    Expr *lhs2 = new_expr(EXPR_MEMBER, 1, 1);
    lhs2->v.member.recv = new_expr(EXPR_THIS, 1, 1);
    lhs2->v.member.name = "Message";
    lhs2->v.member.is_static = 0;
    lhs2->v.member.static_type = 0;
    lhs2->v.member.field_index = -1;
    lhs2->v.member.field_owner = 0;
    lhs2->v.member.is_enum_const = 0;
    Expr *rhs2 = new_expr(EXPR_VAR, 1, 1);
    rhs2->v.var.name = "message";
    Expr *as2 = new_expr(EXPR_ASSIGN, 1, 1);
    as2->v.assign.target = lhs2;
    as2->v.assign.value = rhs2;
    Stmt *st2 = new_stmt(ST_EXPR, 1, 1);
    st2->v.expr.expr = as2;
    stmtlist_push(&body->v.block.stmts, st2);

    // This.Trace = "";
    Expr *lhs3 = new_expr(EXPR_MEMBER, 1, 1);
    lhs3->v.member.recv = new_expr(EXPR_THIS, 1, 1);
    lhs3->v.member.name = "Trace";
    lhs3->v.member.is_static = 0;
    lhs3->v.member.static_type = 0;
    lhs3->v.member.field_index = -1;
    lhs3->v.member.field_owner = 0;
    lhs3->v.member.is_enum_const = 0;
    Expr *rhs3 = new_expr(EXPR_STRING, 1, 1);
    rhs3->v.str_val.ptr = "";
    rhs3->v.str_val.len = 0;
    Expr *as3 = new_expr(EXPR_ASSIGN, 1, 1);
    as3->v.assign.target = lhs3;
    as3->v.assign.value = rhs3;
    Stmt *st3 = new_stmt(ST_EXPR, 1, 1);
    st3->v.expr.expr = as3;
    stmtlist_push(&body->v.block.stmts, st3);

    ctor.body = body;
    methodlist_push(&c.methods, ctor);

    classlist_push(classes, c);
}

static void inject_builtin_io_time_datetime(ClassList *classes)
{
    if (!classes || class_exists(classes, "IO.Time.DateTime"))
        return;

    ClassDecl c;
    kn_memset(&c, 0, sizeof(c));
    c.name = dup_str("IO.Time.DateTime");
    c.unit = dup_str("IO.Time");

    const char *names[] = {
        "Ticks", "Year", "Month", "Day", "Hour", "Minute", "Second", "Millisecond"
    };
    for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])); i++)
    {
        Field f;
        kn_memset(&f, 0, sizeof(f));
        f.name = dup_str(names[i]);
        f.type = type_make(TY_INT);
        f.access = ACC_PUBLIC;
        fieldlist_push(&c.fields, f);
    }

    classlist_push(classes, c);
}

static void inject_builtin_io_collection_dict(ClassList *classes)
{
    if (!classes || class_exists(classes, "IO.Collection.dict"))
        return;

    ClassDecl c;
    kn_memset(&c, 0, sizeof(c));
    c.name = dup_str("IO.Collection.dict");
    c.base = dup_str("IO.Type.Object.Class");
    c.unit = dup_str("IO.Collection");
    c.is_sealed = 1;
    classlist_push(classes, c);
}

static void inject_builtin_io_collection_list(ClassList *classes)
{
    if (!classes || class_exists(classes, "IO.Collection.list"))
        return;
    ClassDecl c;
    kn_memset(&c, 0, sizeof(c));
    c.name = dup_str("IO.Collection.list");
    c.base = dup_str("IO.Type.Object.Class");
    c.unit = dup_str("IO.Collection");
    c.is_sealed = 1;
    classlist_push(classes, c);
}

static void inject_builtin_io_collection_set(ClassList *classes)
{
    if (!classes || class_exists(classes, "IO.Collection.set"))
        return;
    ClassDecl c;
    kn_memset(&c, 0, sizeof(c));
    c.name = dup_str("IO.Collection.set");
    c.base = dup_str("IO.Type.Object.Class");
    c.unit = dup_str("IO.Collection");
    c.is_sealed = 1;
    classlist_push(classes, c);
}

static void inject_builtin_io_async_task(ClassList *classes)
{
    if (!classes || class_exists(classes, "IO.Async.Task"))
        return;

    ClassDecl c;
    kn_memset(&c, 0, sizeof(c));
    c.name = dup_str("IO.Async.Task");
    c.base = dup_str("IO.Type.Object.Class");
    c.unit = dup_str("IO.Async");
    c.is_sealed = 1;
    classlist_push(classes, c);
}

void kn_inject_builtins(ClassList *classes)
{
    inject_builtin_object_class(classes);
    inject_builtin_object_function(classes);
    inject_builtin_object_block(classes);
    if (kn_std_builtin_allowed(KN_BUILTIN_IO_ERROR_CURRENT))
        inject_builtin_io_error(classes);
    if (kn_std_builtin_allowed(KN_BUILTIN_IO_TIME_NOW))
        inject_builtin_io_time_datetime(classes);
    if (kn_std_builtin_allowed(KN_BUILTIN_IO_DICT_NEW))
        inject_builtin_io_collection_dict(classes);
    if (kn_std_builtin_allowed(KN_BUILTIN_IO_LIST_NEW))
        inject_builtin_io_collection_list(classes);
    if (kn_std_builtin_allowed(KN_BUILTIN_IO_SET_NEW))
        inject_builtin_io_collection_set(classes);
    if (kn_std_builtin_allowed(KN_BUILTIN_IO_ASYNC_RUN))
        inject_builtin_io_async_task(classes);
}
