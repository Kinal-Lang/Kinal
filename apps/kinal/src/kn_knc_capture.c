#include "kn/knc_capture.h"
#include "kn/util.h"

typedef struct KncCaptureBinding
{
    const void *declaration;
    const char *name;
    int captured;
    struct KncCaptureBinding *outer;
    struct KncCaptureBinding *next;
} KncCaptureBinding;

struct KncCapturePlan
{
    KncCaptureBinding *bindings;
    KncCaptureBinding *scope;
};

static void scan_statement(KncCapturePlan *plan, Stmt *statement);
static void scan_expression(KncCapturePlan *plan, Expr *expression);

static void declare_binding(KncCapturePlan *plan, const void *declaration, const char *name)
{
    KncCaptureBinding *binding = (KncCaptureBinding *)kn_malloc(sizeof(*binding));
    binding->declaration = declaration;
    binding->name = name;
    binding->captured = 0;
    binding->outer = plan->scope;
    binding->next = plan->bindings;
    plan->scope = binding;
    plan->bindings = binding;
}

static void capture_visible_bindings(KncCapturePlan *plan)
{
    // Match the emitter's lexical environment: nearest declaration wins.
    for (KncCaptureBinding *binding = plan->scope; binding; binding = binding->outer)
    {
        int shadowed = 0;
        for (KncCaptureBinding *inner = plan->scope; inner != binding; inner = inner->outer)
            if (kn_strcmp(inner->name, binding->name) == 0) { shadowed = 1; break; }
        if (!shadowed) binding->captured = 1;
    }
}

static void scan_expressions(KncCapturePlan *plan, ExprList *expressions)
{
    for (int i = 0; i < expressions->count; i++)
        scan_expression(plan, expressions->items[i]);
}

static void scan_expression(KncCapturePlan *plan, Expr *expression)
{
    if (!expression) return;
    switch (expression->kind)
    {
    case EXPR_ANON_FUNC:
    case EXPR_BLOCK_LITERAL:
        capture_visible_bindings(plan);
        break;
    case EXPR_CALL:
        scan_expressions(plan, &expression->v.call.args);
        break;
    case EXPR_MEMBER_CALL:
        scan_expression(plan, expression->v.member_call.recv);
        scan_expressions(plan, &expression->v.member_call.args);
        break;
    case EXPR_INVOKE:
        scan_expression(plan, expression->v.invoke.callee);
        scan_expressions(plan, &expression->v.invoke.args);
        break;
    case EXPR_BINARY:
        scan_expression(plan, expression->v.binary.left);
        scan_expression(plan, expression->v.binary.right);
        break;
    case EXPR_UNARY:
        scan_expression(plan, expression->v.unary.expr);
        break;
    case EXPR_ARRAY:
        scan_expressions(plan, &expression->v.array.items);
        break;
    case EXPR_PACKAGE:
        scan_expressions(plan, &expression->v.package.items);
        break;
    case EXPR_DICT:
        scan_expressions(plan, &expression->v.dict.keys);
        scan_expressions(plan, &expression->v.dict.values);
        break;
    case EXPR_ASSIGN:
        scan_expression(plan, expression->v.assign.target);
        scan_expression(plan, expression->v.assign.value);
        break;
    case EXPR_MEMBER:
        scan_expression(plan, expression->v.member.recv);
        break;
    case EXPR_INDEX:
        scan_expression(plan, expression->v.index.recv);
        scan_expression(plan, expression->v.index.index);
        break;
    case EXPR_IF:
        scan_expression(plan, expression->v.if_expr.cond);
        scan_expression(plan, expression->v.if_expr.then_expr);
        scan_expression(plan, expression->v.if_expr.else_expr);
        break;
    case EXPR_SWITCH:
        scan_expression(plan, expression->v.switch_expr.value);
        for (int i = 0; i < expression->v.switch_expr.cases.count; i++)
        {
            scan_expression(plan, expression->v.switch_expr.cases.items[i].match);
            scan_expression(plan, expression->v.switch_expr.cases.items[i].body);
        }
        break;
    case EXPR_IS:
        scan_expression(plan, expression->v.is_expr.expr);
        break;
    case EXPR_NEW:
        scan_expressions(plan, &expression->v.new_expr.args);
        break;
    case EXPR_CAST:
        scan_expression(plan, expression->v.cast.expr);
        break;
    case EXPR_AWAIT:
        scan_expression(plan, expression->v.await_expr.expr);
        break;
    default:
        break;
    }
}

