#include "kn/hir.h"

#include "kn/std.h"

typedef struct
{
    FuncList *funcs;
    ClassList *classes;
    InterfaceList *interfaces;
} KnHirContext;

static const char *hir_func_name(const Func *func)
{
    if (!func) return 0;
    return func->qname ? func->qname : func->name;
}

static const char *hir_class_name(const ClassDecl *decl)
{
    if (!decl) return 0;
    return decl->qname ? decl->qname : decl->name;
}

static const char *hir_interface_name(const InterfaceDecl *decl)
{
    if (!decl) return 0;
    return decl->qname ? decl->qname : decl->name;
}

static Func *hir_find_func(const KnHirContext *ctx, const char *name)
{
    if (!ctx || !ctx->funcs || !name) return 0;
    for (int i = 0; i < ctx->funcs->count; i++)
    {
        Func *func = &ctx->funcs->items[i];
        const char *candidate = hir_func_name(func);
        if (candidate && kn_strcmp(candidate, name) == 0)
            return func;
    }
    return 0;
}

static ClassDecl *hir_find_class(const KnHirContext *ctx, const char *name)
{
    if (!ctx || !ctx->classes || !name) return 0;
    for (int i = 0; i < ctx->classes->count; i++)
    {
        ClassDecl *decl = &ctx->classes->items[i];
        const char *candidate = hir_class_name(decl);
        if (candidate && kn_strcmp(candidate, name) == 0)
            return decl;
    }
    return 0;
}

static InterfaceDecl *hir_find_interface(const KnHirContext *ctx, const char *name)
{
    if (!ctx || !ctx->interfaces || !name) return 0;
    for (int i = 0; i < ctx->interfaces->count; i++)
    {
        InterfaceDecl *decl = &ctx->interfaces->items[i];
        const char *candidate = hir_interface_name(decl);
        if (candidate && kn_strcmp(candidate, name) == 0)
            return decl;
    }
    return 0;
}

static int hir_method_is_virtual(const Method *method)
{
    return method && (method->is_virtual || method->is_override || method->is_abstract);
}

static void hir_reset_call(Expr *expr)
{
    kn_memset(&expr->resolved_call, 0, sizeof(expr->resolved_call));
    expr->resolved_call.builtin_id = KN_BUILTIN_NONE;
    expr->resolved_call.method_index = -1;
}

static void hir_resolve_direct_call(const KnHirContext *ctx, Expr *expr)
{
    KnResolvedCall *target = &expr->resolved_call;
    hir_reset_call(expr);
    if (expr->v.call.builtin_id != KN_BUILTIN_NONE)
    {
        target->kind = KN_CALL_TARGET_BUILTIN;
        target->builtin_id = expr->v.call.builtin_id;
        return;
    }

    Func *func = hir_find_func(ctx, expr->v.call.name);
    if (!func)
        return;
    target->symbol = hir_func_name(func);
    if (!func->is_delegate)
        target->kind = KN_CALL_TARGET_FUNCTION;
    else if (func->delegate_abi == KN_DELEGATE_ABI_C)
        target->kind = KN_CALL_TARGET_DELEGATE_C;
    else
        target->kind = KN_CALL_TARGET_DELEGATE_KINAL;
}

static void hir_resolve_member_call(const KnHirContext *ctx, Expr *expr)
{
    KnResolvedCall *target = &expr->resolved_call;
    hir_reset_call(expr);
    if (expr->v.member_call.builtin_id != KN_BUILTIN_NONE)
    {
        target->kind = KN_CALL_TARGET_BUILTIN;
        target->builtin_id = expr->v.member_call.builtin_id;
        target->has_receiver = expr->v.member_call.is_static ? 0 : 1;
        return;
    }
    if (expr->v.member_call.is_qual_call && expr->v.member_call.qual_name)
    {
        Func *func = hir_find_func(ctx, expr->v.member_call.qual_name);
        target->symbol = func ? hir_func_name(func) : expr->v.member_call.qual_name;
        if (func && func->is_delegate)
        {
            target->kind = func->delegate_abi == KN_DELEGATE_ABI_C
                ? KN_CALL_TARGET_DELEGATE_C
                : KN_CALL_TARGET_DELEGATE_KINAL;
        }
        else
        {
            target->kind = KN_CALL_TARGET_FUNCTION;
        }
        return;
    }
    if (!expr->v.member_call.method_owner || expr->v.member_call.method_index < 0)
        return;

    target->owner = expr->v.member_call.method_owner;
    target->method_index = expr->v.member_call.method_index;
    target->has_receiver = expr->v.member_call.is_static ? 0 : 1;

    ClassDecl *owner = hir_find_class(ctx, target->owner);
    if (owner)
    {
        int index = target->method_index;
        if (index < 0 || index >= owner->methods.count)
            return;
        Method *method = &owner->methods.items[index];
        if (method->is_static)
            target->kind = KN_CALL_TARGET_STATIC_METHOD;
        else if (expr->v.member_call.recv && expr->v.member_call.recv->kind != EXPR_BASE &&
                 hir_method_is_virtual(method))
            target->kind = KN_CALL_TARGET_VIRTUAL_METHOD;
        else
            target->kind = KN_CALL_TARGET_DIRECT_METHOD;
        return;
    }

    InterfaceDecl *interface_decl = hir_find_interface(ctx, target->owner);
    if (interface_decl && target->method_index < interface_decl->methods.count)
        target->kind = KN_CALL_TARGET_INTERFACE_METHOD;
}

static void hir_lower_expr(const KnHirContext *ctx, Expr *expr);
static void hir_lower_stmt(const KnHirContext *ctx, Stmt *stmt);

static void hir_lower_expr_list(const KnHirContext *ctx, ExprList *list)
{
    if (!list) return;
    for (int i = 0; i < list->count; i++)
        hir_lower_expr(ctx, list->items[i]);
}

static void hir_lower_stmt(const KnHirContext *ctx, Stmt *stmt)
{
    if (!stmt) return;
    switch (stmt->kind)
    {
    case ST_BLOCK:
        for (int i = 0; i < stmt->v.block.stmts.count; i++)
            hir_lower_stmt(ctx, stmt->v.block.stmts.items[i]);
        break;
    case ST_VAR: hir_lower_expr(ctx, stmt->v.var.init); break;
    case ST_ASSIGN: hir_lower_expr(ctx, stmt->v.assign.value); break;
    case ST_EXPR: hir_lower_expr(ctx, stmt->v.expr.expr); break;
    case ST_IF:
        hir_lower_expr(ctx, stmt->v.ifs.cond);
        hir_lower_stmt(ctx, stmt->v.ifs.then_s);
        hir_lower_stmt(ctx, stmt->v.ifs.else_s);
        break;
    case ST_SWITCH:
        hir_lower_expr(ctx, stmt->v.switchs.value);
        for (int i = 0; i < stmt->v.switchs.cases.count; i++)
        {
            hir_lower_expr(ctx, stmt->v.switchs.cases.items[i].match);
            hir_lower_stmt(ctx, stmt->v.switchs.cases.items[i].body);
        }
        break;
    case ST_WHILE:
        hir_lower_expr(ctx, stmt->v.whiles.cond);
        hir_lower_stmt(ctx, stmt->v.whiles.body);
        break;
    case ST_FOR:
        hir_lower_stmt(ctx, stmt->v.fors.init);
        hir_lower_expr(ctx, stmt->v.fors.cond);
        hir_lower_expr(ctx, stmt->v.fors.post);
        hir_lower_stmt(ctx, stmt->v.fors.body);
        break;
    case ST_RETURN: hir_lower_expr(ctx, stmt->v.ret.expr); break;
    case ST_TRY:
        hir_lower_stmt(ctx, stmt->v.trys.try_block);
        hir_lower_stmt(ctx, stmt->v.trys.catch_block);
        break;
    case ST_THROW: hir_lower_expr(ctx, stmt->v.throws.expr); break;
    case ST_BLOCK_JUMP: hir_lower_expr(ctx, stmt->v.jump.target); break;
    case ST_BREAK:
    case ST_CONTINUE:
    case ST_BLOCK_RECORD:
        break;
    }
}