static void scan_statement(KncCapturePlan *plan, Stmt *statement)
{
    if (!statement) return;
    KncCaptureBinding *saved = plan->scope;
    switch (statement->kind)
    {
    case ST_BLOCK:
        for (int i = 0; i < statement->v.block.stmts.count; i++)
            scan_statement(plan, statement->v.block.stmts.items[i]);
        plan->scope = saved;
        break;
    case ST_VAR:
        scan_expression(plan, statement->v.var.init);
        declare_binding(plan, statement, statement->v.var.name);
        break;
    case ST_ASSIGN:
        scan_expression(plan, statement->v.assign.value);
        break;
    case ST_EXPR:
        scan_expression(plan, statement->v.expr.expr);
        break;
    case ST_RETURN:
        scan_expression(plan, statement->v.ret.expr);
        break;
    case ST_THROW:
        scan_expression(plan, statement->v.throws.expr);
        break;
    case ST_IF:
        if (statement->v.ifs.is_const)
        {
            scan_statement(plan, statement->v.ifs.const_value ?
                statement->v.ifs.then_s : statement->v.ifs.else_s);
            plan->scope = saved;
            break;
        }
        scan_expression(plan, statement->v.ifs.cond);
        if (statement->v.ifs.pattern_name)
            declare_binding(plan, statement, statement->v.ifs.pattern_name);
        scan_statement(plan, statement->v.ifs.then_s);
        plan->scope = saved;
        scan_statement(plan, statement->v.ifs.else_s);
        plan->scope = saved;
        break;
    case ST_SWITCH:
        scan_expression(plan, statement->v.switchs.value);
        for (int i = 0; i < statement->v.switchs.cases.count; i++)
        {
            scan_expression(plan, statement->v.switchs.cases.items[i].match);
            scan_statement(plan, statement->v.switchs.cases.items[i].body);
            plan->scope = saved;
        }
        break;
    case ST_WHILE:
        scan_expression(plan, statement->v.whiles.cond);
        scan_statement(plan, statement->v.whiles.body);
        plan->scope = saved;
        break;
    case ST_FOR:
        scan_statement(plan, statement->v.fors.init);
        scan_expression(plan, statement->v.fors.cond);
        scan_statement(plan, statement->v.fors.body);
        scan_expression(plan, statement->v.fors.post);
        plan->scope = saved;
        break;
    case ST_TRY:
        scan_statement(plan, statement->v.trys.try_block);
        plan->scope = saved;
        if (statement->v.trys.has_param)
            declare_binding(plan, statement, statement->v.trys.catch_name);
        scan_statement(plan, statement->v.trys.catch_block);
        plan->scope = saved;
        break;
    case ST_BLOCK_JUMP:
        scan_expression(plan, statement->v.jump.target);
        break;
    default:
        break;
    }
}

KncCapturePlan *knc_capture_plan_create(Stmt *body, ParamList *params, int has_receiver)
{
    KncCapturePlan *plan = (KncCapturePlan *)kn_malloc(sizeof(*plan));
    kn_memset(plan, 0, sizeof(*plan));
    if (has_receiver) declare_binding(plan, 0, "This");
    for (int i = 0; params && i < params->count; i++)
        declare_binding(plan, &params->items[i], params->items[i].name);
    scan_statement(plan, body);
    return plan;
}

int knc_capture_plan_contains(const KncCapturePlan *plan, const void *declaration)
{
    for (KncCaptureBinding *binding = plan ? plan->bindings : 0; binding; binding = binding->next)
        if (binding->declaration == declaration) return binding->captured;
    return 0;
}

void knc_capture_plan_dispose(KncCapturePlan *plan)
{
    if (!plan) return;
    while (plan->bindings)
    {
        KncCaptureBinding *next = plan->bindings->next;
        kn_free(plan->bindings);
        plan->bindings = next;
    }
    kn_free(plan);
}