static void hir_lower_expr(const KnHirContext *ctx, Expr *expr)
{
    if (!expr) return;
    switch (expr->kind)
    {
    case EXPR_CALL:
        hir_lower_expr_list(ctx, &expr->v.call.args);
        hir_resolve_direct_call(ctx, expr);
        break;
    case EXPR_MEMBER_CALL:
        hir_lower_expr(ctx, expr->v.member_call.recv);
        hir_lower_expr_list(ctx, &expr->v.member_call.args);
        hir_resolve_member_call(ctx, expr);
        break;
    case EXPR_INVOKE:
        hir_lower_expr(ctx, expr->v.invoke.callee);
        hir_lower_expr_list(ctx, &expr->v.invoke.args);
        break;
    case EXPR_BINARY:
        hir_lower_expr(ctx, expr->v.binary.left);
        hir_lower_expr(ctx, expr->v.binary.right);
        break;
    case EXPR_UNARY: hir_lower_expr(ctx, expr->v.unary.expr); break;
    case EXPR_ARRAY: hir_lower_expr_list(ctx, &expr->v.array.items); break;
    case EXPR_PACKAGE: hir_lower_expr_list(ctx, &expr->v.package.items); break;
    case EXPR_DICT:
        hir_lower_expr_list(ctx, &expr->v.dict.keys);
        hir_lower_expr_list(ctx, &expr->v.dict.values);
        break;
    case EXPR_ASSIGN:
        hir_lower_expr(ctx, expr->v.assign.target);
        hir_lower_expr(ctx, expr->v.assign.value);
        break;
    case EXPR_MEMBER: hir_lower_expr(ctx, expr->v.member.recv); break;
    case EXPR_INDEX:
        hir_lower_expr(ctx, expr->v.index.recv);
        hir_lower_expr(ctx, expr->v.index.index);
        break;
    case EXPR_IF:
        hir_lower_expr(ctx, expr->v.if_expr.cond);
        hir_lower_expr(ctx, expr->v.if_expr.then_expr);
        hir_lower_expr(ctx, expr->v.if_expr.else_expr);
        break;
    case EXPR_SWITCH:
        hir_lower_expr(ctx, expr->v.switch_expr.value);
        for (int i = 0; i < expr->v.switch_expr.cases.count; i++)
        {
            hir_lower_expr(ctx, expr->v.switch_expr.cases.items[i].match);
            hir_lower_expr(ctx, expr->v.switch_expr.cases.items[i].body);
        }
        break;
    case EXPR_IS: hir_lower_expr(ctx, expr->v.is_expr.expr); break;
    case EXPR_NEW: hir_lower_expr_list(ctx, &expr->v.new_expr.args); break;
    case EXPR_CAST: hir_lower_expr(ctx, expr->v.cast.expr); break;
    case EXPR_ANON_FUNC:
        for (int i = 0; i < expr->v.anon_func.param_count; i++)
            hir_lower_expr(ctx, expr->v.anon_func.params[i].default_value);
        hir_lower_stmt(ctx, expr->v.anon_func.body);
        break;
    case EXPR_BLOCK_LITERAL: hir_lower_stmt(ctx, expr->v.block_lit.body); break;
    case EXPR_AWAIT: hir_lower_expr(ctx, expr->v.await_expr.expr); break;
    case EXPR_INT:
    case EXPR_FLOAT:
    case EXPR_BOOL:
    case EXPR_CHAR:
    case EXPR_DEFAULT:
    case EXPR_NULL:
    case EXPR_STRING:
    case EXPR_VAR:
    case EXPR_THIS:
    case EXPR_BASE:
    case EXPR_FUNC_REF:
        break;
    }
}

static void hir_lower_params(const KnHirContext *ctx, ParamList *params)
{
    if (!params) return;
    for (int i = 0; i < params->count; i++)
        hir_lower_expr(ctx, params->items[i].default_value);
}

void kn_hir_lower_calls(FuncList *funcs, ClassList *classes, InterfaceList *interfaces,
                        StmtList *globals)
{
    KnHirContext ctx;
    ctx.funcs = funcs;
    ctx.classes = classes;
    ctx.interfaces = interfaces;

    if (globals)
        for (int i = 0; i < globals->count; i++)
            hir_lower_stmt(&ctx, globals->items[i]);
    if (funcs)
        for (int i = 0; i < funcs->count; i++)
        {
            hir_lower_params(&ctx, &funcs->items[i].params);
            hir_lower_stmt(&ctx, funcs->items[i].body);
        }
    if (classes)
        for (int i = 0; i < classes->count; i++)
        {
            ClassDecl *decl = &classes->items[i];
            for (int f = 0; f < decl->fields.count; f++)
                hir_lower_expr(&ctx, decl->fields.items[f].init);
            for (int m = 0; m < decl->methods.count; m++)
            {
                hir_lower_params(&ctx, &decl->methods.items[m].params);
                hir_lower_stmt(&ctx, decl->methods.items[m].body);
            }
        }
    if (interfaces)
        for (int i = 0; i < interfaces->count; i++)
            for (int m = 0; m < interfaces->items[i].methods.count; m++)
            {
                hir_lower_params(&ctx, &interfaces->items[i].methods.items[m].params);
                hir_lower_stmt(&ctx, interfaces->items[i].methods.items[m].body);
            }
}

static void hir_stats_expr(const Expr *expr, KnHirStats *stats);
static void hir_stats_stmt(const Stmt *stmt, KnHirStats *stats);

static void hir_stats_expr_list(const ExprList *list, KnHirStats *stats)
{
    if (!list) return;
    for (int i = 0; i < list->count; i++)
        hir_stats_expr(list->items[i], stats);
}

static void hir_stats_call(const Expr *expr, KnHirStats *stats)
{
    stats->calls++;
    switch (expr->resolved_call.kind)
    {
    case KN_CALL_TARGET_BUILTIN: stats->builtin_calls++; break;
    case KN_CALL_TARGET_FUNCTION: stats->function_calls++; break;
    case KN_CALL_TARGET_DELEGATE_KINAL:
    case KN_CALL_TARGET_DELEGATE_C: stats->delegate_calls++; break;
    case KN_CALL_TARGET_STATIC_METHOD: stats->static_method_calls++; break;
    case KN_CALL_TARGET_DIRECT_METHOD: stats->direct_method_calls++; break;
    case KN_CALL_TARGET_VIRTUAL_METHOD: stats->virtual_method_calls++; break;
    case KN_CALL_TARGET_INTERFACE_METHOD: stats->interface_method_calls++; break;
    case KN_CALL_TARGET_UNRESOLVED: stats->unresolved_calls++; break;
    }
}

static void hir_stats_binary(const Expr *expr, KnHirStats *stats)
{
    stats->binaries++;
    switch (expr->resolved_binary.kind)
    {
    case KN_BINARY_LOWER_NUMERIC_ARITHMETIC: stats->binary_numeric_arithmetic++; break;
    case KN_BINARY_LOWER_STRING_CONCAT: stats->binary_string_concat++; break;
    case KN_BINARY_LOWER_POINTER_ARITHMETIC: stats->binary_pointer_arithmetic++; break;
    case KN_BINARY_LOWER_BITWISE: stats->binary_bitwise++; break;
    case KN_BINARY_LOWER_STRING_EQUALITY: stats->binary_string_equality++; break;
    case KN_BINARY_LOWER_REFERENCE_EQUALITY: stats->binary_reference_equality++; break;
    case KN_BINARY_LOWER_SCALAR_EQUALITY: stats->binary_scalar_equality++; break;
    case KN_BINARY_LOWER_NUMERIC_COMPARISON: stats->binary_numeric_comparison++; break;
    case KN_BINARY_LOWER_LOGICAL_SHORT_CIRCUIT: stats->binary_logical_short_circuit++; break;
    case KN_BINARY_LOWER_UNRESOLVED: stats->unresolved_binaries++; break;
    }
}

static void hir_stats_stmt(const Stmt *stmt, KnHirStats *stats)
{
    if (!stmt) return;
    switch (stmt->kind)
    {
    case ST_BLOCK:
        for (int i = 0; i < stmt->v.block.stmts.count; i++)
            hir_stats_stmt(stmt->v.block.stmts.items[i], stats);
        break;
    case ST_VAR: hir_stats_expr(stmt->v.var.init, stats); break;
    case ST_ASSIGN: hir_stats_expr(stmt->v.assign.value, stats); break;
    case ST_EXPR: hir_stats_expr(stmt->v.expr.expr, stats); break;
    case ST_IF:
        hir_stats_expr(stmt->v.ifs.cond, stats);
        hir_stats_stmt(stmt->v.ifs.then_s, stats);
        hir_stats_stmt(stmt->v.ifs.else_s, stats);
        break;
    case ST_SWITCH:
        hir_stats_expr(stmt->v.switchs.value, stats);
        for (int i = 0; i < stmt->v.switchs.cases.count; i++)
        {
            hir_stats_expr(stmt->v.switchs.cases.items[i].match, stats);
            hir_stats_stmt(stmt->v.switchs.cases.items[i].body, stats);
        }
        break;
    case ST_WHILE:
        hir_stats_expr(stmt->v.whiles.cond, stats);
        hir_stats_stmt(stmt->v.whiles.body, stats);
        break;
    case ST_FOR:
        hir_stats_stmt(stmt->v.fors.init, stats);
        hir_stats_expr(stmt->v.fors.cond, stats);
        hir_stats_expr(stmt->v.fors.post, stats);
        hir_stats_stmt(stmt->v.fors.body, stats);
        break;
    case ST_RETURN: hir_stats_expr(stmt->v.ret.expr, stats); break;
    case ST_TRY:
        hir_stats_stmt(stmt->v.trys.try_block, stats);
        hir_stats_stmt(stmt->v.trys.catch_block, stats);
        break;
    case ST_THROW: hir_stats_expr(stmt->v.throws.expr, stats); break;
    case ST_BLOCK_JUMP: hir_stats_expr(stmt->v.jump.target, stats); break;
    case ST_BREAK:
    case ST_CONTINUE:
    case ST_BLOCK_RECORD:
        break;
    }
}

static void hir_stats_expr(const Expr *expr, KnHirStats *stats)
{
    if (!expr) return;
    stats->expressions++;
    if (expr->type.kind != TY_UNKNOWN)
        stats->typed_expressions++;
    switch (expr->kind)
    {
    case EXPR_CALL:
        hir_stats_call(expr, stats);
        hir_stats_expr_list(&expr->v.call.args, stats);
        break;
    case EXPR_MEMBER_CALL:
        hir_stats_call(expr, stats);
        hir_stats_expr(expr->v.member_call.recv, stats);
        hir_stats_expr_list(&expr->v.member_call.args, stats);
        break;
    case EXPR_INVOKE:
        hir_stats_expr(expr->v.invoke.callee, stats);
        hir_stats_expr_list(&expr->v.invoke.args, stats);
        break;
    case EXPR_BINARY:
        hir_stats_binary(expr, stats);
        hir_stats_expr(expr->v.binary.left, stats);
        hir_stats_expr(expr->v.binary.right, stats);
        break;
    case EXPR_UNARY: hir_stats_expr(expr->v.unary.expr, stats); break;
    case EXPR_ARRAY: hir_stats_expr_list(&expr->v.array.items, stats); break;
    case EXPR_PACKAGE: hir_stats_expr_list(&expr->v.package.items, stats); break;
    case EXPR_DICT:
        hir_stats_expr_list(&expr->v.dict.keys, stats);
        hir_stats_expr_list(&expr->v.dict.values, stats);
        break;
    case EXPR_ASSIGN:
        hir_stats_expr(expr->v.assign.target, stats);
        hir_stats_expr(expr->v.assign.value, stats);
        break;
    case EXPR_MEMBER: hir_stats_expr(expr->v.member.recv, stats); break;
    case EXPR_INDEX:
        hir_stats_expr(expr->v.index.recv, stats);
        hir_stats_expr(expr->v.index.index, stats);
        break;
    case EXPR_IF:
        hir_stats_expr(expr->v.if_expr.cond, stats);
        hir_stats_expr(expr->v.if_expr.then_expr, stats);
        hir_stats_expr(expr->v.if_expr.else_expr, stats);
        break;
    case EXPR_SWITCH:
        hir_stats_expr(expr->v.switch_expr.value, stats);
        for (int i = 0; i < expr->v.switch_expr.cases.count; i++)
        {
            hir_stats_expr(expr->v.switch_expr.cases.items[i].match, stats);
            hir_stats_expr(expr->v.switch_expr.cases.items[i].body, stats);
        }
        break;
    case EXPR_IS: hir_stats_expr(expr->v.is_expr.expr, stats); break;
    case EXPR_NEW: hir_stats_expr_list(&expr->v.new_expr.args, stats); break;
    case EXPR_CAST: hir_stats_expr(expr->v.cast.expr, stats); break;
    case EXPR_ANON_FUNC:
        for (int i = 0; i < expr->v.anon_func.param_count; i++)
            hir_stats_expr(expr->v.anon_func.params[i].default_value, stats);
        hir_stats_stmt(expr->v.anon_func.body, stats);
        break;
    case EXPR_BLOCK_LITERAL: hir_stats_stmt(expr->v.block_lit.body, stats); break;
    case EXPR_AWAIT: hir_stats_expr(expr->v.await_expr.expr, stats); break;
    case EXPR_INT:
    case EXPR_FLOAT:
    case EXPR_BOOL:
    case EXPR_CHAR:
    case EXPR_DEFAULT:
    case EXPR_NULL:
    case EXPR_STRING:
    case EXPR_VAR:
    case EXPR_THIS:
    case EXPR_BASE:
    case EXPR_FUNC_REF:
        break;
    }
}

static void hir_stats_params(const ParamList *params, KnHirStats *stats)
{
    if (!params) return;
    for (int i = 0; i < params->count; i++)
        hir_stats_expr(params->items[i].default_value, stats);
}

void kn_hir_collect_stats(const FuncList *funcs, const ClassList *classes,
                          const InterfaceList *interfaces, const StmtList *globals,
                          KnHirStats *out)
{
    if (!out) return;
    kn_memset(out, 0, sizeof(*out));
    if (globals)
        for (int i = 0; i < globals->count; i++)
            hir_stats_stmt(globals->items[i], out);
    if (funcs)
        for (int i = 0; i < funcs->count; i++)
        {
            hir_stats_params(&funcs->items[i].params, out);
            hir_stats_stmt(funcs->items[i].body, out);
        }
    if (classes)
        for (int i = 0; i < classes->count; i++)
        {
            const ClassDecl *decl = &classes->items[i];
            for (int f = 0; f < decl->fields.count; f++)
                hir_stats_expr(decl->fields.items[f].init, out);
            for (int m = 0; m < decl->methods.count; m++)
            {
                hir_stats_params(&decl->methods.items[m].params, out);
                hir_stats_stmt(decl->methods.items[m].body, out);
            }
        }
    if (interfaces)
        for (int i = 0; i < interfaces->count; i++)
            for (int m = 0; m < interfaces->items[i].methods.count; m++)
            {
                hir_stats_params(&interfaces->items[i].methods.items[m].params, out);
                hir_stats_stmt(interfaces->items[i].methods.items[m].body, out);
            }
}

const char *kn_hir_call_target_name(KnCallTargetKind kind)
{
    switch (kind)
    {
    case KN_CALL_TARGET_BUILTIN: return "builtin";
    case KN_CALL_TARGET_FUNCTION: return "function";
    case KN_CALL_TARGET_DELEGATE_KINAL: return "delegate-kinal";
    case KN_CALL_TARGET_DELEGATE_C: return "delegate-c";
    case KN_CALL_TARGET_STATIC_METHOD: return "static-method";
    case KN_CALL_TARGET_DIRECT_METHOD: return "direct-method";
    case KN_CALL_TARGET_VIRTUAL_METHOD: return "virtual-method";
    case KN_CALL_TARGET_INTERFACE_METHOD: return "interface-method";
    case KN_CALL_TARGET_UNRESOLVED: return "unresolved";
    }
    return "unresolved";
}

const char *kn_hir_binary_lowering_name(KnBinaryLoweringKind kind)
{
    switch (kind)
    {
    case KN_BINARY_LOWER_NUMERIC_ARITHMETIC: return "numeric-arithmetic";
    case KN_BINARY_LOWER_STRING_CONCAT: return "string-concat";
    case KN_BINARY_LOWER_POINTER_ARITHMETIC: return "pointer-arithmetic";
    case KN_BINARY_LOWER_BITWISE: return "bitwise";
    case KN_BINARY_LOWER_STRING_EQUALITY: return "string-equality";
    case KN_BINARY_LOWER_REFERENCE_EQUALITY: return "reference-equality";
    case KN_BINARY_LOWER_SCALAR_EQUALITY: return "scalar-equality";
    case KN_BINARY_LOWER_NUMERIC_COMPARISON: return "numeric-comparison";
    case KN_BINARY_LOWER_LOGICAL_SHORT_CIRCUIT: return "logical-short-circuit";
    case KN_BINARY_LOWER_UNRESOLVED: return "unresolved";
    }
    return "unresolved";
}
